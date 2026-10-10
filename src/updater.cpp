#include "updater.hpp"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QMetaObject>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QVersionNumber>

#include <atomic>
#include <memory>
#include <thread>

#include <obs-frontend-api.h>
#include <plugin-support.h>

#include "engine.hpp"
#include "http.hpp"

static const char *kLatestReleaseApi = "https://api.github.com/repos/Rimfire03/elgato-lights-control/releases/latest";

static QString U(const char *s)
{
	return QString::fromUtf8(s);
}

static QVersionNumber parseVersion(const QString &s)
{
	QString v = s.trimmed();
	if (v.startsWith('v') || v.startsWith('V'))
		v.remove(0, 1);
	return QVersionNumber::fromString(v);
}

// Only ever download from GitHub over HTTPS.
static bool trustedUrl(const QUrl &u)
{
	const QString h = u.host().toLower();
	return u.scheme() == "https" &&
	       (h == "github.com" || h.endsWith(".github.com") || h.endsWith(".githubusercontent.com"));
}

Updater::Updater(Engine &engine, QObject *parent) : QObject(parent), engine_(engine) {}

QWidget *Updater::mainWindow() const
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

void Updater::checkAtStartup()
{
	if (!engine_.config().checkUpdates)
		return;

	// Network I/O on a worker thread; the result is handled on the Qt (UI) thread.
	std::thread([this]() {
		const HttpResult r =
			httpsGet(QString::fromUtf8(kLatestReleaseApi), "application/vnd.github+json", nullptr, 15);
		QMetaObject::invokeMethod(
			this,
			[this, r]() {
				if (!r.ok) {
					obs_log(LOG_INFO, "update check skipped: %s", r.error.toUtf8().constData());
					return;
				}
				const QJsonObject rel = QJsonDocument::fromJson(r.body).object();
				if (rel.isEmpty() || rel.value("draft").toBool() || rel.value("prerelease").toBool())
					return;
				onRelease(rel);
			},
			Qt::QueuedConnection);
	}).detach();
}

void Updater::onRelease(const QJsonObject &rel)
{
	const QString tag = rel.value("tag_name").toString();
	const QVersionNumber remote = parseVersion(tag);
	const QVersionNumber local = parseVersion(QString::fromUtf8(PLUGIN_VERSION));
	if (remote.isNull() || local.isNull())
		return;
	const QString remoteStr = remote.toString();
	if (QVersionNumber::compare(remote, local) <= 0) {
		obs_log(LOG_INFO, "plugin is up to date (installed %s, latest %s)", PLUGIN_VERSION,
			remoteStr.toUtf8().constData());
		return;
	}
	if (remoteStr == engine_.config().skippedVersion)
		return;
	obs_log(LOG_INFO, "new version available: %s (installed %s)", remoteStr.toUtf8().constData(), PLUGIN_VERSION);

	// Pick the Windows installer asset.
	QString assetUrl, assetName, digest;
#ifdef Q_OS_WIN
	const QRegularExpression re("^Install-Elgato-Lights.*\\.exe$", QRegularExpression::CaseInsensitiveOption);
	for (const QJsonValue &v : rel.value("assets").toArray()) {
		const QJsonObject a = v.toObject();
		if (re.match(a.value("name").toString()).hasMatch()) {
			assetName = a.value("name").toString();
			assetUrl = a.value("browser_download_url").toString();
			const QString d = a.value("digest").toString(); // "sha256:<hex>"
			if (d.startsWith("sha256:"))
				digest = d.mid(7);
			break;
		}
	}
#endif
	const bool canAutoUpdate = !assetUrl.isEmpty() && trustedUrl(QUrl(assetUrl));
	const QString pageUrl = rel.value("html_url").toString();

	QString notes = rel.value("body").toString().trimmed();
	if (notes.size() > 1200)
		notes = notes.left(1200) + U("…");

	QMessageBox box(mainWindow());
	box.setIcon(QMessageBox::Information);
	box.setWindowTitle(U("Elgato Lights Control - mise à jour"));
	box.setText(U("La version %1 du plugin est disponible (version installée : %2).")
			    .arg(remoteStr, QString::fromUtf8(PLUGIN_VERSION)));
	if (canAutoUpdate)
		box.setInformativeText(U("Le plugin sera mis à jour automatiquement à la fermeture d'OBS, "
					 "puis OBS sera relancé."));
	if (!notes.isEmpty())
		box.setDetailedText(notes);
	QPushButton *update = box.addButton(canAutoUpdate ? U("Mettre à jour") : U("Ouvrir la page de téléchargement"),
					    QMessageBox::AcceptRole);
	box.addButton(U("Plus tard"), QMessageBox::RejectRole);
	QPushButton *skip = box.addButton(U("Ignorer cette version"), QMessageBox::DestructiveRole);
	box.exec();

	if (box.clickedButton() == skip) {
		engine_.config().skippedVersion = remoteStr;
		engine_.config().save();
		return;
	}
	if (box.clickedButton() != update)
		return;

	if (canAutoUpdate)
		download(assetUrl, assetName, digest);
	else if (!pageUrl.isEmpty())
		QDesktopServices::openUrl(QUrl(pageUrl));
}

void Updater::download(const QString &url, const QString &fileName, const QString &sha256)
{
	auto cancel = std::make_shared<std::atomic<bool>>(false);

	// Indeterminate progress dialog (libcurl runs on a worker thread).
	auto *dlg = new QProgressDialog(U("Téléchargement de la mise à jour…"), U("Annuler"), 0, 0, mainWindow());
	dlg->setWindowModality(Qt::WindowModal);
	dlg->setMinimumDuration(0);
	dlg->setAutoClose(false);
	dlg->setAutoReset(false);
	connect(dlg, &QProgressDialog::canceled, this, [cancel]() { cancel->store(true); });
	dlg->show();

	std::thread([this, url, fileName, sha256, cancel, dlg]() {
		const HttpResult r = httpsGet(url, QByteArray(), cancel.get(), 300);
		QMetaObject::invokeMethod(
			this,
			[this, r, fileName, sha256, cancel, dlg]() {
				dlg->close();
				dlg->deleteLater();
				onDownloaded(r, fileName, sha256, cancel->load());
			},
			Qt::QueuedConnection);
	}).detach();
}

void Updater::onDownloaded(const HttpResult &r, const QString &fileName, const QString &sha256, bool cancelled)
{
	if (!r.ok) {
		if (!cancelled)
			QMessageBox::warning(mainWindow(), U("Elgato Lights Control"),
					     U("Échec du téléchargement : ") + r.error);
		return;
	}
	const QByteArray &data = r.body;
	if (data.size() < 4 || data.left(2) != "MZ") {
		QMessageBox::warning(mainWindow(), U("Elgato Lights Control"),
				     U("Le fichier téléchargé n'est pas un exécutable valide."));
		return;
	}
	if (!sha256.isEmpty() &&
	    QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex().toLower() != sha256.toLower().toUtf8()) {
		QMessageBox::warning(mainWindow(), U("Elgato Lights Control"),
				     U("Somme de contrôle SHA-256 invalide : mise à jour annulée."));
		return;
	}

	const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	const QString path = QDir(dir).filePath(QFileInfo(fileName).fileName());
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(data) != data.size()) {
		QMessageBox::warning(mainWindow(), U("Elgato Lights Control"),
				     U("Impossible d'enregistrer la mise à jour dans :\n") + path);
		return;
	}
	f.close();

	if (!QProcess::startDetached(path, {"/update"})) {
		QMessageBox::warning(mainWindow(), U("Elgato Lights Control"),
				     U("Impossible de lancer l'installateur :\n") + path);
		return;
	}
	QMessageBox::information(mainWindow(), U("Elgato Lights Control"),
				 U("Mise à jour prête.\n\nFermez OBS : le plugin sera mis à jour "
				   "automatiquement, puis OBS se relancera."));
}

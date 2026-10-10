#include "updater.hpp"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QPushButton>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QVersionNumber>

#include <obs-frontend-api.h>
#include <plugin-support.h>

#include "engine.hpp"

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

	QNetworkRequest req{QUrl(QString::fromUtf8(kLatestReleaseApi))};
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	req.setRawHeader("Accept", "application/vnd.github+json");
	req.setRawHeader("User-Agent", "obs-elgato-lights-updater");
	req.setTransferTimeout(8000);
	QNetworkReply *reply = nam_.get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			obs_log(LOG_INFO, "update check skipped: %s", reply->errorString().toUtf8().constData());
			return;
		}
		const QJsonObject rel = QJsonDocument::fromJson(reply->readAll()).object();
		if (rel.isEmpty() || rel.value("draft").toBool() || rel.value("prerelease").toBool())
			return;
		onRelease(rel);
	});
}

void Updater::onRelease(const QJsonObject &rel)
{
	const QString tag = rel.value("tag_name").toString();
	const QVersionNumber remote = parseVersion(tag);
	const QVersionNumber local = parseVersion(QString::fromUtf8(PLUGIN_VERSION));
	if (remote.isNull() || local.isNull() || QVersionNumber::compare(remote, local) <= 0)
		return;
	const QString remoteStr = remote.toString();
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
		notes = notes.left(1200) + U("â€¦");

	QMessageBox box(mainWindow());
	box.setIcon(QMessageBox::Information);
	box.setWindowTitle(U("Elgato Lights Control - mise Ã  jour"));
	box.setText(U("La version %1 du plugin est disponible (version installÃ©e : %2).")
			    .arg(remoteStr, QString::fromUtf8(PLUGIN_VERSION)));
	if (canAutoUpdate)
		box.setInformativeText(U("Le plugin sera mis Ã  jour automatiquement Ã  la fermeture d'OBS, "
					 "puis OBS sera relancÃ©."));
	if (!notes.isEmpty())
		box.setDetailedText(notes);
	QPushButton *update = box.addButton(
		canAutoUpdate ? U("Mettre Ã  jour") : U("Ouvrir la page de tÃ©lÃ©chargement"), QMessageBox::AcceptRole);
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
	QNetworkRequest req{QUrl(url)};
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	req.setRawHeader("User-Agent", "obs-elgato-lights-updater");
	req.setTransferTimeout(30000);
	QNetworkReply *reply = nam_.get(req);

	auto *dlg =
		new QProgressDialog(U("TÃ©lÃ©chargement de la mise Ã  jourâ€¦"), U("Annuler"), 0, 100, mainWindow());
	dlg->setWindowModality(Qt::WindowModal);
	dlg->setMinimumDuration(0);
	dlg->setAutoClose(false);
	dlg->setAutoReset(false);
	connect(dlg, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
	connect(reply, &QNetworkReply::downloadProgress, dlg, [dlg](qint64 got, qint64 total) {
		if (total > 0)
			dlg->setValue(int(got * 100 / total));
	});

	connect(reply, &QNetworkReply::finished, this, [this, reply, dlg, fileName, sha256]() {
		reply->deleteLater();
		dlg->close();
		dlg->deleteLater();

		if (reply->error() != QNetworkReply::NoError) {
			if (reply->error() != QNetworkReply::OperationCanceledError)
				QMessageBox::warning(mainWindow(), U("Elgato Lights Control"),
						     U("Ã‰chec du tÃ©lÃ©chargement : ") + reply->errorString());
			return;
		}
		const QByteArray data = reply->readAll();
		if (data.size() < 4 || data.left(2) != "MZ") {
			QMessageBox::warning(mainWindow(), U("Elgato Lights Control"),
					     U("Le fichier tÃ©lÃ©chargÃ© n'est pas un exÃ©cutable valide."));
			return;
		}
		if (!sha256.isEmpty() && QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex().toLower() !=
						 sha256.toLower().toUtf8()) {
			QMessageBox::warning(mainWindow(), U("Elgato Lights Control"),
					     U("Somme de contrÃ´le SHA-256 invalide : mise Ã  jour annulÃ©e."));
			return;
		}

		const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
		const QString path = QDir(dir).filePath(QFileInfo(fileName).fileName());
		QFile f(path);
		if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(data) != data.size()) {
			QMessageBox::warning(mainWindow(), U("Elgato Lights Control"),
					     U("Impossible d'enregistrer la mise Ã  jour dans :\n") + path);
			return;
		}
		f.close();

		if (!QProcess::startDetached(path, {"/update"})) {
			QMessageBox::warning(mainWindow(), U("Elgato Lights Control"),
					     U("Impossible de lancer l'installateur :\n") + path);
			return;
		}
		QMessageBox::information(mainWindow(), U("Elgato Lights Control"),
					 U("Mise Ã  jour prÃªte.\n\nFermez OBS : le plugin sera mis Ã  jour "
					   "automatiquement, puis OBS se relancera."));
	});
}

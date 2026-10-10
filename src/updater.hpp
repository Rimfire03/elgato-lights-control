#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>

class Engine;
class QWidget;

// Checks GitHub Releases at startup and offers to update the plugin.
//  - Windows: downloads the signed installer, then runs it ("/update"): it waits for OBS to exit,
//    installs the new DLL and relaunches OBS.
//  - other platforms: opens the release page.
class Updater : public QObject {
	Q_OBJECT
public:
	explicit Updater(Engine &engine, QObject *parent = nullptr);
	void checkAtStartup();

private:
	void onRelease(const QJsonObject &release);
	void download(const QString &url, const QString &fileName, const QString &sha256);
	QWidget *mainWindow() const;

	Engine &engine_;
	QNetworkAccessManager nam_;
};

#pragma once

#include <QJsonObject>
#include <QObject>

#include "http.hpp"

class Engine;
class QWidget;

// Checks GitHub Releases at startup and offers to update the plugin.
//  - Windows: downloads the signed installer, then runs it ("/update"): it waits for OBS to exit,
//    installs the new DLL and relaunches OBS.
//  - other platforms: opens the release page.
// HTTPS goes through libcurl (Qt's TLS backend is not available inside OBS).
class Updater : public QObject {
	Q_OBJECT
public:
	explicit Updater(Engine &engine, QObject *parent = nullptr);
	void checkAtStartup();

private:
	void onRelease(const QJsonObject &release);
	void download(const QString &url, const QString &fileName, const QString &sha256);
	void onDownloaded(const HttpResult &r, const QString &fileName, const QString &sha256, bool cancelled);
	QWidget *mainWindow() const;

	Engine &engine_;
};

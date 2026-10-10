#pragma once

#include <QByteArray>
#include <QString>

#include <atomic>

// Blocking HTTPS GET through libcurl (shipped with OBS). Qt's own TLS backend is not
// available in OBS' Qt build, so QNetworkAccessManager cannot do HTTPS there.
// Call from a worker thread only.
struct HttpResult {
	bool ok = false;
	long status = 0;
	QByteArray body;
	QString error;
};

HttpResult httpsGet(const QString &url, const QByteArray &accept = QByteArray(),
		    const std::atomic<bool> *cancel = nullptr, long timeoutSec = 60);

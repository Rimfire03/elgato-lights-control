#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <functional>
#include <optional>

// Elgato HTTP API (same one used by Control Center): http://<host>:9123/elgato/lights
// brightness: 0..100, temperature: 143..344 (mired, 7000K..2900K)

constexpr int kMinMired = 143;
constexpr int kMaxMired = 344;

struct LightState {
	bool on = false;
	int brightness = 50;
	int temperature = 200;
};

struct LightPatch {
	std::optional<bool> on;
	std::optional<int> brightness;
	std::optional<int> temperature;
};

struct DiscoveredLight {
	QString name;
	QString host;
	quint16 port = 9123;
	QString serial;
};

class ElgatoClient : public QObject {
	Q_OBJECT
public:
	explicit ElgatoClient(QObject *parent = nullptr);

	void fetch(const QString &host, quint16 port, std::function<void(bool ok, LightState state)> cb);
	void apply(const QString &host, quint16 port, const LightPatch &patch,
		   std::function<void(bool ok)> cb = nullptr);

	// Reads /elgato/accessory-info (serial number, display name).
	void fetchInfo(const QString &host, quint16 port, std::function<void(bool ok, DiscoveredLight info)> cb);

	// Scans every local IPv4 /24 subnet for devices answering on port 9123.
	void scanNetwork();

signals:
	void lightFound(const DiscoveredLight &light);
	void scanProgress(int done, int total);
	void scanFinished();

private:
	void probe(const QString &host);

	QNetworkAccessManager nam_;
	int scanTotal_ = 0;
	int scanDone_ = 0;
};

inline int miredToKelvin(int mired)
{
	return mired > 0 ? 1000000 / mired : 0;
}

inline int kelvinToMired(int kelvin)
{
	int m = kelvin > 0 ? 1000000 / kelvin : kMinMired;
	return m < kMinMired ? kMinMired : (m > kMaxMired ? kMaxMired : m);
}

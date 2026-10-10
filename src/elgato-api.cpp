#include "elgato-api.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>

static QUrl lightsUrl(const QString &host, quint16 port)
{
	return QUrl(QString("http://%1:%2/elgato/lights").arg(host).arg(port));
}

ElgatoClient::ElgatoClient(QObject *parent) : QObject(parent) {}

void ElgatoClient::fetch(const QString &host, quint16 port, std::function<void(bool, LightState)> cb)
{
	QNetworkRequest req(lightsUrl(host, port));
	req.setTransferTimeout(3000);
	QNetworkReply *reply = nam_.get(req);
	connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
		LightState st;
		bool ok = false;
		if (reply->error() == QNetworkReply::NoError) {
			const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
			const QJsonArray arr = o.value("lights").toArray();
			if (!arr.isEmpty()) {
				const QJsonObject l = arr.first().toObject();
				st.on = l.value("on").toInt() != 0;
				st.brightness = l.value("brightness").toInt(50);
				st.temperature = l.value("temperature").toInt(200);
				ok = true;
			}
		}
		reply->deleteLater();
		if (cb)
			cb(ok, st);
	});
}

void ElgatoClient::apply(const QString &host, quint16 port, const LightPatch &patch, std::function<void(bool)> cb)
{
	QJsonObject light;
	if (patch.on)
		light["on"] = *patch.on ? 1 : 0;
	if (patch.brightness)
		light["brightness"] = qBound(0, *patch.brightness, 100);
	if (patch.temperature)
		light["temperature"] = qBound(kMinMired, *patch.temperature, kMaxMired);
	if (light.isEmpty())
		return;

	QJsonObject body;
	body["numberOfLights"] = 1;
	body["lights"] = QJsonArray{light};

	QNetworkRequest req(lightsUrl(host, port));
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setTransferTimeout(3000);
	QNetworkReply *reply = nam_.put(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
	connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
		const bool ok = reply->error() == QNetworkReply::NoError;
		reply->deleteLater();
		if (cb)
			cb(ok);
	});
}

void ElgatoClient::fetchInfo(const QString &host, quint16 port, std::function<void(bool, DiscoveredLight)> cb)
{
	QNetworkRequest req(QUrl(QString("http://%1:%2/elgato/accessory-info").arg(host).arg(port)));
	req.setTransferTimeout(3000);
	QNetworkReply *reply = nam_.get(req);
	connect(reply, &QNetworkReply::finished, this, [reply, cb, host, port]() {
		DiscoveredLight d;
		d.host = host;
		d.port = port;
		bool ok = false;
		if (reply->error() == QNetworkReply::NoError) {
			const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
			d.serial = o.value("serialNumber").toString();
			d.name = o.value("displayName").toString();
			ok = !d.serial.isEmpty();
		}
		reply->deleteLater();
		if (cb)
			cb(ok, d);
	});
}

void ElgatoClient::probe(const QString &host)
{
	QNetworkRequest req(QUrl(QString("http://%1:9123/elgato/accessory-info").arg(host)));
	req.setTransferTimeout(1500);
	QNetworkReply *reply = nam_.get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply, host]() {
		if (reply->error() == QNetworkReply::NoError) {
			const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
			if (o.contains("productName") || o.contains("serialNumber")) {
				DiscoveredLight d;
				d.host = host;
				d.port = 9123;
				d.serial = o.value("serialNumber").toString();
				d.name = o.value("displayName").toString();
				if (d.name.isEmpty())
					d.name = o.value("productName").toString();
				emit lightFound(d);
			}
		}
		reply->deleteLater();
		++scanDone_;
		emit scanProgress(scanDone_, scanTotal_);
		if (scanDone_ >= scanTotal_)
			emit scanFinished();
	});
}

void ElgatoClient::scanNetwork()
{
	QSet<QString> hosts;
	for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
		if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
			continue;
		for (const QNetworkAddressEntry &e : iface.addressEntries()) {
			const QHostAddress ip = e.ip();
			if (ip.protocol() != QAbstractSocket::IPv4Protocol)
				continue;
			const quint32 a = ip.toIPv4Address();
			const quint32 base = a & 0xFFFFFF00u;
			for (quint32 i = 1; i < 255; ++i) {
				if ((base | i) != a)
					hosts.insert(QHostAddress(base | i).toString());
			}
		}
	}
	scanDone_ = 0;
	scanTotal_ = hosts.size();
	if (scanTotal_ == 0) {
		emit scanFinished();
		return;
	}
	for (const QString &h : hosts)
		probe(h);
}

#include "config.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <obs-module.h>
#include <util/platform.h>

static QString configFilePath()
{
	char *dir = obs_module_config_path("");
	QString d = dir ? QString::fromUtf8(dir) : QString();
	bfree(dir);
	if (d.isEmpty())
		return QString();
	os_mkdirs(d.toUtf8().constData());
	return QDir(d).filePath("config.json");
}

QString Config::newId()
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

const LightDef *Config::light(const QString &id) const
{
	for (const LightDef &l : lights)
		if (l.id == id)
			return &l;
	return nullptr;
}

const GroupDef *Config::group(const QString &id) const
{
	for (const GroupDef &g : groups)
		if (g.id == id)
			return &g;
	return nullptr;
}

static QJsonObject actionToJson(const ActionDef &a)
{
	return QJsonObject{{"targetType", a.targetType},
			   {"targetId", a.targetId},
			   {"onMode", a.onMode},
			   {"brightness", a.brightness},
			   {"temperature", a.temperature}};
}

static ActionDef actionFromJson(const QJsonObject &o)
{
	ActionDef a;
	a.targetType = o.value("targetType").toString("light");
	a.targetId = o.value("targetId").toString();
	a.onMode = o.value("onMode").toInt(1);
	a.brightness = o.value("brightness").toInt(-1);
	a.temperature = o.value("temperature").toInt(-1);
	return a;
}

void Config::save() const
{
	const QString path = configFilePath();
	if (path.isEmpty())
		return;

	QJsonArray la, ga, ra;
	for (const LightDef &l : lights)
		la.append(QJsonObject{{"id", l.id}, {"name", l.name}, {"host", l.host}, {"port", l.port}});
	for (const GroupDef &g : groups)
		ga.append(QJsonObject{{"id", g.id},
				      {"name", g.name},
				      {"lights", QJsonArray::fromStringList(g.lightIds)}});
	for (const RuleDef &r : rules)
		ra.append(QJsonObject{{"id", r.id},
				      {"name", r.name},
				      {"enabled", r.enabled},
				      {"cond", r.cond},
				      {"scene", r.scene},
				      {"then", actionToJson(r.thenAction)},
				      {"hasElse", r.hasElse},
				      {"else", actionToJson(r.elseAction)}});

	QJsonObject root{{"version", 1}, {"lights", la}, {"groups", ga}, {"rules", ra}};
	QFile f(path);
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void Config::load()
{
	lights.clear();
	groups.clear();
	rules.clear();

	const QString path = configFilePath();
	QFile f(path);
	if (path.isEmpty() || !f.open(QIODevice::ReadOnly))
		return;
	const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();

	for (const QJsonValue &v : root.value("lights").toArray()) {
		const QJsonObject o = v.toObject();
		LightDef l;
		l.id = o.value("id").toString();
		l.name = o.value("name").toString();
		l.host = o.value("host").toString();
		l.port = o.value("port").toInt(9123);
		if (!l.id.isEmpty())
			lights.append(l);
	}
	for (const QJsonValue &v : root.value("groups").toArray()) {
		const QJsonObject o = v.toObject();
		GroupDef g;
		g.id = o.value("id").toString();
		g.name = o.value("name").toString();
		for (const QJsonValue &lv : o.value("lights").toArray())
			g.lightIds.append(lv.toString());
		if (!g.id.isEmpty())
			groups.append(g);
	}
	for (const QJsonValue &v : root.value("rules").toArray()) {
		const QJsonObject o = v.toObject();
		RuleDef r;
		r.id = o.value("id").toString();
		r.name = o.value("name").toString();
		r.enabled = o.value("enabled").toBool(true);
		r.cond = o.value("cond").toString("streaming");
		r.scene = o.value("scene").toString();
		r.thenAction = actionFromJson(o.value("then").toObject());
		r.hasElse = o.value("hasElse").toBool(false);
		r.elseAction = actionFromJson(o.value("else").toObject());
		if (!r.id.isEmpty())
			rules.append(r);
	}
}

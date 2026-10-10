#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct LightDef {
	QString id;
	QString name;
	QString host;
	int port = 9123;
};

struct GroupDef {
	QString id;
	QString name;
	QStringList lightIds;
};

// What a rule does to a light or a group. brightness/temperature = -1 -> unchanged.
// onMode: 0 = unchanged, 1 = on, 2 = off.
struct ActionDef {
	QString targetType = "light"; // "light" | "group"
	QString targetId;
	int onMode = 1;
	int brightness = -1;
	int temperature = -1; // mired
};

// cond: "streaming" | "recording" | "virtualcam" | "scene"
struct RuleDef {
	QString id;
	QString name;
	bool enabled = true;
	QString cond = "streaming";
	QString scene;
	ActionDef thenAction;
	bool hasElse = false;
	ActionDef elseAction;
};

struct Config {
	QList<LightDef> lights;
	QList<GroupDef> groups;
	QList<RuleDef> rules;

	const LightDef *light(const QString &id) const;
	const GroupDef *group(const QString &id) const;

	void load();
	void save() const;

	static QString newId();
};

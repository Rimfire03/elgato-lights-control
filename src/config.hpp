#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct LightDef {
	QString id;
	QString name;
	QString host;
	int port = 9123;
	QString serial; // stable hardware id: lets us find the light again when its IP changes
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

// cond: "obs" | "obs_closing" | "streaming" | "recording" | "stream_or_record" | "virtualcam" | "scene"
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

	// Mises a jour du plugin
	bool checkUpdates = true;
	QString skippedVersion;

	const LightDef *light(const QString &id) const;
	const GroupDef *group(const QString &id) const;

	// Rules whose "then" or "else" action targets this light directly.
	int rulesUsingLight(const QString &id) const;
	// Removes the light everywhere: groups, and the rules that act on it (a rule whose
	// "then" targets it is deleted; one whose "else" targets it loses the "else").
	void removeLight(const QString &id);

	void load();
	void save() const;

	static QString newId();
};

#pragma once

#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "engine.hpp"
#include "patch-controls.hpp"

// The "Elgato Lights" dock: lights, groups and automation rules tabs.
class ElgatoDock : public QWidget {
	Q_OBJECT
public:
	explicit ElgatoDock(Engine &engine, QWidget *parent = nullptr);

private:
	QWidget *buildLightsTab();
	QWidget *buildGroupsTab();
	QWidget *buildRulesTab();
	QWidget *buildFooter();

	void rebuildLights();
	void rebuildGroups();
	void rebuildRules();

	void addLightBox(const LightDef &light);
	void addGroupBox(const GroupDef &group);

	void addManualLight();
	void addDiscoveredLight(const DiscoveredLight &d);
	void identifyLight(const QString &lightId);
	void renameLight(const QString &lightId);
	void confirmRemoveLight(const QString &lightId);
	void setGroupMember(const QString &groupId, const QString &lightId, bool member);
	void removeGroup(const QString &groupId);
	void editRule(int index); // -1 = new

	Engine &engine_;
	QVBoxLayout *lightsLayout_ = nullptr;
	QVBoxLayout *groupsLayout_ = nullptr;
	QListWidget *rulesList_ = nullptr;
	QLabel *scanLabel_ = nullptr;
	QPushButton *scanBtn_ = nullptr;
	QLabel *logLabel_ = nullptr;
	QHash<QString, PatchControls *> lightControls_;
};

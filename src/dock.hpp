#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "engine.hpp"

// On/off checkbox + brightness slider + color temperature slider. Emits a patch on user change.
class PatchControls : public QWidget {
	Q_OBJECT
public:
	explicit PatchControls(QWidget *parent = nullptr);
	void setState(const LightState &st, bool reachable);

signals:
	void patch(const LightPatch &p);

private:
	void queue(const LightPatch &p);
	void flush();

	QCheckBox *on_;
	QSlider *brightness_;
	QSlider *kelvin_;
	QLabel *brightnessLabel_;
	QLabel *kelvinLabel_;
	QLabel *status_;
	QTimer timer_;
	LightPatch pending_;
};

// Editor for one automation rule.
class RuleDialog : public QDialog {
	Q_OBJECT
public:
	RuleDialog(Engine &engine, const RuleDef &rule, QWidget *parent = nullptr);
	RuleDef result() const;

private:
	struct ActionEditor {
		QComboBox *target = nullptr;
		QComboBox *onMode = nullptr;
		QCheckBox *useBrightness = nullptr;
		QSpinBox *brightness = nullptr;
		QCheckBox *useKelvin = nullptr;
		QSpinBox *kelvin = nullptr;
	};
	QWidget *buildAction(ActionEditor &ed, const ActionDef &a);
	ActionDef readAction(const ActionEditor &ed) const;

	Engine &engine_;
	RuleDef rule_;
	QLineEdit *name_;
	QComboBox *cond_;
	QComboBox *scene_;
	ActionEditor then_;
	QCheckBox *hasElse_;
	QWidget *elseBox_;
	ActionEditor else_;
};

class ElgatoDock : public QWidget {
	Q_OBJECT
public:
	explicit ElgatoDock(Engine &engine, QWidget *parent = nullptr);

private:
	QWidget *buildLightsTab();
	QWidget *buildGroupsTab();
	QWidget *buildRulesTab();

	void rebuildLights();
	void rebuildGroups();
	void rebuildRules();

	void addManualLight();
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

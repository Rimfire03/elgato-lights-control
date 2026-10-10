#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>

#include "engine.hpp"

// One-line description of a rule ("name / si ... : target -> changes / sinon : ..."),
// shown in the automations list.
QString describeRule(const Config &cfg, const RuleDef &rule);

// Editor for one automation rule.
class RuleDialog : public QDialog {
	Q_OBJECT
public:
	RuleDialog(Engine &engine, const RuleDef &rule, QWidget *parent = nullptr);
	RuleDef result() const;

private:
	// Widgets that edit one ActionDef (used for both "then" and "else").
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

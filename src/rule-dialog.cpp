#include "rule-dialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>

#include "ui-util.hpp"

namespace {

// Every condition a rule can use, with its wording in the editor and in the list.
// The engine evaluates the same ids (see Engine::conditionValue).
struct ConditionInfo {
	const char *id;
	const char *editorLabel;
	const char *listLabel; // continues "si ..."
};

constexpr ConditionInfo kConditions[] = {
	{"obs", "OBS s'ouvre", "OBS s'ouvre"},
	{"obs_closing", "OBS est en cours de fermeture", "OBS se ferme"},
	{"streaming", "Le stream est en cours", "stream en cours"},
	{"recording", "L'enregistrement est en cours", "enregistrement en cours"},
	{"stream_or_record", "Le stream OU l'enregistrement est en cours", "stream ou enregistrement en cours"},
	{"virtualcam", "La caméra virtuelle est active", "caméra virtuelle active"},
	{"scene", "La scène active est…", nullptr}, // list label depends on the scene name
};

QString actionSummary(const Config &cfg, const ActionDef &a)
{
	QString target = U("?");
	if (a.targetType == "group") {
		if (const GroupDef *g = cfg.group(a.targetId))
			target = U("groupe ") + g->name;
	} else if (const LightDef *l = cfg.light(a.targetId)) {
		target = l->name;
	}

	QStringList changes;
	if (a.onMode == 1)
		changes << U("allumer");
	else if (a.onMode == 2)
		changes << U("éteindre");
	if (a.brightness >= 0)
		changes << QString("%1 %").arg(a.brightness);
	if (a.temperature >= 0)
		changes << QString("%1 K").arg(miredToKelvin(a.temperature));
	if (changes.isEmpty())
		changes << U("aucun changement");
	return target + " → " + changes.join(", ");
}

QString conditionSummary(const RuleDef &rule)
{
	const auto it = std::find_if(std::begin(kConditions), std::end(kConditions),
				     [&](const ConditionInfo &c) { return rule.cond == c.id; });
	if (it != std::end(kConditions) && it->listLabel)
		return U(it->listLabel);
	return U("scène = ") + rule.scene;
}

// A checkbox that enables a spin box, used for the optional brightness / temperature of an action.
void setupOptionalValue(QCheckBox *use, QSpinBox *spin, bool enabled, int value)
{
	spin->setValue(value);
	use->setChecked(enabled);
	spin->setEnabled(enabled);
	QObject::connect(use, &QCheckBox::toggled, spin, &QWidget::setEnabled);
}

} // namespace

QString describeRule(const Config &cfg, const RuleDef &rule)
{
	QString text = QString("%1\n  %2 : %3")
			       .arg(rule.name, U("si ") + conditionSummary(rule), actionSummary(cfg, rule.thenAction));
	if (rule.hasElse)
		text += U("\n  sinon : ") + actionSummary(cfg, rule.elseAction);
	return text;
}

RuleDialog::RuleDialog(Engine &engine, const RuleDef &rule, QWidget *parent)
	: QDialog(parent),
	  engine_(engine),
	  rule_(rule)
{
	setWindowTitle(U("Règle d'automatisation"));
	auto *root = new QVBoxLayout(this);
	auto *form = new QFormLayout();

	name_ = new QLineEdit(rule.name);
	form->addRow(U("Nom"), name_);

	cond_ = new QComboBox();
	for (const ConditionInfo &c : kConditions)
		cond_->addItem(U(c.editorLabel), c.id);
	cond_->setCurrentIndex(std::max(0, cond_->findData(rule.cond)));
	form->addRow(U("Si"), cond_);

	scene_ = new QComboBox();
	scene_->addItems(engine.sceneNames());
	if (!rule.scene.isEmpty())
		scene_->setCurrentText(rule.scene);
	form->addRow(U("Scène"), scene_);
	root->addLayout(form);

	auto *thenBox = new QGroupBox(U("Alors"));
	auto *thenLayout = new QVBoxLayout(thenBox);
	thenLayout->addWidget(buildAction(then_, rule.thenAction));
	root->addWidget(thenBox);

	hasElse_ = new QCheckBox(U("Sinon (quand la condition redevient fausse, ex. OBS se ferme)"));
	hasElse_->setChecked(rule.hasElse);
	root->addWidget(hasElse_);
	elseBox_ = buildAction(else_, rule.elseAction);
	elseBox_->setVisible(rule.hasElse);
	root->addWidget(elseBox_);
	connect(hasElse_, &QCheckBox::toggled, elseBox_, &QWidget::setVisible);

	// The scene picker only matters for the "scene" condition.
	auto syncSceneEnabled = [this]() {
		scene_->setEnabled(cond_->currentData().toString() == "scene");
	};
	connect(cond_, &QComboBox::currentIndexChanged, this, syncSceneEnabled);
	syncSceneEnabled();

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	root->addWidget(buttons);
}

QWidget *RuleDialog::buildAction(ActionEditor &ed, const ActionDef &a)
{
	auto *w = new QWidget();
	auto *form = new QFormLayout(w);
	form->setContentsMargins(0, 0, 0, 0);

	ed.target = new QComboBox();
	for (const GroupDef &g : engine_.config().groups)
		ed.target->addItem(U("Groupe : ") + g.name, QString("group:") + g.id);
	for (const LightDef &l : engine_.config().lights)
		ed.target->addItem(U("Lumière : ") + l.name, QString("light:") + l.id);
	ed.target->setCurrentIndex(std::max(0, ed.target->findData(a.targetType + ":" + a.targetId)));
	form->addRow(U("Cible"), ed.target);

	ed.onMode = new QComboBox();
	ed.onMode->addItem(U("Ne pas changer"), 0);
	ed.onMode->addItem(U("Allumer"), 1);
	ed.onMode->addItem(U("Éteindre"), 2);
	ed.onMode->setCurrentIndex(std::max(0, ed.onMode->findData(a.onMode)));
	form->addRow(U("État"), ed.onMode);

	ed.useBrightness = new QCheckBox(U("Luminosité"));
	ed.brightness = new QSpinBox();
	ed.brightness->setRange(1, 100);
	ed.brightness->setSuffix(" %");
	setupOptionalValue(ed.useBrightness, ed.brightness, a.brightness >= 0, a.brightness >= 0 ? a.brightness : 50);
	form->addRow(ed.useBrightness, ed.brightness);

	ed.useKelvin = new QCheckBox(U("Température"));
	ed.kelvin = new QSpinBox();
	ed.kelvin->setRange(miredToKelvin(kMaxMired), miredToKelvin(kMinMired));
	ed.kelvin->setSingleStep(100);
	ed.kelvin->setSuffix(" K");
	setupOptionalValue(ed.useKelvin, ed.kelvin, a.temperature >= 0,
			   a.temperature >= 0 ? miredToKelvin(a.temperature) : 5000);
	form->addRow(ed.useKelvin, ed.kelvin);
	return w;
}

ActionDef RuleDialog::readAction(const ActionEditor &ed) const
{
	ActionDef a;
	const QString target = ed.target->currentData().toString(); // "light:<id>" or "group:<id>"
	const int sep = target.indexOf(':');
	a.targetType = sep > 0 ? target.left(sep) : "light";
	a.targetId = sep > 0 ? target.mid(sep + 1) : QString();
	a.onMode = ed.onMode->currentData().toInt();
	a.brightness = ed.useBrightness->isChecked() ? ed.brightness->value() : -1;
	a.temperature = ed.useKelvin->isChecked() ? kelvinToMired(ed.kelvin->value()) : -1;
	return a;
}

RuleDef RuleDialog::result() const
{
	RuleDef r = rule_;
	r.name = name_->text().trimmed();
	r.cond = cond_->currentData().toString();
	r.scene = scene_->currentText();
	r.thenAction = readAction(then_);
	r.hasElse = hasElse_->isChecked();
	r.elseAction = readAction(else_);
	return r;
}

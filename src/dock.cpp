#include "dock.hpp"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QTabWidget>
#include <QTimer>

#include <algorithm>

#include <plugin-support.h>

#include "rule-dialog.hpp"
#include "ui-util.hpp"

namespace {

constexpr int kElgatoPort = 9123;
constexpr int kIdentifyBlinkMs = 350;
constexpr int kIdentifyHalfCycles = 4;

// A vertical, scrollable list inside a tab. Returns the layout that rows are added to.
QVBoxLayout *addScrollableList(QVBoxLayout *tab)
{
	auto *content = new QWidget();
	auto *list = new QVBoxLayout(content);
	list->setAlignment(Qt::AlignTop);
	auto *scroll = new QScrollArea();
	scroll->setWidgetResizable(true);
	scroll->setWidget(content);
	tab->addWidget(scroll, 1);
	return list;
}

} // namespace

ElgatoDock::ElgatoDock(Engine &engine, QWidget *parent) : QWidget(parent), engine_(engine)
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(4, 4, 4, 4);

	auto *tabs = new QTabWidget();
	tabs->addTab(buildLightsTab(), U("Lumières"));
	tabs->addTab(buildGroupsTab(), U("Groupes"));
	tabs->addTab(buildRulesTab(), U("Automatisations"));
	root->addWidget(tabs, 1);

	logLabel_ = new QLabel();
	logLabel_->setWordWrap(true);
	root->addWidget(logLabel_);
	root->addWidget(buildFooter());

	connect(&engine_, &Engine::configChanged, this, [this]() {
		rebuildLights();
		rebuildGroups();
		rebuildRules();
	});
	connect(&engine_, &Engine::lightStateChanged, this, [this](const QString &id, bool reachable) {
		PatchControls *controls = lightControls_.value(id);
		if (!controls)
			return;
		if (auto st = engine_.cachedState(id))
			controls->setState(*st, reachable);
	});
	connect(&engine_, &Engine::rulesApplied, this, [this](const QString &m) { logLabel_->setText(m); });

	rebuildLights();
	rebuildGroups();
	rebuildRules();
	engine_.refreshAll();
}

// Bottom row: update-check toggle on the left, plugin version in the corner.
QWidget *ElgatoDock::buildFooter()
{
	auto *footer = new QWidget();
	auto *row = new QHBoxLayout(footer);
	row->setContentsMargins(0, 0, 0, 0);

	auto *updates = new QCheckBox(U("Vérifier les mises à jour du plugin au démarrage"));
	updates->setChecked(engine_.config().checkUpdates);
	connect(updates, &QCheckBox::toggled, this, [this](bool on) {
		engine_.config().checkUpdates = on;
		engine_.config().save();
	});
	row->addWidget(updates);
	row->addStretch();

	auto *version = new QLabel(QString("V.%1").arg(QString::fromUtf8(PLUGIN_VERSION)));
	version->setStyleSheet("color: gray;");
	row->addWidget(version);
	return footer;
}

// ----------------------------------------------------------------------- lights

QWidget *ElgatoDock::buildLightsTab()
{
	auto *tab = new QWidget();
	auto *column = new QVBoxLayout(tab);

	auto *bar = new QHBoxLayout();
	scanBtn_ = new QPushButton(U("Rechercher sur le réseau"));
	auto *addBtn = new QPushButton(U("Ajouter par IP…"));
	auto *refreshBtn = new QPushButton(U("Actualiser"));
	bar->addWidget(scanBtn_);
	bar->addWidget(addBtn);
	bar->addWidget(refreshBtn);
	column->addLayout(bar);

	scanLabel_ = new QLabel();
	column->addWidget(scanLabel_);
	lightsLayout_ = addScrollableList(column);

	connect(scanBtn_, &QPushButton::clicked, this, [this]() {
		scanBtn_->setEnabled(false);
		scanLabel_->setText(U("Recherche en cours…"));
		engine_.client().scanNetwork();
	});
	connect(&engine_.client(), &ElgatoClient::scanProgress, this, [this](int done, int total) {
		scanLabel_->setText(U("Recherche en cours… %1/%2").arg(done).arg(total));
	});
	connect(&engine_.client(), &ElgatoClient::scanFinished, this, [this]() {
		scanBtn_->setEnabled(true);
		scanLabel_->setText(U("Recherche terminée."));
	});
	connect(&engine_.client(), &ElgatoClient::lightFound, this, &ElgatoDock::addDiscoveredLight);
	connect(addBtn, &QPushButton::clicked, this, &ElgatoDock::addManualLight);
	connect(refreshBtn, &QPushButton::clicked, this, [this]() { engine_.refreshAll(); });
	return tab;
}

void ElgatoDock::addDiscoveredLight(const DiscoveredLight &d)
{
	if (engine_.reconcileDiscovered(d))
		return; // already known (same serial, possibly with a new IP)

	LightDef light;
	light.id = Config::newId();
	light.name = d.name;
	light.host = d.host;
	light.port = d.port;
	light.serial = d.serial;
	engine_.config().lights.append(light);
	engine_.save();
	engine_.refreshLight(light.id);
}

void ElgatoDock::addManualLight()
{
	bool ok = false;
	const QString host = QInputDialog::getText(this, U("Ajouter une lumière"), U("Adresse IP de la lumière :"),
						   QLineEdit::Normal, QString(), &ok)
				     .trimmed();
	if (!ok || host.isEmpty())
		return;

	engine_.client().fetch(host, kElgatoPort, [this, host](bool reachable, LightState) {
		if (!reachable) {
			QMessageBox::warning(this, U("Elgato"), U("Aucune lumière Elgato ne répond à cette adresse."));
			return;
		}
		LightDef light;
		light.id = Config::newId();
		light.name = QString("Elgato %1").arg(host);
		light.host = host;
		engine_.config().lights.append(light);
		engine_.save();
		engine_.refreshLight(light.id);
	});
}

void ElgatoDock::rebuildLights()
{
	lightControls_.clear();
	clearLayout(lightsLayout_);
	for (const LightDef &light : engine_.config().lights)
		addLightBox(light);
	if (engine_.config().lights.isEmpty())
		lightsLayout_->addWidget(new QLabel(U("Aucune lumière. Utilisez « Rechercher sur le réseau » "
						      "ou « Ajouter par IP… ».")));
}

void ElgatoDock::addLightBox(const LightDef &light)
{
	const QString id = light.id;

	auto *box = new QGroupBox(QString("%1  (%2)").arg(light.name, light.host));
	auto *column = new QVBoxLayout(box);

	auto *controls = new PatchControls();
	column->addWidget(controls);
	lightControls_.insert(id, controls);
	if (auto st = engine_.cachedState(id))
		controls->setState(*st, true);
	connect(controls, &PatchControls::patch, this, [this, id](const LightPatch &p) { engine_.setLight(id, p); });

	auto *identify = new QPushButton(U("Identifier"));
	auto *rename = new QPushButton(U("Renommer"));
	auto *remove = new QPushButton(U("Supprimer"));
	auto *buttons = new QHBoxLayout();
	buttons->addWidget(identify);
	buttons->addStretch();
	buttons->addWidget(rename);
	buttons->addWidget(remove);
	column->addLayout(buttons);
	connect(identify, &QPushButton::clicked, this, [this, id]() { identifyLight(id); });
	connect(rename, &QPushButton::clicked, this, [this, id]() { renameLight(id); });
	connect(remove, &QPushButton::clicked, this, [this, id]() { confirmRemoveLight(id); });

	lightsLayout_->addWidget(box);
}

// Blinks the light twice so the user can spot which physical lamp it is, then restores its state.
void ElgatoDock::identifyLight(const QString &lightId)
{
	const LightState previous = engine_.cachedState(lightId).value_or(LightState{true, 50, 200});
	auto setOn = [this, lightId](bool on) {
		LightPatch p;
		p.on = on;
		engine_.setLight(lightId, p);
	};
	for (int i = 0; i < kIdentifyHalfCycles; ++i)
		QTimer::singleShot(i * kIdentifyBlinkMs, this, [setOn, i]() { setOn(i % 2 == 1); });
	QTimer::singleShot(kIdentifyHalfCycles * kIdentifyBlinkMs, this, [setOn, previous]() { setOn(previous.on); });
}

void ElgatoDock::renameLight(const QString &lightId)
{
	for (LightDef &light : engine_.config().lights) {
		if (light.id != lightId)
			continue;
		bool ok = false;
		const QString name =
			QInputDialog::getText(this, U("Renommer"), U("Nom :"), QLineEdit::Normal, light.name, &ok)
				.trimmed();
		if (ok && !name.isEmpty()) {
			light.name = name;
			engine_.save();
		}
		return;
	}
}

void ElgatoDock::confirmRemoveLight(const QString &lightId)
{
	const LightDef *light = engine_.config().light(lightId);
	const int affectedRules = engine_.config().rulesUsingLight(lightId);

	QString message = U("Supprimer la lumière « %1 » ?\n\nElle sera retirée de tous les groupes.")
				  .arg(light ? light->name : QString());
	if (affectedRules > 0)
		message += U("\n%1 automatisation(s) qui la ciblent seront supprimée(s) ou modifiée(s).")
				   .arg(affectedRules);
	if (QMessageBox::question(this, U("Supprimer la lumière"), message) != QMessageBox::Yes)
		return;

	engine_.config().removeLight(lightId);
	engine_.forgetLight(lightId);
	engine_.save();
}

// ----------------------------------------------------------------------- groups

QWidget *ElgatoDock::buildGroupsTab()
{
	auto *tab = new QWidget();
	auto *column = new QVBoxLayout(tab);
	auto *add = new QPushButton(U("Nouveau groupe…"));
	column->addWidget(add);
	groupsLayout_ = addScrollableList(column);

	connect(add, &QPushButton::clicked, this, [this]() {
		bool ok = false;
		const QString name = QInputDialog::getText(this, U("Nouveau groupe"), U("Nom du groupe :"),
							   QLineEdit::Normal, QString(), &ok)
					     .trimmed();
		if (!ok || name.isEmpty())
			return;
		GroupDef group;
		group.id = Config::newId();
		group.name = name;
		engine_.config().groups.append(group);
		engine_.save();
	});
	return tab;
}

void ElgatoDock::rebuildGroups()
{
	clearLayout(groupsLayout_);
	for (const GroupDef &group : engine_.config().groups)
		addGroupBox(group);
	if (engine_.config().groups.isEmpty())
		groupsLayout_->addWidget(new QLabel(U("Aucun groupe.")));
}

void ElgatoDock::addGroupBox(const GroupDef &group)
{
	const QString groupId = group.id;

	auto *box = new QGroupBox(group.name);
	auto *column = new QVBoxLayout(box);

	auto *controls = new PatchControls();
	connect(controls, &PatchControls::patch, this,
		[this, groupId](const LightPatch &p) { engine_.setGroup(groupId, p); });
	column->addWidget(controls);

	column->addWidget(new QLabel(U("Membres :")));
	for (const LightDef &light : engine_.config().lights) {
		const QString lightId = light.id;
		auto *member = new QCheckBox(light.name);
		member->setChecked(group.lightIds.contains(lightId));
		connect(member, &QCheckBox::toggled, this,
			[this, groupId, lightId](bool on) { setGroupMember(groupId, lightId, on); });
		column->addWidget(member);
	}

	auto *del = new QPushButton(U("Supprimer le groupe"));
	connect(del, &QPushButton::clicked, this, [this, groupId]() { removeGroup(groupId); });
	column->addWidget(del, 0, Qt::AlignRight);

	groupsLayout_->addWidget(box);
}

void ElgatoDock::setGroupMember(const QString &groupId, const QString &lightId, bool member)
{
	for (GroupDef &group : engine_.config().groups) {
		if (group.id != groupId)
			continue;
		if (member && !group.lightIds.contains(lightId))
			group.lightIds.append(lightId);
		else if (!member)
			group.lightIds.removeAll(lightId);
	}
	engine_.config().save(); // no UI rebuild while ticking boxes
}

void ElgatoDock::removeGroup(const QString &groupId)
{
	auto &groups = engine_.config().groups;
	groups.erase(std::remove_if(groups.begin(), groups.end(), [&](const GroupDef &g) { return g.id == groupId; }),
		     groups.end());
	engine_.save();
}

// ------------------------------------------------------------------------ rules

QWidget *ElgatoDock::buildRulesTab()
{
	auto *tab = new QWidget();
	auto *column = new QVBoxLayout(tab);
	column->addWidget(new QLabel(U("Une règle s'applique quand sa condition devient vraie "
				       "(et l'état « sinon » quand elle redevient fausse).")));
	rulesList_ = new QListWidget();
	column->addWidget(rulesList_, 1);

	auto *bar = new QHBoxLayout();
	auto *add = new QPushButton(U("Ajouter"));
	auto *edit = new QPushButton(U("Modifier"));
	auto *del = new QPushButton(U("Supprimer"));
	bar->addWidget(add);
	bar->addWidget(edit);
	bar->addWidget(del);
	column->addLayout(bar);

	connect(add, &QPushButton::clicked, this, [this]() { editRule(-1); });
	connect(edit, &QPushButton::clicked, this, [this]() { editRule(rulesList_->currentRow()); });
	connect(rulesList_, &QListWidget::itemDoubleClicked, this, [this]() { editRule(rulesList_->currentRow()); });
	connect(del, &QPushButton::clicked, this, [this]() {
		const int row = rulesList_->currentRow();
		if (row < 0 || row >= engine_.config().rules.size())
			return;
		engine_.config().rules.removeAt(row);
		engine_.save();
	});
	connect(rulesList_, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
		const int row = rulesList_->row(item);
		if (row < 0 || row >= engine_.config().rules.size())
			return;
		engine_.config().rules[row].enabled = item->checkState() == Qt::Checked;
		engine_.config().save();
	});
	return tab;
}

void ElgatoDock::rebuildRules()
{
	const QSignalBlocker block(rulesList_);
	rulesList_->clear();
	for (const RuleDef &rule : engine_.config().rules) {
		auto *item = new QListWidgetItem(describeRule(engine_.config(), rule));
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(rule.enabled ? Qt::Checked : Qt::Unchecked);
		rulesList_->addItem(item);
	}
}

void ElgatoDock::editRule(int index)
{
	if (engine_.config().lights.isEmpty() && engine_.config().groups.isEmpty()) {
		QMessageBox::information(this, U("Elgato"), U("Ajoutez d'abord une lumière."));
		return;
	}

	const bool isNew = index < 0 || index >= engine_.config().rules.size();
	RuleDef rule;
	if (isNew) {
		rule.id = Config::newId();
		rule.name = U("Nouvelle règle");
	} else {
		rule = engine_.config().rules[index];
	}

	RuleDialog dialog(engine_, rule, this);
	if (dialog.exec() != QDialog::Accepted)
		return;

	RuleDef edited = dialog.result();
	if (edited.name.isEmpty())
		edited.name = U("Règle");
	if (isNew)
		engine_.config().rules.append(edited);
	else
		engine_.config().rules[index] = edited;
	engine_.save();
	engine_.evaluateRules();
}

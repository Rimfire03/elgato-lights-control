#include "dock.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QTabWidget>

#include <algorithm>

#include <plugin-support.h>

static QString U(const char *s)
{
	return QString::fromUtf8(s);
}

static void clearLayout(QLayout *l)
{
	while (QLayoutItem *it = l->takeAt(0)) {
		if (QWidget *w = it->widget())
			w->deleteLater();
		delete it;
	}
}

// ---------------------------------------------------------------- PatchControls

PatchControls::PatchControls(QWidget *parent) : QWidget(parent)
{
	on_ = new QCheckBox(U("Allumée"));
	brightness_ = new QSlider(Qt::Horizontal);
	brightness_->setRange(1, 100);
	kelvin_ = new QSlider(Qt::Horizontal);
	kelvin_->setRange(miredToKelvin(kMaxMired), miredToKelvin(kMinMired));
	brightnessLabel_ = new QLabel("50 %");
	kelvinLabel_ = new QLabel("5000 K");
	status_ = new QLabel();
	brightnessLabel_->setMinimumWidth(48);
	kelvinLabel_->setMinimumWidth(48);

	auto *grid = new QVBoxLayout(this);
	grid->setContentsMargins(0, 0, 0, 0);
	auto *top = new QHBoxLayout();
	top->addWidget(on_);
	top->addStretch();
	top->addWidget(status_);
	grid->addLayout(top);
	auto *r1 = new QHBoxLayout();
	r1->addWidget(new QLabel(U("Luminosité")));
	r1->addWidget(brightness_, 1);
	r1->addWidget(brightnessLabel_);
	grid->addLayout(r1);
	auto *r2 = new QHBoxLayout();
	r2->addWidget(new QLabel(U("Température")));
	r2->addWidget(kelvin_, 1);
	r2->addWidget(kelvinLabel_);
	grid->addLayout(r2);

	timer_.setSingleShot(true);
	timer_.setInterval(80);
	connect(&timer_, &QTimer::timeout, this, &PatchControls::flush);

	connect(on_, &QCheckBox::toggled, this, [this](bool v) {
		LightPatch p;
		p.on = v;
		queue(p);
	});
	connect(brightness_, &QSlider::valueChanged, this, [this](int v) {
		brightnessLabel_->setText(QString("%1 %").arg(v));
		LightPatch p;
		p.brightness = v;
		queue(p);
	});
	connect(kelvin_, &QSlider::valueChanged, this, [this](int v) {
		kelvinLabel_->setText(QString("%1 K").arg(v));
		LightPatch p;
		p.temperature = kelvinToMired(v);
		queue(p);
	});
}

void PatchControls::queue(const LightPatch &p)
{
	if (p.on)
		pending_.on = p.on;
	if (p.brightness)
		pending_.brightness = p.brightness;
	if (p.temperature)
		pending_.temperature = p.temperature;
	timer_.start();
}

void PatchControls::flush()
{
	const LightPatch p = pending_;
	pending_ = LightPatch();
	emit patch(p);
}

void PatchControls::setState(const LightState &st, bool reachable)
{
	const QSignalBlocker b1(on_), b2(brightness_), b3(kelvin_);
	on_->setChecked(st.on);
	brightness_->setValue(st.brightness);
	kelvin_->setValue(miredToKelvin(st.temperature));
	brightnessLabel_->setText(QString("%1 %").arg(st.brightness));
	kelvinLabel_->setText(QString("%1 K").arg(miredToKelvin(st.temperature)));
	status_->setText(reachable ? QString() : U("⚠ injoignable"));
}

// ------------------------------------------------------------------ RuleDialog

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
	cond_->addItem(U("OBS s'ouvre"), "obs");
	cond_->addItem(U("OBS est en cours de fermeture"), "obs_closing");
	cond_->addItem(U("Le stream est en cours"), "streaming");
	cond_->addItem(U("L'enregistrement est en cours"), "recording");
	cond_->addItem(U("La caméra virtuelle est active"), "virtualcam");
	cond_->addItem(U("La scène active est…"), "scene");
	cond_->setCurrentIndex(std::max(0, cond_->findData(rule.cond)));
	form->addRow(U("Si"), cond_);

	scene_ = new QComboBox();
	scene_->addItems(engine.sceneNames());
	if (!rule.scene.isEmpty())
		scene_->setCurrentText(rule.scene);
	form->addRow(U("Scène"), scene_);
	root->addLayout(form);

	auto *thenBox = new QGroupBox(U("Alors"));
	auto *tl = new QVBoxLayout(thenBox);
	tl->addWidget(buildAction(then_, rule.thenAction));
	root->addWidget(thenBox);

	hasElse_ = new QCheckBox(U("Sinon (quand la condition redevient fausse, ex. OBS se ferme)"));
	hasElse_->setChecked(rule.hasElse);
	root->addWidget(hasElse_);
	elseBox_ = buildAction(else_, rule.elseAction);
	elseBox_->setVisible(rule.hasElse);
	root->addWidget(elseBox_);
	connect(hasElse_, &QCheckBox::toggled, elseBox_, &QWidget::setVisible);

	auto updateScene = [this]() {
		scene_->setEnabled(cond_->currentData().toString() == "scene");
	};
	connect(cond_, &QComboBox::currentIndexChanged, this, updateScene);
	updateScene();

	auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
	root->addWidget(bb);
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
	ed.brightness->setValue(a.brightness >= 0 ? a.brightness : 50);
	ed.useBrightness->setChecked(a.brightness >= 0);
	ed.brightness->setEnabled(a.brightness >= 0);
	connect(ed.useBrightness, &QCheckBox::toggled, ed.brightness, &QWidget::setEnabled);
	form->addRow(ed.useBrightness, ed.brightness);

	ed.useKelvin = new QCheckBox(U("Température"));
	ed.kelvin = new QSpinBox();
	ed.kelvin->setRange(miredToKelvin(kMaxMired), miredToKelvin(kMinMired));
	ed.kelvin->setSingleStep(100);
	ed.kelvin->setSuffix(" K");
	ed.kelvin->setValue(a.temperature >= 0 ? miredToKelvin(a.temperature) : 5000);
	ed.useKelvin->setChecked(a.temperature >= 0);
	ed.kelvin->setEnabled(a.temperature >= 0);
	connect(ed.useKelvin, &QCheckBox::toggled, ed.kelvin, &QWidget::setEnabled);
	form->addRow(ed.useKelvin, ed.kelvin);
	return w;
}

ActionDef RuleDialog::readAction(const ActionEditor &ed) const
{
	ActionDef a;
	const QString t = ed.target->currentData().toString();
	const int sep = t.indexOf(':');
	a.targetType = sep > 0 ? t.left(sep) : "light";
	a.targetId = sep > 0 ? t.mid(sep + 1) : QString();
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

// ------------------------------------------------------------------- ElgatoDock

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

	auto *updates = new QCheckBox(U("Vérifier les mises à jour du plugin au démarrage"));
	updates->setChecked(engine_.config().checkUpdates);
	connect(updates, &QCheckBox::toggled, this, [this](bool on) {
		engine_.config().checkUpdates = on;
		engine_.config().save();
	});

	// Bottom row: update checkbox on the left, plugin version (V.x.x.x) in the bottom-right corner.
	auto *bottom = new QHBoxLayout();
	bottom->addWidget(updates);
	bottom->addStretch();
	auto *versionLabel = new QLabel(QString("V.%1").arg(QString::fromUtf8(PLUGIN_VERSION)));
	versionLabel->setStyleSheet("color: gray;");
	bottom->addWidget(versionLabel);
	root->addLayout(bottom);

	connect(&engine_, &Engine::configChanged, this, [this]() {
		rebuildLights();
		rebuildGroups();
		rebuildRules();
	});
	connect(&engine_, &Engine::lightStateChanged, this, [this](const QString &id, bool reachable) {
		if (PatchControls *c = lightControls_.value(id)) {
			if (auto st = engine_.cachedState(id))
				c->setState(*st, reachable);
		}
	});
	connect(&engine_, &Engine::rulesApplied, this, [this](const QString &m) { logLabel_->setText(m); });

	rebuildLights();
	rebuildGroups();
	rebuildRules();
	engine_.refreshAll();
}

QWidget *ElgatoDock::buildLightsTab()
{
	auto *w = new QWidget();
	auto *v = new QVBoxLayout(w);

	auto *bar = new QHBoxLayout();
	scanBtn_ = new QPushButton(U("Rechercher sur le réseau"));
	auto *addBtn = new QPushButton(U("Ajouter par IP…"));
	auto *refreshBtn = new QPushButton(U("Actualiser"));
	bar->addWidget(scanBtn_);
	bar->addWidget(addBtn);
	bar->addWidget(refreshBtn);
	v->addLayout(bar);
	scanLabel_ = new QLabel();
	v->addWidget(scanLabel_);

	auto *content = new QWidget();
	lightsLayout_ = new QVBoxLayout(content);
	lightsLayout_->setAlignment(Qt::AlignTop);
	auto *scroll = new QScrollArea();
	scroll->setWidgetResizable(true);
	scroll->setWidget(content);
	v->addWidget(scroll, 1);

	connect(scanBtn_, &QPushButton::clicked, this, [this]() {
		scanBtn_->setEnabled(false);
		scanLabel_->setText(U("Recherche en cours…"));
		engine_.client().scanNetwork();
	});
	connect(&engine_.client(), &ElgatoClient::scanProgress, this,
		[this](int d, int t) { scanLabel_->setText(U("Recherche en cours… %1/%2").arg(d).arg(t)); });
	connect(&engine_.client(), &ElgatoClient::scanFinished, this, [this]() {
		scanBtn_->setEnabled(true);
		scanLabel_->setText(U("Recherche terminée."));
	});
	connect(&engine_.client(), &ElgatoClient::lightFound, this, [this](const DiscoveredLight &d) {
		if (engine_.reconcileDiscovered(d))
			return; // already known (same serial, possibly with a new IP)
		LightDef l;
		l.id = Config::newId();
		l.name = d.name;
		l.host = d.host;
		l.port = d.port;
		l.serial = d.serial;
		engine_.config().lights.append(l);
		engine_.save();
		engine_.refreshLight(l.id);
	});
	connect(addBtn, &QPushButton::clicked, this, &ElgatoDock::addManualLight);
	connect(refreshBtn, &QPushButton::clicked, this, [this]() { engine_.refreshAll(); });
	return w;
}

void ElgatoDock::addManualLight()
{
	bool ok = false;
	const QString host = QInputDialog::getText(this, U("Ajouter une lumière"), U("Adresse IP de la lumière :"),
						   QLineEdit::Normal, QString(), &ok)
				     .trimmed();
	if (!ok || host.isEmpty())
		return;
	engine_.client().fetch(host, 9123, [this, host](bool reachable, LightState) {
		if (!reachable) {
			QMessageBox::warning(this, U("Elgato"), U("Aucune lumière Elgato ne répond à cette adresse."));
			return;
		}
		LightDef l;
		l.id = Config::newId();
		l.name = QString("Elgato %1").arg(host);
		l.host = host;
		engine_.config().lights.append(l);
		engine_.save();
		engine_.refreshLight(l.id);
	});
}

void ElgatoDock::rebuildLights()
{
	lightControls_.clear();
	clearLayout(lightsLayout_);
	for (const LightDef &l : engine_.config().lights) {
		auto *box = new QGroupBox(QString("%1  (%2)").arg(l.name, l.host));
		auto *bl = new QVBoxLayout(box);
		auto *ctl = new PatchControls();
		bl->addWidget(ctl);
		auto *btns = new QHBoxLayout();
		auto *rename = new QPushButton(U("Renommer"));
		auto *remove = new QPushButton(U("Supprimer"));
		auto *identify = new QPushButton(U("Identifier"));
		btns->addWidget(identify);
		btns->addStretch();
		btns->addWidget(rename);
		btns->addWidget(remove);
		bl->addLayout(btns);
		lightsLayout_->addWidget(box);
		lightControls_.insert(l.id, ctl);
		if (auto st = engine_.cachedState(l.id))
			ctl->setState(*st, true);

		const QString id = l.id;
		connect(ctl, &PatchControls::patch, this, [this, id](const LightPatch &p) { engine_.setLight(id, p); });
		connect(identify, &QPushButton::clicked, this, [this, id]() {
			// Blink twice to locate the physical light, then restore.
			auto prev = engine_.cachedState(id).value_or(LightState{true, 50, 200});
			auto set = [this, id](bool on) {
				LightPatch p;
				p.on = on;
				engine_.setLight(id, p);
			};
			for (int i = 0; i < 4; ++i)
				QTimer::singleShot(i * 350, this, [set, i]() { set(i % 2 == 1); });
			QTimer::singleShot(4 * 350, this, [set, prev]() { set(prev.on); });
		});
		connect(rename, &QPushButton::clicked, this, [this, id]() {
			for (LightDef &x : engine_.config().lights) {
				if (x.id != id)
					continue;
				bool ok = false;
				const QString n = QInputDialog::getText(this, U("Renommer"), U("Nom :"),
									QLineEdit::Normal, x.name, &ok);
				if (ok && !n.trimmed().isEmpty()) {
					x.name = n.trimmed();
					engine_.save();
				}
				return;
			}
		});
		connect(remove, &QPushButton::clicked, this, [this, id]() {
			const LightDef *light = engine_.config().light(id);
			const QString name = light ? light->name : QString();
			const int nRules = engine_.config().rulesUsingLight(id);
			QString msg =
				U("Supprimer la lumière « %1 » ?\n\nElle sera retirée de tous les groupes.").arg(name);
			if (nRules > 0)
				msg += U("\n%1 automatisation(s) qui la ciblent seront supprimée(s) ou modifiée(s).")
					       .arg(nRules);
			if (QMessageBox::question(this, U("Supprimer la lumière"), msg) != QMessageBox::Yes)
				return;
			engine_.config().removeLight(id);
			engine_.forgetLight(id);
			engine_.save();
		});
	}
	if (engine_.config().lights.isEmpty())
		lightsLayout_->addWidget(new QLabel(U("Aucune lumière. Utilisez « Rechercher sur le réseau » "
						      "ou « Ajouter par IP… ».")));
}

QWidget *ElgatoDock::buildGroupsTab()
{
	auto *w = new QWidget();
	auto *v = new QVBoxLayout(w);
	auto *add = new QPushButton(U("Nouveau groupe…"));
	v->addWidget(add);

	auto *content = new QWidget();
	groupsLayout_ = new QVBoxLayout(content);
	groupsLayout_->setAlignment(Qt::AlignTop);
	auto *scroll = new QScrollArea();
	scroll->setWidgetResizable(true);
	scroll->setWidget(content);
	v->addWidget(scroll, 1);

	connect(add, &QPushButton::clicked, this, [this]() {
		bool ok = false;
		const QString n = QInputDialog::getText(this, U("Nouveau groupe"), U("Nom du groupe :"),
							QLineEdit::Normal, QString(), &ok)
					  .trimmed();
		if (!ok || n.isEmpty())
			return;
		GroupDef g;
		g.id = Config::newId();
		g.name = n;
		engine_.config().groups.append(g);
		engine_.save();
	});
	return w;
}

void ElgatoDock::rebuildGroups()
{
	clearLayout(groupsLayout_);
	for (const GroupDef &g : engine_.config().groups) {
		auto *box = new QGroupBox(g.name);
		auto *bl = new QVBoxLayout(box);

		auto *ctl = new PatchControls();
		const QString gid = g.id;
		connect(ctl, &PatchControls::patch, this,
			[this, gid](const LightPatch &p) { engine_.setGroup(gid, p); });
		bl->addWidget(ctl);

		bl->addWidget(new QLabel(U("Membres :")));
		for (const LightDef &l : engine_.config().lights) {
			auto *cb = new QCheckBox(l.name);
			cb->setChecked(g.lightIds.contains(l.id));
			const QString lid = l.id;
			connect(cb, &QCheckBox::toggled, this, [this, gid, lid](bool on) {
				for (GroupDef &x : engine_.config().groups) {
					if (x.id != gid)
						continue;
					if (on && !x.lightIds.contains(lid))
						x.lightIds.append(lid);
					else if (!on)
						x.lightIds.removeAll(lid);
				}
				engine_.config().save(); // no UI rebuild while ticking boxes
			});
			bl->addWidget(cb);
		}
		auto *del = new QPushButton(U("Supprimer le groupe"));
		connect(del, &QPushButton::clicked, this, [this, gid]() {
			auto &gs = engine_.config().groups;
			gs.erase(std::remove_if(gs.begin(), gs.end(), [&](const GroupDef &x) { return x.id == gid; }),
				 gs.end());
			engine_.save();
		});
		bl->addWidget(del, 0, Qt::AlignRight);
		groupsLayout_->addWidget(box);
	}
	if (engine_.config().groups.isEmpty())
		groupsLayout_->addWidget(new QLabel(U("Aucun groupe.")));
}

static QString actionSummary(const Config &cfg, const ActionDef &a)
{
	QString target = U("?");
	if (a.targetType == "group") {
		if (auto *g = cfg.group(a.targetId))
			target = U("groupe ") + g->name;
	} else if (auto *l = cfg.light(a.targetId)) {
		target = l->name;
	}
	QStringList parts;
	if (a.onMode == 1)
		parts << U("allumer");
	else if (a.onMode == 2)
		parts << U("éteindre");
	if (a.brightness >= 0)
		parts << QString("%1 %").arg(a.brightness);
	if (a.temperature >= 0)
		parts << QString("%1 K").arg(miredToKelvin(a.temperature));
	if (parts.isEmpty())
		parts << U("aucun changement");
	return target + " → " + parts.join(", ");
}

QWidget *ElgatoDock::buildRulesTab()
{
	auto *w = new QWidget();
	auto *v = new QVBoxLayout(w);
	v->addWidget(new QLabel(U("Une règle s'applique quand sa condition devient vraie "
				  "(et l'état « sinon » quand elle redevient fausse).")));
	rulesList_ = new QListWidget();
	v->addWidget(rulesList_, 1);

	auto *bar = new QHBoxLayout();
	auto *add = new QPushButton(U("Ajouter"));
	auto *edit = new QPushButton(U("Modifier"));
	auto *del = new QPushButton(U("Supprimer"));
	bar->addWidget(add);
	bar->addWidget(edit);
	bar->addWidget(del);
	v->addLayout(bar);

	connect(add, &QPushButton::clicked, this, [this]() { editRule(-1); });
	connect(edit, &QPushButton::clicked, this, [this]() { editRule(rulesList_->currentRow()); });
	connect(rulesList_, &QListWidget::itemDoubleClicked, this, [this]() { editRule(rulesList_->currentRow()); });
	connect(del, &QPushButton::clicked, this, [this]() {
		const int i = rulesList_->currentRow();
		if (i < 0 || i >= engine_.config().rules.size())
			return;
		engine_.config().rules.removeAt(i);
		engine_.save();
	});
	connect(rulesList_, &QListWidget::itemChanged, this, [this](QListWidgetItem *it) {
		const int i = rulesList_->row(it);
		if (i < 0 || i >= engine_.config().rules.size())
			return;
		engine_.config().rules[i].enabled = it->checkState() == Qt::Checked;
		engine_.config().save();
	});
	return w;
}

void ElgatoDock::rebuildRules()
{
	const QSignalBlocker block(rulesList_);
	rulesList_->clear();
	for (const RuleDef &r : engine_.config().rules) {
		QString cond;
		if (r.cond == "obs")
			cond = U("OBS s'ouvre");
		else if (r.cond == "obs_closing")
			cond = U("OBS se ferme");
		else if (r.cond == "streaming")
			cond = U("stream en cours");
		else if (r.cond == "recording")
			cond = U("enregistrement en cours");
		else if (r.cond == "virtualcam")
			cond = U("caméra virtuelle active");
		else
			cond = U("scène = ") + r.scene;
		QString text = QString("%1\n  %2 : %3")
				       .arg(r.name, U("si ") + cond, actionSummary(engine_.config(), r.thenAction));
		if (r.hasElse)
			text += U("\n  sinon : ") + actionSummary(engine_.config(), r.elseAction);
		auto *it = new QListWidgetItem(text);
		it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
		it->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked);
		rulesList_->addItem(it);
	}
}

void ElgatoDock::editRule(int index)
{
	RuleDef r;
	const bool isNew = index < 0 || index >= engine_.config().rules.size();
	if (isNew) {
		r.id = Config::newId();
		r.name = U("Nouvelle règle");
	} else {
		r = engine_.config().rules[index];
	}
	if (engine_.config().lights.isEmpty() && engine_.config().groups.isEmpty()) {
		QMessageBox::information(this, U("Elgato"), U("Ajoutez d'abord une lumière."));
		return;
	}
	RuleDialog dlg(engine_, r, this);
	if (dlg.exec() != QDialog::Accepted)
		return;
	RuleDef out = dlg.result();
	if (out.name.isEmpty())
		out.name = U("Règle");
	if (isNew)
		engine_.config().rules.append(out);
	else
		engine_.config().rules[index] = out;
	engine_.save();
	engine_.evaluateRules();
}

#pragma once

#include <QHash>
#include <QObject>

#include "config.hpp"
#include "elgato-api.hpp"

// Owns the configuration, talks to the lights and evaluates automation rules.
class Engine : public QObject {
	Q_OBJECT
public:
	explicit Engine(QObject *parent = nullptr);

	Config &config() { return cfg_; }
	ElgatoClient &client() { return client_; }

	void save()
	{
		cfg_.save();
		emit configChanged();
	}

	// Direct control (lights tab / groups tab).
	void setLight(const QString &lightId, const LightPatch &patch);
	void setGroup(const QString &groupId, const LightPatch &patch);
	void refreshLight(const QString &lightId);
	void refreshAll();
	std::optional<LightState> cachedState(const QString &lightId) const;

	// Called from OBS frontend events (any thread -> marshalled to the Qt thread).
	void onFrontendEvent(int event);
	void evaluateRules();

	QStringList sceneNames() const;

signals:
	void configChanged();
	void lightStateChanged(const QString &lightId, bool reachable);
	void rulesApplied(const QString &message);

private:
	void runAction(const ActionDef &a);
	void syncObsState();
	bool conditionValue(const RuleDef &r) const;

	Config cfg_;
	ElgatoClient client_;
	QHash<QString, LightState> states_;
	QHash<QString, bool> reachable_;
	QHash<QString, int> ruleLast_; // ruleId -> 0/1
	bool streaming_ = false;
	bool recording_ = false;
	bool virtualcam_ = false;
	QString scene_;
};

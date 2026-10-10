#pragma once

#include <QElapsedTimer>
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

	// Matches a discovered device with a configured light (by serial, else by IP). Updates the
	// stored IP when the light moved. Returns true if it belongs to an existing light.
	bool reconcileDiscovered(const DiscoveredLight &d);
	// Drops cached runtime state of a removed light.
	void forgetLight(const QString &lightId);

signals:
	void configChanged();
	void lightStateChanged(const QString &lightId, bool reachable);
	void rulesApplied(const QString &message);

private:
	void runAction(const ActionDef &a);
	void flushPending(int timeoutMs);
	void syncObsState();
	bool conditionValue(const RuleDef &r) const;
	void onLightUnreachable(const QString &lightId, const LightPatch *failed);

	Config cfg_;
	ElgatoClient client_;
	ElgatoClient scanner_; // background re-discovery (never adds new lights)
	QElapsedTimer rescanClock_;
	bool rescanning_ = false;
	QHash<QString, LightPatch> failedPatch_; // actions that could not be delivered (light unreachable)
	QHash<QString, LightState> states_;
	QHash<QString, bool> reachable_;
	QHash<QString, int> ruleLast_; // ruleId -> 0/1
	bool streaming_ = false;
	bool recording_ = false;
	bool virtualcam_ = false;
	bool obsOpen_ = true;     // the plugin only runs while OBS is open
	bool obsClosing_ = false; // true once OBS announced it is exiting
	int pending_ = 0;         // in-flight light requests
	QString scene_;
};

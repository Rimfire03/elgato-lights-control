#include "engine.hpp"

#include <obs-frontend-api.h>
#include <obs.h>

Engine::Engine(QObject *parent) : QObject(parent)
{
	cfg_.load();
	syncObsState();
}

void Engine::syncObsState()
{
	streaming_ = obs_frontend_streaming_active();
	recording_ = obs_frontend_recording_active();
	virtualcam_ = obs_frontend_virtualcam_active();
	obs_source_t *src = obs_frontend_get_current_scene();
	if (src) {
		scene_ = QString::fromUtf8(obs_source_get_name(src));
		obs_source_release(src);
	}
}

QStringList Engine::sceneNames() const
{
	QStringList out;
	char **names = obs_frontend_get_scene_names();
	if (names) {
		for (char **n = names; *n; ++n)
			out << QString::fromUtf8(*n);
		bfree(names);
	}
	return out;
}

std::optional<LightState> Engine::cachedState(const QString &lightId) const
{
	auto it = states_.constFind(lightId);
	if (it == states_.cend())
		return std::nullopt;
	return it.value();
}

void Engine::refreshLight(const QString &lightId)
{
	const LightDef *l = cfg_.light(lightId);
	if (!l)
		return;
	client_.fetch(l->host, (quint16)l->port, [this, lightId](bool ok, LightState st) {
		reachable_[lightId] = ok;
		if (ok)
			states_[lightId] = st;
		emit lightStateChanged(lightId, ok);
	});
}

void Engine::refreshAll()
{
	for (const LightDef &l : cfg_.lights)
		refreshLight(l.id);
}

void Engine::setLight(const QString &lightId, const LightPatch &patch)
{
	const LightDef *l = cfg_.light(lightId);
	if (!l)
		return;
	LightState &st = states_[lightId];
	if (patch.on)
		st.on = *patch.on;
	if (patch.brightness)
		st.brightness = *patch.brightness;
	if (patch.temperature)
		st.temperature = *patch.temperature;
	client_.apply(l->host, (quint16)l->port, patch, [this, lightId](bool ok) {
		reachable_[lightId] = ok;
		emit lightStateChanged(lightId, ok);
	});
}

void Engine::setGroup(const QString &groupId, const LightPatch &patch)
{
	const GroupDef *g = cfg_.group(groupId);
	if (!g)
		return;
	for (const QString &id : g->lightIds)
		setLight(id, patch);
}

void Engine::runAction(const ActionDef &a)
{
	LightPatch p;
	if (a.onMode == 1)
		p.on = true;
	else if (a.onMode == 2)
		p.on = false;
	if (a.brightness >= 0)
		p.brightness = a.brightness;
	if (a.temperature >= 0)
		p.temperature = a.temperature;

	if (a.targetType == "group")
		setGroup(a.targetId, p);
	else
		setLight(a.targetId, p);
}

bool Engine::conditionValue(const RuleDef &r) const
{
	if (r.cond == "streaming")
		return streaming_;
	if (r.cond == "recording")
		return recording_;
	if (r.cond == "virtualcam")
		return virtualcam_;
	if (r.cond == "scene")
		return !r.scene.isEmpty() && scene_ == r.scene;
	return false;
}

void Engine::evaluateRules()
{
	for (const RuleDef &r : cfg_.rules) {
		if (!r.enabled)
			continue;
		const int now = conditionValue(r) ? 1 : 0;
		const auto it = ruleLast_.constFind(r.id);
		const int last = it == ruleLast_.cend() ? -1 : it.value();
		if (now == last)
			continue;
		ruleLast_[r.id] = now;

		if (now == 1) {
			runAction(r.thenAction);
			emit rulesApplied(QString::fromUtf8("Règle « %1 » : condition vraie").arg(r.name));
		} else if (last == 1 && r.hasElse) {
			runAction(r.elseAction);
			emit rulesApplied(QString::fromUtf8("Règle « %1 » : condition fausse").arg(r.name));
		}
	}
}

void Engine::onFrontendEvent(int event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		streaming_ = true;
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		streaming_ = false;
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		recording_ = true;
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		recording_ = false;
		break;
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED:
		virtualcam_ = true;
		break;
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
		virtualcam_ = false;
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		syncObsState();
		break;
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		syncObsState();
		break;
	default:
		return;
	}
	evaluateRules();
}

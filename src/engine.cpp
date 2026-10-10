#include "engine.hpp"

#include <QEventLoop>
#include <QTimer>

#include <obs-frontend-api.h>
#include <obs.h>
#include <plugin-support.h>

static constexpr int kRescanMinIntervalMs = 30000;

Engine::Engine(QObject *parent) : QObject(parent)
{
	cfg_.load();
	syncObsState();
	connect(&scanner_, &ElgatoClient::lightFound, this,
		[this](const DiscoveredLight &d) { reconcileDiscovered(d); });
	connect(&scanner_, &ElgatoClient::scanFinished, this, [this]() { rescanning_ = false; });
}

bool Engine::reconcileDiscovered(const DiscoveredLight &d)
{
	LightDef *match = nullptr;
	if (!d.serial.isEmpty()) {
		for (LightDef &l : cfg_.lights)
			if (l.serial == d.serial) {
				match = &l;
				break;
			}
	}
	if (!match) {
		for (LightDef &l : cfg_.lights)
			if (l.host == d.host && (l.serial.isEmpty() || d.serial.isEmpty() || l.serial == d.serial)) {
				match = &l;
				break;
			}
	}
	if (!match)
		return false;

	bool changed = false;
	const bool moved = match->host != d.host || match->port != (int)d.port;
	if (moved) {
		obs_log(LOG_INFO, "light '%s' moved: %s -> %s", match->name.toUtf8().constData(),
			match->host.toUtf8().constData(), d.host.toUtf8().constData());
		match->host = d.host;
		match->port = d.port;
		changed = true;
	}
	if (match->serial.isEmpty() && !d.serial.isEmpty()) {
		match->serial = d.serial;
		changed = true;
	}
	if (changed) {
		const QString id = match->id; // save() may not invalidate, but do not keep the pointer around
		save();
		if (moved) {
			reachable_[id] = true;
			auto it = failedPatch_.find(id);
			if (it != failedPatch_.end()) {
				const LightPatch p = it.value();
				failedPatch_.erase(it);
				setLight(id, p); // deliver the action that was lost while the IP was stale
			} else {
				refreshLight(id);
			}
		}
	}
	return true;
}

void Engine::forgetLight(const QString &lightId)
{
	states_.remove(lightId);
	reachable_.remove(lightId);
	failedPatch_.remove(lightId);
}

void Engine::onLightUnreachable(const QString &lightId, const LightPatch *failed)
{
	if (failed) {
		LightPatch &acc = failedPatch_[lightId];
		if (failed->on)
			acc.on = failed->on;
		if (failed->brightness)
			acc.brightness = failed->brightness;
		if (failed->temperature)
			acc.temperature = failed->temperature;
	}
	if (obsClosing_ || rescanning_)
		return;
	const LightDef *l = cfg_.light(lightId);
	if (!l || l->serial.isEmpty())
		return; // unknown hardware id: cannot recognise it elsewhere
	if (rescanClock_.isValid() && rescanClock_.elapsed() < kRescanMinIntervalMs)
		return;
	obs_log(LOG_INFO, "light '%s' unreachable at %s: scanning the network", l->name.toUtf8().constData(),
		l->host.toUtf8().constData());
	rescanning_ = true;
	rescanClock_.restart();
	scanner_.scanNetwork();
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
		if (!ok) {
			onLightUnreachable(lightId, nullptr);
			return;
		}
		const LightDef *cur = cfg_.light(lightId);
		if (cur && cur->serial.isEmpty()) {
			// Migration / first contact: remember the hardware id for later re-discovery.
			client_.fetchInfo(cur->host, (quint16)cur->port,
					  [this, lightId](bool got, DiscoveredLight info) {
						  if (!got)
							  return;
						  for (LightDef &x : cfg_.lights)
							  if (x.id == lightId && x.serial.isEmpty()) {
								  x.serial = info.serial;
								  cfg_.save();
								  return;
							  }
					  });
		}
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
	++pending_;
	client_.apply(l->host, (quint16)l->port, patch, [this, lightId, patch](bool ok) {
		--pending_;
		reachable_[lightId] = ok;
		emit lightStateChanged(lightId, ok);
		if (ok)
			failedPatch_.remove(lightId);
		else
			onLightUnreachable(lightId, &patch);
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
	if (r.cond == "obs")
		return obsOpen_;
	if (r.cond == "obs_closing")
		return obsClosing_;
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
	case OBS_FRONTEND_EVENT_EXIT:
		// Apply the "else" states now: OBS is about to close.
		obsOpen_ = false;
		obsClosing_ = true;
		evaluateRules();
		flushPending(2000);
		return;
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

// Keeps the Qt event loop running until the pending light requests are done (or the timeout elapses).
void Engine::flushPending(int timeoutMs)
{
	if (pending_ <= 0)
		return;
	QEventLoop loop;
	QTimer timeout;
	timeout.setSingleShot(true);
	connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
	QTimer poll;
	connect(&poll, &QTimer::timeout, &loop, [this, &loop]() {
		if (pending_ <= 0)
			loop.quit();
	});
	poll.start(25);
	timeout.start(timeoutMs);
	loop.exec();
}
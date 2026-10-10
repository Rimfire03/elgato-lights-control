/*
Elgato Lights Control - OBS plugin
Controls Elgato Key Light / Light Strip devices (same ones as Elgato Control Center)
through their local HTTP API, with groups and event-driven automation rules.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <QMetaObject>

#include "dock.hpp"
#include "engine.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static Engine *g_engine = nullptr;

// Frontend events are delivered on the Qt UI thread. The engine and the dock are
// created only once OBS has finished loading, when the main window is fully ready.
static void frontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && !g_engine) {
		obs_log(LOG_INFO, "creating engine");
		g_engine = new Engine();
		obs_log(LOG_INFO, "creating dock");
		auto *dock = new ElgatoDock(*g_engine);
		obs_frontend_add_dock_by_id("elgato-lights-control", "Elgato Lights", dock);
		obs_log(LOG_INFO, "dock added");
		g_engine->evaluateRules();
		return;
	}
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		// Detach before OBS tears the main window (and our dock) down.
		obs_frontend_remove_event_callback(frontendEvent, nullptr);
		return;
	}
	if (g_engine)
		g_engine->onFrontendEvent((int)event);
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	obs_frontend_add_event_callback(frontendEvent, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	g_engine = nullptr; // the engine has no parent but dies with the process; the dock is owned by OBS
	obs_log(LOG_INFO, "plugin unloaded");
}

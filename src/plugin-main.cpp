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
#include <QPointer>

#include "dock.hpp"
#include "engine.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static Engine *g_engine = nullptr;

static void frontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT || !g_engine)
		return;
	// Marshal to the Qt thread that owns the engine.
	QMetaObject::invokeMethod(g_engine, [event]() {
		if (g_engine)
			g_engine->onFrontendEvent((int)event);
	}, Qt::QueuedConnection);
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_post_load(void)
{
	g_engine = new Engine();
	auto *dock = new ElgatoDock(*g_engine);
	// The dock is owned by OBS' main window from here on.
	obs_frontend_add_dock_by_id("elgato-lights-control", "Elgato Lights", dock);
	obs_frontend_add_event_callback(frontendEvent, nullptr);
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	delete g_engine;
	g_engine = nullptr;
	obs_log(LOG_INFO, "plugin unloaded");
}

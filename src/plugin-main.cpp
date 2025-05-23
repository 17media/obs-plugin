/*
Plugin Name
Copyright (C) <Year> <Developer> <Email Address>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/threading.h>
#include <util/platform.h>
#include <util/util.hpp>
#include <thread>

#include <QMainWindow>

#if defined(__APPLE__)
#include "include/wrapper/cef_library_loader.h"
#endif

#include <plugin-support.h>
#include "17live/SeventeenLiveCoreManager.hpp"
#include "17live/cef-view.hpp"

using namespace std;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "[%s] loading (version %s)", PLUGIN_NAME, PLUGIN_VERSION);
	
#if defined(__APPLE__)
	/* Load CEF at runtime as required on macOS */
	CefScopedLibraryLoader library_loader;
	if (!library_loader.LoadInMain()) {
		obs_log(LOG_ERROR, "Failed to load CEF library");
		return false;
	}
#endif

	cef_view_load(); // Initialize CEF view functionality

	obs_log(LOG_INFO, "[%s] loaded successfully (version %s)", PLUGIN_NAME, PLUGIN_VERSION);

	return true;
}

void handle_obs_frontend_event(enum obs_frontend_event event, [[maybe_unused]] void *data)
{
	static bool isRunning = true;

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING: {
		isRunning = true;

		obs_log(LOG_INFO, "[obs-17live]: initializing");

		// 获取OBS主窗体
		QMainWindow* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
		if (!mainWindow) {
			obs_log(LOG_ERROR, "无法获取OBS主窗体");
			isRunning = false;
			return;
		}
	
		// 初始化SeventeenLiveCoreManager
		try {
			auto& manager = SeventeenLiveCoreManager::getInstance(mainWindow);
			if (!manager.initialize()) {
				obs_log(LOG_ERROR, "SeventeenLiveCoreManager初始化失败");
				isRunning = false;
				return;
			}
			obs_log(LOG_INFO, "SeventeenLiveCoreManager初始化成功");
		} catch (const std::exception& e) {
			obs_log(LOG_ERROR, "SeventeenLiveCoreManager初始化异常: %s", e.what());
			isRunning = false;
			return;
		}

		obs_log(LOG_INFO, "[obs-17live]: init done");
		break;
	}
	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
	case OBS_FRONTEND_EVENT_EXIT: {
		if (!isRunning)
			return;

		isRunning = false;

		obs_frontend_remove_event_callback(handle_obs_frontend_event,
								nullptr);

		// Shutdown 17Live plugin
		obs_log(LOG_INFO, "[obs-17live]: shutting down");

		// 释放SeventeenLiveCoreManager资源
		try {
			auto& manager = SeventeenLiveCoreManager::getInstance();
			manager.shutdown();
			obs_log(LOG_INFO, "SeventeenLiveCoreManager资源已释放");
		} catch (const std::exception& e) {
			obs_log(LOG_ERROR, "SeventeenLiveCoreManager释放资源异常: %s", e.what());
		}

		cef_view_unload();

		obs_log(LOG_INFO, "[obs-17live]: shutdown complete");
		break;
	}
	default:
		break;
	}
}

MODULE_EXPORT void obs_module_post_load(void)
{
	obs_frontend_add_event_callback(handle_obs_frontend_event, nullptr);
}

void obs_module_unload(void)
{   
	obs_log(LOG_INFO, "[obs-17live] plugin unloaded");
}

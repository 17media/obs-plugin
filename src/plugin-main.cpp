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

#include <plugin-support.h>

#include <QMainWindow>
#include "17live/SeventeenLiveCoreManager.hpp"

#include "browser/sl-browser-app.hpp"

#ifdef _WIN32
// #include <util/windows/ComPtr.hpp>
// #include <dxgi.h>
// #include <dxgi1_2.h>
// #include <d3d11.h>
#else
#include "signal-restore.hpp"
#endif

using namespace seventeenlive;
using namespace std;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static CefRefPtr<BrowserApp> browserApp;

os_event_t *cef_started_event = nullptr;

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);

	return true;
}

bool InitializeCef()
{
  obs_log(LOG_INFO, "初始化 CEF 环境");

	string path = obs_get_module_binary_path(obs_current_module());
	path = path.substr(0, path.find_last_of('/') + 1);
	path += "/SeventeenLiveBrowserSubprocess";
    
  // 根据不同平台创建 CefMainArgs
#ifdef _WIN32
	path += ".exe";
  CefMainArgs args;
#else
  // macOS 环境下，需要传递命令行参数给 CEF
  struct obs_cmdline_args cmdline_args = obs_get_cmdline_args();
  obs_log(LOG_INFO, "CEF 命令行参数数量：%d", cmdline_args.argc);
  obs_log(LOG_INFO, "CEF 命令行参数：%s", cmdline_args.argv[0]);
  CefMainArgs args(cmdline_args.argc, cmdline_args.argv);
#endif

  BPtr<char> conf_path = obs_module_config_path("");
  os_mkdir(conf_path);

  obs_log(LOG_INFO, "CEF 配置路径：%s", obs_module_config_path(""));

  // 配置 CEF 设置
  CefSettings settings;
  BPtr<char> log_path = obs_module_config_path("debug.log");
	BPtr<char> log_path_abs = os_get_abs_path_ptr(log_path);
	CefString(&settings.log_file) = log_path_abs;
	settings.windowless_rendering_enabled = true;
	settings.no_sandbox = true;

  uint32_t obs_ver = obs_get_version();
	uint32_t obs_maj = obs_ver >> 24;
	uint32_t obs_min = (obs_ver >> 16) & 0xFF;
	uint32_t obs_pat = obs_ver & 0xFFFF;

	/* This allows servers the ability to determine that browser panels and
	 * browser sources are coming from OBS. */
	std::stringstream prod_ver;
	prod_ver << "Chrome/";
	prod_ver << std::to_string(cef_version_info(4)) << "." << std::to_string(cef_version_info(5)) << "."
		 << std::to_string(cef_version_info(6)) << "." << std::to_string(cef_version_info(7));
	prod_ver << " OBS/";
	prod_ver << std::to_string(obs_maj) << "." << std::to_string(obs_min) << "." << std::to_string(obs_pat);
  prod_ver << " SeventeenLive/";
  prod_ver << PLUGIN_VERSION;
    
  obs_log(LOG_INFO, "User agent: %s", prod_ver.str().c_str());

#if CHROME_VERSION_BUILD >= 4472
	CefString(&settings.user_agent_product) = prod_ver.str();
#else
	CefString(&settings.product_version) = prod_ver.str();
#endif

#ifdef ENABLE_BROWSER_QT_LOOP
	settings.external_message_pump = true;
	settings.multi_threaded_message_loop = false;
#endif

  std::string obs_locale = obs_get_locale();
  std::string accepted_languages;
  if (obs_locale != "en-US") {
    accepted_languages = obs_locale;
    accepted_languages += ",";
    accepted_languages += "en-US,en";
  } else {
    accepted_languages = "en-US,en";
  }

  BPtr<char> conf_path_abs = os_get_abs_path_ptr(conf_path);
	CefString(&settings.locale) = obs_get_locale();
	CefString(&settings.accept_language_list) = accepted_languages;
#if CHROME_VERSION_BUILD <= 6533
	settings.persist_user_preferences = 1;
#endif
	CefString(&settings.cache_path) = conf_path_abs;
// #if !defined(__APPLE__) || defined(ENABLE_BROWSER_LEGACY)
  obs_log(LOG_INFO, "CEF 子进程路径：%s", path.c_str());
	char *abs_path = os_get_abs_path_ptr(path.c_str());
	obs_log(LOG_INFO, "CEF 子进程路径 1：%s", abs_path);
	CefString(&settings.browser_subprocess_path) = abs_path;
	bfree(abs_path);
// #endif

  browserApp = new BrowserApp();

#ifdef _WIN32
	CefExecuteProcess(args, browserApp, nullptr);
#endif

  obs_log(LOG_INFO, "CEF initialize");
    
#if !defined(_WIN32)
  obs_log(LOG_INFO, "CEF initialize: 1");
	BackupSignalHandlers();
	bool success = CefInitialize(args, settings, browserApp, nullptr);
	RestoreSignalHandlers();
#elif (CHROME_VERSION_BUILD > 3770)
  obs_log(LOG_INFO, "CEF initialize: 2");
	bool success = CefInitialize(args, settings, browserApp, nullptr);
#else
  obs_log(LOG_INFO, "CEF initialize: 3");
	/* Massive (but amazing) hack to prevent chromium from modifying our
	 * process tokens and permissions, which caused us problems with winrt,
	 * used with window capture.  Note, the structure internally is just
	 * two pointers normally.  If it causes problems with future versions
	 * we'll just switch back to the static library but I doubt we'll need
	 * to. */
	uintptr_t zeroed_memory_lol[32] = {};
	bool success = CefInitialize(args, settings, browserApp, zeroed_memory_lol);
#endif

   
  if (!success) {
		blog(LOG_ERROR, "[obs-17live-browser]: CEF failed to initialize. Exit code: %d", CefGetExitCode());
		return false;
	}
    
  // os_event_signal(cef_started_event);

  obs_log(LOG_INFO, "CEF 初始化成功");
  return true;
}

void ShutdownCef()
{
  obs_log(LOG_INFO, "正在清理 CEF 环境");
    
  // 释放全局 browserApp 引用
  if (browserApp) {
    browserApp = nullptr;
  }
    
  // 关闭 CEF
  CefShutdown();
    
  obs_log(LOG_INFO, "CEF 环境已清理完成");
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
			auto& manager = seventeenlive::SeventeenLiveCoreManager::getInstance(mainWindow);
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

		if (!InitializeCef()) {
			obs_log(LOG_ERROR, "CEF 初始化失败");
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
			auto& manager = seventeenlive::SeventeenLiveCoreManager::getInstance();
			manager.shutdown();
			obs_log(LOG_INFO, "SeventeenLiveCoreManager资源已释放");
		} catch (const std::exception& e) {
			obs_log(LOG_ERROR, "SeventeenLiveCoreManager释放资源异常: %s", e.what());
		}

		ShutdownCef();
	
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

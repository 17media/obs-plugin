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

#ifdef _WIN32
// #include <util/windows/ComPtr.hpp>
// #include <dxgi.h>
// #include <dxgi1_2.h>
// #include <d3d11.h>
#else
#include "browser/signal-restore.hpp"
#endif

#include <QMainWindow>

#include "browser/cef-headers.hpp"

#include <plugin-support.h>
#include "17live/SeventeenLiveCoreManager.hpp"
#include "browser/sl-browser-app.hpp"

using namespace std;

static thread manager_thread;
static bool manager_initialized = false;
os_event_t *cef_started_event = nullptr;

#ifdef ENABLE_BROWSER_QT_LOOP
extern MessageObject messageObject;
#endif

#ifdef ENABLE_BROWSER_QT_LOOP
#include <QApplication>
#include <QThread>
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static CefRefPtr<BrowserApp> browserApp;
bool InitializeCef()
{
  obs_log(LOG_INFO, "初始化 CEF 环境");

	struct obs_cmdline_args cmdline_args = obs_get_cmdline_args();
  CefMainArgs args(cmdline_args.argc, cmdline_args.argv);
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

	BPtr<char> conf_path = obs_module_config_path("");
  BPtr<char> conf_path_abs = os_get_abs_path_ptr(conf_path);
	CefString(&settings.locale) = obs_get_locale();
	CefString(&settings.accept_language_list) = accepted_languages;
#if CHROME_VERSION_BUILD <= 6533
	settings.persist_user_preferences = 1;
#endif
	CefString(&settings.cache_path) = conf_path_abs;

	browserApp = new BrowserApp();
    
	BackupSignalHandlers();
	bool success = CefInitialize(args, settings, browserApp, nullptr);
	RestoreSignalHandlers();

	if (!success) {
//		blog(LOG_ERROR, "[obs-17live-browser]: CEF failed to initialize. Exit code: %d", CefGetExitCode());
    blog(LOG_ERROR, "[obs-17live-browser]: CEF failed to initialize.");
		return false;
	}

  obs_log(LOG_INFO, "CEF 初始化成功");

	os_event_signal(cef_started_event);
  return true;
}

void ShutdownCef()
{
  obs_log(LOG_INFO, "正在清理 CEF 环境");
  
	// 关闭 CEF
  CefShutdown();

  // 释放全局 browserApp 引用
  if (browserApp) {
    browserApp = nullptr;
  }
    
  obs_log(LOG_INFO, "CEF 环境已清理完成");
}

static void BrowserShutdown(void)
{
#if !ENABLE_LOCAL_FILE_URL_SCHEME
	CefClearSchemeHandlerFactories();
#endif
#ifdef ENABLE_BROWSER_QT_LOOP
	while (messageObject.ExecuteNextBrowserTask())
		;
	CefDoMessageLoopWork();
#endif
	ShutdownCef();
	browserApp = nullptr;
}

#ifndef ENABLE_BROWSER_QT_LOOP
static void BrowserManagerThread(void)
{
	InitializeCef();
	CefRunMessageLoop();
	ShutdownCef();
}
#endif

extern "C" EXPORT void obs_browser_initialize(void)
{
	if (!os_atomic_set_bool(&manager_initialized, true)) {
#ifdef ENABLE_BROWSER_QT_LOOP
		InitializeCef();
#else
		manager_thread = thread(BrowserManagerThread);
#endif
	}
}

class BrowserTask : public CefTask {
public:
	std::function<void()> task;

	inline BrowserTask(std::function<void()> task_) : task(task_) {}
	virtual void Execute() override
	{
#ifdef ENABLE_BROWSER_QT_LOOP
		/* you have to put the tasks on the Qt event queue after this
		 * call otherwise the CEF message pump may stop functioning
		 * correctly, it's only supposed to take 10ms max */
		QMetaObject::invokeMethod(&messageObject, "ExecuteTask", Qt::QueuedConnection,
					  Q_ARG(MessageTask, task));
#else
		task();
#endif
	}

	IMPLEMENT_REFCOUNTING(BrowserTask);
};
bool QueueCEFTask([[maybe_unused]] std::function<void()> task)
{
	return CefPostTask(TID_UI, CefRefPtr<BrowserTask>(new BrowserTask(task)));
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "[%s] loading (version %s)", PLUGIN_NAME, PLUGIN_VERSION);

#ifdef ENABLE_BROWSER_QT_LOOP
	qRegisterMetaType<MessageTask>("MessageTask");
#endif

	os_event_init(&cef_started_event, OS_EVENT_TYPE_MANUAL);

	/* Load CEF at runtime as required on macOS */
	CefScopedLibraryLoader library_loader;
	if (!library_loader.LoadInMain()) {
		obs_log(LOG_ERROR, "Failed to load CEF library");
		return false;
	}

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

		// if (!InitializeCef()) {
		// 	obs_log(LOG_ERROR, "CEF 初始化失败");
		// 	isRunning = false;
		// 	return;
		// }
		obs_browser_initialize();

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

#ifdef ENABLE_BROWSER_QT_LOOP
	BrowserShutdown();
#else
	if (manager_thread.joinable()) {
		if (!QueueCEFTask([]() { CefQuitMessageLoop(); }))
			blog(LOG_DEBUG, "[obs-browser]: Failed to post CefQuit task to loop");

		manager_thread.join();
	}
#endif
	
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

	os_event_destroy(cef_started_event);
}

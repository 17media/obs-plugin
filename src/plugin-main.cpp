/*
OBS 17Live Plugin
Copyright (C) 2023-2025 17Live Inc.

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

// This file integrates plugin-support functionality, using PLUGIN_NAME and PLUGIN_VERSION constants
// and obs_log function

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>
#include <util/threading.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <thread>
#include <util/util.hpp>

#include "17live/OneSevenLiveCoreManager.hpp"
#include "17live/utility/Common.hpp"
#include "17live/utility/CrashSentinel.hpp"

using namespace std;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void) {
    obs_log(LOG_INFO, "[%s] loading (version %s)", PLUGIN_NAME, PLUGIN_VERSION);

    seventeen::utility::CrashSentinel::Initialize();

    InitThreadPool();

    return true;
}

static void schedule_init_core_impl(QMainWindow* mainWindow, bool* isRunningPtr) {
    try {
        auto& manager = OneSevenLiveCoreManager::getInstance(mainWindow);
        if (!manager.initialize()) {
            obs_log(LOG_ERROR, "OneSevenLiveCoreManager initialization failed");
            if (isRunningPtr)
                *isRunningPtr = false;
            return;
        }
        obs_log(LOG_INFO, "OneSevenLiveCoreManager initialized successfully");
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "OneSevenLiveCoreManager initialization exception: %s", e.what());
        if (isRunningPtr)
            *isRunningPtr = false;
        return;
    }
}

void handle_obs_frontend_event(enum obs_frontend_event event, [[maybe_unused]] void* data) {
    static bool isRunning = true;

    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING: {
        isRunning = true;

        obs_log(LOG_INFO, "[obs-17live]: initializing");

        // Get OBS main window
        QMainWindow* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
        if (!mainWindow) {
            obs_log(LOG_ERROR, "Failed to get OBS main window");
            isRunning = false;
            return;
        }

        // check ONESEVENLIVE_API_URL include sta
        QLabel* label =
            new QLabel(QString("%1 [%2]%3")
                           .arg(PLUGIN_NAME, PLUGIN_VERSION,
                                (strstr(ONESEVENLIVE_API_URL, "sta") != nullptr ? " (stage)" : "")),
                       mainWindow);
        mainWindow->statusBar()->addWidget(label);

        // Initialize OneSevenLiveCoreManager
        schedule_init_core_impl(mainWindow, &isRunning);

        obs_log(LOG_INFO, "[obs-17live]: init done");
        break;
    }
    case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
    case OBS_FRONTEND_EVENT_EXIT: {
        if (!isRunning)
            return;

        isRunning = false;

        obs_frontend_remove_event_callback(handle_obs_frontend_event, nullptr);

        // Shutdown 17Live plugin
        obs_log(LOG_INFO, "[obs-17live]: shutting down");

        // Release OneSevenLiveCoreManager resources
        try {
            auto& manager = OneSevenLiveCoreManager::getInstance();
            manager.shutdown();

            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

            // Wait for all background tasks to complete BEFORE destroying the manager
            // This ensures tasks don't access destroyed members (like apiWrapper or m_cancelFlag)
            DestroyThreadPool();

            OneSevenLiveCoreManager::destroyInstance();

            // Force process deferred deletions (like QDockWidget::deleteLater)
            // to ensure widgets are destroyed before the plugin library is unloaded
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

            obs_log(LOG_INFO, "OneSevenLiveCoreManager resources released");
        } catch (const std::exception& e) {
            obs_log(LOG_ERROR, "OneSevenLiveCoreManager resource release exception: %s", e.what());
        }

        obs_log(LOG_INFO, "shutdown complete");
        break;
    }
    default:
        break;
    }
}

MODULE_EXPORT void obs_module_post_load(void) {
    obs_frontend_add_event_callback(handle_obs_frontend_event, nullptr);
}

void obs_module_unload(void) {
    seventeen::utility::CrashSentinel::Shutdown();
    // Ensure thread pool is destroyed on unload as well
    DestroyThreadPool();
    obs_log(LOG_INFO, "[obs-17live] plugin unloaded");
}

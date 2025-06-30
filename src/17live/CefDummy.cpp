#include "CefDummy.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>  // For os_event_t, etc.
#include <util/threading.h>

#include <obs.hpp>
#include <util/dstr.hpp>  // For DStr

#include "plugin-support.h"

static bool cef_initialized = false;
static os_event_t *cef_started_event = nullptr;
static obs_source_t *dummy_source = nullptr;

// Function to initialize CEF

static bool create_dummy_browser_source(void) {
    if (dummy_source) {
        blog(LOG_WARNING, "[obs-17live] Dummy browser source already exists.");
        return true;
    }

    // Get browser source type (ensure obs-browser plugin is loaded)
    const char *source_id = "browser_source";

    obs_data_t *settings = obs_get_source_defaults(source_id);

    // Create source
    dummy_source = obs_source_create(source_id, "DummyBrowser", settings, nullptr);
    if (!dummy_source) {
        blog(LOG_ERROR, "[obs-17live] Failed to create browser source");
        obs_data_release(settings);
        return false;
    }

    blog(LOG_INFO, "[obs-17live] Browser source created successfully");

    obs_data_release(settings);
    return true;
}

static bool initialize_cef() {
    if (cef_initialized)
        return true;

    obs_log(LOG_INFO, "Initializing CEF...");

    if (!create_dummy_browser_source()) {
        obs_log(LOG_ERROR, "Failed to create dummy browser source.");
        return false;
    }

    cef_initialized = true;

    obs_log(LOG_INFO, "CEF initialized successfully.");
    return true;
}

// Function to shutdown CEF
static void shutdown_cef() {
    if (!cef_initialized)
        return;
    obs_log(LOG_INFO, "Shutting down CEF...");

    obs_source_release(dummy_source);
    dummy_source = nullptr;

    cef_initialized = false;
    obs_log(LOG_INFO, "CEF shutdown complete.");
}

// Called when the OBS frontend is available
static void obs_frontend_event_callback(enum obs_frontend_event event, void *private_data) {
    UNUSED_PARAMETER(private_data);
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        // Initialize CEF (if not already done)
        // It's better to initialize CEF early, perhaps in obs_module_load, but ensure it's on the
        // correct thread. For now, deferring until first use or here.
        if (!cef_initialized) {
            // Running CEF initialization on the main UI thread is crucial.
            // OBS might call this callback on the UI thread.
            // If not, CefInitialize needs to be posted to the UI thread.
            // For simplicity, assuming this callback is on an appropriate thread.
            // A more robust solution would use a dedicated thread for CEF message loop and
            // initialization.
            os_event_init(&cef_started_event, OS_EVENT_TYPE_MANUAL);
            if (!initialize_cef()) {
                obs_log(LOG_ERROR, "CEF View: Failed to initialize CEF during frontend load.");
            } else {
                // If CEF needs its own message loop and is not integrated with Qt's loop:
                // std::thread cef_message_loop_thread([](){
                // CefRunMessageLoop();
                // });
                // cef_message_loop_thread.detach();
            }
        }
    }
}

// Called by OBS when the module is loaded
void cef_view_load(void) {
    obs_log(LOG_INFO, "CEF View plugin loading...");
    obs_frontend_add_event_callback(obs_frontend_event_callback, nullptr);
}

// Called by OBS when the module is unloaded
void cef_view_unload(void) {
    obs_log(LOG_INFO, "CEF View plugin unloading...");
    shutdown_cef();
    if (cef_started_event) {
        os_event_destroy(cef_started_event);
        cef_started_event = nullptr;
    }
}

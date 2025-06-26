#include "cef-view.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>  // For os_event_t, etc.
#include <util/threading.h>

#include <obs.hpp>
#include <util/dstr.hpp>  // For DStr

#include "plugin-support.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4996)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// CEF includes - Assuming they are correctly set up in CMakeLists.txt
// These paths might need to be adjusted based on your CEF binary structure
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"
#include "include/cef_frame.h"
// #include "include/cef_runnable.h"
#include "include/cef_process_message.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_helpers.h"

// Qt includes (if needed for windowing, though CEF can create its own)
#include <QAction>
#include <QDockWidget>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QWidget>

// Forward declarations
// static void cef_view_open_url(const char *url_str);
static void cef_view_show_window(const char *url);

// Global CEF browser instance and window
static CefRefPtr<CefBrowser> cef_browser_instance;
// static QMainWindow *cef_window = nullptr;
QDockWidget *cef_window = nullptr;
static bool cef_initialized = false;
static os_event_t *cef_started_event = nullptr;

static obs_source_t *dummy_source = nullptr;



// A simple CEF application implementation
class SimpleCefApp : public CefApp, public CefBrowserProcessHandler {
   public:
    SimpleCefApp() {}

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

    void OnContextInitialized() override {
        CEF_REQUIRE_UI_THREAD();
        if (cef_started_event)
            os_event_signal(cef_started_event);
        cef_initialized = true;
        obs_log(LOG_INFO, "CEF context initialized.");
    }

    void OnBeforeCommandLineProcessing(const CefString &process_type,
                                       CefRefPtr<CefCommandLine> command_line) override {
        // Enable experimental features if needed, or other command line switches
        // command_line->AppendSwitch("enable-experimental-web-platform-features");
        // Disable GPU acceleration if causing issues, for example:
        // command_line->AppendSwitch("disable-gpu");
        // command_line->AppendSwitch("disable-gpu-compositing");
    }

   private:
    IMPLEMENT_REFCOUNTING(SimpleCefApp);
};

// A simple CefClient implementation
class SimpleCefClient : public CefClient, public CefLifeSpanHandler {
   public:
    SimpleCefClient() {}

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        if (!cef_browser_instance) {
            cef_browser_instance = browser;
        }
        obs_log(LOG_INFO, "CEF Browser created.");
    }

    bool DoClose(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        if (cef_browser_instance &&
            cef_browser_instance->GetIdentifier() == browser->GetIdentifier()) {
            cef_browser_instance = nullptr;
            if (cef_window) {
                // This will trigger OnBeforeClose
                // For a QWidget hosted CEF, we might need to close the QWidget
            }
        }
        return false;  // Allow close
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        obs_log(LOG_INFO, "CEF Browser closing.");
        if (cef_browser_instance &&
            cef_browser_instance->GetIdentifier() == browser->GetIdentifier()) {
            cef_browser_instance = nullptr;
        }
    }

   private:
    IMPLEMENT_REFCOUNTING(SimpleCefClient);
};

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
    if (cef_browser_instance) {
        cef_browser_instance->GetHost()->CloseBrowser(true);
        cef_browser_instance = nullptr;
    }
    if (cef_window) {
        delete cef_window;  // Clean up Qt window
        cef_window = nullptr;
    }
   
    obs_source_release(dummy_source);
    dummy_source = nullptr;

    cef_initialized = false;
    obs_log(LOG_INFO, "CEF shutdown complete.");
}

// Callback for the menu item
static void cef_view_action_callback(void *private_data) {
    UNUSED_PARAMETER(private_data);
    // The 'checked' state of the QAction is not passed here.
    // For a simple menu trigger, it's usually not needed.
    cef_view_open_url(nullptr);  // Open with default URL
}

// Function to open a URL in the CEF view
void cef_view_open_url(const char *url_str) {
    if (!cef_initialized) {
        if (!initialize_cef()) {
            obs_log(LOG_ERROR, "Failed to initialize CEF. Cannot open URL.");
            return;
        }
    }

    obs_log(LOG_INFO, "Opening URL: %s", url_str);

    // TODO: handle null or empty URLs
    std::string url_to_load = (url_str && strlen(url_str) > 0) ? url_str : "";
    cef_view_show_window(url_to_load.c_str());
}

static bool cef_view_create_browser(QDockWidget *cef_window, const char *url)
{
        // CEF window info
    CefWindowInfo window_info;
    CefBrowserSettings browser_settings;

#if defined(OS_WIN)
    // On Windows, provide the parent window handle
    CefRect rect(0, 0, 378, 600);
    window_info.SetAsChild((HWND) cef_window->winId(), rect);
#elif defined(OS_MAC)
    // On macOS, you might embed CEF into an NSView. For a top-level window, this is different.
    // If using Qt, Qt handles the NSView creation. We pass the view's handle.
    // For a simple top-level window, CEF can create its own.
    // Let's assume Qt provides a view that CEF can use.
    // This requires a QWidget to host the CEF view.
    QWidget *cef_widget_host = new QWidget(cef_window);
    cef_window->setWidget(cef_widget_host);
    //    cef_window->setCentralWidget(cef_widget_host);
    window_info.SetAsChild((cef_window_handle_t) cef_widget_host->winId(),
                           CefRect(0, 0, 378, 600));  // Placeholder, might need adjustment
#else  // Linux
    // On Linux, provide the X11 window ID
    window_info.SetAsChild((unsigned long) cef_window->winId(), CefRect(0, 0, 1024, 768));
#endif

    CefRefPtr<SimpleCefClient> client = new SimpleCefClient();

    obs_log(LOG_INFO, "Creating CEF browser... %s", url);
    // Create the browser asynchronously
    bool browser_created = CefBrowserHost::CreateBrowser(window_info, client.get(), url,
                                                         browser_settings, nullptr, nullptr);

    if (!browser_created) {
        obs_log(LOG_ERROR, "Failed to create CEF browser.");
        delete cef_window;
        cef_window = nullptr;
    }

    return browser_created;
}

// Function to create and show the CEF window
static void cef_view_show_window(const char *url) {
    CEF_REQUIRE_UI_THREAD();  // Ensure this is called on the UI thread if CEF expects it for window
                              // creation

    obs_log(LOG_INFO, "Show CEF window. %s", url);

    if (cef_window && cef_window->isVisible()) {
        
        if (cef_browser_instance) {
            obs_log(LOG_INFO, "CEF window already visible.");
            cef_browser_instance->GetMainFrame()->LoadURL(url);
        } else {
            obs_log(LOG_INFO, "CEF window already visible but no browser instance.");
            if (!cef_view_create_browser(cef_window, url)) return;
        }

        cef_window->raise();
        cef_window->activateWindow();
        return;
    }

    if (cef_window) {  // Window exists but is hidden
        
        if (cef_browser_instance) {
            obs_log(LOG_INFO, "Showing CEF window.");
            cef_browser_instance->GetMainFrame()->LoadURL(url);
        } else {
            obs_log(LOG_INFO, "No browser instance.");
            if (!cef_view_create_browser(cef_window, url)) return;
        }

        cef_window->show();
        cef_window->raise();
        cef_window->activateWindow();
        return;
    }

    obs_log(LOG_INFO, "Creating CEF window.");

    // Create the main window (using Qt as an example)
    // cef_window = new QMainWindow();
    // cef_window->setWindowTitle(obs_module_text("ChatRoom.Title"));
    // cef_window->resize(1024, 768);
    // cef_window->resize(378, 600);

    QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());

    cef_window = new QDockWidget(mainWindow);
    cef_window->setWindowTitle(obs_module_text("ChatRoom.Title"));
    cef_window->resize(378, 600);
    // Set as floating window first to avoid size adjustment issues after adding to dock area
    cef_window->setFloating(true);
    // Set allowed dock areas
    cef_window->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);

    cef_view_create_browser(cef_window, url);

    cef_window->show();
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

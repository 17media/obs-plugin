#include "OneSevenLivePreviewWidget.hpp"

#include "../../plugin-support.h"
#include <obs-module.h>
#ifdef __APPLE__
#include <objc/objc.h>
#endif
#include <graphics/graphics.h>
#include <QResizeEvent>
#include <QTimer>
#include <QPaintEvent>
#include <QPainter>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QLabel>
#include <QHBoxLayout>
#include <cmath>

#include "moc_OneSevenLivePreviewWidget.cpp"

// Helper function to get module data path
static std::string get_obs_module_data_path_str() {
    const char* path = obs_get_module_data_path(obs_current_module());
    if (!path) {
        return "";
    }
    return std::string(path);
}

OneSevenLivePreviewWidget::OneSevenLivePreviewWidget(QWidget* parent)
    : QWidget(parent),
      previewDisplay(nullptr),
      display_created(false),
      currentSource(nullptr),
      refreshTimer(new QTimer(this)),
      display_width(0),
      display_height(0),
      notificationBar(nullptr),
      alertIcon(nullptr),
      notificationText(nullptr),
      browserSource(nullptr),
      configLoader(new OneSevenLivePreviewConfigLoader(this)),
      browserRefreshTimer(new QTimer(this)) {
    
    // Set widget attributes for proper native rendering
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    
    // Set minimum size and background
    setMinimumSize(320, 240);
    setAutoFillBackground(true);
    
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, Qt::black);
    setPalette(palette);
    
    // Set up refresh timer (30 FPS)
    refreshTimer->setInterval(33);
    connect(refreshTimer, &QTimer::timeout, this, &OneSevenLivePreviewWidget::refreshVideo);
    refreshTimer->start();
    
    // Connect to OBS frontend events
    obs_frontend_add_event_callback(frontendEvent, this);
    
    // Create notification bar
    createNotificationBar();
    
    // Load browser source configuration and create browser source
    loadBrowserSourceConfig();
    createBrowserSource();
    
    // Set up browser refresh timer
    browserRefreshTimer->setInterval(1000); // Refresh every second
    connect(browserRefreshTimer, &QTimer::timeout, this, &OneSevenLivePreviewWidget::updateBrowserSource);
    browserRefreshTimer->start();
}

OneSevenLivePreviewWidget::~OneSevenLivePreviewWidget() {
    if (refreshTimer) {
        refreshTimer->stop();
    }
    if (browserRefreshTimer) {
        browserRefreshTimer->stop();
    }
    obs_frontend_remove_event_callback(frontendEvent, this);
    destroyBrowserSource();
    destroyDisplay();
}

void OneSevenLivePreviewWidget::createDisplay() {
    if (display_created || !isVisible()) {
        return;
    }
    
    // Get widget dimensions
    display_width = width();
    display_height = height();
    
    if (display_width <= 0 || display_height <= 0) {
        return;
    }
    
    // Get device pixel ratio for HiDPI support
    qreal device_pixel_ratio = 1.0;
    QWindow* window_handle = windowHandle();
    if (!window_handle) {
        window_handle = window()->windowHandle();
    }
    if (window_handle) {
        device_pixel_ratio = window_handle->devicePixelRatio();
    } else {
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) {
            device_pixel_ratio = screen->devicePixelRatio();
        }
    }
    
    // Calculate physical dimensions for HiDPI
    int phys_cx = static_cast<int>(std::lround(static_cast<double>(display_width) * static_cast<double>(device_pixel_ratio)));
    int phys_cy = static_cast<int>(std::lround(static_cast<double>(display_height) * static_cast<double>(device_pixel_ratio)));
    
    obs_log(LOG_INFO, "Creating display: logical=%dx%d, physical=%dx%d, dpr=%.2f", 
            display_width, display_height, phys_cx, phys_cy, device_pixel_ratio);
    
    // Create OBS display
    gs_init_data info = {};
    info.cx = static_cast<uint32_t>(phys_cx);
    info.cy = static_cast<uint32_t>(phys_cy);
    info.num_backbuffers = 2;
    info.format = GS_BGRA;
    info.zsformat = GS_ZS_NONE;
    info.adapter = 0;

#ifdef __APPLE__
    // Ensure native window is created
    if (!testAttribute(Qt::WA_NativeWindow)) {
        setAttribute(Qt::WA_NativeWindow, true);
    }
    WId wid = winId();
    info.window.view = (id)reinterpret_cast<void*>(wid);
#elif defined(_WIN32)
    WId wid = winId();
    info.window.hwnd = reinterpret_cast<void*>(wid);
#else
    WId wid = winId();
    info.window.id = static_cast<uint32_t>(wid);
    info.window.display = nullptr;
#endif

    previewDisplay = obs_display_create(&info, 0x00000000);
    if (previewDisplay) {
        obs_display_add_draw_callback(previewDisplay, drawCallback, this);
        display_created = true;
        obs_log(LOG_INFO, "Preview display created successfully");
    } else {
        obs_log(LOG_ERROR, "Failed to create preview display");
    }
}

void OneSevenLivePreviewWidget::destroyDisplay() {
    if (previewDisplay) {
        obs_display_remove_draw_callback(previewDisplay, drawCallback, this);
        obs_display_destroy(previewDisplay);
        previewDisplay = nullptr;
    }
    display_created = false;
    
    if (currentSource) {
        obs_source_release(currentSource);
        currentSource = nullptr;
    }
}

void OneSevenLivePreviewWidget::drawCallback(void* data, uint32_t cx, uint32_t cy) {
    auto* widget = static_cast<OneSevenLivePreviewWidget*>(data);
    if (!widget) {
        return;
    }
    
    widget->renderScene(cx, cy);
}

void OneSevenLivePreviewWidget::renderScene(uint32_t cx, uint32_t cy) {
    // Set up viewport and projection
    gs_viewport_push();
    gs_projection_push();
    
    gs_set_viewport(0, 0, cx, cy);
    gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);
    
    // Clear background
    vec4 clear_color;
    vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
    gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
    
    // Render main source
    if (currentSource) {
        uint32_t source_width = obs_source_get_width(currentSource);
        uint32_t source_height = obs_source_get_height(currentSource);
        
        if (source_width > 0 && source_height > 0) {
            // Calculate scaling to fit while maintaining aspect ratio
            float scale_x = (float)cx / (float)source_width;
            float scale_y = (float)cy / (float)source_height;
            float scale = std::min(scale_x, scale_y);
            
            // Calculate rendered size and centering offset
            float rendered_width = (float)source_width * scale;
            float rendered_height = (float)source_height * scale;
            float offset_x = ((float)cx - rendered_width) / 2.0f;
            float offset_y = ((float)cy - rendered_height) / 2.0f;
            
            // Apply transformation and render
            gs_matrix_push();
            gs_matrix_scale3f(scale, scale, 1.0f);
            gs_matrix_translate3f(offset_x, offset_y, 0.0f);
            obs_source_video_render(currentSource);
            gs_matrix_pop();
        } else {
            // Render without scaling if dimensions are invalid
            obs_source_video_render(currentSource);
        }
    }
    
    // Render browser source overlay - improved implementation
    if (browserSource) {
        obs_log(LOG_INFO, "[DEBUG] Checking browser source for rendering - pointer: %p", browserSource);
        
        // Check if browser source is still valid (similar to obs-replay approach)
        const char* source_name = obs_source_get_name(browserSource);
        if (!source_name) {
            obs_log(LOG_WARNING, "[DEBUG] Browser source has no name, may be invalid");
        } else {
            obs_log(LOG_INFO, "[DEBUG] Browser source name: %s", source_name);
        }
        
        // Check source dimensions and status
        uint32_t browser_width = obs_source_get_width(browserSource);
        uint32_t browser_height = obs_source_get_height(browserSource);
        bool source_active = obs_source_active(browserSource);
        bool source_showing = obs_source_showing(browserSource);
        
        obs_log(LOG_INFO, "[DEBUG] Browser source status - dimensions: %dx%d, active: %s, showing: %s", 
                browser_width, browser_height, source_active ? "true" : "false", source_showing ? "true" : "false");
        
        // Only render if source has valid dimensions (content is loaded)
        if (browser_width > 0 && browser_height > 0) {
            obs_log(LOG_INFO, "[DEBUG] Browser source ready for rendering");
            
            // Reset matrix for overlay rendering
            gs_matrix_push();
            
            // Use simplified fixed position and scale for debugging
            float overlay_scale = 0.5f;
            float overlay_offset_x = 50.0f;
            float overlay_offset_y = 50.0f;
            
            obs_log(LOG_INFO, "[DEBUG] Applying overlay transform - scale: %.3f, offset: %.1f,%.1f", 
                    overlay_scale, overlay_offset_x, overlay_offset_y);
            
            // Apply transformation (translate first, then scale)
            gs_matrix_translate3f(overlay_offset_x, overlay_offset_y, 0.0f);
            gs_matrix_scale3f(overlay_scale, overlay_scale, 1.0f);
            
            // Enable blending for transparency
            gs_enable_blending(true);
            gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
            
            // Render browser source overlay
            obs_log(LOG_INFO, "[DEBUG] Calling obs_source_video_render for browser source");
            obs_source_video_render(browserSource);
            obs_log(LOG_INFO, "[DEBUG] Browser source render completed");
            
            gs_matrix_pop();
        } else {
            obs_log(LOG_INFO, "[DEBUG] Browser source not ready - dimensions: %dx%d (waiting for content to load)", 
                    browser_width, browser_height);
        }
    } else {
        obs_log(LOG_WARNING, "[DEBUG] Browser source is null, skipping overlay render");
    }
    
    gs_projection_pop();
    gs_viewport_pop();
}

void OneSevenLivePreviewWidget::refreshVideo() {
    // Update current program source
    obs_source_t* newSource = getCurrentProgramSource();
    
    if (currentSource != newSource) {
        if (currentSource) {
            obs_source_release(currentSource);
        }
        currentSource = newSource;
        if (currentSource) {
            obs_source_get_ref(currentSource);
        }
    }
    
    // Release temporary reference
    if (newSource) {
        obs_source_release(newSource);
    }
    
    // Force display refresh
    if (previewDisplay && display_created) {
        obs_display_set_enabled(previewDisplay, true);
    }
}

obs_source_t* OneSevenLivePreviewWidget::getCurrentProgramSource() {
    return obs_frontend_get_current_scene();
}

void OneSevenLivePreviewWidget::updateVideoInfo() {
    if (previewDisplay) {
        display_width = width();
        display_height = height();
        
        // Get device pixel ratio for HiDPI support
        qreal device_pixel_ratio = 1.0;
        QWindow* window_handle = windowHandle();
        if (!window_handle) {
            window_handle = window()->windowHandle();
        }
        if (window_handle) {
            device_pixel_ratio = window_handle->devicePixelRatio();
        } else {
            QScreen* screen = QGuiApplication::primaryScreen();
            if (screen) {
                device_pixel_ratio = screen->devicePixelRatio();
            }
        }
        
        // Calculate physical dimensions for HiDPI
        int phys_cx = static_cast<int>(std::lround(static_cast<double>(display_width) * static_cast<double>(device_pixel_ratio)));
        int phys_cy = static_cast<int>(std::lround(static_cast<double>(display_height) * static_cast<double>(device_pixel_ratio)));
        
        obs_log(LOG_INFO, "Resizing display: logical=%dx%d, physical=%dx%d, dpr=%.2f", 
                display_width, display_height, phys_cx, phys_cy, device_pixel_ratio);
        
        obs_display_resize(previewDisplay, phys_cx, phys_cy);
    }
}

void OneSevenLivePreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateVideoInfo();
    updateNotificationBarPosition();
}

void OneSevenLivePreviewWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    
    obs_log(LOG_INFO, "Preview widget shown - size: %dx%d", width(), height());
    createDisplay();
}

void OneSevenLivePreviewWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    // Keep display for performance, just hide widget
}

void OneSevenLivePreviewWidget::paintEvent(QPaintEvent* event) {
    // Only draw placeholder when display isn't ready
    if (!display_created && paintEngine()) {
        QPainter painter(this);
        painter.fillRect(event->rect(), Qt::black);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "Initializing video display...");
    }
}

void OneSevenLivePreviewWidget::frontendEvent(enum obs_frontend_event event, void* data) {
    auto* widget = static_cast<OneSevenLivePreviewWidget*>(data);
    if (!widget) {
        return;
    }
    
    if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
        QMetaObject::invokeMethod(widget, "refreshVideo", Qt::QueuedConnection);
    }
}

void OneSevenLivePreviewWidget::createNotificationBar() {
    // Create notification bar widget
    notificationBar = new QWidget(this);
    
    // Create layout for notification bar
    QHBoxLayout* layout = new QHBoxLayout(notificationBar);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(8);
    
    // Create alert icon using QPixmap and QLabel
    alertIcon = new QLabel(this);
    QPixmap alertPixmap(":/resources/alert-white.svg");
    alertPixmap = alertPixmap.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    alertIcon->setPixmap(alertPixmap);
    alertIcon->setFixedSize(16, 16);
    
    // Create notification text
    notificationText = new QLabel("視窗僅展示動畫效果，不推流。", this);
    notificationText->setStyleSheet("color: white; font-size: 12px;");
    
    // Add widgets to layout
    layout->addWidget(alertIcon);
    layout->addWidget(notificationText);
    layout->addStretch(); // Add stretch to center the content
    
    // Style the notification bar
    notificationBar->setStyleSheet(
        "QWidget {"
        "    background-color: rgba(0, 0, 0, 0.7);"
        "    border-radius: 4px;"
        "}"
    );
    
    // Position and show the notification bar
    updateNotificationBarPosition();
    notificationBar->show();
}

void OneSevenLivePreviewWidget::updateNotificationBarPosition() {
    if (!notificationBar) {
        return;
    }
    
    // Calculate notification bar size
    notificationBar->adjustSize();
    int barWidth = notificationBar->sizeHint().width();
    int barHeight = notificationBar->sizeHint().height();
    
    // Position at top center with some margin
    int x = (width() - barWidth) / 2;
    int y = 10; // 10px from top
    
    notificationBar->setGeometry(x, y, barWidth, barHeight);
}

void OneSevenLivePreviewWidget::loadBrowserSourceConfig() {
    std::string dataPath = get_obs_module_data_path_str();
    QString configPath = QString("%1/preview_config.json").arg(QString::fromStdString(dataPath));
    
    obs_log(LOG_INFO, "[DEBUG] Attempting to load browser source config from: %s", configPath.toUtf8().constData());
    obs_log(LOG_INFO, "[DEBUG] Module data path: %s", dataPath.c_str());
    
    if (configLoader->loadConfiguration(configPath)) {
        browserConfig = configLoader->getConfiguration();
        obs_log(LOG_INFO, "[DEBUG] Browser source config loaded successfully:");
        obs_log(LOG_INFO, "[DEBUG] - URL: %s", browserConfig.url.toUtf8().constData());
        obs_log(LOG_INFO, "[DEBUG] - Width: %d", browserConfig.width);
        obs_log(LOG_INFO, "[DEBUG] - Height: %d", browserConfig.height);
        obs_log(LOG_INFO, "[DEBUG] - FPS: %d", browserConfig.fps);
        obs_log(LOG_INFO, "[DEBUG] - isValid: %s", browserConfig.isValid ? "true" : "false");
        obs_log(LOG_INFO, "[DEBUG] - CSS: %s", browserConfig.style.toUtf8().constData());
    } else {
        obs_log(LOG_WARNING, "[DEBUG] Failed to load browser source config from: %s", configPath.toUtf8().constData());
        obs_log(LOG_WARNING, "[DEBUG] Setting browserConfig.isValid to false");
        browserConfig.isValid = false;
    }
}

void OneSevenLivePreviewWidget::createBrowserSource() {
    obs_log(LOG_INFO, "[DEBUG] createBrowserSource() called - creating test browser source");
    
    // Destroy existing browser source if any
    if (browserSource) {
        obs_log(LOG_WARNING, "[DEBUG] Browser source already exists, destroying first");
        destroyBrowserSource();
    }
    
    // Check if browser source plugin is available
    const char* source_id = "browser_source";
    if (!obs_source_get_display_name(source_id)) {
        obs_log(LOG_ERROR, "[DEBUG] Browser source plugin not available! Source ID '%s' not found", source_id);
        return;
    }
    
    // Create simple test settings
    obs_data_t* settings = obs_data_create();
    
    // Use simple HTML content for testing
    const char* test_html = "data:text/html,<html><body style='background:red;color:white;font-size:48px;text-align:center;padding-top:100px;'>TEST OVERLAY</body></html>";
    obs_data_set_string(settings, "url", test_html);
    obs_data_set_int(settings, "width", 640);
    obs_data_set_int(settings, "height", 480);
    obs_data_set_int(settings, "fps", 30);
    obs_data_set_bool(settings, "shutdown", false);
    obs_data_set_bool(settings, "restart_when_active", false);
    obs_data_set_bool(settings, "reroute_audio", false);
    
    obs_log(LOG_INFO, "[DEBUG] Creating test browser source with simple HTML content");
    
    // Create browser source
    browserSource = obs_source_create("browser_source", "TestBrowserOverlay", settings, nullptr);
    
    if (browserSource) {
        obs_log(LOG_INFO, "[DEBUG] Test browser source created successfully! Pointer: %p", browserSource);
        
        // Get reference to ensure proper lifecycle management
        obs_source_get_ref(browserSource);
        
        // Force the source to start showing and become active (like obs-replay does for hidden sources)
        obs_source_inc_showing(browserSource);
        obs_source_inc_active(browserSource);
        
        obs_log(LOG_INFO, "[DEBUG] Browser source activated - showing and active state incremented");
        
        // Get initial source dimensions
        uint32_t width = obs_source_get_width(browserSource);
        uint32_t height = obs_source_get_height(browserSource);
        obs_log(LOG_INFO, "[DEBUG] Initial browser source dimensions: %dx%d", width, height);
        
        // Check source status
        bool is_active = obs_source_active(browserSource);
        bool is_showing = obs_source_showing(browserSource);
        obs_log(LOG_INFO, "[DEBUG] Browser source status - active: %s, showing: %s", 
                is_active ? "true" : "false", is_showing ? "true" : "false");
        
    } else {
        obs_log(LOG_ERROR, "[DEBUG] Failed to create test browser source");
    }
    
    obs_data_release(settings);
    
    // Set browserConfig as valid for testing
    browserConfig.isValid = true;
    browserConfig.width = 640;
    browserConfig.height = 480;
    
    obs_log(LOG_INFO, "[DEBUG] Browser source creation completed");
}

void OneSevenLivePreviewWidget::destroyBrowserSource() {
    if (browserSource) {
        obs_log(LOG_INFO, "[DEBUG] Destroying browser source - pointer: %p", browserSource);
        
        // Decrement showing and active state (reverse of what we did in create)
        obs_source_dec_showing(browserSource);
        obs_source_dec_active(browserSource);
        
        // Release our reference
        obs_source_release(browserSource);
        browserSource = nullptr;
        
        obs_log(LOG_INFO, "[DEBUG] Browser source destroyed and cleaned up");
    } else {
        obs_log(LOG_INFO, "[DEBUG] No browser source to destroy");
    }
    
    browserConfig.isValid = false;
}

void OneSevenLivePreviewWidget::updateBrowserSource() {
    if (!browserSource) {
        return;
    }
    
    // Force browser source to refresh by triggering a property update
    obs_data_t* settings = obs_source_get_settings(browserSource);
    if (settings) {
        // Update the URL to trigger a refresh (set to same URL)
        obs_data_set_string(settings, "url", browserConfig.url.toUtf8().constData());
        obs_source_update(browserSource, settings);
        obs_data_release(settings);
    }
}

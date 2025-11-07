#include "OneSevenLivePreviewWidget.hpp"

#include <obs-module.h>

#include "../../plugin-support.h"
#ifdef __APPLE__
#include <objc/objc.h>
#endif
#include <graphics/graphics.h>

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QWindow>
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
      browserRefreshTimer(new QTimer(this)),
      overlayScale(1.0f),
      overlayUrl_() {
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
    browserRefreshTimer->setInterval(1000);  // Refresh every second
    connect(browserRefreshTimer, &QTimer::timeout, this,
            &OneSevenLivePreviewWidget::updateBrowserSource);
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

    // Get the native window handle
    WId windowId = winId();
    if (windowId == 0) {
        return;
    }

    // Calculate display dimensions with device pixel ratio
    QScreen* screen = QGuiApplication::primaryScreen();
    qreal dpr = screen ? screen->devicePixelRatio() : 1.0;

    int logical_width = width();
    int logical_height = height();
    int physical_width = static_cast<int>(logical_width * dpr);
    int physical_height = static_cast<int>(logical_height * dpr);

    // Create OBS display
    gs_init_data init_data = {};
    init_data.cx = physical_width;
    init_data.cy = physical_height;
    init_data.format = GS_BGRA;
    init_data.zsformat = GS_ZS_NONE;

#ifdef __APPLE__
    init_data.window.view = (id) windowId;
#elif defined(_WIN32)
    init_data.window.hwnd = reinterpret_cast<HWND>(windowId);
#else
    init_data.window.id = windowId;
#endif

    previewDisplay = obs_display_create(&init_data, 0x0);

    if (previewDisplay) {
        display_created = true;
        display_width = physical_width;
        display_height = physical_height;

        obs_display_add_draw_callback(previewDisplay, drawCallback, this);
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
    gs_ortho(0.0f, (float) cx, 0.0f, (float) cy, -100.0f, 100.0f);

    // Clear background
    vec4 clear_color;
    vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
    gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);

    // Render main source
    if (currentSource) {
        uint32_t source_width = obs_source_get_width(currentSource);
        uint32_t source_height = obs_source_get_height(currentSource);

        if (source_width > 0 && source_height > 0) {
            // Calculate scaling to fit while maintaining aspect ratio (ensure entire video is visible)
            float scale_x = (float) cx / (float) source_width;
            float scale_y = (float) cy / (float) source_height;
            // Use the smaller scale to ensure entire video content is visible within preview bounds
            float scale = std::min(scale_x, scale_y);

            // Center the source
            float scaled_width = (float) source_width * scale;
            float scaled_height = (float) source_height * scale;
            float offset_x = ((float) cx - scaled_width) * 0.5f;
            float offset_y = ((float) cy - scaled_height) * 0.5f;

            // Apply transformation and render
            gs_matrix_push();
            gs_matrix_translate3f(offset_x, offset_y, 0.0f);
            gs_matrix_scale3f(scale, scale, 1.0f);

            obs_source_video_render(currentSource);

            gs_matrix_pop();
        }
    }

    // Render browser source overlay
    if (browserSource) {
        // Get fresh reference to ensure source is still valid
        obs_source_t* source_ref = obs_source_get_ref(browserSource);
        if (source_ref) {
            const char* source_name = obs_source_get_name(source_ref);
            if (!source_name || strlen(source_name) == 0) {
                obs_source_release(source_ref);
                return;
            }

            uint32_t browser_width = obs_source_get_width(source_ref);
            uint32_t browser_height = obs_source_get_height(source_ref);
            bool is_active = obs_source_active(source_ref);
            bool is_showing = obs_source_showing(source_ref);

            if (browser_width > 0 && browser_height > 0 && is_active && is_showing) {
                // Apply overlay transformation to cover entire preview area
                gs_matrix_push();

                // Calculate scale to fill the entire preview area
                float preview_width = static_cast<float>(cx);
                float preview_height = static_cast<float>(cy);
                float browser_width_f = static_cast<float>(browser_width);
                float browser_height_f = static_cast<float>(browser_height);
                
                // Calculate scale factors for both dimensions
                float scale_x = preview_width / browser_width_f;
                float scale_y = preview_height / browser_height_f;
                
                // Use the larger scale to ensure overlay covers entire area
                float fill_scale = qMax(scale_x, scale_y);
                
                // Apply the overlay scale factor from OneSevenLivePreviewScreen
                float final_scale = fill_scale * overlayScale;
                
                // Calculate position to center the scaled overlay
                float scaled_browser_width = browser_width_f * final_scale;
                float scaled_browser_height = browser_height_f * final_scale;
                float overlay_x = (preview_width - scaled_browser_width) * 0.5f;
                float overlay_y = (preview_height - scaled_browser_height) * 0.5f;

                gs_matrix_translate3f(overlay_x, overlay_y, 0.0f);
                gs_matrix_scale3f(final_scale, final_scale, 1.0f);

                obs_source_video_render(source_ref);

                gs_matrix_pop();
            }

            obs_source_release(source_ref);
        }
    }

    // Restore graphics state
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
        int phys_cx = static_cast<int>(std::lround(static_cast<double>(display_width) *
                                                   static_cast<double>(device_pixel_ratio)));
        int phys_cy = static_cast<int>(std::lround(static_cast<double>(display_height) *
                                                   static_cast<double>(device_pixel_ratio)));

        obs_log(LOG_INFO, "Resizing display: logical=%dx%d, physical=%dx%d, dpr=%.2f",
                display_width, display_height, phys_cx, phys_cy, device_pixel_ratio);

        obs_display_resize(previewDisplay, phys_cx, phys_cy);
    }
}

void OneSevenLivePreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    if (display_created && previewDisplay) {
        QScreen* screen = QGuiApplication::primaryScreen();
        qreal dpr = screen ? screen->devicePixelRatio() : 1.0;

        int logical_width = event->size().width();
        int logical_height = event->size().height();
        int physical_width = static_cast<int>(logical_width * dpr);
        int physical_height = static_cast<int>(logical_height * dpr);

        display_width = physical_width;
        display_height = physical_height;

        obs_display_resize(previewDisplay, physical_width, physical_height);
        
        // Force refresh to ensure content scales properly with new size
        forceRefresh();
    }

    updateNotificationBarPosition();
}

void OneSevenLivePreviewWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
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
    layout->addStretch();  // Add stretch to center the content

    // Style the notification bar
    notificationBar->setStyleSheet(
        "QWidget {"
        "    background-color: rgba(0, 0, 0, 0.7);"
        "    border-radius: 4px;"
        "}");

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
    int y = 10;  // 10px from top

    notificationBar->setGeometry(x, y, barWidth, barHeight);
}

void OneSevenLivePreviewWidget::loadBrowserSourceConfig() {
    // Get plugin data directory
    std::string dataPath = get_obs_module_data_path_str();
    QString configPath = QString("%1/preview_config.json").arg(QString::fromStdString(dataPath));

    if (configLoader->loadConfiguration(configPath)) {
        browserConfig = configLoader->getConfiguration();
    } else {
        obs_log(LOG_WARNING, "Failed to load browser source config, using defaults");
        browserConfig.isValid = false;
    }
}

void OneSevenLivePreviewWidget::createBrowserSource() {
    if (browserSource) {
        destroyBrowserSource();
    }

    // Check if browser source plugin is available
    const char* source_id = "browser_source";
    if (!obs_source_get_display_name(source_id)) {
        obs_log(LOG_ERROR, "Browser source plugin not available! Source ID '%s' not found",
                source_id);
        return;
    }

    // Only create browser source if we have valid configuration
    if (!browserConfig.isValid) {
        obs_log(LOG_INFO, "No valid browser source configuration, skipping browser source creation");
        return;
    }

    // Create settings from configuration (allow override by overlayUrl_)
    obs_data_t* settings = obs_data_create();

    const QString effectiveUrl = overlayUrl_.isEmpty() ? browserConfig.url : overlayUrl_;
    obs_log(LOG_INFO, "Using overlay URL: %s", effectiveUrl.toUtf8().constData());
    obs_data_set_string(settings, "url", effectiveUrl.toUtf8().constData());
    obs_data_set_int(settings, "width", browserConfig.width);
    obs_data_set_int(settings, "height", browserConfig.height);
    obs_data_set_int(settings, "fps", browserConfig.fps);
    obs_data_set_bool(settings, "shutdown", false);
    obs_data_set_bool(settings, "restart_when_active", false);
    obs_data_set_bool(settings, "reroute_audio", false);

    // Create browser source
    browserSource = obs_source_create("browser_source", "LivePreviewOverlay", settings, nullptr);

    if (browserSource) {
        // Get reference and activate source
        obs_source_t* source_ref = obs_source_get_ref(browserSource);
        if (source_ref) {
            obs_source_inc_showing(source_ref);
            obs_source_inc_active(source_ref);
            obs_source_release(source_ref);
        }
        obs_log(LOG_INFO, "Browser source created successfully");
    } else {
        obs_log(LOG_ERROR, "Failed to create browser source");
    }

    obs_data_release(settings);
}

void OneSevenLivePreviewWidget::destroyBrowserSource() {
    if (browserSource) {
        // Get reference and properly deactivate
        obs_source_t* source_ref = obs_source_get_ref(browserSource);
        if (source_ref) {
            obs_source_dec_showing(source_ref);
            obs_source_dec_active(source_ref);
            obs_source_release(source_ref);
        }

        obs_source_release(browserSource);
        browserSource = nullptr;
    }
}

void OneSevenLivePreviewWidget::updateBrowserSource() {
    if (!browserSource) {
        return;
    }

    // Force browser source to refresh by triggering a property update
    obs_data_t* settings = obs_source_get_settings(browserSource);
    if (settings) {
        // Update the URL to trigger a refresh; overlayUrl_ overrides config
        const QString effectiveUrl = overlayUrl_.isEmpty() ? browserConfig.url : overlayUrl_;
        obs_data_set_string(settings, "url", effectiveUrl.toUtf8().constData());
        obs_source_update(browserSource, settings);
        obs_data_release(settings);
    }
}

void OneSevenLivePreviewWidget::setOverlayScale(float scale) {
    overlayScale = qMax(0.1f, qMin(5.0f, scale)); // Clamp between 0.1 and 5.0
    
    // Force refresh to apply new scale
    forceRefresh();
}

void OneSevenLivePreviewWidget::setOverlayUrl(const QString& url) {
    overlayUrl_ = url;
    // Apply immediately if browser source exists
    updateBrowserSource();
    obs_log(LOG_INFO, "Preview overlay URL %s",
            overlayUrl_.isEmpty() ? "(using config)" : overlayUrl_.toUtf8().constData());
}

void OneSevenLivePreviewWidget::forceRefresh() {
    if (previewDisplay && display_created) {
        // Invalidate the display to force re-rendering
        obs_display_set_enabled(previewDisplay, false);
        obs_display_set_enabled(previewDisplay, true);
        
        // Also trigger a video refresh
        refreshVideo();
    }
}

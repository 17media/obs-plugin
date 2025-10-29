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

#include "moc_OneSevenLivePreviewWidget.cpp"

OneSevenLivePreviewWidget::OneSevenLivePreviewWidget(QWidget* parent)
    : QWidget(parent),
      mainLayout(nullptr),
      statusLabel(nullptr),
      previewDisplay(nullptr),
      overlaySource(nullptr),
      compositeScene(nullptr),
      compositeSceneSource(nullptr),
      backgroundItem(nullptr),
      overlayItem(nullptr),
      previewActive(false),
      previewWidth(1920),
      previewHeight(1080),
      refreshTimer(new QTimer(this)) {
    
    setupPreviewDisplay();
    
    // Set up refresh timer for dynamic content updates
    refreshTimer->setInterval(33); // Refresh at ~30 FPS for smooth dynamic content updates
    connect(refreshTimer, &QTimer::timeout, this, [this]() {
        if (previewDisplay && previewActive) {
            // Force display refresh for dynamic content
            obs_display_set_enabled(previewDisplay, true);
        }
    });
    refreshTimer->start();
    
    // Connect to OBS frontend events
    obs_frontend_add_event_callback(frontendEvent, this);
}

OneSevenLivePreviewWidget::~OneSevenLivePreviewWidget() {
    if (refreshTimer) {
        refreshTimer->stop();
    }
    obs_frontend_remove_event_callback(frontendEvent, this);
    cleanupPreview();
}

void OneSevenLivePreviewWidget::setupPreviewDisplay() {
    // Set minimum size for the widget
    setMinimumSize(320, 240);
    
    // Set background color to black
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, Qt::black);
    setPalette(palette);
    
    // Set widget attributes for proper native rendering - directly on main widget
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    
    // Set focus policy to accept focus
    setFocusPolicy(Qt::StrongFocus);
    
    // Enable automatic background filling
    setAutoFillBackground(true);
    
    // Create status label if needed (can be overlaid or positioned separately)
    statusLabel = new QLabel(obs_module_text("PreviewDock.Status.Ready"), this);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("color: #ffffff; padding: 5px; background-color: rgba(0,0,0,128);");
    statusLabel->hide(); // Hide by default, show when needed
}

void OneSevenLivePreviewWidget::setupPreview() {
    if (previewDisplay) {
        return; // Already setup
    }
    
    // Create composite scene for overlay
    compositeScene = obs_scene_create("PreviewDockComposite");
    compositeSceneSource = obs_scene_get_source(compositeScene);
    
    previewActive = true;
    updatePreview();
    
    obs_log(LOG_INFO, "Preview dock display setup completed");
}

void OneSevenLivePreviewWidget::createOverlaySource(
    const OneSevenLivePreviewConfigLoader::PreviewConfig& config) {
    
    if (!config.isValid || config.sourceType != "browser_source") {
        obs_log(LOG_WARNING, "Invalid or unsupported overlay config");
        return;
    }
    
    createBrowserSource(config);
}

void OneSevenLivePreviewWidget::createBrowserSource(
    const OneSevenLivePreviewConfigLoader::PreviewConfig& config) {
    
    // Create browser source
    obs_data_t* settings = obs_data_create();
    obs_data_set_string(settings, "url", config.url.toUtf8().constData());
    obs_data_set_string(settings, "css", config.style.toUtf8().constData());
    obs_data_set_int(settings, "width", config.width);
    obs_data_set_int(settings, "height", config.height);
    obs_data_set_int(settings, "fps", config.fps);
    obs_data_set_bool(settings, "shutdown", false);
    
    overlaySource = obs_source_create("browser_source", "PreviewDockOverlay", 
                                     settings, nullptr);
    
    if (overlaySource) {
        // Add overlay to composite scene
        overlayItem = obs_scene_add(compositeScene, overlaySource);

        // Overlay will use default scaling

        // Overlay is added after background, so it is on top by default

        obs_log(LOG_INFO, "Browser source overlay created successfully");
    } else {
        obs_log(LOG_ERROR, "Failed to create browser source overlay");
    }
    
    obs_data_release(settings);
}

void OneSevenLivePreviewWidget::updatePreview() {
    if (!previewActive || !compositeScene) {
        return;
    }
    
    // Get current preview scene
    obs_source_t* currentScene = obs_frontend_get_current_preview_scene();
    if (!currentScene) {
        currentScene = obs_frontend_get_current_scene();
    }
    
    if (currentScene) {
        // Remove previous background item if exists
        if (backgroundItem) {
            obs_sceneitem_remove(backgroundItem);
            backgroundItem = nullptr;
        }

        // Add current scene as background
        backgroundItem = obs_scene_add(compositeScene, currentScene);

        // Ensure overlay exists
        if (!overlayItem && overlaySource) {
            overlayItem = obs_scene_add(compositeScene, overlaySource);
            // Overlay added after background will be on top
        }

        // Display draw callback will render compositeSceneSource

        obs_source_release(currentScene);
    }
}

void OneSevenLivePreviewWidget::onSceneChanged() {
    updatePreview();
}

void OneSevenLivePreviewWidget::renderPreview(void* data, uint32_t cx, uint32_t cy) {
    auto* widget = static_cast<OneSevenLivePreviewWidget*>(data);
    if (!widget || !widget->compositeSceneSource)
        return;

    obs_log(LOG_DEBUG, "Rendering preview with size %dx%d", cx, cy);
    
    // Set up viewport and projection
    gs_viewport_push();
    gs_projection_push();
    
    // Set viewport to match display size
    gs_set_viewport(0, 0, cx, cy);
    
    // Set up orthographic projection (standard coordinate system)
    gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);
    
    // Clear the background
    vec4 clear_color;
    vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
    gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
    
    // Get source dimensions
    uint32_t source_width = obs_source_get_width(widget->compositeSceneSource);
    uint32_t source_height = obs_source_get_height(widget->compositeSceneSource);
    
    if (source_width > 0 && source_height > 0) {
        // Calculate scaling to fit the widget while maintaining aspect ratio
        float scale_x = (float)cx / (float)source_width;
        float scale_y = (float)cy / (float)source_height;
        
        // Use the smaller scale to maintain aspect ratio
        float scale = std::min(scale_x, scale_y);
        
        // Calculate the actual rendered size
        float rendered_width = (float)source_width * scale;
        float rendered_height = (float)source_height * scale;
        
        // Calculate offsets to center the video
        float offset_x = ((float)cx - rendered_width) / 2.0f;
        float offset_y = ((float)cy - rendered_height) / 2.0f;
        
        obs_log(LOG_DEBUG, "Source %dx%d, Widget %dx%d, Scale: %.2f, Rendered: %.0fx%.0f, Offset: %.0f,%.0f", 
                source_width, source_height, cx, cy, scale, rendered_width, rendered_height, offset_x, offset_y);
        
        // Set up transformation matrix with scaling and centering
        gs_matrix_push();
        gs_matrix_translate3f(offset_x, offset_y, 0.0f);
        gs_matrix_scale3f(scale, scale, 1.0f);
        
        // Render the composite scene source
        obs_source_video_render(widget->compositeSceneSource);
        
        gs_matrix_pop();
    } else {
        // If no valid source dimensions, just render without scaling
        obs_source_video_render(widget->compositeSceneSource);
    }
    
    gs_projection_pop();
    gs_viewport_pop();
}

void OneSevenLivePreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    
    if (previewDisplay) {
        // Update display size to match widget size
        obs_display_resize(previewDisplay, width(), height());
    }
    
    // Position status label if visible
    if (statusLabel && statusLabel->isVisible()) {
        statusLabel->resize(width(), 30);
        statusLabel->move(0, height() - 30);
    }
}

void OneSevenLivePreviewWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    
    if (!previewDisplay) {
        // Create display when widget becomes visible - directly on main widget
        gs_init_data info = {};
        info.cx = static_cast<uint32_t>(width());
        info.cy = static_cast<uint32_t>(height());
        info.num_backbuffers = 2;
        info.format = GS_BGRA;
        info.zsformat = GS_ZS_NONE;
        info.adapter = 0;

#ifdef __APPLE__
        // Ensure a native window is created and retrieve its handle
        if (!testAttribute(Qt::WA_NativeWindow)) {
            setAttribute(Qt::WA_NativeWindow, true);
        }
        WId wid = winId(); // forces native window creation on main widget
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
            obs_display_add_draw_callback(previewDisplay, renderPreview, this);
            setupPreview();
        }
    }
}

void OneSevenLivePreviewWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    // Keep display active for performance, just hide widget
}

void OneSevenLivePreviewWidget::paintEvent(QPaintEvent* event) {
    // Avoid QPainter usage if paint engine is unavailable (e.g., during shutdown or WA_PaintOnScreen)
    if (!paintEngine()) {
        return;
    }
    
    // The OBS display handles rendering; we only draw simple placeholders when safe
    QPainter painter(this);
    
    // Fill background with black in case display isn't ready
    painter.fillRect(event->rect(), Qt::black);
    
    // Draw status text if display isn't created
    if (!previewDisplay) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "Initializing video display...");
    }
}

void OneSevenLivePreviewWidget::cleanupPreview() {
    previewActive = false;
    
    if (previewDisplay) {
        obs_display_remove_draw_callback(previewDisplay, renderPreview, this);
        obs_display_destroy(previewDisplay);
        previewDisplay = nullptr;
    }
    
    if (overlaySource) {
        obs_source_release(overlaySource);
        overlaySource = nullptr;
    }
    
    if (compositeSceneSource) {
        obs_source_release(compositeSceneSource);
        compositeSceneSource = nullptr;
    }
    
    if (compositeScene) {
        obs_scene_release(compositeScene);
        compositeScene = nullptr;
    }
}

void OneSevenLivePreviewWidget::frontendEvent(enum obs_frontend_event event, void* data) {
    auto* widget = static_cast<OneSevenLivePreviewWidget*>(data);
    if (!widget)
        return;
    if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
        QMetaObject::invokeMethod(widget, "onSceneChanged", Qt::QueuedConnection);
    }
}

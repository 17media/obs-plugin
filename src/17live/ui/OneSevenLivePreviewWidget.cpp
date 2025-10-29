#include "OneSevenLivePreviewWidget.hpp"

#include "../../plugin-support.h"
#include <obs-module.h>
#ifdef __APPLE__
#include <objc/objc.h>
#endif
#include <graphics/graphics.h>
#include <QResizeEvent>
#include <QTimer>

#include "moc_OneSevenLivePreviewWidget.cpp"

OneSevenLivePreviewWidget::OneSevenLivePreviewWidget(QWidget* parent)
    : QWidget(parent),
      mainLayout(nullptr),
      previewContainer(nullptr),
      statusLabel(nullptr),
      previewDisplay(nullptr),
      overlaySource(nullptr),
      compositeScene(nullptr),
      compositeSceneSource(nullptr),
      backgroundItem(nullptr),
      overlayItem(nullptr),
      previewActive(false),
      previewWidth(1920),
      previewHeight(1080) {
    
    setupPreviewDisplay();
    
    // Connect to OBS frontend events
    obs_frontend_add_event_callback(frontendEvent, this);
}

OneSevenLivePreviewWidget::~OneSevenLivePreviewWidget() {
    obs_frontend_remove_event_callback(frontendEvent, this);
    cleanupPreview();
}

void OneSevenLivePreviewWidget::setupPreviewDisplay() {
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    previewContainer = new QWidget(this);
    previewContainer->setMinimumSize(320, 240);
    previewContainer->setStyleSheet("background-color: #000000;");
    previewContainer->setAttribute(Qt::WA_NativeWindow, true);
    
    statusLabel = new QLabel(obs_module_text("PreviewDock.Status.Ready"), this);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("color: #ffffff; padding: 5px;");
    
    mainLayout->addWidget(previewContainer, 1);
    mainLayout->addWidget(statusLabel, 0);
    
    setLayout(mainLayout);
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

    // Render the composite scene source
    UNUSED_PARAMETER(cx);
    UNUSED_PARAMETER(cy);
    obs_source_video_render(widget->compositeSceneSource);
}

void OneSevenLivePreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    
    if (previewDisplay && previewContainer) {
        // Update display size
        QSize containerSize = previewContainer->size();
        obs_display_resize(previewDisplay, containerSize.width(), containerSize.height());
    }
}

void OneSevenLivePreviewWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    
    if (!previewDisplay && previewContainer) {
        // Create display when widget becomes visible
        gs_init_data info = {};
        info.cx = static_cast<uint32_t>(previewContainer->width());
        info.cy = static_cast<uint32_t>(previewContainer->height());
        info.num_backbuffers = 2;
        info.format = GS_BGRA;
        info.zsformat = GS_ZS_NONE;
        info.adapter = 0;

#ifdef __APPLE__
        // Ensure a native window is created and retrieve its handle
        if (!previewContainer->testAttribute(Qt::WA_NativeWindow)) {
            previewContainer->setAttribute(Qt::WA_NativeWindow, true);
        }
        WId wid = previewContainer->winId(); // forces native window creation
        info.window.view = (id)reinterpret_cast<void*>(wid);
#elif defined(_WIN32)
        WId wid = previewContainer->winId();
        info.window.hwnd = reinterpret_cast<void*>(wid);
#else
        WId wid = previewContainer->winId();
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

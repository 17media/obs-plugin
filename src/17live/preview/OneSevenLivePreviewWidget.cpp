#include "OneSevenLivePreviewWidget.hpp"

#include <obs-module.h>

#include "../../plugin-support.h"
#ifdef __APPLE__
#include <objc/objc.h>
#endif
#include <graphics/graphics.h>

#include <QDir>
#include <QFont>
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

#include "../OneSevenLiveCoreManager.hpp"
#include "../streaming/OneSevenLiveStreamManager.hpp"

#include "moc_OneSevenLivePreviewWidget.cpp"
#include "utility/Common.hpp"

OneSevenLivePreviewWidget::OneSevenLivePreviewWidget(QWidget* parent, const QString& overlayUrl, const QString& enterAnimUrl)
    : QWidget(parent),
      previewDisplay(nullptr),
      display_created(false),
      currentSource(nullptr),
      refreshTimer(new QTimer(this)),
      display_width(0),
      display_height(0),
      browserSource(nullptr),
      enterAnimSource(nullptr),
      configLoader(new OneSevenLivePreviewConfigLoader(this)),
      browserRefreshTimer(new QTimer(this)),
      overlayScale(1.0f),
      overlayUrl_(overlayUrl),
      enterAnimUrl_(enterAnimUrl) {
    // Set widget attributes for proper native rendering
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    // Set background only; allow full responsive sizing
    setAutoFillBackground(false);

    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, Qt::black);
    setPalette(palette);

#ifdef _WIN32
    QFont safeFont;
    safeFont.setFamily("Segoe UI");
    safeFont.setPointSize(10);
    setFont(safeFont);
#endif

    // Set up refresh timer (30 FPS)
    refreshTimer->setInterval(33);
    connect(refreshTimer, &QTimer::timeout, this, &OneSevenLivePreviewWidget::refreshVideo);
    refreshTimer->start();

    // Connect to OBS frontend events
    obs_frontend_add_event_callback(frontendEvent, this);

    createPreviewScene();

    // Load browser source configuration and create browser source
    loadBrowserSourceConfig();
    createBrowserSource();
    syncProgramSource();
    updateSceneLayout();
    lastOverlayUrl_ = overlayUrl_.isEmpty() ? browserConfig.url : overlayUrl_;
    lastEnterAnimUrl_ = enterAnimUrl_;
    lastEnterAnimWidth_ = 0;
    lastEnterAnimHeight_ = 0;

    // Set up browser refresh timer
    browserRefreshTimer->setInterval(1000);  // Refresh every second
    connect(browserRefreshTimer, &QTimer::timeout, this,
            &OneSevenLivePreviewWidget::updateBrowserSource);
    browserRefreshTimer->start();
}

OneSevenLivePreviewWidget::~OneSevenLivePreviewWidget() {
    // Disconnect all signals to prevent calling slots on destroyed objects
    disconnect(this, nullptr, nullptr, nullptr);

    if (refreshTimer) {
        refreshTimer->stop();
    }
    if (browserRefreshTimer) {
        browserRefreshTimer->stop();
    }
    obs_frontend_remove_event_callback(frontendEvent, this);
    setPreviewSceneVisible(false);
    destroyBrowserSource();
    destroyDisplay();
    destroyPreviewScene();
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

    obs_enter_graphics();
    previewDisplay = obs_display_create(&init_data, 0x0);
    obs_leave_graphics();

    if (previewDisplay) {
        display_created = true;
        display_width = physical_width;
        display_height = physical_height;

        obs_display_add_draw_callback(previewDisplay, drawCallback, this);
        updateSceneLayout();

        emit displayCreated(true);
    }
}

void OneSevenLivePreviewWidget::destroyDisplay() {
    if (previewDisplay) {
        obs_display_remove_draw_callback(previewDisplay, drawCallback, this);
        obs_enter_graphics();
        obs_display_destroy(previewDisplay);
        obs_leave_graphics();
        previewDisplay = nullptr;
    }
    display_created = false;

    emit displayCreated(false);
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

    if (previewSceneSource_) {
        obs_source_video_render(previewSceneSource_);
    }

    // Restore graphics state
    gs_projection_pop();
    gs_viewport_pop();
}

void OneSevenLivePreviewWidget::refreshVideo() {
    syncProgramSource();
    updateSceneLayout();

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
        const int logical_width = width();
        const int logical_height = height();
        int phys_cx = static_cast<int>(std::lround(static_cast<double>(logical_width) *
                                                   static_cast<double>(device_pixel_ratio)));
        int phys_cy = static_cast<int>(std::lround(static_cast<double>(logical_height) *
                                                   static_cast<double>(device_pixel_ratio)));

        // obs_log(LOG_INFO, "Resizing display: logical=%dx%d, physical=%dx%d, dpr=%.2f",
        //         display_width, display_height, phys_cx, phys_cy, device_pixel_ratio);

        display_width = phys_cx;
        display_height = phys_cy;

        obs_enter_graphics();
        obs_display_resize(previewDisplay, phys_cx, phys_cy);
        obs_leave_graphics();
        updateSceneLayout();
    }
}

void OneSevenLivePreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    if (display_created && previewDisplay) {
        qreal dpr = 1.0;
        QWindow* window_handle = windowHandle();
        if (!window_handle) {
            window_handle = window()->windowHandle();
        }
        if (window_handle) {
            dpr = window_handle->devicePixelRatio();
        } else {
            QScreen* screen = QGuiApplication::primaryScreen();
            if (screen) {
                dpr = screen->devicePixelRatio();
            }
        }

        int logical_width = event->size().width();
        int logical_height = event->size().height();
        int physical_width = static_cast<int>(logical_width * dpr);
        int physical_height = static_cast<int>(logical_height * dpr);

        display_width = physical_width;
        display_height = physical_height;

        obs_enter_graphics();
        obs_display_resize(previewDisplay, physical_width, physical_height);
        obs_leave_graphics();
        updateSceneLayout();

        // Force refresh to ensure content scales properly with new size
        forceRefresh();
    }
}

void OneSevenLivePreviewWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    setPreviewSceneVisible(true);
    if (refreshTimer) {
        refreshTimer->start();
    }
    if (browserRefreshTimer) {
        browserRefreshTimer->start();
    }
    QTimer::singleShot(0, this, &OneSevenLivePreviewWidget::createDisplay);
}

void OneSevenLivePreviewWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (refreshTimer) {
        refreshTimer->stop();
    }
    if (browserRefreshTimer) {
        browserRefreshTimer->stop();
    }
    setPreviewSceneVisible(false);
    destroyDisplay();
}

void OneSevenLivePreviewWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
}

QPaintEngine* OneSevenLivePreviewWidget::paintEngine() const {
    return nullptr;
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

void OneSevenLivePreviewWidget::createPreviewScene() {
    if (previewScene_) {
        return;
    }

    previewScene_ = obs_scene_create_private("17LivePreviewScene");
    if (!previewScene_) {
        obs_log(LOG_ERROR, "Failed to create private preview scene");
        return;
    }

    previewSceneSource_ = obs_source_get_ref(obs_scene_get_source(previewScene_));
    rebuildPreviewSceneItems();
}

void OneSevenLivePreviewWidget::destroyPreviewScene() {
    if (programItem_) {
        removeSceneItem(programItem_);
    }
    if (browserItem_) {
        removeSceneItem(browserItem_);
    }
    if (enterAnimItem_) {
        removeSceneItem(enterAnimItem_);
    }

    if (currentSource) {
        obs_source_release(currentSource);
        currentSource = nullptr;
    }

    if (previewSceneSource_) {
        obs_source_release(previewSceneSource_);
        previewSceneSource_ = nullptr;
    }

    if (previewScene_) {
        obs_scene_release(previewScene_);
        previewScene_ = nullptr;
    }
}

void OneSevenLivePreviewWidget::setPreviewSceneVisible(bool visible) {
    if (!previewSceneSource_ || previewSceneVisible_ == visible) {
        previewSceneVisible_ = visible;
        return;
    }

    if (visible) {
        obs_source_inc_showing(previewSceneSource_);
        obs_source_inc_active(previewSceneSource_);
    } else {
        obs_source_dec_showing(previewSceneSource_);
        obs_source_dec_active(previewSceneSource_);
    }

    previewSceneVisible_ = visible;
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

    // Only create browser source if we have valid configuration or an overlay URL
    if (!browserConfig.isValid && overlayUrl_.isEmpty()) {
        obs_log(LOG_INFO,
                "No valid browser source configuration and no overlay URL, skipping browser source "
                "creation");
        return;
    }

    // Create settings from configuration (allow override by overlayUrl_)
    ObsDataPtr settings{obs_data_create()};

    QString effectiveUrl;
    if (!overlayUrl_.isEmpty()) {
        effectiveUrl = overlayUrl_;
    } else if (browserConfig.isValid) {
        effectiveUrl = browserConfig.url;
    } else {
        effectiveUrl = "about:blank";  // Should not happen given check above
    }

    // obs_log(LOG_INFO, "Using overlay URL: %s", effectiveUrl.toUtf8().constData());
    obs_data_set_string(settings.get(), "url", effectiveUrl.toUtf8().constData());
    obs_data_set_int(settings.get(), "width", browserConfig.isValid ? browserConfig.width : 1920);
    obs_data_set_int(settings.get(), "height", browserConfig.isValid ? browserConfig.height : 1080);
    obs_data_set_int(settings.get(), "fps", browserConfig.isValid ? browserConfig.fps : 30);
    obs_data_set_bool(settings.get(), "shutdown", false);
    obs_data_set_bool(settings.get(), "restart_when_active", false);
    obs_data_set_bool(settings.get(), "reroute_audio", false);

    QString uniquePath = QDir::homePath() + "/.17Live/obs_browser_storage_preview";
    QDir().mkpath(uniquePath);
    obs_data_set_string(settings.get(), "local_storage_path", uniquePath.toStdString().c_str());

    // Create browser source
    browserSource =
        obs_source_create_private("browser_source", "LivePreviewOverlay", settings.get());

    if (browserSource) {
        obs_log(LOG_INFO, "Preview Cartoon Browser source created successfully");
    } else {
        obs_log(LOG_ERROR, "Failed to create browser source");
    }

    settings.reset();

    // Create enter animation browser source
    if (!enterAnimUrl_.isEmpty()) {
        bool isLandscape = true;
        auto& core = OneSevenLiveCoreManager::getInstance();
        if (core.getStreamManager()) {
            isLandscape = core.getStreamManager()->getRoomInfo().landscape;
        }

        const int enterW = isLandscape ? (browserConfig.isValid ? browserConfig.width : 1920) : 640;
        const int enterH = isLandscape ? (browserConfig.isValid ? browserConfig.height : 1080) : 1136;

        ObsDataPtr enterAnimSettings{obs_data_create()};
        obs_data_set_string(enterAnimSettings.get(), "url", enterAnimUrl_.toUtf8().constData());
        obs_data_set_int(enterAnimSettings.get(), "width", enterW);
        obs_data_set_int(enterAnimSettings.get(), "height", enterH);
        obs_data_set_int(enterAnimSettings.get(), "fps", browserConfig.isValid ? browserConfig.fps : 30);
        obs_data_set_bool(enterAnimSettings.get(), "shutdown", false);
        obs_data_set_bool(enterAnimSettings.get(), "restart_when_active", false);
        obs_data_set_bool(enterAnimSettings.get(), "reroute_audio", false);

        QString enterAnimPath = QDir::homePath() + "/.17Live/obs_browser_storage_enter_anim";
        QDir().mkpath(enterAnimPath);
        obs_data_set_string(enterAnimSettings.get(), "local_storage_path", enterAnimPath.toStdString().c_str());

        enterAnimSource =
            obs_source_create_private("browser_source", "LiveEnterAnimOverlay", enterAnimSettings.get());

        if (enterAnimSource) {
            obs_log(LOG_INFO, "Preview Enter Anim Browser source created successfully");
        } else {
            obs_log(LOG_ERROR, "Failed to create enter anim browser source");
        }

        lastEnterAnimWidth_ = enterW;
        lastEnterAnimHeight_ = enterH;
    }

    rebuildPreviewSceneItems();
    updateSceneLayout();
}

void OneSevenLivePreviewWidget::destroyBrowserSource() {
    if (browserSource) {
        removeSceneItem(browserItem_);
        obs_source_release(browserSource);
        browserSource = nullptr;
    }
    
    if (enterAnimSource) {
        removeSceneItem(enterAnimItem_);
        obs_source_release(enterAnimSource);
        enterAnimSource = nullptr;
    }
}

void OneSevenLivePreviewWidget::updateBrowserSource() {
    if (browserSource) {
        // Force browser source to refresh by triggering a property update
        ObsDataPtr settings{obs_source_get_settings(browserSource)};
        if (settings) {
            // Update the URL to trigger a refresh; overlayUrl_ overrides config
            const QString effectiveUrl = overlayUrl_.isEmpty() ? browserConfig.url : overlayUrl_;
            if (effectiveUrl != lastOverlayUrl_) {
                obs_data_set_string(settings.get(), "url", effectiveUrl.toUtf8().constData());
                obs_source_update(browserSource, settings.get());
                lastOverlayUrl_ = effectiveUrl;
            }
            settings.reset();
        }
    }
    
    if (enterAnimSource) {
        ObsDataPtr settings{obs_source_get_settings(enterAnimSource)};
        if (settings) {
            bool isLandscape = true;
            auto& core = OneSevenLiveCoreManager::getInstance();
            if (core.getStreamManager()) {
                isLandscape = core.getStreamManager()->getRoomInfo().landscape;
            }

            const int enterW = isLandscape ? (browserConfig.isValid ? browserConfig.width : 1920) : 640;
            const int enterH = isLandscape ? (browserConfig.isValid ? browserConfig.height : 1080) : 1136;

            bool changed = false;
            if (enterAnimUrl_ != lastEnterAnimUrl_) {
                obs_data_set_string(settings.get(), "url", enterAnimUrl_.toUtf8().constData());
                lastEnterAnimUrl_ = enterAnimUrl_;
                changed = true;
            }
            if (enterW != lastEnterAnimWidth_ || enterH != lastEnterAnimHeight_) {
                obs_data_set_int(settings.get(), "width", enterW);
                obs_data_set_int(settings.get(), "height", enterH);
                lastEnterAnimWidth_ = enterW;
                lastEnterAnimHeight_ = enterH;
                changed = true;
            }
            if (changed) {
                obs_source_update(enterAnimSource, settings.get());
            }
            settings.reset();
        }
    }
}

void OneSevenLivePreviewWidget::setOverlayScale(float scale) {
    overlayScale = qMax(0.1f, qMin(5.0f, scale));  // Clamp between 0.1 and 5.0

    // Force refresh to apply new scale
    updateSceneLayout();
    forceRefresh();
}

void OneSevenLivePreviewWidget::setOverlayUrl(const QString& url) {
    overlayUrl_ = url;
    if (!browserSource && (!overlayUrl_.isEmpty() || browserConfig.isValid)) {
        createBrowserSource();
    }
    updateBrowserSource();
    updateSceneLayout();
    obs_log(LOG_INFO, "Preview overlay URL %s",
            overlayUrl_.isEmpty() ? "(using config)" : overlayUrl_.toUtf8().constData());
}

void OneSevenLivePreviewWidget::forceRefresh() {
    if (previewDisplay && display_created) {
        obs_display_set_enabled(previewDisplay, true);

        // Also trigger a video refresh
        refreshVideo();
    }
}

void OneSevenLivePreviewWidget::syncDisplaySize() {
    updateVideoInfo();
}

void OneSevenLivePreviewWidget::rebuildDisplay() {
    destroyDisplay();
    if (isVisible()) {
        QTimer::singleShot(0, this, &OneSevenLivePreviewWidget::createDisplay);
    }
}

void OneSevenLivePreviewWidget::syncProgramSource() {
    obs_source_t* newSource = getCurrentProgramSource();
    if (newSource == currentSource) {
        if (newSource) {
            obs_source_release(newSource);
        }
        return;
    }

    removeSceneItem(programItem_);

    if (currentSource) {
        obs_source_release(currentSource);
        currentSource = nullptr;
    }

    currentSource = newSource;

    if (previewScene_ && currentSource) {
        programItem_ = obs_scene_add(previewScene_, currentSource);
        if (programItem_) {
            obs_sceneitem_set_order(programItem_, OBS_ORDER_MOVE_BOTTOM);
        }
    }

    updateSceneLayout();
}

void OneSevenLivePreviewWidget::rebuildPreviewSceneItems() {
    if (!previewScene_) {
        return;
    }

    if (!programItem_ && currentSource) {
        programItem_ = obs_scene_add(previewScene_, currentSource);
        if (programItem_) {
            obs_sceneitem_set_order(programItem_, OBS_ORDER_MOVE_BOTTOM);
        }
    }

    if (!browserItem_ && browserSource) {
        browserItem_ = obs_scene_add(previewScene_, browserSource);
        if (browserItem_) {
            obs_sceneitem_set_order(browserItem_, OBS_ORDER_MOVE_TOP);
        }
    }

    if (!enterAnimItem_ && enterAnimSource) {
        enterAnimItem_ = obs_scene_add(previewScene_, enterAnimSource);
        if (enterAnimItem_) {
            obs_sceneitem_set_order(enterAnimItem_, OBS_ORDER_MOVE_TOP);
        }
    }
}

void OneSevenLivePreviewWidget::removeSceneItem(obs_sceneitem_t*& item) {
    if (!item) {
        return;
    }

    obs_sceneitem_remove(item);
    item = nullptr;
}

void OneSevenLivePreviewWidget::updateSceneLayout() {
    rebuildPreviewSceneItems();

    if (display_width <= 0 || display_height <= 0) {
        return;
    }

    const float previewWidth = static_cast<float>(display_width);
    const float previewHeight = static_cast<float>(display_height);

    if (programItem_) {
        obs_transform_info itemInfo = {};
        vec2_set(&itemInfo.pos, 0.0f, 0.0f);
        vec2_set(&itemInfo.scale, 1.0f, 1.0f);
        itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
        itemInfo.rot = 0.0f;
        vec2_set(&itemInfo.bounds, previewWidth, previewHeight);
        itemInfo.bounds_type = OBS_BOUNDS_SCALE_INNER;
        itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
        itemInfo.crop_to_bounds = false;
        obs_sceneitem_set_info2(programItem_, &itemInfo);
        obs_sceneitem_set_visible(programItem_, true);
    }

    if (browserItem_) {
        obs_transform_info itemInfo = {};
        vec2_set(&itemInfo.pos, 0.0f, 0.0f);
        vec2_set(&itemInfo.scale, overlayScale, overlayScale);
        itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
        itemInfo.rot = 0.0f;
        vec2_set(&itemInfo.bounds, previewWidth, previewHeight);
        itemInfo.bounds_type = OBS_BOUNDS_SCALE_OUTER;
        itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
        itemInfo.crop_to_bounds = false;
        obs_sceneitem_set_info2(browserItem_, &itemInfo);
        obs_sceneitem_set_visible(browserItem_, true);
        obs_sceneitem_set_order(browserItem_, OBS_ORDER_MOVE_TOP);
    }

    if (enterAnimItem_) {
        bool isLandscape = true;
        obs_video_info ovi{};
        if (obs_get_video_info(&ovi) && ovi.base_width > 0 && ovi.base_height > 0) {
            isLandscape = ovi.base_width >= ovi.base_height;
        }

        const float roomAspect = isLandscape ? (16.0f / 9.0f) : (640.0f / 1136.0f);
        float regionHeight = previewHeight;
        float regionWidth = regionHeight * roomAspect;
        if (regionWidth > previewWidth) {
            regionWidth = previewWidth;
            regionHeight = regionWidth / roomAspect;
        }

        const float regionX = (previewWidth - regionWidth) * 0.5f;
        const float regionY =
            isLandscape ? (previewHeight - regionHeight) * 0.5f : (previewHeight - regionHeight);

        obs_transform_info itemInfo = {};
        vec2_set(&itemInfo.pos, regionX, regionY);
        vec2_set(&itemInfo.scale, overlayScale, overlayScale);
        itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
        itemInfo.rot = 0.0f;
        vec2_set(&itemInfo.bounds, regionWidth, regionHeight);
        itemInfo.bounds_type = OBS_BOUNDS_SCALE_INNER;
        itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
        itemInfo.crop_to_bounds = false;
        obs_sceneitem_set_info2(enterAnimItem_, &itemInfo);
        obs_sceneitem_set_visible(enterAnimItem_, !enterAnimUrl_.isEmpty());
        obs_sceneitem_set_order(enterAnimItem_, OBS_ORDER_MOVE_TOP);
    }
}

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
#include <cmath>

#include "moc_OneSevenLivePreviewWidget.cpp"

OneSevenLivePreviewWidget::OneSevenLivePreviewWidget(QWidget* parent)
    : QWidget(parent),
      previewDisplay(nullptr),
      display_created(false),
      currentSource(nullptr),
      refreshTimer(new QTimer(this)),
      display_width(0),
      display_height(0) {
    
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
}

OneSevenLivePreviewWidget::~OneSevenLivePreviewWidget() {
    if (refreshTimer) {
        refreshTimer->stop();
    }
    obs_frontend_remove_event_callback(frontendEvent, this);
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
    if (!currentSource) {
        // Clear background if no source
        vec4 clear_color;
        vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
        gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
        return;
    }
    
    // Set up viewport and projection
    gs_viewport_push();
    gs_projection_push();
    
    // Set viewport to match display size
    gs_set_viewport(0, 0, cx, cy);
    
    // Set up orthographic projection
    gs_ortho(0.0f, static_cast<float>(cx), 0.0f, static_cast<float>(cy), -100.0f, 100.0f);
    
    // Clear background
    vec4 clear_color;
    vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
    gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
    
    // Get source dimensions
    uint32_t source_width = obs_source_get_width(currentSource);
    uint32_t source_height = obs_source_get_height(currentSource);
    
    if (source_width > 0 && source_height > 0) {
        // Calculate scaling to fit the widget while maintaining aspect ratio
        float scale_x = static_cast<float>(cx) / static_cast<float>(source_width);
        float scale_y = static_cast<float>(cy) / static_cast<float>(source_height);
        
        // Use the smaller scale to maintain aspect ratio
        float scale = std::min(scale_x, scale_y);
        
        // Calculate the actual rendered size
        float rendered_width = static_cast<float>(source_width) * scale;
        float rendered_height = static_cast<float>(source_height) * scale;
        
        // Calculate offsets to center the video
        float offset_x = (static_cast<float>(cx) - rendered_width) / 2.0f;
        float offset_y = (static_cast<float>(cy) - rendered_height) / 2.0f;
        
        obs_log(LOG_INFO, "OneSevenLivePreviewWidget: Source %dx%d Widget %dx%d Scale:%.3f Rendered:%.1fx%.1f Offset:%.1f,%.1f", 
                source_width, source_height, cx, cy, scale, rendered_width, rendered_height, offset_x, offset_y);
        
        // Set up transformation matrix with scaling and centering (correct order: scale first, then translate)
        gs_matrix_push();
        gs_matrix_scale3f(scale, scale, 1.0f);
        gs_matrix_translate3f(offset_x, offset_y, 0.0f);
        
        // Render the source
        obs_source_video_render(currentSource);
        
        gs_matrix_pop();
    } else {
        // Render without scaling if dimensions are invalid
        obs_source_video_render(currentSource);
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

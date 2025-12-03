#pragma once

#include <obs-frontend-api.h>
#include <obs.h>

#include <QLabel>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "OneSevenLivePreviewConfigLoader.hpp"

class OneSevenLivePreviewWidget : public QWidget {
    Q_OBJECT

   public:
    explicit OneSevenLivePreviewWidget(QWidget* parent = nullptr);
    ~OneSevenLivePreviewWidget();

    /**
     * @brief Set the overlay scale factor for browser sources
     * @param scale Scale factor (1.0 = original size)
     */
    void setOverlayScale(float scale);

    /**
     * @brief Force refresh the display and overlays
     */
    void forceRefresh();
    void syncDisplaySize();

    /**
     * @brief Set an override URL for the browser overlay.
     *        When set (non-empty), this URL is used instead of config-defined URL.
     */
    void setOverlayUrl(const QString& url);

   protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

   private slots:
    void refreshVideo();

   private:
    void createDisplay();
    void destroyDisplay();
    void updateVideoInfo();
    void loadBrowserSourceConfig();
    void createBrowserSource();
    void destroyBrowserSource();
    void updateBrowserSource();
    obs_source_t* getCurrentProgramSource();
    static void drawCallback(void* data, uint32_t cx, uint32_t cy);
    void renderScene(uint32_t cx, uint32_t cy);
    static void frontendEvent(enum obs_frontend_event event, void* data);

    // Core display components
    obs_display_t* previewDisplay;
    bool display_created;

    // Video source management
    obs_source_t* currentSource;
    QTimer* refreshTimer;

    // Display dimensions
    int display_width;
    int display_height;

    QLabel* initPlaceholderLabel = nullptr;

    // Browser source overlay components
    obs_source_t* browserSource = nullptr;
    OneSevenLivePreviewConfigLoader* configLoader = nullptr;
    OneSevenLivePreviewConfigLoader::PreviewConfig browserConfig;
    QTimer* browserRefreshTimer = nullptr;

    // Overlay scaling
    float overlayScale = 1.0f;

    // Optional overlay URL override
    QString overlayUrl_;
};

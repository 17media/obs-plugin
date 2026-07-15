#pragma once

#include <obs-frontend-api.h>
#include <obs.h>

#include <QLabel>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "OneSevenLivePreviewConfigLoader.hpp"

class QEvent;
class QWindow;

class OneSevenLivePreviewWidget : public QWidget {
    Q_OBJECT

   public:
    explicit OneSevenLivePreviewWidget(QWidget* parent = nullptr,
                                       const QString& overlayUrl = QString(),
                                       const QString& enterAnimUrl = QString());
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
    void rebuildDisplay();
    void rebuildDisplayAfterDelay(int delayMs);

    /**
     * @brief Set an override URL for the browser overlay.
     *        When set (non-empty), this URL is used instead of config-defined URL.
     */
    void setOverlayUrl(const QString& url);

   protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    QPaintEngine* paintEngine() const override;

   private slots:
    void refreshVideo();

   signals:
    void displayCreated(bool created);

   private:
    void scheduleCreateDisplay(int delayMs = 0);
    void scheduleRefresh(int delayMs = 0);
    void updateTrackedWindow();
    void clearTrackedWindow();
    void createDisplay();
    void destroyDisplay();
    void updateVideoInfo();
    void createPreviewScene();
    void destroyPreviewScene();
    void setPreviewSceneVisible(bool visible);
    void loadBrowserSourceConfig();
    void createBrowserSource();
    void destroyBrowserSource();
    void updateBrowserSource();
    void syncProgramSource();
    void rebuildPreviewSceneItems();
    void removeSceneItem(obs_sceneitem_t*& item);
    void updateSceneLayout();
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
    obs_scene_t* previewScene_{nullptr};
    obs_source_t* previewSceneSource_{nullptr};
    obs_sceneitem_t* programItem_{nullptr};
    obs_sceneitem_t* browserItem_{nullptr};
    obs_sceneitem_t* enterAnimItem_{nullptr};
    bool previewSceneVisible_{false};

    // Display dimensions
    int display_width;
    int display_height;
    WId boundWindowId_{0};
    QPointer<QWindow> trackedWindow_{nullptr};
    QTimer* createDisplayTimer_{nullptr};
    QTimer* refreshDisplayTimer_{nullptr};

    // Browser source overlay components
    obs_source_t* browserSource = nullptr;
    obs_source_t* enterAnimSource = nullptr;
    OneSevenLivePreviewConfigLoader* configLoader = nullptr;
    OneSevenLivePreviewConfigLoader::PreviewConfig browserConfig;
    QTimer* browserRefreshTimer = nullptr;

    // Overlay scaling
    float overlayScale = 1.0f;

    // Optional overlay URL override
    QString overlayUrl_;
    QString enterAnimUrl_;
    QString lastOverlayUrl_;
    QString lastEnterAnimUrl_;
    int lastEnterAnimWidth_{0};
    int lastEnterAnimHeight_{0};
};

#pragma once

#include <obs-frontend-api.h>
#include <obs.h>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>

#include "../utility/OneSevenLivePreviewConfigLoader.hpp"

// Custom container widget that properly handles paintEvent when WA_PaintOnScreen is set
class PreviewContainerWidget : public QWidget {
    Q_OBJECT

public:
    explicit PreviewContainerWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

class OneSevenLivePreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit OneSevenLivePreviewWidget(QWidget* parent = nullptr);
    ~OneSevenLivePreviewWidget();

    void setupPreview();
    void createOverlaySource(const OneSevenLivePreviewConfigLoader::PreviewConfig& config);
    void updatePreview();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onSceneChanged();

private:
    void setupPreviewDisplay();
    void cleanupPreview();
    void createBrowserSource(const OneSevenLivePreviewConfigLoader::PreviewConfig& config);
    static void renderPreview(void* data, uint32_t cx, uint32_t cy);
    static void frontendEvent(enum obs_frontend_event event, void* data);

    QVBoxLayout* mainLayout;
    PreviewContainerWidget* previewContainer;
    QLabel* statusLabel;
    QTimer* refreshTimer;
    
    obs_display_t* previewDisplay;
    obs_source_t* overlaySource;
    obs_scene_t* compositeScene;
    obs_source_t* compositeSceneSource;
    obs_sceneitem_t* backgroundItem;
    obs_sceneitem_t* overlayItem;
    
    bool previewActive;
    uint32_t previewWidth;
    uint32_t previewHeight;
};

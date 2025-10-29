#pragma once

#include <obs-frontend-api.h>
#include <obs.h>
#include <QObject>
#include <QWidget>
#include <QTimer>

class OneSevenLivePreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit OneSevenLivePreviewWidget(QWidget* parent = nullptr);
    ~OneSevenLivePreviewWidget();

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
};

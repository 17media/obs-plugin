#pragma once

#include <QDockWidget>
#include <QPointer>

#include "OneSevenLivePreviewWidget.hpp"
#include "../utility/OneSevenLivePreviewConfigLoader.hpp"

class OneSevenLivePreviewDock : public QDockWidget {
    Q_OBJECT

public:
    explicit OneSevenLivePreviewDock(QWidget* parent = nullptr);
    ~OneSevenLivePreviewDock();

    void initializePreview();

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

signals:
    void dockClosed();

private:
    void setupUi();
    void loadConfiguration();
    
    QPointer<OneSevenLivePreviewWidget> previewWidget;
    QPointer<OneSevenLivePreviewConfigLoader> configLoader;
    
    bool initialized;
};

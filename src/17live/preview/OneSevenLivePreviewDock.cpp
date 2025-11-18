#include "OneSevenLivePreviewDock.hpp"

#include <obs-module.h>

#include <QCloseEvent>
#include <QShowEvent>

#include "../../plugin-support.h"
#include "../streaming/OneSevenLiveStreamManager.hpp"
#include "moc_OneSevenLivePreviewDock.cpp"

OneSevenLivePreviewDock::OneSevenLivePreviewDock(QWidget* parent, const QString& overlayUrl)
    : QDockWidget(obs_module_text("PreviewDock.Title"), parent),
      overlayUrl_(overlayUrl),
      previewWidget(nullptr),
      initialized(false) {
    setupUi();
}

OneSevenLivePreviewDock::~OneSevenLivePreviewDock() {
    if (previewWidget) {
        previewWidget->deleteLater();
    }
}

void OneSevenLivePreviewDock::setupUi() {
    setObjectName("OneSevenLivePreviewDock");
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                QDockWidget::DockWidgetClosable);

    // Set minimum size for vertical preview (160x284 minimum)
    setMinimumSize(180, 320);
    resize(400, 720);

    previewWidget = new OneSevenLivePreviewWidget(this);
    setWidget(previewWidget);
    if (previewWidget && !overlayUrl_.isEmpty()) {
        previewWidget->setOverlayUrl(overlayUrl_);
    }
}

void OneSevenLivePreviewDock::updatePreviewGeometry() {
    if (!previewWidget) return;
    const QRect cr = contentsRect();
    int cw = cr.width();
    int ch = cr.height();

    bool isLandscape = true;
    auto& core = OneSevenLiveCoreManager::getInstance();
    if (core.getStreamManager()) {
        isLandscape = core.getStreamManager()->getRoomInfo().landscape;
    }

    double aspect = isLandscape ? (16.0 / 9.0) : (640.0 / 1136.0);
    int targetW = cw;
    int targetH = static_cast<int>(std::round(targetW / aspect));
    if (targetH > ch) {
        targetH = ch;
        targetW = cw; // keep width full per requirement
    }
    int x = cr.x() + (cw - targetW) / 2;
    int y = cr.y() + (ch - targetH) / 2;
    previewWidget->setGeometry(x, y, targetW, targetH);
}

void OneSevenLivePreviewDock::initializePreview() {
    if (!initialized && previewWidget) {
        initialized = true;
    }
}

void OneSevenLivePreviewDock::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);

    if (!initialized) {
        initializePreview();
    }

    updatePreviewGeometry();
}

void OneSevenLivePreviewDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);
    updatePreviewGeometry();
}

void OneSevenLivePreviewDock::closeEvent(QCloseEvent* event) {
    emit dockClosed();
    QDockWidget::closeEvent(event);
}

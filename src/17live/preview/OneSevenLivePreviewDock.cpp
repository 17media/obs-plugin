#include "OneSevenLivePreviewDock.hpp"

#include <obs-module.h>

#include <QCloseEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QLabel>
#include <QSizePolicy>

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

    container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QHBoxLayout* hintLayout = new QHBoxLayout();
    hintLayout->setContentsMargins(10, 10, 10, 0);
    hintLayout->setSpacing(5);
    hintLayout->setAlignment(Qt::AlignHCenter);

    QLabel* icon = new QLabel(container);
    icon->setFixedSize(20, 20);
    icon->setPixmap(QPixmap(":/resources/alert-white.svg")
                        .scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    notificationLabel = new QLabel(
        QString::fromUtf8(obs_module_text("PreviewDock.Tip.AnimationOnly")), container);
    notificationLabel->setWordWrap(true);
    notificationLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    notificationLabel->setStyleSheet("color: white; font-size: 14px;");

    // Add leading stretch to center contents
    hintLayout->addStretch();
    hintLayout->addWidget(icon);
    hintLayout->addWidget(notificationLabel);
    hintLayout->addStretch();

    QWidget* hintContainer = new QWidget(container);
    hintContainer->setLayout(hintLayout);

    previewContainer = new QWidget(container);
    previewContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(hintContainer);
    layout->addWidget(previewContainer, 1);

    setWidget(container);

    previewWidget = new OneSevenLivePreviewWidget(previewContainer);
    if (previewWidget && !overlayUrl_.isEmpty()) {
        previewWidget->setOverlayUrl(overlayUrl_);
    }

    // Loading overlay
    loadingOverlay = new QWidget(container);
    loadingOverlay->setStyleSheet("background-color: rgba(0, 0, 0, 180);");
    loadingOverlay->hide();

    QVBoxLayout* overlayLayout = new QVBoxLayout(loadingOverlay);
    overlayLayout->setAlignment(Qt::AlignCenter);

    loadingLabel = new QLabel(obs_module_text("PreviewDock.LoadingGifts"), loadingOverlay);
    loadingLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    overlayLayout->addWidget(loadingLabel);

    auto& core = OneSevenLiveCoreManager::getInstance();
    connect(&core, &OneSevenLiveCoreManager::giftsLoaded, this,
            &OneSevenLivePreviewDock::onGiftsLoaded);

    if (!core.isGiftsLoaded()) {
        loadingOverlay->show();
        loadingOverlay->raise();
    }
}

void OneSevenLivePreviewDock::updatePreviewGeometry() {
    if (!previewWidget)
        return;
    if (!previewContainer)
        return;
    int cw = previewContainer->width();
    int ch = previewContainer->height();
    if (cw <= 0 || ch <= 0)
        return;

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
        targetW = cw;  // keep width full per requirement
    }
    int x = (cw - targetW) / 2;
    int y = (ch - targetH) / 2;
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

    if (loadingOverlay && loadingOverlay->isVisible() && container) {
        loadingOverlay->resize(container->size());
        loadingOverlay->raise();
    }

    if (previewWidget) {
        QTimer::singleShot(0, previewWidget, &OneSevenLivePreviewWidget::syncDisplaySize);
        QTimer::singleShot(0, previewWidget, &OneSevenLivePreviewWidget::forceRefresh);
    }
}

void OneSevenLivePreviewDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);
    updatePreviewGeometry();

    if (loadingOverlay && loadingOverlay->isVisible() && container) {
        loadingOverlay->resize(container->size());
        loadingOverlay->raise();
    }

    if (previewWidget) {
        QTimer::singleShot(0, previewWidget, &OneSevenLivePreviewWidget::syncDisplaySize);
        QTimer::singleShot(0, previewWidget, &OneSevenLivePreviewWidget::forceRefresh);
    }
}

void OneSevenLivePreviewDock::closeEvent(QCloseEvent* event) {
    emit dockClosed();
    QDockWidget::closeEvent(event);
}

void OneSevenLivePreviewDock::onGiftsLoaded() {
    if (loadingOverlay) {
        loadingOverlay->hide();
    }
}

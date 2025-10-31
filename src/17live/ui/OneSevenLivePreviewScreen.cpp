#include "OneSevenLivePreviewScreen.hpp"

#include <obs-module.h>
#include <graphics/graphics.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QPaintEvent>
#include <cmath>

#include "../../plugin-support.h"
#include "utility/OneSevenLivePreviewConfigLoader.hpp"
#include "moc_OneSevenLivePreviewScreen.cpp"

OneSevenLivePreviewScreen::OneSevenLivePreviewScreen(QWidget* parent)
    : QWidget(parent),
      previewWidget(nullptr),
      configLoader(nullptr),
      currentPreviewSize(PREVIEW_WIDTH, PREVIEW_HEIGHT),
      currentPreviewPosition(0, 0),
      layoutUpdatePending(false),
      layoutUpdateTimer(nullptr) {
    setupUi();
}

OneSevenLivePreviewScreen::~OneSevenLivePreviewScreen() {
    if (previewWidget) {
        previewWidget->deleteLater();
    }

    if (layoutUpdateTimer) {
        layoutUpdateTimer->deleteLater();
    }
}

void OneSevenLivePreviewScreen::setupUi() {
    setObjectName("OneSevenLivePreviewScreen");
    
    // Set minimum size based on preview screen constraints
    setMinimumSize(MIN_WIDTH, MIN_HEIGHT);
    
    // Create layout update timer
    layoutUpdateTimer = new QTimer(this);
    layoutUpdateTimer->setSingleShot(true);
    layoutUpdateTimer->setInterval(16); // ~60fps update rate
    connect(layoutUpdateTimer, &QTimer::timeout, this, &OneSevenLivePreviewScreen::updatePreviewLayout);
    
    // Create preview widget
    previewWidget = new OneSevenLivePreviewWidget(this);
    previewWidget->setObjectName("PreviewWidget");
    
    // Set initial preview geometry
    updatePreviewGeometry();
    
    // Enable custom painting for rounded border
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);
}

OneSevenLivePreviewWidget* OneSevenLivePreviewScreen::getPreviewWidget() const {
    return previewWidget;
}

void OneSevenLivePreviewScreen::setConfigLoader(OneSevenLivePreviewConfigLoader* loader) {
    configLoader = loader;
    if (previewWidget && configLoader) {
        // Pass config loader to preview widget if needed
        // previewWidget->setConfigLoader(configLoader);
    }
}

void OneSevenLivePreviewScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    
    // Schedule layout update to avoid excessive updates during resize
    if (!layoutUpdatePending) {
        layoutUpdatePending = true;
        layoutUpdateTimer->start();
    }
}

void OneSevenLivePreviewScreen::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw background
    painter.fillRect(rect(), QColor(30, 30, 30)); // Dark background
    
    // Draw preview screen border with rounded corners
    if (currentPreviewSize.isValid() && !currentPreviewSize.isEmpty()) {
        QRect previewRect(currentPreviewPosition, currentPreviewSize);
        
        // Create rounded rectangle path
        QPainterPath path;
        path.addRoundedRect(previewRect, BORDER_RADIUS, BORDER_RADIUS);
        
        // Draw border
        painter.setPen(QPen(QColor(100, 100, 100), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
        
        // Draw inner shadow effect
        painter.setPen(QPen(QColor(0, 0, 0, 50), 1));
        QRect innerRect = previewRect.adjusted(1, 1, -1, -1);
        QPainterPath innerPath;
        innerPath.addRoundedRect(innerRect, BORDER_RADIUS - 1, BORDER_RADIUS - 1);
        painter.drawPath(innerPath);
    }
}

void OneSevenLivePreviewScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updatePreviewLayout();
}

void OneSevenLivePreviewScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
}

void OneSevenLivePreviewScreen::updatePreviewLayout() {
    layoutUpdatePending = false;
    updatePreviewGeometry();
    update(); // Trigger repaint for border
}

QSize OneSevenLivePreviewScreen::calculatePreviewSize(const QSize& containerSize) const {
    if (containerSize.isEmpty()) {
        return QSize(MIN_WIDTH, MIN_HEIGHT);
    }
    
    // Calculate size based on container while maintaining aspect ratio
    int maxWidth = containerSize.width() - 20; // Leave some margin
    int maxHeight = containerSize.height() - 20;
    
    // Calculate size that fits within container while maintaining aspect ratio
    int targetWidth = maxWidth;
    int targetHeight = static_cast<int>(targetWidth / ASPECT_RATIO);
    
    if (targetHeight > maxHeight) {
        targetHeight = maxHeight;
        targetWidth = static_cast<int>(targetHeight * ASPECT_RATIO);
    }
    
    // Apply size constraints
    targetWidth = qBound(MIN_WIDTH, targetWidth, PREVIEW_WIDTH);
    targetHeight = qBound(MIN_HEIGHT, targetHeight, PREVIEW_HEIGHT);
    
    // Ensure aspect ratio is maintained
    if (static_cast<double>(targetWidth) / targetHeight > ASPECT_RATIO) {
        targetWidth = static_cast<int>(targetHeight * ASPECT_RATIO);
    } else {
        targetHeight = static_cast<int>(targetWidth / ASPECT_RATIO);
    }
    
    return QSize(targetWidth, targetHeight);
}

QPoint OneSevenLivePreviewScreen::calculatePreviewPosition(const QSize& containerSize, const QSize& previewSize) const {
    if (containerSize.isEmpty() || previewSize.isEmpty()) {
        return QPoint(0, 0);
    }
    
    // Center the preview screen in the container
    int x = (containerSize.width() - previewSize.width()) / 2;
    int y = (containerSize.height() - previewSize.height()) / 2;
    
    return QPoint(qMax(0, x), qMax(0, y));
}

void OneSevenLivePreviewScreen::updatePreviewGeometry() {
    if (!previewWidget) {
        return;
    }
    
    QSize containerSize = size();
    currentPreviewSize = calculatePreviewSize(containerSize);
    currentPreviewPosition = calculatePreviewPosition(containerSize, currentPreviewSize);
    
    // Update preview widget geometry to fill the preview screen area
    QRect previewRect(currentPreviewPosition, currentPreviewSize);
    
    // Apply border radius clipping to preview widget
    previewWidget->setGeometry(previewRect);
    
    // Set clipping mask for rounded corners
    QPainterPath clipPath;
    clipPath.addRoundedRect(QRect(0, 0, currentPreviewSize.width(), currentPreviewSize.height()), 
                           BORDER_RADIUS, BORDER_RADIUS);
    
    QRegion clipRegion(clipPath.toFillPolygon().toPolygon());
    previewWidget->setMask(clipRegion);
    
    // Calculate overlay scale based on preview screen size relative to reference size
    float scaleX = static_cast<float>(currentPreviewSize.width()) / PREVIEW_WIDTH;
    float scaleY = static_cast<float>(currentPreviewSize.height()) / PREVIEW_HEIGHT;
    float overlayScale = qMin(scaleX, scaleY); // Use the smaller scale to maintain aspect ratio
    
    // Set overlay scale for browser sources
    previewWidget->setOverlayScale(overlayScale);
    
    // Ensure preview widget is visible
    previewWidget->setVisible(true);
    previewWidget->raise();
}

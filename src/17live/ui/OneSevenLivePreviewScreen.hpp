#pragma once

#include <QWidget>
#include <QPointer>
#include <QTimer>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QShowEvent>
#include <QHideEvent>

#include "OneSevenLivePreviewWidget.hpp"

class OneSevenLivePreviewConfigLoader;

/**
 * @brief A vertical preview screen widget with 640x1136 aspect ratio
 * 
 * This widget provides a mobile-like vertical preview experience with:
 * - Fixed 640x1136 aspect ratio (9:16)
 * - Rounded border with 8px radius
 * - Proportional scaling with size constraints (160x284 to 640x1136)
 * - Centered positioning when container is larger than preview screen
 * - Overlay sources that scale with the preview screen
 */
class OneSevenLivePreviewScreen : public QWidget {
    Q_OBJECT

public:
    explicit OneSevenLivePreviewScreen(QWidget* parent = nullptr);
    ~OneSevenLivePreviewScreen();

    // Preview screen constants
    static constexpr int PREVIEW_WIDTH = 640;
    static constexpr int PREVIEW_HEIGHT = 1136;
    static constexpr int MIN_WIDTH = 160;
    static constexpr int MIN_HEIGHT = 284;
    static constexpr int BORDER_RADIUS = 8;
    static constexpr double ASPECT_RATIO = static_cast<double>(PREVIEW_WIDTH) / PREVIEW_HEIGHT;

    /**
     * @brief Get the current preview widget
     * @return Pointer to the preview widget
     */
    OneSevenLivePreviewWidget* getPreviewWidget() const;

    /**
     * @brief Set configuration loader for the preview
     * @param loader Configuration loader instance
     */
    void setConfigLoader(OneSevenLivePreviewConfigLoader* loader);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    /**
     * @brief Update preview screen layout when container size changes
     */
    void updatePreviewLayout();

private:
    /**
     * @brief Setup the UI components
     */
    void setupUi();

    /**
     * @brief Calculate optimal preview screen size based on container size
     * @param containerSize Size of the container widget
     * @return Optimal preview screen size
     */
    QSize calculatePreviewSize(const QSize& containerSize) const;

    /**
     * @brief Calculate preview screen position for centering
     * @param containerSize Size of the container widget
     * @param previewSize Size of the preview screen
     * @return Position for centering the preview screen
     */
    QPoint calculatePreviewPosition(const QSize& containerSize, const QSize& previewSize) const;

    /**
     * @brief Update preview widget geometry and scaling
     */
    void updatePreviewGeometry();

    // UI Components
    QPointer<OneSevenLivePreviewWidget> previewWidget;
    QPointer<OneSevenLivePreviewConfigLoader> configLoader;

    // Layout properties
    QSize currentPreviewSize;
    QPoint currentPreviewPosition;
    bool layoutUpdatePending;

    // Update timer to avoid excessive layout updates
    QTimer* layoutUpdateTimer;
};

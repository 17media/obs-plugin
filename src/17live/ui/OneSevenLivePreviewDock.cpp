#include "OneSevenLivePreviewDock.hpp"

#include <obs-module.h>

#include <QCloseEvent>
#include <QShowEvent>
#include <QStandardPaths>

#include "../../plugin-support.h"
#include "moc_OneSevenLivePreviewDock.cpp"


// Helper function to get module data path (static to avoid symbol conflicts)
static std::string get_obs_module_data_path_str() {
    const char* path = obs_get_module_data_path(obs_current_module());
    if (path) {
        return std::string(path);
    }
    return "";  // Or throw exception, or return a default known path
}

OneSevenLivePreviewDock::OneSevenLivePreviewDock(QWidget* parent, const QString& overlayUrl)
    : QDockWidget(obs_module_text("PreviewDock.Title"), parent),
      overlayUrl_(overlayUrl),
      previewScreen(nullptr),
      configLoader(nullptr),
      initialized(false) {
    setupUi();
    loadConfiguration();
}

OneSevenLivePreviewDock::~OneSevenLivePreviewDock() {
    if (previewScreen) {
        previewScreen->deleteLater();
    }

    if (configLoader) {
        configLoader->deleteLater();
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

    // Create preview screen widget
    previewScreen = new OneSevenLivePreviewScreen(this);
    setWidget(previewScreen);

    // If an overlayUrl was provided, pass it to the preview widget to override
    if (previewScreen && !overlayUrl_.isEmpty()) {
        OneSevenLivePreviewWidget* widget = previewScreen->getPreviewWidget();
        if (widget) {
            widget->setOverlayUrl(overlayUrl_);
        }
    }
}

void OneSevenLivePreviewDock::loadConfiguration() {
    configLoader = new OneSevenLivePreviewConfigLoader(this);

    // Get plugin data directory
    std::string dataPath = get_obs_module_data_path_str();
    QString configPath = QString("%1/preview_config.json").arg(QString::fromStdString(dataPath));

    if (configLoader->loadConfiguration(configPath)) {
        // Pass configuration to preview screen
        if (previewScreen) {
            previewScreen->setConfigLoader(configLoader);
        }
    } else {
        obs_log(LOG_WARNING, "Failed to load preview configuration, using defaults");
    }
}

void OneSevenLivePreviewDock::initializePreview() {
    if (!initialized && previewScreen) {
        // Preview screen is automatically initialized when shown
        initialized = true;
    }
}

void OneSevenLivePreviewDock::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);

    if (!initialized) {
        initializePreview();
    }

    // Preview screen automatically updates when shown
}

void OneSevenLivePreviewDock::closeEvent(QCloseEvent* event) {
    emit dockClosed();
    QDockWidget::closeEvent(event);
}

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

OneSevenLivePreviewDock::OneSevenLivePreviewDock(QWidget* parent)
    : QDockWidget(obs_module_text("PreviewDock.Title"), parent),
      previewWidget(nullptr),
      configLoader(nullptr),
      initialized(false) {
    setupUi();
    loadConfiguration();
}

OneSevenLivePreviewDock::~OneSevenLivePreviewDock() {
    if (previewWidget) {
        previewWidget->deleteLater();
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

    // Set minimum size
    setMinimumSize(320, 240);
    resize(640, 480);

    // Create preview widget
    previewWidget = new OneSevenLivePreviewWidget(this);
    setWidget(previewWidget);
}

void OneSevenLivePreviewDock::loadConfiguration() {
    configLoader = new OneSevenLivePreviewConfigLoader(this);

    // Get plugin data directory
    std::string dataPath = get_obs_module_data_path_str();
    QString configPath = QString("%1/preview_config.json").arg(QString::fromStdString(dataPath));

    if (configLoader->loadConfiguration(configPath)) {
        // Configuration loaded but not applied to simplified preview widget
    } else {
        obs_log(LOG_WARNING, "Failed to load preview configuration, using defaults");
    }
}

void OneSevenLivePreviewDock::initializePreview() {
    if (!initialized && previewWidget) {
        // Preview widget is automatically initialized when shown
        initialized = true;
    }
}

void OneSevenLivePreviewDock::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);

    if (!initialized) {
        initializePreview();
    }

    // Preview widget automatically updates when shown
}

void OneSevenLivePreviewDock::closeEvent(QCloseEvent* event) {
    emit dockClosed();
    QDockWidget::closeEvent(event);
}

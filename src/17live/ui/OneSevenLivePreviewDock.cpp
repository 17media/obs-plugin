#include "OneSevenLivePreviewDock.hpp"

#include "../../plugin-support.h"
#include <obs-module.h>
#include <QCloseEvent>
#include <QShowEvent>
#include <QStandardPaths>

#include "moc_OneSevenLivePreviewDock.cpp"

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
    setFeatures(QDockWidget::DockWidgetMovable | 
                QDockWidget::DockWidgetFloatable | 
                QDockWidget::DockWidgetClosable);
    
    // Set minimum size
    setMinimumSize(320, 240);
    resize(640, 480);
    
    // Create preview widget
    previewWidget = new OneSevenLivePreviewWidget(this);
    setWidget(previewWidget);
    
    obs_log(LOG_INFO, "Preview dock UI setup completed");
}

void OneSevenLivePreviewDock::loadConfiguration() {
    configLoader = new OneSevenLivePreviewConfigLoader(this);
    
    // Get plugin data directory
    char* dataPath = obs_module_get_config_path(obs_current_module(), "");
    QString configPath = QString("%1/preview_config.json").arg(dataPath);
    bfree(dataPath);
    
    if (configLoader->loadConfiguration(configPath)) {
        // Configuration loaded but not applied to simplified preview widget
        obs_log(LOG_INFO, "Preview configuration loaded successfully");
    } else {
        obs_log(LOG_WARNING, "Failed to load preview configuration, using defaults");
    }
}

void OneSevenLivePreviewDock::initializePreview() {
    if (!initialized && previewWidget) {
        // Preview widget is automatically initialized when shown
        initialized = true;
        obs_log(LOG_INFO, "Preview dock initialized");
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

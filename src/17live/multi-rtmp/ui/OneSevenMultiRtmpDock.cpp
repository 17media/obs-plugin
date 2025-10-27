#include "OneSevenMultiRtmpDock.hpp"
#include "OneSevenMultiRtmpListWidget.hpp"
#include "OneSevenMultiRtmpConfigDialog.hpp"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>

OneSevenMultiRtmpDock::OneSevenMultiRtmpDock(QWidget* parent)
    : QDockWidget(parent)
    , m_manager(OneSevenMultiRtmpManager::getInstance())
    , m_streamListWidget(nullptr)
    , m_configDialog(nullptr)
{
    obs_log(LOG_INFO, "[MultiRTMP-Dock] Manager instance obtained, initialization will be done on first use");
    
    setupUI();
    setupConnections();
    setupManagerCallbacks();
}

OneSevenMultiRtmpDock::~OneSevenMultiRtmpDock()
{
    if (m_statsUpdateTimer) {
        m_statsUpdateTimer->stop();
    }
    
    if (m_configDialog) {
        m_configDialog->deleteLater();
    }
}

void OneSevenMultiRtmpDock::setupUI()
{
    // Create central widget
    m_centralWidget = new QWidget(this);
    setWidget(m_centralWidget);
    
    // Main layout
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Stream list section (top part)
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameStyle(QFrame::NoFrame);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    m_streamListWidget = new OneSevenMultiRtmpListWidget();
    m_scrollArea->setWidget(m_streamListWidget);
    
    // Control section (bottom part)
    m_controlFrame = new QFrame();
    m_controlFrame->setFrameStyle(QFrame::NoFrame);
    m_controlLayout = new QVBoxLayout(m_controlFrame);
    m_controlLayout->setContentsMargins(8, 8, 8, 8);
    m_controlLayout->setSpacing(8);
    
    // Top row: "新建推流" button
    m_addStreamButton = new QPushButton("新建推流");
    m_addStreamButton->setMinimumHeight(40);
    m_addStreamButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #FF0001;"
        "    color: white;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #E60001;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #CC0001;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #999999;"
        "}"
    );
    
    // Bottom row: "全部开始" and "全部停止" buttons
    QHBoxLayout* bottomButtonLayout = new QHBoxLayout();
    bottomButtonLayout->setSpacing(8);
    
    m_startAllButton = new QPushButton("全部开始");
    m_startAllButton->setMinimumHeight(40);
    m_startAllButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #FF0001;"
        "    color: white;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #E60001;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #CC0001;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #999999;"
        "}"
    );
    
    m_stopAllButton = new QPushButton("全部停止");
    m_stopAllButton->setMinimumHeight(40);
    m_stopAllButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #007AFF;"
        "    color: white;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #0056CC;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #004499;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #999999;"
        "}"
    );
    
    bottomButtonLayout->addWidget(m_startAllButton);
    bottomButtonLayout->addWidget(m_stopAllButton);
    
    // Add buttons to control layout
    m_controlLayout->addWidget(m_addStreamButton);
    m_controlLayout->addLayout(bottomButtonLayout);
    
    // Add sections to main layout
    m_mainLayout->addWidget(m_scrollArea, 1); // Give scroll area most space
    m_mainLayout->addWidget(m_controlFrame);
    
    // Setup stats update timer
    m_statsUpdateTimer = new QTimer(this);
    m_statsUpdateTimer->setInterval(1000); // Update every second
    connect(m_statsUpdateTimer, &QTimer::timeout, this, &OneSevenMultiRtmpDock::onStatsUpdateTimer);
    m_statsUpdateTimer->start();
}

void OneSevenMultiRtmpDock::setupConnections()
{
    // Control buttons
    connect(m_addStreamButton, &QPushButton::clicked, this, &OneSevenMultiRtmpDock::onAddStreamClicked);
    connect(m_startAllButton, &QPushButton::clicked, this, &OneSevenMultiRtmpDock::onStartAllClicked);
    connect(m_stopAllButton, &QPushButton::clicked, this, &OneSevenMultiRtmpDock::onStopAllClicked);
    
    // Stream list widget signals
    if (m_streamListWidget) {
        connect(m_streamListWidget, &OneSevenMultiRtmpListWidget::streamStartRequested,
                this, [this](const std::string& streamId) {
                    if (m_manager) {
                        m_manager->startStream(streamId);
                    }
                });
        
        connect(m_streamListWidget, &OneSevenMultiRtmpListWidget::streamStopRequested,
                this, [this](const std::string& streamId) {
                    if (m_manager) {
                        m_manager->stopStream(streamId);
                    }
                });
        
        connect(m_streamListWidget, &OneSevenMultiRtmpListWidget::streamEditRequested,
                this, [this](const std::string& streamId) {
                    if (m_manager) {
                        auto config = m_manager->getStreamConfig(streamId);
                        showConfigDialog(config);
                    }
                });
        
        connect(m_streamListWidget, &OneSevenMultiRtmpListWidget::streamDeleteRequested,
                this, [this](const std::string& streamId) {
                    auto reply = QMessageBox::question(this,
                        getMultiRtmpText("MultiRTMP.Delete.Title"),
                        getMultiRtmpText("MultiRTMP.Delete.Confirm"),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No);
                    
                    if (reply == QMessageBox::Yes && m_manager) {
                        m_manager->removeStreamConfig(streamId);
                        onStreamDeleted(streamId);
                    }
                });
    }
}

bool OneSevenMultiRtmpDock::ensureManagerInitialized()
{
    if (!m_manager) {
        obs_log(LOG_ERROR, "[MultiRTMP-Dock] Manager instance is null");
        return false;
    }
    
    if (!m_manager->isInitialized()) {
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Initializing MultiRTMP manager on first use");
        if (!m_manager->initialize()) {
            obs_log(LOG_ERROR, "[MultiRTMP-Dock] Failed to initialize MultiRTMP manager");
            return false;
        }
        obs_log(LOG_INFO, "[MultiRTMP-Dock] MultiRTMP manager initialized successfully");
    }
    
    return true;
}

void OneSevenMultiRtmpDock::setupManagerCallbacks()
{
    if (!ensureManagerInitialized()) {
        return;
    }
    
    // Set up callbacks for manager events
    m_manager->setStreamStatusCallback(
        [this](const std::string& streamId, const OneSevenMultiRtmpStreamStatus& status) {
            updateStreamStatus(streamId, status);
        }
    );
    
    m_manager->setStreamStatsCallback(
        [this](const std::string& streamId, const OneSevenMultiRtmpStreamStats& stats) {
            updateStreamStats(streamId, stats);
        }
    );
    
    m_manager->setConfigChangeCallback(
        [this](const std::string& streamId, const OneSevenMultiRtmpConfig& config) {
            Q_UNUSED(config); // Parameter not used in current implementation
            onStreamConfigChanged(streamId);
        }
    );
    
    m_manager->setConfigDeleteCallback(
        [this](const std::string& streamId) {
            onStreamDeleted(streamId);
        }
    );
}

void OneSevenMultiRtmpDock::refreshStreamList()
{
    obs_log(LOG_INFO, "[MultiRTMP-Dock] refreshStreamList() called");
    
    if (!ensureManagerInitialized()) {
        obs_log(LOG_ERROR, "[MultiRTMP-Dock] refreshStreamList() failed: manager not initialized");
        return;
    }
    
    if (!m_streamListWidget) {
        obs_log(LOG_ERROR, "[MultiRTMP-Dock] refreshStreamList() failed: m_streamListWidget is null");
        return;
    }
    
    if (m_isUpdatingUI) {
        obs_log(LOG_WARNING, "[MultiRTMP-Dock] refreshStreamList() skipped: UI update already in progress");
        return;
    }
    
    try {
        m_isUpdatingUI = true;
        
        // Clear existing streams
        m_streamListWidget->clearAllStreams();
        
        // Add all configured streams
        auto configs = m_manager->getAllStreamConfigs();
        for (size_t i = 0; i < configs.size(); ++i) {
            const auto& config = configs[i];
            
            try {
                m_streamListWidget->addStream(config);
                
                // Update with current status and stats
                auto status = m_manager->getStreamStatus(config.id);
                auto stats = m_manager->getStreamStats(config.id);
                m_streamListWidget->updateStreamStatus(config.id, status);
                
            } catch (const std::exception& e) {
                obs_log(LOG_ERROR, "[MultiRTMP-Dock] Exception while processing stream %s: %s", config.id.c_str(), e.what());
                // Continue with next stream
            } catch (...) {
                obs_log(LOG_ERROR, "[MultiRTMP-Dock] Unknown exception while processing stream %s", config.id.c_str());
                // Continue with next stream
            }
        }
        
        updateButtonStates();
        
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-Dock] Exception in refreshStreamList(): %s", e.what());
    } catch (...) {
        obs_log(LOG_ERROR, "[MultiRTMP-Dock] Unknown exception in refreshStreamList()");
    }
    
    m_isUpdatingUI = false;
}

void OneSevenMultiRtmpDock::updateStreamStatus(const std::string& streamId, const OneSevenMultiRtmpStreamStatus& status)
{
    if (m_streamListWidget && !m_isUpdatingUI) {
        m_streamListWidget->updateStreamStatus(streamId, status);
        updateButtonStates();
    }
}

void OneSevenMultiRtmpDock::updateStreamStats(const std::string& streamId, const OneSevenMultiRtmpStreamStats& stats)
{
    if (m_streamListWidget && !m_isUpdatingUI) {
        m_streamListWidget->updateStreamStats(streamId, stats);
    }
}

void OneSevenMultiRtmpDock::onAddStreamClicked()
{
    showConfigDialog();
}

void OneSevenMultiRtmpDock::onStartAllClicked()
{
    if (ensureManagerInitialized()) {
        m_startAllButton->setEnabled(false);
        
        m_manager->startAllStreams();
        
        // Button states will be updated automatically through status callbacks
        // Add a timer to re-enable the button in case callbacks don't come through
        QTimer::singleShot(2000, this, [this]() {
            if (m_startAllButton && !m_startAllButton->isEnabled()) {
                updateButtonStates();
            }
        });
    }
}

void OneSevenMultiRtmpDock::onStopAllClicked()
{
    if (ensureManagerInitialized()) {
        m_stopAllButton->setEnabled(false);
        
        m_manager->stopAllStreams();
        
        // Button states will be updated automatically through status callbacks
        // Add a timer to re-enable the button in case callbacks don't come through
        QTimer::singleShot(2000, this, [this]() {
            if (m_stopAllButton && !m_stopAllButton->isEnabled()) {
                updateButtonStates();
            }
        });
    }
}

void OneSevenMultiRtmpDock::onRefreshClicked()
{
    refreshStreamList();
}

void OneSevenMultiRtmpDock::onStreamConfigChanged(const std::string& streamId)
{
    if (m_streamListWidget) {
        // Get the updated config from the manager
        if (ensureManagerInitialized()) {
            auto config = m_manager->getStreamConfig(streamId);
            m_streamListWidget->updateStream(config);
        }
    }
}

void OneSevenMultiRtmpDock::onStreamDeleted(const std::string& streamId)
{
    if (m_streamListWidget) {
        m_streamListWidget->removeStream(streamId);
        updateButtonStates();
    }
}

void OneSevenMultiRtmpDock::onStatsUpdateTimer()
{
    if (!ensureManagerInitialized() || !m_streamListWidget || m_isUpdatingUI) {
        return;
    }
    
    // Update stats for all active streams
    auto activeIds = m_manager->getActiveStreamIds();
    for (const auto& streamId : activeIds) {
        auto stats = m_manager->getStreamStats(streamId);
        m_streamListWidget->updateStreamStats(streamId, stats);
    }
}

void OneSevenMultiRtmpDock::updateButtonStates()
{
    if (!ensureManagerInitialized()) {
        return;
    }
    
    size_t streamCount = m_manager->getStreamCount();
    auto activeIds = m_manager->getActiveStreamIds();
    size_t activeCount = activeIds.size();
    
    // Enable/disable buttons based on state
    m_startAllButton->setEnabled(streamCount > 0 && activeCount < streamCount);
    m_stopAllButton->setEnabled(activeCount > 0);
    
    // Update button text with counts
    if (streamCount > 0) {
        m_startAllButton->setText(QString("全部开始 (%1)")
            .arg(streamCount - activeCount));
        m_stopAllButton->setText(QString("全部停止 (%1)")
            .arg(activeCount));
    } else {
        m_startAllButton->setText("全部开始");
        m_stopAllButton->setText("全部停止");
    }
}

void OneSevenMultiRtmpDock::showConfigDialog(const OneSevenMultiRtmpConfig& config)
{
    obs_log(LOG_INFO, "[MultiRTMP-Dock] showConfigDialog() called");
    
    try {
        // Create dialog with configuration
        bool isEdit = !config.id.empty();
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Dialog mode: %s", isEdit ? "edit" : "new");
        
        std::shared_ptr<OneSevenMultiRtmpConfig> configPtr = std::make_shared<OneSevenMultiRtmpConfig>(config);
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Created config pointer");
        
        m_configDialog = new OneSevenMultiRtmpConfigDialog(this, configPtr);
        if (!m_configDialog) {
            obs_log(LOG_ERROR, "[MultiRTMP-Dock] Failed to create config dialog");
            return;
        }
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Created config dialog successfully");
        
        m_configDialog->setEditMode(isEdit);
        
        if (isEdit) {
            m_configDialog->setWindowTitle(getMultiRtmpText("MultiRTMP.EditStream.Title"));
        } else {
            m_configDialog->setWindowTitle(getMultiRtmpText("MultiRTMP.AddStream.Title"));
        }
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Dialog setup completed");
        
        // Show dialog and handle result
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Showing dialog");
        if (m_configDialog->exec() == QDialog::Accepted) {
            obs_log(LOG_INFO, "[MultiRTMP-Dock] Dialog accepted, calling SaveConfig()");
            
            try {
                auto newConfig = m_configDialog->SaveConfig();
                
                // Add detailed logging for configuration data
                obs_log(LOG_INFO, "[MultiRTMP-Dock] Configuration dialog accepted");
                obs_log(LOG_INFO, "[MultiRTMP-Dock] Stream name: '%s'", newConfig.streamName.c_str());
                obs_log(LOG_INFO, "[MultiRTMP-Dock] Config ID from SaveConfig: '%s'", newConfig.id.c_str());
                
                if (ensureManagerInitialized()) {
                    bool success = false;
                    
                    if (isEdit) {
                        obs_log(LOG_INFO, "[MultiRTMP-Dock] Updating existing stream config: %s", config.id.c_str());
                        // For edit mode, preserve the original ID
                        newConfig.id = config.id;
                        success = m_manager->updateStreamConfig(config.id, newConfig);
                    } else {
                        // Generate new ID for new stream only if not already set
                        if (newConfig.id.empty()) {
                            newConfig.id = m_manager->generateStreamId();
                            obs_log(LOG_INFO, "[MultiRTMP-Dock] Generated new ID for stream: %s", newConfig.id.c_str());
                        }
                        obs_log(LOG_INFO, "[MultiRTMP-Dock] Adding new stream config with ID: %s", newConfig.id.c_str());
                        
                        success = m_manager->addStreamConfig(newConfig);
                    }
                    
                    if (success) {
                        obs_log(LOG_INFO, "[MultiRTMP-Dock] Stream configuration %s successfully", isEdit ? "updated" : "added");
                        
                        try {
                            if (isEdit) {
                                obs_log(LOG_INFO, "[MultiRTMP-Dock] Calling onStreamConfigChanged for: %s", config.id.c_str());
                                onStreamConfigChanged(config.id);
                            } else {
                                obs_log(LOG_INFO, "[MultiRTMP-Dock] Calling refreshStreamList for new stream");
                                refreshStreamList();
                            }
                            obs_log(LOG_INFO, "[MultiRTMP-Dock] UI update completed successfully");
                        } catch (const std::exception& e) {
                            obs_log(LOG_ERROR, "[MultiRTMP-Dock] Exception during UI update: %s", e.what());
                        } catch (...) {
                            obs_log(LOG_ERROR, "[MultiRTMP-Dock] Unknown exception during UI update");
                        }
                    } else {
                        obs_log(LOG_ERROR, "[MultiRTMP-Dock] Failed to %s stream configuration", isEdit ? "update" : "add");
                        QMessageBox::warning(this,
                            getMultiRtmpText("MultiRTMP.Error.Title"),
                            isEdit ? 
                                getMultiRtmpText("MultiRTMP.Error.UpdateFailed") :
                                getMultiRtmpText("MultiRTMP.Error.AddFailed"));
                    }
                } else {
                    obs_log(LOG_ERROR, "[MultiRTMP-Dock] Manager initialization failed");
                }
            } catch (const std::exception& e) {
                obs_log(LOG_ERROR, "[MultiRTMP-Dock] Exception in SaveConfig(): %s", e.what());
                QMessageBox::critical(this, "Error", QString("Failed to save configuration: %1").arg(e.what()));
            } catch (...) {
                obs_log(LOG_ERROR, "[MultiRTMP-Dock] Unknown exception in SaveConfig()");
                QMessageBox::critical(this, "Error", "Failed to save configuration: Unknown error");
            }
        } else {
            obs_log(LOG_INFO, "[MultiRTMP-Dock] Configuration dialog cancelled");
        }
        
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-Dock] Exception in showConfigDialog(): %s", e.what());
        QMessageBox::critical(this, "Error", QString("Failed to show configuration dialog: %1").arg(e.what()));
    } catch (...) {
        obs_log(LOG_ERROR, "[MultiRTMP-Dock] Unknown exception in showConfigDialog()");
        QMessageBox::critical(this, "Error", "Failed to show configuration dialog: Unknown error");
    }
    
    // Clean up dialog
    if (m_configDialog) {
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Cleaning up config dialog");
        delete m_configDialog;
        m_configDialog = nullptr;
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Config dialog cleaned up successfully");
    }
    
    obs_log(LOG_INFO, "[MultiRTMP-Dock] showConfigDialog() completed");
}

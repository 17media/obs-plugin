#include "OneSevenLiveMultiRtmpDock.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QStyle>

#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveCoreManager.hpp"
#include "OneSevenLiveMultiRtmpConfigDialog.hpp"
#include "OneSevenLiveMultiRtmpListWidget.hpp"
#include "streaming/OneSevenLiveStreamManager.hpp"
#include "youtube/ui/OneSevenLiveYouTubeBroadcastDialog.hpp"

OneSevenLiveMultiRtmpDock::OneSevenLiveMultiRtmpDock(QWidget* parent)
    : QDockWidget(obs_module_text("MultiRTMP.Dock.Title"), parent),
      m_manager(OneSevenLiveMultiRtmpManager::getInstance()),
      m_isFirstShow(true),
      m_isUpdatingUI(false) {
    obs_log(LOG_INFO,
            "[MultiRTMP-Dock] Manager instance obtained, initialization will be done on first use");

    setupUI();
    setupConnections();
    setupManagerCallbacks();
}

OneSevenLiveMultiRtmpDock::~OneSevenLiveMultiRtmpDock() {
    if (m_statsUpdateTimer) {
        if (m_statsUpdateTimer->isActive()) {
            m_statsUpdateTimer->stop();
        }
    }

    if (m_manager && OneSevenLiveMultiRtmpManager::peekInstance()) {
        m_manager->setStreamStatusCallback(nullptr);
        m_manager->setStreamStatsCallback(nullptr);
        m_manager->setConfigChangeCallback(nullptr);
        m_manager->setConfigDeleteCallback(nullptr);
    }

    if (m_configDialog) {
        m_configDialog->deleteLater();
    }
}

void OneSevenLiveMultiRtmpDock::setupUI() {
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

    m_streamListWidget = new OneSevenLiveMultiRtmpListWidget();
    m_streamListWidget->setManager(m_manager);
    m_scrollArea->setWidget(m_streamListWidget);

    // Control section (bottom part)
    m_controlFrame = new QFrame();
    m_controlFrame->setFrameStyle(QFrame::NoFrame);
    m_controlLayout = new QVBoxLayout(m_controlFrame);
    m_controlLayout->setContentsMargins(8, 8, 8, 8);
    m_controlLayout->setSpacing(8);

    // Top row: Add stream button
    m_addStreamButton = new QPushButton(obs_module_text("MultiRTMP.AddStream"));
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
        "}");

    // Bottom row: Start All and Stop All buttons
    QHBoxLayout* bottomButtonLayout = new QHBoxLayout();
    bottomButtonLayout->setSpacing(8);

    m_startAllButton = new QPushButton(obs_module_text("MultiRTMP.Dock.StartAll"));
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
        "}");

    m_stopAllButton = new QPushButton(obs_module_text("MultiRTMP.Dock.StopAll"));
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
        "}");

    bottomButtonLayout->addWidget(m_startAllButton);
    bottomButtonLayout->addWidget(m_stopAllButton);

    // Add buttons to control layout
    m_controlLayout->addWidget(m_addStreamButton);
    m_controlLayout->addLayout(bottomButtonLayout);

    // Add sections to main layout
    m_mainLayout->addWidget(m_scrollArea, 1);  // Give scroll area most space
    m_mainLayout->addWidget(m_controlFrame);

    // Setup stats update timer
    m_statsUpdateTimer = new QTimer(this);
    m_statsUpdateTimer->setInterval(1000);  // Update every second
    connect(m_statsUpdateTimer, &QTimer::timeout, this,
            &OneSevenLiveMultiRtmpDock::onStatsUpdateTimer);
    m_statsUpdateTimer->start();
}

void OneSevenLiveMultiRtmpDock::setupConnections() {
    // Control buttons
    connect(m_addStreamButton, &QPushButton::clicked, this,
            &OneSevenLiveMultiRtmpDock::onAddStreamClicked);
    connect(m_startAllButton, &QPushButton::clicked, this,
            &OneSevenLiveMultiRtmpDock::onStartAllClicked);
    connect(m_stopAllButton, &QPushButton::clicked, this,
            &OneSevenLiveMultiRtmpDock::onStopAllClicked);

    // Stream list widget signals
    if (m_streamListWidget) {
        connect(m_streamListWidget, &OneSevenLiveMultiRtmpListWidget::streamStartRequested, this,
                [this](const std::string& streamId) {
                    if (m_manager) {
                        auto config = m_manager->getStreamConfig(streamId);
                        if (config.streamName == "YouTube") {
                            OneSevenLiveYouTubeBroadcastDialog dialog(this);
                            if (dialog.exec() == QDialog::Accepted) {
                                auto* cfgMgr =
                                    OneSevenLiveCoreManager::getInstance().getConfigManager();
                                if (cfgMgr) {
                                    cfgMgr->setYouTubeBroadcastInfo(dialog.getBroadcastId(),
                                                                    dialog.getLiveChatId());
                                }

                                std::string url = dialog.getIngestionUrl().toStdString();
                                std::string key = dialog.getStreamKey().toStdString();

                                if (!url.empty() && !key.empty()) {
                                    config.serviceSettings["server"] = url;
                                    config.serviceSettings["key"] = key;
                                    m_manager->updateStreamConfig(streamId, config);
                                    obs_log(LOG_INFO,
                                            "GET server & key --------------------------------");
                                    obs_log(LOG_INFO, "YouTube broadcastId: %s",
                                            dialog.getBroadcastId().toStdString().c_str());
                                    obs_log(LOG_INFO, "YouTube liveChatId: %s",
                                            dialog.getLiveChatId().toStdString().c_str());
                                    obs_log(LOG_INFO, "YouTube server: %s", url.c_str());
                                    obs_log(LOG_INFO, "YouTube key: %s", key.c_str());
                                }

                                m_manager->startStream(streamId);
                                updateButtonStates();
                            }
                        } else {
                            m_manager->startStream(streamId);
                            updateButtonStates();
                        }
                    }
                });

        connect(m_streamListWidget, &OneSevenLiveMultiRtmpListWidget::streamStopRequested, this,
                [this](const std::string& streamId) {
                    if (m_manager) {
                        m_manager->stopStream(streamId);
                        // Update button states immediately since we now use actual stream item
                        // states
                        updateButtonStates();
                    }
                });

        connect(m_streamListWidget, &OneSevenLiveMultiRtmpListWidget::streamEditRequested, this,
                [this](const std::string& streamId) {
                    if (m_manager) {
                        auto config = m_manager->getStreamConfig(streamId);
                        showConfigDialog(config);
                    }
                });

        connect(m_streamListWidget, &OneSevenLiveMultiRtmpListWidget::streamDeleteRequested, this,
                [this](const std::string& streamId) {
                    auto reply = QMessageBox::question(
                        this, QString::fromUtf8(obs_module_text("MultiRTMP.Delete.Title")),
                        QString::fromUtf8(obs_module_text("MultiRTMP.Delete.Confirm")),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

                    if (reply == QMessageBox::Yes && m_manager) {
                        m_manager->removeStreamConfig(streamId);
                        onStreamDeleted(streamId);
                    }
                });
    }
}

bool OneSevenLiveMultiRtmpDock::ensureManagerInitialized() {
    if (OneSevenLiveCoreManager::getInstance().isShuttingDown()) {
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Skipping ensureManagerInitialized during shutdown");
        return false;
    }
    auto* inst = OneSevenLiveMultiRtmpManager::peekInstance();
    if (!inst) {
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Manager instance not alive (peekInstance=null)");
        return false;
    }
    if (m_manager != inst) {
        m_manager = inst;
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

void OneSevenLiveMultiRtmpDock::setupManagerCallbacks() {
    if (!ensureManagerInitialized()) {
        return;
    }

    // Set up callbacks for manager events
    QPointer<OneSevenLiveMultiRtmpDock> self(this);
    m_manager->setStreamStatusCallback(
        [self](const std::string& streamId, const OneSevenLiveMultiRtmpStreamStatus& status) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, streamId, status]() {
                    if (!self)
                        return;
                    self->updateStreamStatus(streamId, status);
                },
                Qt::QueuedConnection);
        });

    m_manager->setStreamStatsCallback(
        [self](const std::string& streamId, const OneSevenLiveMultiRtmpStreamStats& stats) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, streamId, stats]() {
                    if (!self)
                        return;
                    self->updateStreamStats(streamId, stats);
                },
                Qt::QueuedConnection);
        });

    m_manager->setConfigChangeCallback(
        [self](const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config) {
            Q_UNUSED(config);
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, streamId]() {
                    if (!self)
                        return;
                    self->onStreamConfigChanged(streamId);
                },
                Qt::QueuedConnection);
        });

    m_manager->setConfigDeleteCallback([self](const std::string& streamId) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, streamId]() {
                if (!self)
                    return;
                self->onStreamDeleted(streamId);
            },
            Qt::QueuedConnection);
    });

    m_manager->startStatsMonitoring();
}

void OneSevenLiveMultiRtmpDock::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);

    // Only load streams on first show to avoid unnecessary reloads
    if (m_isFirstShow) {
        obs_log(LOG_INFO,
                "[MultiRTMP-Dock] First show event - loading stored stream configurations");
        m_isFirstShow = false;

        // Ensure manager is initialized before loading streams
        if (ensureManagerInitialized()) {
            refreshStreamList();
        } else {
            obs_log(LOG_WARNING, "[MultiRTMP-Dock] Failed to initialize manager during first show");
        }
    }
}

void OneSevenLiveMultiRtmpDock::refreshStreamList() {
    obs_log(LOG_INFO, "[MultiRTMP-Dock] refreshStreamList() called");

    if (!ensureManagerInitialized()) {
        obs_log(LOG_ERROR, "[MultiRTMP-Dock] refreshStreamList() failed: manager not initialized");
        return;
    }

    if (!m_streamListWidget) {
        obs_log(LOG_ERROR,
                "[MultiRTMP-Dock] refreshStreamList() failed: m_streamListWidget is null");
        return;
    }

    if (m_isUpdatingUI) {
        obs_log(LOG_WARNING,
                "[MultiRTMP-Dock] refreshStreamList() skipped: UI update already in progress");
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
                obs_log(LOG_ERROR, "[MultiRTMP-Dock] Exception while processing stream %s: %s",
                        config.id.c_str(), e.what());
                // Continue with next stream
            } catch (...) {
                obs_log(LOG_ERROR, "[MultiRTMP-Dock] Unknown exception while processing stream %s",
                        config.id.c_str());
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

void OneSevenLiveMultiRtmpDock::updateStreamStatus(
    const std::string& streamId, const OneSevenLiveMultiRtmpStreamStatus& status) {
    // Update stream status in the list widget if not in bulk update mode
    if (m_streamListWidget && !m_isUpdatingUI) {
        m_streamListWidget->updateStreamStatus(streamId, status);
    } else if (m_isUpdatingUI) {
        obs_log(LOG_DEBUG,
                "[MultiRTMP-Dock] updateStreamStatus skipped for stream %s due to bulk UI update "
                "in progress",
                streamId.c_str());
    }

    // Always update button states for individual stream status changes
    // This ensures "Start All" and "Stop All" buttons reflect current state immediately
    updateButtonStates();
}

void OneSevenLiveMultiRtmpDock::updateStreamStats(const std::string& streamId,
                                                  const OneSevenLiveMultiRtmpStreamStats& stats) {
    if (m_streamListWidget && !m_isUpdatingUI) {
        m_streamListWidget->updateStreamStats(streamId, stats);
    }
}

void OneSevenLiveMultiRtmpDock::onAddStreamClicked() {
    showConfigDialog();
}

void OneSevenLiveMultiRtmpDock::onStartAllClicked() {
    // Pre-check 17LIVE streaming status before starting MultiRTMP
    {
        auto& core = OneSevenLiveCoreManager::getInstance();
        OneSevenLiveStreamManager* streamMgr = core.getStreamManager();
        if (!streamMgr) {
            obs_log(LOG_ERROR, "[MultiRTMP-Manager] 17LIVE StreamManager not available");
            QMessageBox::warning(nullptr, obs_module_text("Live.Common.Notice"),
                                 obs_module_text("MultiRTMP.Precheck.StreamManagerUnavailable"));
            return;
        }

        // 1) If 17live live NOT started
        if (!streamMgr->hasActiveLiveStream() ||
            streamMgr->getCurrentStreamingStatus() == OneSevenLiveStreamingStatus::NotStarted) {
            obs_log(LOG_WARNING, "[MultiRTMP-Manager] 17LIVE live not started; blocking MultiRTMP");
            QMessageBox::information(nullptr, obs_module_text("Live.Common.Notice"),
                                     obs_module_text("MultiRTMP.Precheck.LiveNotStarted"));
            return;
        }

        // Fetch current info to inspect group call flag
        const OneSevenLiveStreamInfo& liveInfo = streamMgr->getCurrentLiveStreamInfo();
        const OneSevenLiveRtmpRequest& liveReq = streamMgr->getCurrentStreamRequest();
        const bool isGroupCall = liveReq.enableOBSGroupCall || liveInfo.request.enableOBSGroupCall;

        // 2) If live is groupcall (party live), block
        if (isGroupCall) {
            obs_log(LOG_WARNING,
                    "[MultiRTMP-Manager] 17LIVE live is GroupCall; MultiRTMP unsupported");
            QMessageBox::warning(nullptr, obs_module_text("Live.Common.Notice"),
                                 obs_module_text("MultiRTMP.Precheck.GroupCallNotSupported"));
            return;
        }

        // 3) If live started but not streaming, prompt user to start streaming first
        if (streamMgr->getCurrentStreamingStatus() == OneSevenLiveStreamingStatus::Live &&
            !streamMgr->isOBSStreaming()) {
            obs_log(LOG_INFO,
                    "[MultiRTMP-Manager] 17LIVE live started but OBS not streaming; prompt user");
            QMessageBox::information(nullptr, obs_module_text("Live.Common.Notice"),
                                     obs_module_text("MultiRTMP.Precheck.StartObsStreamingFirst"));
            // Do not return here per requirement 3: prompt then continue starting MultiRTMP
        }
        // 4) If already streaming, proceed directly (no-op)
    }

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

void OneSevenLiveMultiRtmpDock::onStopAllClicked() {
    if (ensureManagerInitialized()) {
        {
            auto& core = OneSevenLiveCoreManager::getInstance();
            OneSevenLiveStreamManager* streamMgr = core.getStreamManager();
            if (streamMgr &&
                streamMgr->getCurrentStreamingStatus() != OneSevenLiveStreamingStatus::NotStarted) {
                QMessageBox::information(this, obs_module_text("Live.Common.Notice"),
                                         obs_module_text("MultiRTMP.Stop.InfoTip17LIVE"));
            }
        }
        m_stopAllButton->setEnabled(false);

        QMessageBox::StandardButton ret = QMessageBox::question(
            this, QString::fromUtf8(obs_module_text("MultiRTMP.Dock.StopAll")),
            QString::fromUtf8(obs_module_text("MultiRTMP.StopAllConfirm")),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) {
            m_stopAllButton->setEnabled(true);
            return;
        }

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

void OneSevenLiveMultiRtmpDock::onRefreshClicked() {
    refreshStreamList();
}

void OneSevenLiveMultiRtmpDock::onStreamConfigChanged(const std::string& streamId) {
    if (m_streamListWidget) {
        // Get the updated config from the manager
        if (ensureManagerInitialized()) {
            auto config = m_manager->getStreamConfig(streamId);
            m_streamListWidget->updateStream(config);
        }
    }
}

void OneSevenLiveMultiRtmpDock::onStreamDeleted(const std::string& streamId) {
    if (m_streamListWidget) {
        m_streamListWidget->removeStream(streamId);
        updateButtonStates();
    }
}

void OneSevenLiveMultiRtmpDock::onStatsUpdateTimer() {
    if (OneSevenLiveCoreManager::getInstance().isShuttingDown()) {
        return;
    }
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

void OneSevenLiveMultiRtmpDock::updateButtonStates() {
    if (OneSevenLiveCoreManager::getInstance().isShuttingDown()) {
        return;
    }
    if (!ensureManagerInitialized() || !m_streamListWidget) {
        return;
    }

    // Get actual status statistics directly from stream items
    auto stats = m_streamListWidget->getStreamStatusStats();

    // Calculate counts for button logic
    size_t totalCount = stats.totalCount;
    size_t activeCount = stats.activeCount;
    size_t inactiveCount =
        totalCount - activeCount - stats.connectingCount;  // Stopped + Error streams can be started

    obs_log(LOG_INFO,
            "[MultiRTMP-Dock] updateButtonStates(): totalCount=%zu, activeCount=%zu, "
            "connectingCount=%zu, stoppedCount=%zu, errorCount=%zu",
            totalCount, activeCount, stats.connectingCount, stats.stoppedCount, stats.errorCount);

    // Enable/disable buttons based on actual stream item states
    m_startAllButton->setEnabled(totalCount > 0 && inactiveCount > 0);
    m_stopAllButton->setEnabled(activeCount > 0 || stats.connectingCount > 0);

    // Update button text with actual counts
    if (totalCount > 0) {
        m_startAllButton->setText(
            QString(QString::fromUtf8(obs_module_text("MultiRTMP.Dock.StartAll.WithCount")))
                .arg(inactiveCount));
        m_stopAllButton->setText(
            QString(QString::fromUtf8(obs_module_text("MultiRTMP.Dock.StopAll.WithCount")))
                .arg(activeCount + stats.connectingCount));
    } else {
        m_startAllButton->setText(QString::fromUtf8(obs_module_text("MultiRTMP.Dock.StartAll")));
        m_stopAllButton->setText(QString::fromUtf8(obs_module_text("MultiRTMP.Dock.StopAll")));
    }

    bool hasYouTube = false;
    bool hasTwitch = false;
    if (!OneSevenLiveCoreManager::getInstance().isShuttingDown() && ensureManagerInitialized()) {
        auto configs = m_manager->getAllStreamConfigs();
        for (const auto& cfg : configs) {
            std::string n = cfg.streamName;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (n == std::string("youtube"))
                hasYouTube = true;
            else if (n == std::string("twitch"))
                hasTwitch = true;
        }
    }
    if (m_addStreamButton)
        m_addStreamButton->setVisible(!(hasYouTube && hasTwitch));
}

void OneSevenLiveMultiRtmpDock::showConfigDialog(const OneSevenLiveMultiRtmpConfig& config) {
    obs_log(LOG_INFO, "[MultiRTMP-Dock] showConfigDialog() called");

    try {
        // Create dialog with configuration
        bool isEdit = !config.id.empty();
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Dialog mode: %s", isEdit ? "edit" : "new");

        std::shared_ptr<OneSevenLiveMultiRtmpConfig> configPtr =
            std::make_shared<OneSevenLiveMultiRtmpConfig>(config);
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Created config pointer");

        m_configDialog = new OneSevenLiveMultiRtmpConfigDialog(this, configPtr, isEdit);
        if (!m_configDialog) {
            obs_log(LOG_ERROR, "[MultiRTMP-Dock] Failed to create config dialog");
            return;
        }
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Created config dialog successfully");

        if (isEdit) {
            m_configDialog->setWindowTitle(
                QString::fromUtf8(obs_module_text("MultiRTMP.EditStream.Title")));
        } else {
            m_configDialog->setWindowTitle(
                QString::fromUtf8(obs_module_text("MultiRTMP.AddStream.Title")));
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
                obs_log(LOG_INFO, "[MultiRTMP-Dock] Stream name: '%s'",
                        newConfig.streamName.c_str());
                obs_log(LOG_INFO, "[MultiRTMP-Dock] Config ID from SaveConfig: '%s'",
                        newConfig.id.c_str());

                if (ensureManagerInitialized()) {
                    bool success = false;

                    if (isEdit) {
                        obs_log(LOG_INFO, "[MultiRTMP-Dock] Updating existing stream config: %s",
                                config.id.c_str());
                        // For edit mode, preserve the original ID
                        newConfig.id = config.id;
                        success = m_manager->updateStreamConfig(config.id, newConfig);
                    } else {
                        // Generate new ID for new stream only if not already set
                        if (newConfig.id.empty()) {
                            newConfig.id = m_manager->generateStreamId();
                            obs_log(LOG_INFO, "[MultiRTMP-Dock] Generated new ID for stream: %s",
                                    newConfig.id.c_str());
                        }
                        obs_log(LOG_INFO, "[MultiRTMP-Dock] Adding new stream config with ID: %s",
                                newConfig.id.c_str());

                        success = m_manager->addStreamConfig(newConfig);
                    }

                    if (success) {
                        obs_log(LOG_INFO, "[MultiRTMP-Dock] Stream configuration %s successfully",
                                isEdit ? "updated" : "added");

                        try {
                            if (isEdit) {
                                obs_log(LOG_INFO,
                                        "[MultiRTMP-Dock] Calling onStreamConfigChanged for: %s",
                                        config.id.c_str());
                                onStreamConfigChanged(config.id);
                            } else {
                                obs_log(
                                    LOG_INFO,
                                    "[MultiRTMP-Dock] Calling refreshStreamList for new stream");
                                refreshStreamList();
                            }
                            obs_log(LOG_INFO, "[MultiRTMP-Dock] UI update completed successfully");
                        } catch (const std::exception& e) {
                            obs_log(LOG_ERROR, "[MultiRTMP-Dock] Exception during UI update: %s",
                                    e.what());
                        } catch (...) {
                            obs_log(LOG_ERROR,
                                    "[MultiRTMP-Dock] Unknown exception during UI update");
                        }
                    } else {
                        obs_log(LOG_ERROR, "[MultiRTMP-Dock] Failed to %s stream configuration",
                                isEdit ? "update" : "add");
                        QMessageBox::warning(
                            this, QString::fromUtf8(obs_module_text("MultiRTMP.Error.Title")),
                            isEdit
                                ? QString::fromUtf8(obs_module_text("MultiRTMP.Error.UpdateFailed"))
                                : QString::fromUtf8(obs_module_text("MultiRTMP.Error.AddFailed")));
                    }
                } else {
                    obs_log(LOG_ERROR, "[MultiRTMP-Dock] Manager initialization failed");
                }
            } catch (const std::exception& e) {
                obs_log(LOG_ERROR, "[MultiRTMP-Dock] Exception in SaveConfig(): %s", e.what());
                QMessageBox::critical(this, "Error",
                                      QString("Failed to save configuration: %1").arg(e.what()));
            } catch (...) {
                obs_log(LOG_ERROR, "[MultiRTMP-Dock] Unknown exception in SaveConfig()");
                QMessageBox::critical(this, "Error", "Failed to save configuration: Unknown error");
            }
        } else {
            obs_log(LOG_INFO, "[MultiRTMP-Dock] Configuration dialog cancelled");
        }

    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-Dock] Exception in showConfigDialog(): %s", e.what());
        QMessageBox::critical(this, "Error",
                              QString("Failed to show configuration dialog: %1").arg(e.what()));
    } catch (...) {
        obs_log(LOG_ERROR, "[MultiRTMP-Dock] Unknown exception in showConfigDialog()");
        QMessageBox::critical(this, "Error", "Failed to show configuration dialog: Unknown error");
    }

    // Clean up dialog
    if (m_configDialog) {
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Cleaning up config dialog");
        m_configDialog->deleteLater();
        m_configDialog = nullptr;
        obs_log(LOG_INFO, "[MultiRTMP-Dock] Config dialog cleaned up successfully");
    }

    obs_log(LOG_INFO, "[MultiRTMP-Dock] showConfigDialog() completed");
}

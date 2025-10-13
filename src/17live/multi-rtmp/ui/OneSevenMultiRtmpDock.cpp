#include "OneSevenMultiRtmpDock.hpp"
#include "OneSevenMultiRtmpListWidget.hpp"
#include "OneSevenMultiRtmpConfigDialog.hpp"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>

OneSevenMultiRtmpDock::OneSevenMultiRtmpDock(QWidget* parent)
    : QDockWidget(parent)
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_headerFrame(nullptr)
    , m_headerLayout(nullptr)
    , m_titleLabel(nullptr)
    , m_streamCountLabel(nullptr)
    , m_addStreamButton(nullptr)
    , m_refreshButton(nullptr)
    , m_controlFrame(nullptr)
    , m_controlLayout(nullptr)
    , m_startAllButton(nullptr)
    , m_stopAllButton(nullptr)
    , m_scrollArea(nullptr)
    , m_streamListWidget(nullptr)
    , m_statusFrame(nullptr)
    , m_statusLayout(nullptr)
    , m_statusLabel(nullptr)
    , m_configDialog(nullptr)
    , m_statsUpdateTimer(nullptr)
    , m_manager(nullptr)
    , m_isUpdatingUI(false)
{
    setObjectName("OneSevenMultiRtmpDock");
    setWindowTitle(getMultiRtmpText("MultiRTMP.Dock.Title"));
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    
    // Get manager instance
    m_manager = OneSevenMultiRtmpManager::getInstance();
    
    setupUI();
    setupConnections();
    setupManagerCallbacks();
    
    // Initialize with current data
    refreshStreamList();
    updateButtonStates();
    updateStreamCount();
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
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    m_mainLayout->setSpacing(8);
    
    // Header section
    m_headerFrame = new QFrame();
    m_headerFrame->setFrameStyle(QFrame::StyledPanel);
    m_headerLayout = new QHBoxLayout(m_headerFrame);
    m_headerLayout->setContentsMargins(8, 6, 8, 6);
    
    m_titleLabel = new QLabel(getMultiRtmpText("MultiRTMP.Title"));
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    m_streamCountLabel = new QLabel("0");
    m_streamCountLabel->setStyleSheet("color: #666; font-size: 12px;");
    
    m_addStreamButton = new QPushButton(getMultiRtmpText("MultiRTMP.AddStream"));
    m_addStreamButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    m_addStreamButton->setToolTip(getMultiRtmpText("MultiRTMP.AddStream.Tooltip"));
    
    m_refreshButton = new QPushButton(getMultiRtmpText("MultiRTMP.Refresh"));
    m_refreshButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
    m_refreshButton->setToolTip(getMultiRtmpText("MultiRTMP.Refresh.Tooltip"));
    
    m_headerLayout->addWidget(m_titleLabel);
    m_headerLayout->addWidget(m_streamCountLabel);
    m_headerLayout->addStretch();
    m_headerLayout->addWidget(m_refreshButton);
    m_headerLayout->addWidget(m_addStreamButton);
    
    // Control section
    m_controlFrame = new QFrame();
    m_controlFrame->setFrameStyle(QFrame::StyledPanel);
    m_controlLayout = new QHBoxLayout(m_controlFrame);
    m_controlLayout->setContentsMargins(8, 6, 8, 6);
    
    m_startAllButton = new QPushButton(getMultiRtmpText("MultiRTMP.StartAll"));
    m_startAllButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
    m_startAllButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    
    m_stopAllButton = new QPushButton(getMultiRtmpText("MultiRTMP.StopAll"));
    m_stopAllButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaStop));
    m_stopAllButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    
    m_controlLayout->addWidget(m_startAllButton);
    m_controlLayout->addWidget(m_stopAllButton);
    m_controlLayout->addStretch();
    
    // Stream list section
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameStyle(QFrame::StyledPanel);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    m_streamListWidget = new OneSevenMultiRtmpListWidget();
    m_scrollArea->setWidget(m_streamListWidget);
    
    // Status section
    m_statusFrame = new QFrame();
    m_statusFrame->setFrameStyle(QFrame::StyledPanel);
    m_statusLayout = new QHBoxLayout(m_statusFrame);
    m_statusLayout->setContentsMargins(8, 4, 8, 4);
    
    m_statusLabel = new QLabel(getMultiRtmpText("MultiRTMP.Status.Ready"));
    m_statusLabel->setStyleSheet("color: #666; font-size: 11px;");
    
    m_statusLayout->addWidget(m_statusLabel);
    m_statusLayout->addStretch();
    
    // Add all sections to main layout
    m_mainLayout->addWidget(m_headerFrame);
    m_mainLayout->addWidget(m_controlFrame);
    m_mainLayout->addWidget(m_scrollArea, 1); // Give scroll area most space
    m_mainLayout->addWidget(m_statusFrame);
    
    // Setup stats update timer
    m_statsUpdateTimer = new QTimer(this);
    m_statsUpdateTimer->setInterval(1000); // Update every second
    connect(m_statsUpdateTimer, &QTimer::timeout, this, &OneSevenMultiRtmpDock::onStatsUpdateTimer);
    m_statsUpdateTimer->start();
}

void OneSevenMultiRtmpDock::setupConnections()
{
    // Header buttons
    connect(m_addStreamButton, &QPushButton::clicked, this, &OneSevenMultiRtmpDock::onAddStreamClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &OneSevenMultiRtmpDock::onRefreshClicked);
    
    // Control buttons
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
        
        connect(m_streamListWidget, &OneSevenMultiRtmpListWidget::streamDuplicateRequested,
                this, [this](const std::string& streamId) {
                    if (m_manager) {
                        auto config = m_manager->getStreamConfig(streamId);
                        config.id = m_manager->generateStreamId();
                        config.streamName += " (Copy)";
                        showConfigDialog(config);
                    }
                });
    }
}

void OneSevenMultiRtmpDock::setupManagerCallbacks()
{
    if (!m_manager) {
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
    if (!m_manager || !m_streamListWidget || m_isUpdatingUI) {
        return;
    }
    
    m_isUpdatingUI = true;
    
    // Clear existing streams
    m_streamListWidget->clearAllStreams();
    
    // Add all configured streams
    auto configs = m_manager->getAllStreamConfigs();
    for (const auto& config : configs) {
        m_streamListWidget->addStream(config);
        
        // Update with current status and stats
        auto status = m_manager->getStreamStatus(config.id);
        auto stats = m_manager->getStreamStats(config.id);
        
        m_streamListWidget->updateStreamStatus(config.id, status);
        m_streamListWidget->updateStreamStats(config.id, stats);
    }
    
    updateStreamCount();
    updateButtonStates();
    
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
    if (m_manager) {
        m_startAllButton->setEnabled(false);
        m_statusLabel->setText(getMultiRtmpText("MultiRTMP.Status.StartingAll"));
        
        bool success = m_manager->startAllStreams();
        
        m_statusLabel->setText(success ? 
            getMultiRtmpText("MultiRTMP.Status.AllStarted") :
            getMultiRtmpText("MultiRTMP.Status.StartFailed"));
        
        updateButtonStates();
    }
}

void OneSevenMultiRtmpDock::onStopAllClicked()
{
    if (m_manager) {
        m_stopAllButton->setEnabled(false);
        m_statusLabel->setText(getMultiRtmpText("MultiRTMP.Status.StoppingAll"));
        
        bool success = m_manager->stopAllStreams();
        
        m_statusLabel->setText(success ? 
            getMultiRtmpText("MultiRTMP.Status.AllStopped") :
            getMultiRtmpText("MultiRTMP.Status.StopFailed"));
        
        updateButtonStates();
    }
}

void OneSevenMultiRtmpDock::onRefreshClicked()
{
    m_statusLabel->setText(getMultiRtmpText("MultiRTMP.Status.Refreshing"));
    refreshStreamList();
    m_statusLabel->setText(getMultiRtmpText("MultiRTMP.Status.Ready"));
}

void OneSevenMultiRtmpDock::onStreamConfigChanged(const std::string& streamId)
{
    if (m_manager && m_streamListWidget) {
        auto config = m_manager->getStreamConfig(streamId);
        m_streamListWidget->updateStream(config);
        updateStreamCount();
    }
}

void OneSevenMultiRtmpDock::onStreamDeleted(const std::string& streamId)
{
    if (m_streamListWidget) {
        m_streamListWidget->removeStream(streamId);
        updateStreamCount();
        updateButtonStates();
    }
}

void OneSevenMultiRtmpDock::onStatsUpdateTimer()
{
    if (!m_manager || !m_streamListWidget || m_isUpdatingUI) {
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
    if (!m_manager) {
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
        m_startAllButton->setText(QString("%1 (%2)")
            .arg(getMultiRtmpText("MultiRTMP.StartAll"))
            .arg(streamCount - activeCount));
        m_stopAllButton->setText(QString("%1 (%2)")
            .arg(getMultiRtmpText("MultiRTMP.StopAll"))
            .arg(activeCount));
    } else {
        m_startAllButton->setText(getMultiRtmpText("MultiRTMP.StartAll"));
        m_stopAllButton->setText(getMultiRtmpText("MultiRTMP.StopAll"));
    }
}

void OneSevenMultiRtmpDock::updateStreamCount()
{
    if (!m_manager) {
        return;
    }
    
    size_t count = m_manager->getStreamCount();
    m_streamCountLabel->setText(QString("(%1 %2)")
        .arg(count)
        .arg(count == 1 ? 
            getMultiRtmpText("MultiRTMP.Stream.Singular") :
            getMultiRtmpText("MultiRTMP.Stream.Plural")));
}

void OneSevenMultiRtmpDock::showConfigDialog(const OneSevenMultiRtmpConfig& config)
{
    if (!m_configDialog) {
        m_configDialog = new OneSevenMultiRtmpConfigDialog(this);
    }
    
    // Set configuration and mode
    bool isEdit = !config.id.empty();
    m_configDialog->setEditMode(isEdit);
    
    if (isEdit) {
        m_configDialog->setConfig(config);
        m_configDialog->setWindowTitle(getMultiRtmpText("MultiRTMP.EditStream.Title"));
    } else {
        m_configDialog->resetToDefaults();
        m_configDialog->setWindowTitle(getMultiRtmpText("MultiRTMP.AddStream.Title"));
    }
    
    // Show dialog and handle result
    if (m_configDialog->exec() == QDialog::Accepted) {
        auto newConfig = m_configDialog->getConfig();
        
        if (m_manager) {
            bool success = false;
            
            if (isEdit) {
                success = m_manager->updateStreamConfig(config.id, newConfig);
            } else {
                // Generate new ID for new stream
                newConfig.id = m_manager->generateStreamId();
                success = m_manager->addStreamConfig(newConfig);
            }
            
            if (success) {
                if (isEdit) {
                    onStreamConfigChanged(config.id);
                } else {
                    refreshStreamList();
                }
                
                m_statusLabel->setText(isEdit ? 
                    getMultiRtmpText("MultiRTMP.Status.StreamUpdated") :
                    getMultiRtmpText("MultiRTMP.Status.StreamAdded"));
            } else {
                QMessageBox::warning(this,
                    getMultiRtmpText("MultiRTMP.Error.Title"),
                    isEdit ? 
                        getMultiRtmpText("MultiRTMP.Error.UpdateFailed") :
                        getMultiRtmpText("MultiRTMP.Error.AddFailed"));
            }
        }
    }
}

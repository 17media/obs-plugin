#include "OneSevenMultiRtmpStreamItem.hpp"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>

// Static style class constants
const QString OneSevenMultiRtmpStreamItem::STATUS_IDLE_CLASS = "status-idle";
const QString OneSevenMultiRtmpStreamItem::STATUS_CONNECTING_CLASS = "status-connecting";
const QString OneSevenMultiRtmpStreamItem::STATUS_ACTIVE_CLASS = "status-active";
const QString OneSevenMultiRtmpStreamItem::STATUS_ERROR_CLASS = "status-error";
const QString OneSevenMultiRtmpStreamItem::STATUS_STOPPING_CLASS = "status-stopping";

OneSevenMultiRtmpStreamItem::OneSevenMultiRtmpStreamItem(const OneSevenMultiRtmpConfig& config, QWidget* parent)
    : QFrame(parent)
    , m_config(config)
    , m_mainLayout(nullptr)
    , m_infoLayout(nullptr)
    , m_nameLabel(nullptr)
    , m_urlLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_statsLayout(nullptr)
    , m_bitrateLabel(nullptr)
    , m_durationLabel(nullptr)
    , m_framesLabel(nullptr)
    , m_connectionProgress(nullptr)
    , m_controlLayout(nullptr)
    , m_startStopButton(nullptr)
    , m_editButton(nullptr)
    , m_menuButton(nullptr)
    , m_contextMenu(nullptr)
    , m_duplicateAction(nullptr)
    , m_deleteAction(nullptr)
    , m_statsTimer(nullptr)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setLineWidth(1);
    setMidLineWidth(0);
    
    setupUI();
    setupContextMenu();
    updateUI();
    
    // Setup stats update timer
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(1000); // Update every second
    connect(m_statsTimer, &QTimer::timeout, this, &OneSevenMultiRtmpStreamItem::onStatsUpdateTimer);
    m_statsTimer->start();
}

OneSevenMultiRtmpStreamItem::~OneSevenMultiRtmpStreamItem()
{
    if (m_statsTimer) {
        m_statsTimer->stop();
    }
    
    if (m_contextMenu) {
        m_contextMenu->deleteLater();
    }
}

void OneSevenMultiRtmpStreamItem::setupUI()
{
    // Main horizontal layout
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(12, 8, 12, 8);
    m_mainLayout->setSpacing(12);
    
    // Left section - Stream info
    m_infoLayout = new QVBoxLayout();
    m_infoLayout->setSpacing(4);
    
    m_nameLabel = new QLabel();
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #333;");
    m_nameLabel->setWordWrap(false);
    m_nameLabel->setMaximumWidth(200);
    
    m_urlLabel = new QLabel();
    m_urlLabel->setStyleSheet("font-size: 11px; color: #666;");
    m_urlLabel->setWordWrap(false);
    m_urlLabel->setMaximumWidth(200);
    
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("font-size: 11px; font-weight: bold;");
    
    m_infoLayout->addWidget(m_nameLabel);
    m_infoLayout->addWidget(m_urlLabel);
    m_infoLayout->addWidget(m_statusLabel);
    m_infoLayout->addStretch();
    
    // Center section - Statistics
    m_statsLayout = new QVBoxLayout();
    m_statsLayout->setSpacing(2);
    
    m_bitrateLabel = new QLabel("0 kbps");
    m_bitrateLabel->setStyleSheet("font-size: 11px; color: #555;");
    
    m_durationLabel = new QLabel("00:00:00");
    m_durationLabel->setStyleSheet("font-size: 11px; color: #555;");
    
    m_framesLabel = new QLabel("0 fps");
    m_framesLabel->setStyleSheet("font-size: 11px; color: #555;");
    
    m_connectionProgress = new QProgressBar();
    m_connectionProgress->setMaximumHeight(6);
    m_connectionProgress->setTextVisible(false);
    m_connectionProgress->setVisible(false);
    m_connectionProgress->setStyleSheet(
        "QProgressBar { "
        "  border: 1px solid #ccc; "
        "  border-radius: 3px; "
        "  background-color: #f0f0f0; "
        "} "
        "QProgressBar::chunk { "
        "  background-color: #4CAF50; "
        "  border-radius: 2px; "
        "}"
    );
    
    m_statsLayout->addWidget(m_bitrateLabel);
    m_statsLayout->addWidget(m_durationLabel);
    m_statsLayout->addWidget(m_framesLabel);
    m_statsLayout->addWidget(m_connectionProgress);
    m_statsLayout->addStretch();
    
    // Right section - Controls
    m_controlLayout = new QVBoxLayout();
    m_controlLayout->setSpacing(4);
    
    m_startStopButton = new QPushButton();
    m_startStopButton->setMinimumSize(80, 28);
    m_startStopButton->setMaximumSize(80, 28);
    connect(m_startStopButton, &QPushButton::clicked, this, &OneSevenMultiRtmpStreamItem::onStartStopClicked);
    
    m_editButton = new QPushButton(obs_module_text("MultiRTMP.Edit"));
    m_editButton->setMinimumSize(60, 24);
    m_editButton->setMaximumSize(60, 24);
    m_editButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(m_editButton, &QPushButton::clicked, this, &OneSevenMultiRtmpStreamItem::onEditClicked);
    
    m_menuButton = new QPushButton("⋮");
    m_menuButton->setMinimumSize(24, 24);
    m_menuButton->setMaximumSize(24, 24);
    m_menuButton->setToolTip(obs_module_text("MultiRTMP.Menu.Tooltip"));
    connect(m_menuButton, &QPushButton::clicked, this, &OneSevenMultiRtmpStreamItem::onMenuRequested);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(4);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_menuButton);
    
    m_controlLayout->addWidget(m_startStopButton);
    m_controlLayout->addLayout(buttonLayout);
    m_controlLayout->addStretch();
    
    // Add all sections to main layout
    m_mainLayout->addLayout(m_infoLayout, 2);
    m_mainLayout->addLayout(m_statsLayout, 1);
    m_mainLayout->addLayout(m_controlLayout, 0);
    
    // Set minimum height
    setMinimumHeight(80);
    setMaximumHeight(80);
}

void OneSevenMultiRtmpStreamItem::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    m_duplicateAction = m_contextMenu->addAction(
        QApplication::style()->standardIcon(QStyle::SP_FileIcon),
        obs_module_text("MultiRTMP.Duplicate")
    );
    connect(m_duplicateAction, &QAction::triggered, this, &OneSevenMultiRtmpStreamItem::onDuplicateAction);
    
    m_contextMenu->addSeparator();
    
    m_deleteAction = m_contextMenu->addAction(
        QApplication::style()->standardIcon(QStyle::SP_TrashIcon),
        obs_module_text("MultiRTMP.Delete")
    );
    connect(m_deleteAction, &QAction::triggered, this, &OneSevenMultiRtmpStreamItem::onDeleteAction);
}

void OneSevenMultiRtmpStreamItem::updateConfig(const OneSevenMultiRtmpConfig& config)
{
    m_config = config;
    updateUI();
}

void OneSevenMultiRtmpStreamItem::updateStatus(const OneSevenMultiRtmpStreamStatus& status)
{
    m_status = status;
    updateStatusDisplay();
    updateButtonStates();
}

void OneSevenMultiRtmpStreamItem::updateStats(const OneSevenMultiRtmpStreamStats& stats)
{
    m_stats = stats;
    updateStatsDisplay();
}

bool OneSevenMultiRtmpStreamItem::isActive() const
{
    return m_status.state == OneSevenMultiRtmpStreamStatus::State::STREAMING;
}

bool OneSevenMultiRtmpStreamItem::isConnecting() const
{
    return m_status.state == OneSevenMultiRtmpStreamStatus::State::CONNECTING || 
           m_status.state == OneSevenMultiRtmpStreamStatus::State::RECONNECTING;
}

bool OneSevenMultiRtmpStreamItem::isError() const
{
    return m_status.state == OneSevenMultiRtmpStreamStatus::State::ERROR;
}

void OneSevenMultiRtmpStreamItem::onStartStopClicked()
{
    if (isActive() || isConnecting()) {
        emit stopRequested(m_config.id);
    } else {
        emit startRequested(m_config.id);
    }
}

void OneSevenMultiRtmpStreamItem::onEditClicked()
{
    emit editRequested(m_config.id);
}

void OneSevenMultiRtmpStreamItem::onMenuRequested()
{
    if (m_contextMenu) {
        // Update menu state
        m_duplicateAction->setEnabled(true);
        m_deleteAction->setEnabled(!isActive() && !isConnecting());
        
        // Show menu at button position
        QPoint globalPos = m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height()));
        m_contextMenu->exec(globalPos);
    }
}

void OneSevenMultiRtmpStreamItem::onDuplicateAction()
{
    emit duplicateRequested(m_config.id);
}

void OneSevenMultiRtmpStreamItem::onDeleteAction()
{
    emit deleteRequested(m_config.id);
}

void OneSevenMultiRtmpStreamItem::onStatsUpdateTimer()
{
    // Update display if stream is active
    if (isActive()) {
        updateStatsDisplay();
    }
}

void OneSevenMultiRtmpStreamItem::updateUI()
{
    // Update basic info
    m_nameLabel->setText(QString::fromStdString(m_config.streamName));
    
    QString url = QString::fromStdString(m_config.service.serverUrl);
    if (url.length() > 40) {
        url = url.left(37) + "...";
    }
    m_urlLabel->setText(url);
    
    updateStatusDisplay();
    updateStatsDisplay();
    updateButtonStates();
}

void OneSevenMultiRtmpStreamItem::updateStatusDisplay()
{
    QString statusText;
    QString styleClass;
    
    switch (m_status.state) {
        case OneSevenMultiRtmpStreamStatus::State::STOPPED:
            statusText = obs_module_text("MultiRTMP.Status.Idle");
            styleClass = STATUS_IDLE_CLASS;
            m_connectionProgress->setVisible(false);
            break;
            
        case OneSevenMultiRtmpStreamStatus::State::CONNECTING:
            statusText = obs_module_text("MultiRTMP.Status.Connecting");
            styleClass = STATUS_CONNECTING_CLASS;
            m_connectionProgress->setVisible(true);
            m_connectionProgress->setRange(0, 0); // Indeterminate
            break;
            
        case OneSevenMultiRtmpStreamStatus::State::STREAMING:
            statusText = obs_module_text("MultiRTMP.Status.Active");
            styleClass = STATUS_ACTIVE_CLASS;
            m_connectionProgress->setVisible(false);
            break;
            
        case OneSevenMultiRtmpStreamStatus::State::RECONNECTING:
            statusText = obs_module_text("MultiRTMP.Status.Connecting");
            styleClass = STATUS_CONNECTING_CLASS;
            m_connectionProgress->setVisible(true);
            m_connectionProgress->setRange(0, 0); // Indeterminate
            break;
            
        case OneSevenMultiRtmpStreamStatus::State::ERROR:
            statusText = QString("%1: %2")
                .arg(obs_module_text("MultiRTMP.Status.Error"))
                .arg(QString::fromStdString(m_status.errorMessage));
            styleClass = STATUS_ERROR_CLASS;
            m_connectionProgress->setVisible(false);
            break;
    }
    
    m_statusLabel->setText(statusText);
    setStatusStyle(styleClass);
}

void OneSevenMultiRtmpStreamItem::updateStatsDisplay()
{
    if (isActive()) {
        // Format bitrate directly since currentBitrate is already in kbps
        if (m_stats.currentBitrate >= 1000.0) {
            m_bitrateLabel->setText(QString("%1 Mbps").arg(m_stats.currentBitrate / 1000.0, 0, 'f', 1));
        } else {
            m_bitrateLabel->setText(QString("%1 kbps").arg(static_cast<int>(m_stats.currentBitrate)));
        }
        
        // Convert duration to seconds and format
        uint64_t seconds = static_cast<uint64_t>(m_stats.duration.count());
        m_durationLabel->setText(formatDuration(seconds));
        
        // Convert int to double for frame rate
        m_framesLabel->setText(formatFrameRate(static_cast<double>(m_stats.currentFPS)));
    } else {
        m_bitrateLabel->setText("0 kbps");
        m_durationLabel->setText("00:00:00");
        m_framesLabel->setText("0 fps");
    }
}

void OneSevenMultiRtmpStreamItem::updateButtonStates()
{
    bool canStart = (m_status.state == OneSevenMultiRtmpStreamStatus::State::STOPPED || 
                     m_status.state == OneSevenMultiRtmpStreamStatus::State::ERROR);
    bool canStop = (m_status.state == OneSevenMultiRtmpStreamStatus::State::STREAMING || 
                    m_status.state == OneSevenMultiRtmpStreamStatus::State::CONNECTING ||
                    m_status.state == OneSevenMultiRtmpStreamStatus::State::RECONNECTING);
    
    if (canStart) {
        m_startStopButton->setText(obs_module_text("MultiRTMP.Start"));
        m_startStopButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
        m_startStopButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
        m_startStopButton->setEnabled(true);
    } else if (canStop) {
        m_startStopButton->setText(obs_module_text("MultiRTMP.Stop"));
        m_startStopButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaStop));
        m_startStopButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
        m_startStopButton->setEnabled(true);
    } else {
        m_startStopButton->setText(obs_module_text("MultiRTMP.Wait"));
        m_startStopButton->setIcon(QIcon());
        m_startStopButton->setStyleSheet("QPushButton { background-color: #999; color: white; }");
        m_startStopButton->setEnabled(false);
    }
    
    // Edit button is disabled when streaming
    m_editButton->setEnabled(!isActive() && !isConnecting());
}

void OneSevenMultiRtmpStreamItem::setStatusStyle(const QString& className)
{
    QString baseStyle = "font-size: 11px; font-weight: bold; padding: 2px 6px; border-radius: 3px;";
    QString colorStyle;
    
    if (className == STATUS_IDLE_CLASS) {
        colorStyle = "color: #666; background-color: #f5f5f5; border: 1px solid #ddd;";
    } else if (className == STATUS_CONNECTING_CLASS) {
        colorStyle = "color: #ff9800; background-color: #fff3e0; border: 1px solid #ffcc02;";
    } else if (className == STATUS_ACTIVE_CLASS) {
        colorStyle = "color: #4caf50; background-color: #e8f5e8; border: 1px solid #4caf50;";
    } else if (className == STATUS_ERROR_CLASS) {
        colorStyle = "color: #f44336; background-color: #ffebee; border: 1px solid #f44336;";
    } else if (className == STATUS_STOPPING_CLASS) {
        colorStyle = "color: #9c27b0; background-color: #f3e5f5; border: 1px solid #9c27b0;";
    }
    
    m_statusLabel->setStyleSheet(baseStyle + colorStyle);
}

QString OneSevenMultiRtmpStreamItem::formatBitrate(uint64_t bytes) const
{
    if (bytes == 0) {
        return "0 kbps";
    }
    
    // Convert bytes to kbps (assuming 1 second duration for rate calculation)
    double kbps = (bytes * 8.0) / 1000.0;
    
    if (kbps >= 1000.0) {
        return QString("%1 Mbps").arg(kbps / 1000.0, 0, 'f', 1);
    } else {
        return QString("%1 kbps").arg(static_cast<int>(kbps));
    }
}

QString OneSevenMultiRtmpStreamItem::formatDuration(uint64_t seconds) const
{
    uint64_t hours = seconds / 3600;
    uint64_t minutes = (seconds % 3600) / 60;
    uint64_t secs = seconds % 60;
    
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

QString OneSevenMultiRtmpStreamItem::formatFrameRate(double fps) const
{
    if (fps <= 0.0) {
        return "0 fps";
    }
    
    return QString("%1 fps").arg(fps, 0, 'f', 1);
}

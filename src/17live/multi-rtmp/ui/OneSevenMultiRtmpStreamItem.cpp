#include "OneSevenMultiRtmpStreamItem.hpp"
#include "../OneSevenMultiRtmpManager.hpp"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <obs-output.h>
#include <cmath>
#include <chrono>

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
    , m_topLayout(nullptr)
    , m_nameLabel(nullptr)
    , m_statusLayout(nullptr)
    , m_statusDot(nullptr)
    , m_statusLabel(nullptr)
    , m_statsLayout(nullptr)
    , m_durationLabel(nullptr)
    , m_bitrateLabel(nullptr)
    , m_framesLabel(nullptr)
    , m_controlLayout(nullptr)
    , m_startStopButton(nullptr)
    , m_editButton(nullptr)
    , m_menuButton(nullptr)
    , m_urlLabel(nullptr)
    , m_connectionProgress(nullptr)
    , m_contextMenu(nullptr)
    , m_duplicateAction(nullptr)
    , m_deleteAction(nullptr)
    , m_statsTimer(nullptr)
    , m_manager(nullptr)
    , m_lastTotalBytes(0)
    , m_lastTotalFrames(0)
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
    // Main vertical layout (3 layers)
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(12, 8, 12, 8);
    m_mainLayout->setSpacing(6);
    
    // Top layer - Name and status
    m_topLayout = new QHBoxLayout();
    m_topLayout->setSpacing(8);
    
    // Stream name (left side)
    m_nameLabel = new QLabel();
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #FFFFFF;background: transparent;");
    m_nameLabel->setWordWrap(false);
    
    // Status section (right side)
    m_statusLayout = new QHBoxLayout();
    m_statusLayout->setSpacing(6);
    m_statusLayout->setAlignment(Qt::AlignRight);
    
    // Status dot (14px x 14px colored circle)
    m_statusDot = new QLabel();
    m_statusDot->setFixedSize(14, 14);
    m_statusDot->setStyleSheet("background-color: #A1A9B6; border-radius: 7px; background: transparent;");
    
    // Status text
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("font-size: 12px; color: #CCCCCC;");
    m_statusLabel->setWordWrap(false);
    
    m_statusLayout->addWidget(m_statusDot);
    m_statusLayout->addWidget(m_statusLabel);
    
    m_topLayout->addWidget(m_nameLabel, 1);
    m_topLayout->addLayout(m_statusLayout, 0);
    
    // Middle layer - Statistics (vertical stack)
    m_statsLayout = new QVBoxLayout();
    m_statsLayout->setSpacing(2);
    
    // Duration
    m_durationLabel = new QLabel("-");
    m_durationLabel->setStyleSheet("font-size: 11px; color: #AAAAAA;");
    
    // Upload speed
    m_bitrateLabel = new QLabel("-");
    m_bitrateLabel->setStyleSheet("font-size: 11px; color: #AAAAAA;");
    
    // Frame rate
    m_framesLabel = new QLabel("-");
    m_framesLabel->setStyleSheet("font-size: 11px; color: #AAAAAA;");
    
    m_statsLayout->addWidget(m_durationLabel);
    m_statsLayout->addWidget(m_bitrateLabel);
    m_statsLayout->addWidget(m_framesLabel);
    
    // Bottom layer - Controls (horizontal, right-aligned)
    m_controlLayout = new QHBoxLayout();
    m_controlLayout->setSpacing(8);
    m_controlLayout->setAlignment(Qt::AlignRight);
    
    // Play/Stop button
    m_startStopButton = new QPushButton();
    m_startStopButton->setMinimumSize(24, 24);
    m_startStopButton->setMaximumSize(24, 24);
    m_startStopButton->setIcon(QIcon(":/resources/play.svg"));
    m_startStopButton->setIconSize(QSize(16, 16));
    m_startStopButton->setStyleSheet("QPushButton { border: none; background: transparent; } QPushButton:hover { background-color: rgba(255,255,255,0.1); border-radius: 12px; }");
    m_startStopButton->setToolTip(obs_module_text("MultiRTMP.Start"));
    connect(m_startStopButton, &QPushButton::clicked, this, &OneSevenMultiRtmpStreamItem::onStartStopClicked);
    
    // Settings button
    m_editButton = new QPushButton();
    m_editButton->setMinimumSize(24, 24);
    m_editButton->setMaximumSize(24, 24);
    m_editButton->setIcon(QIcon(":/resources/settings.svg"));
    m_editButton->setIconSize(QSize(16, 16));
    m_editButton->setStyleSheet("QPushButton { border: none; background: transparent; } QPushButton:hover { background-color: rgba(255,255,255,0.1); border-radius: 12px; }");
    m_editButton->setToolTip(obs_module_text("MultiRTMP.Edit"));
    connect(m_editButton, &QPushButton::clicked, this, &OneSevenMultiRtmpStreamItem::onEditClicked);
    
    // Delete button
    m_menuButton = new QPushButton();
    m_menuButton->setMinimumSize(24, 24);
    m_menuButton->setMaximumSize(24, 24);
    m_menuButton->setIcon(QIcon(":/resources/trash.svg"));
    m_menuButton->setIconSize(QSize(16, 16));
    m_menuButton->setStyleSheet("QPushButton { border: none; background: transparent; } QPushButton:hover { background-color: rgba(255,255,255,0.1); border-radius: 12px; }");
    m_menuButton->setToolTip(obs_module_text("MultiRTMP.Delete"));
    connect(m_menuButton, &QPushButton::clicked, this, &OneSevenMultiRtmpStreamItem::onDeleteClicked);
    
    m_controlLayout->addWidget(m_startStopButton);
    m_controlLayout->addWidget(m_editButton);
    m_controlLayout->addWidget(m_menuButton);
    
    // Add all layers to main layout
    m_mainLayout->addLayout(m_topLayout);
    m_mainLayout->addLayout(m_statsLayout);
    m_mainLayout->addLayout(m_controlLayout);
    
    // Set minimum height and dark background
    setMinimumHeight(170);
    setMaximumHeight(170);
    setStyleSheet("OneSevenMultiRtmpStreamItem { background-color: #2D2D30; border-radius: 8px; }");
    
    // Initialize unused legacy widgets to nullptr
    m_urlLabel = nullptr;
    m_connectionProgress = nullptr;
}

void OneSevenMultiRtmpStreamItem::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
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
    // Record start time when stream becomes active
    if (status.state == OneSevenMultiRtmpStreamStatus::STREAMING && 
        m_status.state != OneSevenMultiRtmpStreamStatus::STREAMING) {
        m_startTime = std::chrono::steady_clock::now();
        m_lastStatsTime = m_startTime;
        m_lastTotalBytes = 0;
        m_lastTotalFrames = 0;
    }
    
    m_status = status;
    updateStatusDisplay();
}

void OneSevenMultiRtmpStreamItem::updateStats(const OneSevenMultiRtmpStreamStats& stats)
{
    m_stats = stats;
    updateStatsDisplay();
}

void OneSevenMultiRtmpStreamItem::setManager(OneSevenMultiRtmpManager* manager)
{
    m_manager = manager;
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
    if (m_contextMenu && m_duplicateAction && m_deleteAction && m_menuButton) {
        // Update menu state
        m_duplicateAction->setEnabled(true);
        m_deleteAction->setEnabled(!isActive() && !isConnecting());
        
        // Show menu at button position
        QPoint globalPos = m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height()));
        m_contextMenu->exec(globalPos);
    }
}

void OneSevenMultiRtmpStreamItem::onDeleteAction()
{
    emit deleteRequested(m_config.id);
}

void OneSevenMultiRtmpStreamItem::onDeleteClicked()
{
    emit deleteRequested(m_config.id);
}

void OneSevenMultiRtmpStreamItem::onStatsUpdateTimer()
{
    // Update display if stream is active
    if (isActive()) {
        collectRealTimeStats();
        updateStatsDisplay();
    }
}

void OneSevenMultiRtmpStreamItem::updateUI()
{
    // Update basic info
    if (m_nameLabel) {
        m_nameLabel->setText(QString::fromStdString(m_config.streamName));
    }
    
    updateStatusDisplay();
    updateStatusDot();
    updateStatsDisplay();
    updateButtonStates();
}

void OneSevenMultiRtmpStreamItem::updateStatusDisplay()
{
    // Update status text
    QString statusText = getStatusText();
    if (m_statusLabel) {
        m_statusLabel->setText(statusText);
    }
    
    // Update status dot color
    updateStatusDot();
}

void OneSevenMultiRtmpStreamItem::updateStatsDisplay()
{
    // Update individual stats labels based on connection state
    bool isConnected = (m_status.state == OneSevenMultiRtmpStreamStatus::State::STREAMING);
    
    if (isConnected) {
        // Show actual stats when connected with "label: value" format
        QString duration = formatDuration(static_cast<uint64_t>(m_stats.duration.count()));
        QString bitrate = formatBitrate(static_cast<uint64_t>(m_stats.currentBitrate * 1000)); // Convert to bps
        QString fps = formatFrameRate(m_stats.currentFPS);
        
        if (m_durationLabel) m_durationLabel->setText(QString("连线时长: %1").arg(duration));
        if (m_bitrateLabel) m_bitrateLabel->setText(QString("上传速率: %1 Kbps").arg(bitrate));
        if (m_framesLabel) m_framesLabel->setText(QString("帧率: %1 FPS").arg(fps));
    } else {
        // Show labels with dashes when not connected
        if (m_durationLabel) m_durationLabel->setText("连线时长: --:--:--");
        if (m_bitrateLabel) m_bitrateLabel->setText("上传速率: -- Kbps");
        if (m_framesLabel) m_framesLabel->setText("帧率: -- FPS");
    }
}

void OneSevenMultiRtmpStreamItem::updateButtonStates()
{
    if (!m_startStopButton) {
        return;
    }
    
    bool canStart = (m_status.state == OneSevenMultiRtmpStreamStatus::State::STOPPED || 
                     m_status.state == OneSevenMultiRtmpStreamStatus::State::ERROR);
    bool canStop = (m_status.state == OneSevenMultiRtmpStreamStatus::State::STREAMING || 
                    m_status.state == OneSevenMultiRtmpStreamStatus::State::CONNECTING ||
                    m_status.state == OneSevenMultiRtmpStreamStatus::State::RECONNECTING);
    
    if (canStart) {
        m_startStopButton->setIcon(QIcon(":/resources/play.svg"));
        m_startStopButton->setToolTip(obs_module_text("MultiRTMP.Start"));
        m_startStopButton->setEnabled(true);
    } else if (canStop) {
        m_startStopButton->setIcon(QIcon(":/resources/stop.svg"));
        m_startStopButton->setToolTip(obs_module_text("MultiRTMP.Stop"));
        m_startStopButton->setEnabled(true);
    } else {
        m_startStopButton->setIcon(QIcon(":/resources/play.svg"));
        m_startStopButton->setToolTip(obs_module_text("MultiRTMP.Wait"));
        m_startStopButton->setEnabled(false);
    }
    
    // Edit and delete buttons are disabled when streaming
    if (m_editButton) {
        m_editButton->setEnabled(!isActive() && !isConnecting());
    }
    if (m_menuButton) {
        m_menuButton->setEnabled(!isActive() && !isConnecting());
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
        return "0";
    }
    
    return QString("%1").arg(static_cast<int>(fps));
}

void OneSevenMultiRtmpStreamItem::updateStatusDot()
{
    if (!m_statusDot) {
        return;
    }
    
    QString color = getStatusColor();
    m_statusDot->setStyleSheet(QString("background-color: %1; border-radius: 7px;").arg(color));
}

QString OneSevenMultiRtmpStreamItem::getStatusText() const
{
    switch (m_status.state) {
        case OneSevenMultiRtmpStreamStatus::State::STOPPED:
            return obs_module_text("MultiRTMP.Status.Disconnected");
            
        case OneSevenMultiRtmpStreamStatus::State::CONNECTING:
            return obs_module_text("MultiRTMP.Status.Connecting");
            
        case OneSevenMultiRtmpStreamStatus::State::STREAMING:
            return obs_module_text("MultiRTMP.Status.Connected");
            
        case OneSevenMultiRtmpStreamStatus::State::RECONNECTING:
            return obs_module_text("MultiRTMP.Status.Connecting");
            
        case OneSevenMultiRtmpStreamStatus::State::ERROR:
            return obs_module_text("MultiRTMP.Status.Disconnected");
            
        default:
            return obs_module_text("MultiRTMP.Status.Disconnected");
    }
}

QString OneSevenMultiRtmpStreamItem::getStatusColor() const
{
    switch (m_status.state) {
        case OneSevenMultiRtmpStreamStatus::State::STOPPED:
            return "#A1A9B6"; // Disconnected - gray
            
        case OneSevenMultiRtmpStreamStatus::State::CONNECTING:
            return "#FF873D"; // Connecting - orange
            
        case OneSevenMultiRtmpStreamStatus::State::STREAMING:
            return "#00D22E"; // Connected - green
            
        case OneSevenMultiRtmpStreamStatus::State::RECONNECTING:
            return "#FF873D"; // Reconnecting - orange
            
        case OneSevenMultiRtmpStreamStatus::State::ERROR:
            return "#A1A9B6"; // Error - gray
            
        default:
            return "#A1A9B6"; // Default - gray
    }
}

QString OneSevenMultiRtmpStreamItem::formatBitrate(uint64_t bytes) const
{
    if (bytes == 0) {
        return "0 Kbps";
    }
    
    static const char* units[] = {"bps", "Kbps", "Mbps", "Gbps"};
    int unitIndex = static_cast<int>(log10(bytes) / 3);
    if (unitIndex >= 4) unitIndex = 3;
    
    double value = bytes / pow(1000, unitIndex);
    return QString("%1 %2").arg(value, 0, 'f', 1).arg(units[unitIndex]);
}

void OneSevenMultiRtmpStreamItem::collectRealTimeStats()
{
    if (!m_manager || !isActive()) {
        return;
    }
    
    // Get obs_output_t* from manager
    obs_output_t* output = m_manager->getStreamOutput(m_config.id);
    if (!output) {
        return;
    }
    
    using namespace std::chrono;
    
    auto now = steady_clock::now();
    auto newBytes = obs_output_get_total_bytes(output);
    auto newFrames = obs_output_get_total_frames(output);
    
    // Calculate time interval
    auto interval = duration_cast<duration<double>>(now - m_lastStatsTime).count();
    
    if (interval > 0 && m_lastStatsTime != m_startTime) {
        // Calculate duration since start
        // Update stats structure with real-time data
        m_stats.duration = duration_cast<std::chrono::milliseconds>(now - m_startTime);
        
        // Calculate bitrate (bits per second)
        if (newBytes > m_lastTotalBytes) {
            auto byteDiff = newBytes - m_lastTotalBytes;
            m_stats.currentBitrate = (byteDiff * 8) / (interval * 1000); // Convert to Kbps
        }
        
        // Calculate frame rate
        if (newFrames > static_cast<int>(m_lastTotalFrames)) {
            auto frameDiff = newFrames - m_lastTotalFrames;
            m_stats.currentFPS = static_cast<int>(frameDiff / interval);
        }
        
        // Update total stats
        m_stats.totalFrames = newFrames;
        m_stats.droppedFrames = obs_output_get_frames_dropped(output);
    }
    
    // Update tracking variables
    m_lastTotalBytes = newBytes;
    m_lastTotalFrames = newFrames;
    m_lastStatsTime = now;
}

#include "moc_OneSevenMultiRtmpStreamItem.cpp"

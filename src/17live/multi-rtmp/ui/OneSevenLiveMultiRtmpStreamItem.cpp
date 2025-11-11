#include "OneSevenLiveMultiRtmpStreamItem.hpp"

#include <obs-output.h>

#include <QApplication>
#include <QMessageBox>
#include <QStyle>
#include <chrono>
#include <cmath>

#include "../OneSevenLiveMultiRtmpManager.hpp"

// Static style class constants
const QString OneSevenLiveMultiRtmpStreamItem::STATUS_IDLE_CLASS = "status-idle";
const QString OneSevenLiveMultiRtmpStreamItem::STATUS_CONNECTING_CLASS = "status-connecting";
const QString OneSevenLiveMultiRtmpStreamItem::STATUS_ACTIVE_CLASS = "status-active";
const QString OneSevenLiveMultiRtmpStreamItem::STATUS_ERROR_CLASS = "status-error";
const QString OneSevenLiveMultiRtmpStreamItem::STATUS_STOPPING_CLASS = "status-stopping";

OneSevenLiveMultiRtmpStreamItem::OneSevenLiveMultiRtmpStreamItem(const OneSevenLiveMultiRtmpConfig& config,
                                                         QWidget* parent)
    : QFrame(parent),
      m_config(config),
      m_mainLayout(nullptr),
      m_topLayout(nullptr),
      m_nameLabel(nullptr),
      m_statusLayout(nullptr),
      m_statusDot(nullptr),
      m_statusLabel(nullptr),
      m_statsLayout(nullptr),
      m_durationLabel(nullptr),
      m_bitrateLabel(nullptr),
      m_framesLabel(nullptr),
      m_controlLayout(nullptr),
      m_startStopButton(nullptr),
      m_editButton(nullptr),
      m_menuButton(nullptr),
      m_urlLabel(nullptr),
      m_connectionProgress(nullptr),
      m_contextMenu(nullptr),
      m_duplicateAction(nullptr),
      m_deleteAction(nullptr),
      m_statsTimer(nullptr),
      m_manager(nullptr),
      m_lastTotalBytes(0),
      m_lastTotalFrames(0) {
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setLineWidth(1);
    setMidLineWidth(0);

    setupUI();
    setupContextMenu();
    updateUI();

    // Setup stats update timer
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(1000);  // Update every second
    connect(m_statsTimer, &QTimer::timeout, this, &OneSevenLiveMultiRtmpStreamItem::onStatsUpdateTimer);
    m_statsTimer->start();
}

OneSevenLiveMultiRtmpStreamItem::~OneSevenLiveMultiRtmpStreamItem() {
    if (m_statsTimer) {
        m_statsTimer->stop();
    }

    if (m_contextMenu) {
        m_contextMenu->deleteLater();
    }
}

void OneSevenLiveMultiRtmpStreamItem::setupUI() {
    // Main vertical layout (3 layers)
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(12, 8, 12, 8);
    m_mainLayout->setSpacing(6);

    // Top layer - Name and status
    m_topLayout = new QHBoxLayout();
    m_topLayout->setSpacing(8);

    // Stream name (left side)
    m_nameLabel = new QLabel();
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #FFFFFF;");
    m_nameLabel->setWordWrap(false);

    // Status section (right side)
    m_statusLayout = new QHBoxLayout();
    m_statusLayout->setSpacing(6);
    m_statusLayout->setAlignment(Qt::AlignRight);

    // Status dot (14px x 14px colored circle)
    m_statusDot = new QLabel();
    m_statusDot->setFixedSize(14, 14);
    m_statusDot->setStyleSheet("background-color: #A1A9B6; border-radius: 7px;");

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
    m_startStopButton->setStyleSheet(
        "QPushButton { border: none; background: transparent; } QPushButton:hover { "
        "background-color: rgba(255,255,255,0.1); border-radius: 12px; }");
    m_startStopButton->setToolTip(obs_module_text("MultiRTMP.Start"));
    connect(m_startStopButton, &QPushButton::clicked, this,
            &OneSevenLiveMultiRtmpStreamItem::onStartStopClicked);

    // Settings button
    m_editButton = new QPushButton();
    m_editButton->setMinimumSize(24, 24);
    m_editButton->setMaximumSize(24, 24);
    m_editButton->setIcon(QIcon(":/resources/settings.svg"));
    m_editButton->setIconSize(QSize(16, 16));
    m_editButton->setStyleSheet(
        "QPushButton { border: none; background: transparent; } QPushButton:hover { "
        "background-color: rgba(255,255,255,0.1); border-radius: 12px; }");
    m_editButton->setToolTip(obs_module_text("MultiRTMP.Edit"));
    connect(m_editButton, &QPushButton::clicked, this, &OneSevenLiveMultiRtmpStreamItem::onEditClicked);

    // Delete button
    m_menuButton = new QPushButton();
    m_menuButton->setMinimumSize(24, 24);
    m_menuButton->setMaximumSize(24, 24);
    m_menuButton->setIcon(QIcon(":/resources/trash.svg"));
    m_menuButton->setIconSize(QSize(16, 16));
    m_menuButton->setStyleSheet(
        "QPushButton { border: none; background: transparent; } QPushButton:hover { "
        "background-color: rgba(255,255,255,0.1); border-radius: 12px; }");
    m_menuButton->setToolTip(obs_module_text("MultiRTMP.Delete"));
    connect(m_menuButton, &QPushButton::clicked, this,
            &OneSevenLiveMultiRtmpStreamItem::onDeleteClicked);

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
    setStyleSheet("OneSevenLiveMultiRtmpStreamItem { background-color: #2D2D30; border-radius: 8px; }");

    // Initialize unused legacy widgets to nullptr
    m_urlLabel = nullptr;
    m_connectionProgress = nullptr;
}

void OneSevenLiveMultiRtmpStreamItem::setupContextMenu() {
    m_contextMenu = new QMenu(this);

    m_deleteAction =
        m_contextMenu->addAction(QApplication::style()->standardIcon(QStyle::SP_TrashIcon),
                                 obs_module_text("MultiRTMP.Delete"));
    connect(m_deleteAction, &QAction::triggered, this,
            &OneSevenLiveMultiRtmpStreamItem::onDeleteAction);
}

void OneSevenLiveMultiRtmpStreamItem::updateConfig(const OneSevenLiveMultiRtmpConfig& config) {
    m_config = config;
    updateUI();
}

void OneSevenLiveMultiRtmpStreamItem::updateStatus(const OneSevenLiveMultiRtmpStreamStatus& status) {
    // Record start time when stream becomes active
    if (status.state == OneSevenLiveMultiRtmpStreamStatus::STREAMING &&
        m_status.state != OneSevenLiveMultiRtmpStreamStatus::STREAMING) {
        m_startTime = std::chrono::steady_clock::now();
        m_lastStatsTime = m_startTime;
        m_lastTotalBytes = 0;
        m_lastTotalFrames = 0;
    }

    m_status = status;
    updateStatusDisplay();
    updateButtonStates();  // Ensure button states are updated when status changes
}

void OneSevenLiveMultiRtmpStreamItem::updateStats(const OneSevenLiveMultiRtmpStreamStats& stats) {
    m_stats = stats;
    updateStatsDisplay();
}

void OneSevenLiveMultiRtmpStreamItem::setManager(OneSevenLiveMultiRtmpManager* manager) {
    m_manager = manager;
}

bool OneSevenLiveMultiRtmpStreamItem::isActive() const {
    return m_status.state == OneSevenLiveMultiRtmpStreamStatus::State::STREAMING;
}

bool OneSevenLiveMultiRtmpStreamItem::isConnecting() const {
    return m_status.state == OneSevenLiveMultiRtmpStreamStatus::State::CONNECTING ||
           m_status.state == OneSevenLiveMultiRtmpStreamStatus::State::RECONNECTING;
}

bool OneSevenLiveMultiRtmpStreamItem::isError() const {
    return m_status.state == OneSevenLiveMultiRtmpStreamStatus::State::ERROR_STATE;
}

void OneSevenLiveMultiRtmpStreamItem::onStartStopClicked() {
    if (isActive() || isConnecting()) {
        emit stopRequested(m_config.id);
    } else {
        emit startRequested(m_config.id);
    }
}

void OneSevenLiveMultiRtmpStreamItem::onEditClicked() {
    emit editRequested(m_config.id);
}

void OneSevenLiveMultiRtmpStreamItem::onMenuRequested() {
    if (m_contextMenu && m_duplicateAction && m_deleteAction && m_menuButton) {
        // Update menu state
        m_duplicateAction->setEnabled(true);
        m_deleteAction->setEnabled(!isActive() && !isConnecting());

        // Show menu at button position
        QPoint globalPos = m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height()));
        m_contextMenu->exec(globalPos);
    }
}

void OneSevenLiveMultiRtmpStreamItem::onDeleteAction() {
    emit deleteRequested(m_config.id);
}

void OneSevenLiveMultiRtmpStreamItem::onDeleteClicked() {
    emit deleteRequested(m_config.id);
}

void OneSevenLiveMultiRtmpStreamItem::onStatsUpdateTimer() {
    // Update display if stream is active
    if (isActive()) {
        collectRealTimeStats();
        updateStatsDisplay();
    }
}

void OneSevenLiveMultiRtmpStreamItem::updateUI() {
    // Update basic info
    if (m_nameLabel) {
        m_nameLabel->setText(QString::fromStdString(m_config.streamName));
    }

    updateStatusDisplay();
    updateStatusDot();
    updateStatsDisplay();
    updateButtonStates();
}

void OneSevenLiveMultiRtmpStreamItem::updateStatusDisplay() {
    // Update status text
    QString statusText = getStatusText();
    if (m_statusLabel) {
        m_statusLabel->setText(statusText);
    }

    // Update status dot color
    updateStatusDot();
}

void OneSevenLiveMultiRtmpStreamItem::updateStatsDisplay() {
    // Update individual stats labels based on connection state
    bool isConnected = (m_status.state == OneSevenLiveMultiRtmpStreamStatus::State::STREAMING);

    if (isConnected) {
        // Show actual stats when connected with "label: value" format
        QString duration = formatDuration(static_cast<uint64_t>(m_stats.duration.count()));
        QString bitrate =
            formatBitrate(static_cast<uint64_t>(m_stats.currentBitrate * 1000));  // Convert to bps
        QString fps = formatFrameRate(m_stats.currentFPS);

        if (m_durationLabel)
            m_durationLabel->setText(
                QString("%1: %2").arg(obs_module_text("MultiRTMP.Stats.Duration")).arg(duration));
        if (m_bitrateLabel)
            m_bitrateLabel->setText(QString("%1: %2 Kbps")
                                        .arg(obs_module_text("MultiRTMP.Stats.UploadRate"))
                                        .arg(bitrate));
        if (m_framesLabel)
            m_framesLabel->setText(
                QString("%1: %2 FPS").arg(obs_module_text("MultiRTMP.Stats.FrameRate")).arg(fps));
    } else {
        // Show labels with dashes when not connected
        if (m_durationLabel)
            m_durationLabel->setText(
                QString("%1: --:--:--").arg(obs_module_text("MultiRTMP.Stats.Duration")));
        if (m_bitrateLabel)
            m_bitrateLabel->setText(
                QString("%1: -- Kbps").arg(obs_module_text("MultiRTMP.Stats.UploadRate")));
        if (m_framesLabel)
            m_framesLabel->setText(
                QString("%1: -- FPS").arg(obs_module_text("MultiRTMP.Stats.FrameRate")));
    }
}

void OneSevenLiveMultiRtmpStreamItem::updateButtonStates() {
    if (!m_startStopButton) {
        return;
    }

    bool canStart = (m_status.state == OneSevenLiveMultiRtmpStreamStatus::State::STOPPED ||
                     m_status.state == OneSevenLiveMultiRtmpStreamStatus::State::ERROR_STATE);
    bool canStop = (m_status.state == OneSevenLiveMultiRtmpStreamStatus::State::STREAMING ||
                    m_status.state == OneSevenLiveMultiRtmpStreamStatus::State::CONNECTING ||
                    m_status.state == OneSevenLiveMultiRtmpStreamStatus::State::RECONNECTING);

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

QString OneSevenLiveMultiRtmpStreamItem::formatDuration(uint64_t seconds) const {
    uint64_t hours = seconds / 3600;
    uint64_t minutes = (seconds % 3600) / 60;
    uint64_t secs = seconds % 60;

    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

QString OneSevenLiveMultiRtmpStreamItem::formatFrameRate(double fps) const {
    if (fps <= 0.0) {
        return "0";
    }

    return QString("%1").arg(static_cast<int>(fps));
}

void OneSevenLiveMultiRtmpStreamItem::updateStatusDot() {
    if (!m_statusDot) {
        return;
    }

    QString color = getStatusColor();
    m_statusDot->setStyleSheet(QString("background-color: %1; border-radius: 7px;").arg(color));
}

QString OneSevenLiveMultiRtmpStreamItem::getStatusText() const {
    switch (m_status.state) {
    case OneSevenLiveMultiRtmpStreamStatus::State::STOPPED:
        return obs_module_text("MultiRTMP.Status.Disconnected");

    case OneSevenLiveMultiRtmpStreamStatus::State::CONNECTING:
        return obs_module_text("MultiRTMP.Status.Connecting");

    case OneSevenLiveMultiRtmpStreamStatus::State::STREAMING:
        return obs_module_text("MultiRTMP.Status.Connected");

    case OneSevenLiveMultiRtmpStreamStatus::State::RECONNECTING:
        return obs_module_text("MultiRTMP.Status.Connecting");

    case OneSevenLiveMultiRtmpStreamStatus::State::ERROR_STATE:
        return obs_module_text("MultiRTMP.Status.Disconnected");

    default:
        return obs_module_text("MultiRTMP.Status.Disconnected");
    }
}

QString OneSevenLiveMultiRtmpStreamItem::getStatusColor() const {
    switch (m_status.state) {
    case OneSevenLiveMultiRtmpStreamStatus::State::STOPPED:
        return "#A1A9B6";  // Disconnected - gray

    case OneSevenLiveMultiRtmpStreamStatus::State::CONNECTING:
        return "#FF873D";  // Connecting - orange

    case OneSevenLiveMultiRtmpStreamStatus::State::STREAMING:
        return "#00D22E";  // Connected - green

    case OneSevenLiveMultiRtmpStreamStatus::State::RECONNECTING:
        return "#FF873D";  // Reconnecting - orange

    case OneSevenLiveMultiRtmpStreamStatus::State::ERROR_STATE:
        return "#A1A9B6";  // Error - gray

    default:
        return "#A1A9B6";  // Default - gray
    }
}

QString OneSevenLiveMultiRtmpStreamItem::formatBitrate(uint64_t bytes) const {
    if (bytes == 0) {
        return "0 Kbps";
    }

    static const char* units[] = {"bps", "Kbps", "Mbps", "Gbps"};
    int unitIndex = static_cast<int>(log10(bytes) / 3);
    if (unitIndex >= 4)
        unitIndex = 3;

    double value = bytes / pow(1000, unitIndex);
    return QString("%1 %2").arg(value, 0, 'f', 1).arg(units[unitIndex]);
}

void OneSevenLiveMultiRtmpStreamItem::collectRealTimeStats() {
    if (!m_manager || !isActive()) {
        // Reset stats when not active to prevent stale data
        m_stats.currentBitrate = 0.0;
        m_stats.currentFPS = 0;
        return;
    }

    // Get obs_output_t* from manager
    obs_output_t* output = m_manager->getStreamOutput(m_config.id);
    if (!output) {
        // Reset stats when output is not available
        m_stats.currentBitrate = 0.0;
        m_stats.currentFPS = 0;
        return;
    }

    using namespace std::chrono;

    auto now = steady_clock::now();
    auto newBytes = obs_output_get_total_bytes(output);
    auto newFrames = obs_output_get_total_frames(output);

    // Validate OBS data - ensure we have valid values
    if (newBytes == 0 && newFrames == 0) {
        // OBS might not have started collecting stats yet, keep previous values
        return;
    }

    // Calculate time interval with minimum threshold to avoid division by very small numbers
    auto interval = duration_cast<duration<double>>(now - m_lastStatsTime).count();
    const double MIN_INTERVAL = 0.1;  // Minimum 100ms interval

    if (interval >= MIN_INTERVAL && m_lastStatsTime != m_startTime) {
        // Calculate duration since start
        m_stats.duration = duration_cast<std::chrono::milliseconds>(now - m_startTime);

        // Calculate bitrate with validation
        if (newBytes >= m_lastTotalBytes) {  // Use >= to handle equal case
            auto byteDiff = newBytes - m_lastTotalBytes;
            if (byteDiff > 0) {
                double newBitrate = (byteDiff * 8.0) / (interval * 1000.0);  // Convert to Kbps
                
                // Apply reasonable bounds (0 to 100 Mbps)
                if (newBitrate >= 0.0 && newBitrate <= 100000.0) {
                    // Apply simple smoothing to reduce flickering
                    const double SMOOTHING_FACTOR = 0.3;
                    if (m_stats.currentBitrate > 0.0) {
                        m_stats.currentBitrate = m_stats.currentBitrate * (1.0 - SMOOTHING_FACTOR) + 
                                               newBitrate * SMOOTHING_FACTOR;
                    } else {
                        m_stats.currentBitrate = newBitrate;
                    }
                }
            }
        } else {
            // Handle case where bytes decreased (shouldn't happen normally)
            // This might indicate a stream restart, reset tracking
            m_lastTotalBytes = newBytes;
            m_lastTotalFrames = newFrames;
            m_lastStatsTime = now;
            return;
        }

        // Calculate frame rate with validation
        if (newFrames >= static_cast<int>(m_lastTotalFrames)) {  // Use >= to handle equal case
            auto frameDiff = newFrames - m_lastTotalFrames;
            if (frameDiff > 0) {
                double newFPS = static_cast<double>(frameDiff) / interval;
                
                // Apply reasonable bounds (0 to 120 FPS)
                if (newFPS >= 0.0 && newFPS <= 120.0) {
                    // Apply simple smoothing to reduce flickering
                    const double SMOOTHING_FACTOR = 0.3;
                    if (m_stats.currentFPS > 0) {
                        double smoothedFPS = m_stats.currentFPS * (1.0 - SMOOTHING_FACTOR) + 
                                           newFPS * SMOOTHING_FACTOR;
                        m_stats.currentFPS = static_cast<int>(std::round(smoothedFPS));
                    } else {
                        m_stats.currentFPS = static_cast<int>(std::round(newFPS));
                    }
                }
            }
        } else {
            // Handle case where frames decreased (shouldn't happen normally)
            // This might indicate a stream restart, reset tracking
            m_lastTotalBytes = newBytes;
            m_lastTotalFrames = newFrames;
            m_lastStatsTime = now;
            return;
        }

        // Update total stats with validation
        m_stats.totalFrames = newFrames;
        
        // Get dropped frames with validation
        uint32_t droppedFrames = obs_output_get_frames_dropped(output);
        if (droppedFrames <= static_cast<uint32_t>(newFrames)) {  // Sanity check
            m_stats.droppedFrames = droppedFrames;
        }
    }

    // Update tracking variables only if we have valid data
    if (newBytes >= m_lastTotalBytes && newFrames >= static_cast<int>(m_lastTotalFrames)) {
        m_lastTotalBytes = newBytes;
        m_lastTotalFrames = newFrames;
        m_lastStatsTime = now;
    }
}

#include "moc_OneSevenLiveMultiRtmpStreamItem.cpp"

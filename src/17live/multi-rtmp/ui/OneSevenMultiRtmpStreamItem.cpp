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
    
    // Stream name
    m_nameLabel = new QLabel();
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #FFFFFF;");
    m_nameLabel->setWordWrap(false);
    
    // Status/Stats label (combined)
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("font-size: 12px; color: #CCCCCC;");
    m_statusLabel->setWordWrap(false);
    
    m_infoLayout->addWidget(m_nameLabel);
    m_infoLayout->addWidget(m_statusLabel);
    m_infoLayout->addStretch();
    
    // Right section - Controls
    m_controlLayout = new QVBoxLayout();
    m_controlLayout->setSpacing(8);
    m_controlLayout->setAlignment(Qt::AlignRight);
    
    // Start/Stop button
    m_startStopButton = new QPushButton();
    m_startStopButton->setMinimumSize(60, 28);
    m_startStopButton->setMaximumSize(60, 28);
    connect(m_startStopButton, &QPushButton::clicked, this, &OneSevenMultiRtmpStreamItem::onStartStopClicked);
    
    // Button container for edit and delete
    QHBoxLayout* iconButtonLayout = new QHBoxLayout();
    iconButtonLayout->setSpacing(8);
    iconButtonLayout->setAlignment(Qt::AlignRight);
    
    // Edit button with SVG icon
    m_editButton = new QPushButton();
    m_editButton->setMinimumSize(24, 24);
    m_editButton->setMaximumSize(24, 24);
    m_editButton->setIcon(QIcon(":/resources/edit.svg"));
    m_editButton->setIconSize(QSize(16, 16));
    m_editButton->setStyleSheet("QPushButton { border: none; background: transparent; } QPushButton:hover { background-color: rgba(255,255,255,0.1); border-radius: 12px; }");
    m_editButton->setToolTip("编辑");
    connect(m_editButton, &QPushButton::clicked, this, &OneSevenMultiRtmpStreamItem::onEditClicked);
    
    // Delete button with SVG icon  
    m_menuButton = new QPushButton();
    m_menuButton->setMinimumSize(24, 24);
    m_menuButton->setMaximumSize(24, 24);
    m_menuButton->setIcon(QIcon(":/resources/delete.svg"));
    m_menuButton->setIconSize(QSize(16, 16));
    m_menuButton->setStyleSheet("QPushButton { border: none; background: transparent; } QPushButton:hover { background-color: rgba(255,255,255,0.1); border-radius: 12px; }");
    m_menuButton->setToolTip("删除");
    connect(m_menuButton, &QPushButton::clicked, this, &OneSevenMultiRtmpStreamItem::onDeleteClicked);
    
    iconButtonLayout->addWidget(m_editButton);
    iconButtonLayout->addWidget(m_menuButton);
    
    m_controlLayout->addWidget(m_startStopButton);
    m_controlLayout->addLayout(iconButtonLayout);
    m_controlLayout->addStretch();
    
    // Add sections to main layout
    m_mainLayout->addLayout(m_infoLayout, 1);
    m_mainLayout->addStretch();
    m_mainLayout->addLayout(m_controlLayout, 0);
    
    // Set minimum height and dark background
    setMinimumHeight(60);
    setMaximumHeight(60);
    setStyleSheet("OneSevenMultiRtmpStreamItem { background-color: #2D2D30; border-radius: 8px; }");
    
    // Initialize unused widgets to nullptr
    m_urlLabel = nullptr;
    m_statsLayout = nullptr;
    m_bitrateLabel = nullptr;
    m_durationLabel = nullptr;
    m_framesLabel = nullptr;
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
    updateStatsDisplay();
    updateButtonStates();
}

void OneSevenMultiRtmpStreamItem::updateStatusDisplay()
{
    QString statusText;
    
    switch (m_status.state) {
        case OneSevenMultiRtmpStreamStatus::State::STOPPED:
            statusText = "未推流";
            break;
            
        case OneSevenMultiRtmpStreamStatus::State::CONNECTING:
            statusText = "连接中...";
            break;
            
        case OneSevenMultiRtmpStreamStatus::State::STREAMING:
            // Show duration, Mbps, FPS when streaming
            {
                QString duration = formatDuration(static_cast<uint64_t>(m_stats.duration.count()));
                QString bitrate;
                if (m_stats.currentBitrate >= 1000.0) {
                    bitrate = QString("%1 Mbps").arg(m_stats.currentBitrate / 1000.0, 0, 'f', 1);
                } else {
                    bitrate = QString("%1 kbps").arg(static_cast<int>(m_stats.currentBitrate));
                }
                QString fps = QString("%1 FPS").arg(static_cast<int>(m_stats.currentFPS));
                statusText = QString("%1 %2 %3").arg(duration).arg(bitrate).arg(fps);
            }
            break;
            
        case OneSevenMultiRtmpStreamStatus::State::RECONNECTING:
            statusText = "重连中...";
            break;
            
        case OneSevenMultiRtmpStreamStatus::State::ERROR:
            statusText = QString("错误: %1").arg(QString::fromStdString(m_status.errorMessage));
            break;
    }
    
    if (m_statusLabel) {
        m_statusLabel->setText(statusText);
    }
}

void OneSevenMultiRtmpStreamItem::updateStatsDisplay()
{
    // Stats are now displayed in the status label when streaming
    // This method is kept for compatibility but doesn't need to do anything
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
        m_startStopButton->setText("开启");
        m_startStopButton->setStyleSheet("QPushButton { background-color: #FF0001; color: white; font-weight: bold; border: none; border-radius: 4px; }");
        m_startStopButton->setEnabled(true);
    } else if (canStop) {
        m_startStopButton->setText("停止");
        m_startStopButton->setStyleSheet("QPushButton { background-color: #007AFF; color: white; font-weight: bold; border: none; border-radius: 4px; }");
        m_startStopButton->setEnabled(true);
    } else {
        m_startStopButton->setText("等待");
        m_startStopButton->setStyleSheet("QPushButton { background-color: #999; color: white; border: none; border-radius: 4px; }");
        m_startStopButton->setEnabled(false);
    }
    
    // Edit and delete buttons are disabled when streaming
    m_editButton->setEnabled(!isActive() && !isConnecting());
    m_menuButton->setEnabled(!isActive() && !isConnecting());
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

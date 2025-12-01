#pragma once

#include <obs-module.h>

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <chrono>

#include "../OneSevenLiveMultiRtmpModels.hpp"
#include "plugin-support.h"

// Forward declaration
class OneSevenLiveMultiRtmpManager;

/**
 * Individual stream item widget
 * Displays stream configuration, status, and controls for a single RTMP stream
 */
class OneSevenLiveMultiRtmpStreamItem : public QFrame {
    Q_OBJECT

   public:
    explicit OneSevenLiveMultiRtmpStreamItem(const OneSevenLiveMultiRtmpConfig& config,
                                             QWidget* parent = nullptr);
    ~OneSevenLiveMultiRtmpStreamItem();

    // Configuration management
    void updateConfig(const OneSevenLiveMultiRtmpConfig& config);
    void updateStatus(const OneSevenLiveMultiRtmpStreamStatus& status);
    void updateStats(const OneSevenLiveMultiRtmpStreamStats& stats);

    // Manager access
    void setManager(OneSevenLiveMultiRtmpManager* manager);

    // Getters
    const std::string& getStreamId() const {
        return m_config.id;
    }

    const OneSevenLiveMultiRtmpConfig& getConfig() const {
        return m_config;
    }

    const OneSevenLiveMultiRtmpStreamStatus& getStatus() const {
        return m_status;
    }

    const OneSevenLiveMultiRtmpStreamStats& getStats() const {
        return m_stats;
    }

    // State queries
    bool isActive() const;
    bool isConnecting() const;
    bool isError() const;

   signals:
    void startRequested(const std::string& streamId);
    void stopRequested(const std::string& streamId);
    void editRequested(const std::string& streamId);
    void deleteRequested(const std::string& streamId);

   private slots:
    void onStartStopClicked();
    void onEditClicked();
    void onMenuRequested();
    void onDeleteAction();
    void onStatsUpdateTimer();

   private:
    void setupUI();
    void setupContextMenu();
    void updateUI();
    void updateStatusDisplay();
    void updateStatsDisplay();
    void updateButtonStates();
    void setStatusStyle(const QString& className);
    void updateStatusDot();
    QString getStatusText() const;
    QString getStatusColor() const;
    QString formatBitrate(uint64_t bytes) const;
    QString formatDuration(uint64_t seconds) const;
    QString formatFrameRate(double fps) const;

    // Real-time statistics collection
    void collectRealTimeStats();

    // Configuration and state
    OneSevenLiveMultiRtmpConfig m_config;
    OneSevenLiveMultiRtmpStreamStatus m_status;
    OneSevenLiveMultiRtmpStreamStats m_stats;

    // UI components - Main layout (3-layer vertical)
    QVBoxLayout* m_mainLayout = nullptr;

    // Top layer - Name and status
    QHBoxLayout* m_topLayout = nullptr;
    QLabel* m_nameLabel = nullptr;
    QHBoxLayout* m_statusLayout = nullptr;
    QLabel* m_statusDot = nullptr;
    QLabel* m_statusLabel = nullptr;

    // Middle layer - Statistics
    QVBoxLayout* m_statsLayout = nullptr;
    QLabel* m_durationLabel = nullptr;
    QLabel* m_bitrateLabel = nullptr;
    QLabel* m_framesLabel = nullptr;

    // Bottom layer - Controls
    QHBoxLayout* m_controlLayout = nullptr;
    QPushButton* m_startStopButton = nullptr;
    QPushButton* m_editButton = nullptr;
    QPushButton* m_menuButton = nullptr;

    // Unused legacy components (kept for compatibility)
    QLabel* m_urlLabel = nullptr;
    QProgressBar* m_connectionProgress = nullptr;

    // Context menu
    QMenu* m_contextMenu = nullptr;
    QAction* m_duplicateAction = nullptr;
    QAction* m_deleteAction = nullptr;

    // Update timer
    QTimer* m_statsTimer = nullptr;

    // Manager reference for real-time stats
    OneSevenLiveMultiRtmpManager* m_manager = nullptr;

    // Real-time statistics tracking
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_lastStatsTime;
    uint64_t m_lastTotalBytes = 0;
    uint64_t m_lastTotalFrames = 0;

    // Style classes for different states
    static const QString STATUS_IDLE_CLASS;
    static const QString STATUS_CONNECTING_CLASS;
    static const QString STATUS_ACTIVE_CLASS;
    static const QString STATUS_ERROR_CLASS;
    static const QString STATUS_STOPPING_CLASS;
    QHBoxLayout* m_errorHintLayout = nullptr;
    QLabel* m_errorIconLabel = nullptr;
    QLabel* m_errorTextLabel = nullptr;

    void updateErrorHint();
    QString composeErrorTooltip(const QString& brief, const QString& detail,
                                const QString& solution) const;

    struct ErrorMapping {
        QString titleKey;
        QString descKey;
        QString solutionKey;
    };

    ErrorMapping mapErrorCode(const QString& code) const;
};

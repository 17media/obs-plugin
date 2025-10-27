#pragma once

#include "../OneSevenMultiRtmpModels.hpp"
#include "plugin-support.h"
#include <obs-module.h>
#include <QWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QMenu>
#include <QAction>

/**
 * Individual stream item widget
 * Displays stream configuration, status, and controls for a single RTMP stream
 */
class OneSevenMultiRtmpStreamItem : public QFrame {
    Q_OBJECT

public:
    explicit OneSevenMultiRtmpStreamItem(const OneSevenMultiRtmpConfig& config, QWidget* parent = nullptr);
    ~OneSevenMultiRtmpStreamItem();

    // Configuration management
    void updateConfig(const OneSevenMultiRtmpConfig& config);
    void updateStatus(const OneSevenMultiRtmpStreamStatus& status);
    void updateStats(const OneSevenMultiRtmpStreamStats& stats);
    
    // Getters
    const std::string& getStreamId() const { return m_config.id; }
    const OneSevenMultiRtmpConfig& getConfig() const { return m_config; }
    const OneSevenMultiRtmpStreamStatus& getStatus() const { return m_status; }
    const OneSevenMultiRtmpStreamStats& getStats() const { return m_stats; }

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
    void onDeleteClicked();
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

    // Configuration and state
    OneSevenMultiRtmpConfig m_config;
    OneSevenMultiRtmpStreamStatus m_status;
    OneSevenMultiRtmpStreamStats m_stats;

    // UI components - Main layout (3-layer vertical)
    QVBoxLayout* m_mainLayout;
    
    // Top layer - Name and status
    QHBoxLayout* m_topLayout;
    QLabel* m_nameLabel;
    QHBoxLayout* m_statusLayout;
    QLabel* m_statusDot;
    QLabel* m_statusLabel;
    
    // Middle layer - Statistics
    QVBoxLayout* m_statsLayout;
    QLabel* m_durationLabel;
    QLabel* m_bitrateLabel;
    QLabel* m_framesLabel;
    
    // Bottom layer - Controls
    QHBoxLayout* m_controlLayout;
    QPushButton* m_startStopButton;
    QPushButton* m_editButton;
    QPushButton* m_menuButton;
    
    // Unused legacy components (kept for compatibility)
    QLabel* m_urlLabel;
    QProgressBar* m_connectionProgress;
    
    // Context menu
    QMenu* m_contextMenu;
    QAction* m_duplicateAction;
    QAction* m_deleteAction;
    
    // Update timer
    QTimer* m_statsTimer;
    
    // Style classes for different states
    static const QString STATUS_IDLE_CLASS;
    static const QString STATUS_CONNECTING_CLASS;
    static const QString STATUS_ACTIVE_CLASS;
    static const QString STATUS_ERROR_CLASS;
    static const QString STATUS_STOPPING_CLASS;
};

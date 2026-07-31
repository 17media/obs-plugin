#pragma once

#include <obs-module.h>

#include <QDockWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

#include "../OneSevenLiveMultiRtmpManager.hpp"
#include "plugin-support.h"

class OneSevenLiveMultiRtmpListWidget;
class OneSevenLiveMultiRtmpConfigDialog;

/**
 * Main dock widget for Multi-RTMP functionality
 * Provides the primary UI interface for managing multiple RTMP streams
 */
class OneSevenLiveMultiRtmpDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveMultiRtmpDock(QWidget* parent = nullptr);
    ~OneSevenLiveMultiRtmpDock();

    // Public interface
    void refreshStreamList();
    void updateStreamStatus(const std::string& streamId,
                            const OneSevenLiveMultiRtmpStreamStatus& status);
    void updateStreamStats(const std::string& streamId,
                           const OneSevenLiveMultiRtmpStreamStats& stats);

   public slots:
    void onAddStreamClicked();
    void onStartAllClicked();
    void onStopAllClicked();
    void onRefreshClicked();
    void onStreamConfigChanged(const std::string& streamId);
    void onStreamDeleted(const std::string& streamId);

   private slots:
    void onStatsUpdateTimer();

   protected:
    void showEvent(QShowEvent* event) override;

   private:
    void setupUI();
    void setupConnections();
    void setupManagerCallbacks();
    void updateButtonStates();
    void updateStreamCount();
    void showConfigDialog(const OneSevenLiveMultiRtmpConfig& config = {});
    bool ensureManagerInitialized();
    bool startYouTubeStream(const std::string& streamId);

    // UI components
    QWidget* m_centralWidget = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;

    // Header section
    QPushButton* m_addStreamButton = nullptr;

    // Control section
    QFrame* m_controlFrame = nullptr;
    QVBoxLayout* m_controlLayout = nullptr;
    QPushButton* m_startAllButton = nullptr;
    QPushButton* m_stopAllButton = nullptr;

    // Stream list section
    QScrollArea* m_scrollArea = nullptr;
    OneSevenLiveMultiRtmpListWidget* m_streamListWidget = nullptr;

    // Dialog
    QPointer<OneSevenLiveMultiRtmpConfigDialog> m_configDialog;

    // Timer for periodic updates
    QTimer* m_statsUpdateTimer = nullptr;

    // Manager reference
    OneSevenLiveMultiRtmpManager* m_manager = nullptr;

    // State management
    bool m_isUpdatingUI;
    bool m_isFirstShow;
};

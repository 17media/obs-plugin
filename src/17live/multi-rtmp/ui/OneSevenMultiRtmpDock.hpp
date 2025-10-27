#pragma once

#include "../OneSevenMultiRtmpManager.hpp"
#include "plugin-support.h"
#include <obs-module.h>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QShowEvent>
#include <memory>

class OneSevenMultiRtmpListWidget;
class OneSevenMultiRtmpConfigDialog;

/**
 * Main dock widget for Multi-RTMP functionality
 * Provides the primary UI interface for managing multiple RTMP streams
 */
class OneSevenMultiRtmpDock : public QDockWidget {
    Q_OBJECT

public:
    explicit OneSevenMultiRtmpDock(QWidget* parent = nullptr);
    ~OneSevenMultiRtmpDock();

    // Public interface
    void refreshStreamList();
    void updateStreamStatus(const std::string& streamId, const OneSevenMultiRtmpStreamStatus& status);
    void updateStreamStats(const std::string& streamId, const OneSevenMultiRtmpStreamStats& stats);

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
    void showConfigDialog(const OneSevenMultiRtmpConfig& config = {});
    bool ensureManagerInitialized();

    // UI components
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    
    // Header section
    QFrame* m_headerFrame;
    QHBoxLayout* m_headerLayout;
    QLabel* m_titleLabel;
    QLabel* m_streamCountLabel;
    QPushButton* m_addStreamButton;
    QPushButton* m_refreshButton;
    
    // Control section
    QFrame* m_controlFrame;
    QVBoxLayout* m_controlLayout;
    QPushButton* m_startAllButton;
    QPushButton* m_stopAllButton;
    
    // Stream list section
    QScrollArea* m_scrollArea;
    OneSevenMultiRtmpListWidget* m_streamListWidget;
    
    // Status section
    QFrame* m_statusFrame;
    QHBoxLayout* m_statusLayout;
    QLabel* m_statusLabel;
    
    // Dialog
    OneSevenMultiRtmpConfigDialog* m_configDialog;
    
    // Timer for periodic updates
    QTimer* m_statsUpdateTimer;
    
    // Manager reference
    OneSevenMultiRtmpManager* m_manager;
    
    // State tracking
    bool m_isUpdatingUI;
    bool m_isFirstShow;
};

// Helper function to get localized text
inline QString getMultiRtmpText(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

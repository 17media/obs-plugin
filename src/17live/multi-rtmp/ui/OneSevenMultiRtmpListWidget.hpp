#pragma once

#include "../OneSevenMultiRtmpModels.hpp"
#include "plugin-support.h"
#include <obs-module.h>
#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QFrame>
#include <vector>
#include <memory>

class OneSevenMultiRtmpStreamItem;
class OneSevenMultiRtmpManager;

/**
 * List widget for displaying multiple RTMP stream items
 * Manages the layout and interaction of individual stream widgets
 */
class OneSevenMultiRtmpListWidget : public QWidget {
    Q_OBJECT

public:
    explicit OneSevenMultiRtmpListWidget(QWidget* parent = nullptr);
    ~OneSevenMultiRtmpListWidget();

    // Stream management
    void addStream(const OneSevenMultiRtmpConfig& config);
    void removeStream(const std::string& streamId);
    void updateStream(const OneSevenMultiRtmpConfig& config);
    void updateStreamStatus(const std::string& streamId, const OneSevenMultiRtmpStreamStatus& status);
    void updateStreamStats(const std::string& streamId, const OneSevenMultiRtmpStreamStats& stats);
    void clearAllStreams();
    void refreshAllStreams();

    // State queries
    bool hasStream(const std::string& streamId) const;
    size_t getStreamCount() const;
    std::vector<std::string> getAllStreamIds() const;
    std::vector<std::string> getActiveStreamIds() const;
    
    // Manager access
    void setManager(OneSevenMultiRtmpManager* manager);

signals:
    void streamStartRequested(const std::string& streamId);
    void streamStopRequested(const std::string& streamId);
    void streamEditRequested(const std::string& streamId);
    void streamDeleteRequested(const std::string& streamId);
    void streamDuplicateRequested(const std::string& streamId);

private slots:
    void onStreamItemStartClicked(const std::string& streamId);
    void onStreamItemStopClicked(const std::string& streamId);
    void onStreamItemEditClicked(const std::string& streamId);
    void onStreamItemDeleteClicked(const std::string& streamId);
    void onStreamItemDuplicateClicked(const std::string& streamId);

private:
    void setupUI();
    void setupEmptyState();
    void updateEmptyState();
    OneSevenMultiRtmpStreamItem* findStreamItem(const std::string& streamId) const;
    void removeStreamItem(OneSevenMultiRtmpStreamItem* item);

    // UI components
    QVBoxLayout* m_mainLayout;
    QVBoxLayout* m_streamLayout;
    QWidget* m_streamContainer;
    
    // Empty state
    QFrame* m_emptyFrame;
    QVBoxLayout* m_emptyLayout;
    QLabel* m_emptyTextLabel;
    
    // Stream items
    std::vector<OneSevenMultiRtmpStreamItem*> m_streamItems;
    
    // State
    bool m_showEmptyState;
    
    // Manager reference
    OneSevenMultiRtmpManager* m_manager;
};

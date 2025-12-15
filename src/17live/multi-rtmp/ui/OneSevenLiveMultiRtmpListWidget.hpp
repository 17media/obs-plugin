#pragma once

#include <obs-module.h>

#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>
#include <vector>

#include "../OneSevenLiveMultiRtmpModels.hpp"
#include "plugin-support.h"

class OneSevenLiveMultiRtmpStreamItem;
class OneSevenLiveMultiRtmpManager;

/**
 * List widget for displaying multiple RTMP stream items
 * Manages the layout and interaction of individual stream widgets
 */
class OneSevenLiveMultiRtmpListWidget : public QWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveMultiRtmpListWidget(QWidget* parent = nullptr);
    ~OneSevenLiveMultiRtmpListWidget();

    // Stream management
    void addStream(const OneSevenLiveMultiRtmpConfig& config);
    void removeStream(const std::string& streamId);
    void updateStream(const OneSevenLiveMultiRtmpConfig& config);
    void updateStreamStatus(const std::string& streamId,
                            const OneSevenLiveMultiRtmpStreamStatus& status);
    void updateStreamStats(const std::string& streamId,
                           const OneSevenLiveMultiRtmpStreamStats& stats);
    void clearAllStreams();
    void refreshAllStreams();

    // State queries
    bool hasStream(const std::string& streamId) const;
    size_t getStreamCount() const;
    std::vector<std::string> getAllStreamIds() const;
    std::vector<std::string> getActiveStreamIds() const;

    // Stream status statistics
    struct StreamStatusStats {
        size_t totalCount = 0;
        size_t activeCount = 0;
        size_t connectingCount = 0;
        size_t stoppedCount = 0;
        size_t errorCount = 0;
    };

    StreamStatusStats getStreamStatusStats() const;

    // Manager access
    void setManager(OneSevenLiveMultiRtmpManager* manager);

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
    OneSevenLiveMultiRtmpStreamItem* findStreamItem(const std::string& streamId) const;
    void removeStreamItem(OneSevenLiveMultiRtmpStreamItem* item);

    // UI components
    QVBoxLayout* m_mainLayout = nullptr;
    QVBoxLayout* m_streamLayout = nullptr;
    QWidget* m_streamContainer = nullptr;

    // Empty state
    QFrame* m_emptyFrame = nullptr;
    QVBoxLayout* m_emptyLayout = nullptr;
    QLabel* m_emptyTextLabel = nullptr;

    // Stream items
    std::vector<OneSevenLiveMultiRtmpStreamItem*> m_streamItems;

    // State
    bool m_showEmptyState = false;

    // Manager reference
    OneSevenLiveMultiRtmpManager* m_manager = nullptr;
};

#include "OneSevenLiveMultiRtmpListWidget.hpp"

#include <algorithm>

#include "OneSevenLiveMultiRtmpStreamItem.hpp"

OneSevenLiveMultiRtmpListWidget::OneSevenLiveMultiRtmpListWidget(QWidget* parent)
    : QWidget(parent),
      m_mainLayout(nullptr),
      m_streamLayout(nullptr),
      m_streamContainer(nullptr),
      m_emptyFrame(nullptr),
      m_emptyLayout(nullptr),
      m_emptyTextLabel(nullptr),
      m_showEmptyState(true),
      m_manager(nullptr) {
    setupUI();
    updateEmptyState();
}

OneSevenLiveMultiRtmpListWidget::~OneSevenLiveMultiRtmpListWidget() {
    clearAllStreams();
}

void OneSevenLiveMultiRtmpListWidget::setupUI() {
    // Main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(4);

    // Stream container
    m_streamContainer = new QWidget();
    m_streamLayout = new QVBoxLayout(m_streamContainer);
    m_streamLayout->setContentsMargins(0, 0, 0, 0);
    m_streamLayout->setSpacing(4);
    m_streamLayout->addStretch();  // Push items to top

    // Setup empty state
    setupEmptyState();

    // Add to main layout
    m_mainLayout->addWidget(m_streamContainer);
    m_mainLayout->addWidget(m_emptyFrame);
    m_mainLayout->addStretch();
}

void OneSevenLiveMultiRtmpListWidget::setupEmptyState() {
    m_emptyFrame = new QFrame();
    m_emptyFrame->setStyleSheet(
        "QFrame { "
        "  background-color: transparent; "
        "  border: none; "
        "}");

    m_emptyLayout = new QVBoxLayout(m_emptyFrame);
    m_emptyLayout->setContentsMargins(20, 40, 20, 40);
    m_emptyLayout->setSpacing(0);

    // Simple text message
    m_emptyTextLabel = new QLabel(obs_module_text("MultiRTMP.List.EmptyTip"));
    m_emptyTextLabel->setAlignment(Qt::AlignCenter);
    m_emptyTextLabel->setStyleSheet(
        "font-size: 16px; "
        "color: #999; "
        "font-weight: normal;");

    m_emptyLayout->addStretch();
    m_emptyLayout->addWidget(m_emptyTextLabel);
    m_emptyLayout->addStretch();
}

void OneSevenLiveMultiRtmpListWidget::addStream(const OneSevenLiveMultiRtmpConfig& config) {
    // Check if stream already exists
    if (hasStream(config.id)) {
        updateStream(config);
        return;
    }

    // Create new stream item
    auto* streamItem = new OneSevenLiveMultiRtmpStreamItem(config, this);

    // Set manager reference
    if (m_manager) {
        streamItem->setManager(m_manager);
    }

    // Connect signals
    connect(streamItem, &OneSevenLiveMultiRtmpStreamItem::startRequested, this,
            &OneSevenLiveMultiRtmpListWidget::onStreamItemStartClicked);
    connect(streamItem, &OneSevenLiveMultiRtmpStreamItem::stopRequested, this,
            &OneSevenLiveMultiRtmpListWidget::onStreamItemStopClicked);
    connect(streamItem, &OneSevenLiveMultiRtmpStreamItem::editRequested, this,
            &OneSevenLiveMultiRtmpListWidget::onStreamItemEditClicked);
    connect(streamItem, &OneSevenLiveMultiRtmpStreamItem::deleteRequested, this,
            &OneSevenLiveMultiRtmpListWidget::onStreamItemDeleteClicked);

    // Add to layout (before stretch)
    int insertIndex = m_streamLayout->count() - 1;  // Before stretch
    m_streamLayout->insertWidget(insertIndex, streamItem);

    // Add to tracking list
    m_streamItems.push_back(streamItem);

    updateEmptyState();
}

void OneSevenLiveMultiRtmpListWidget::removeStream(const std::string& streamId) {
    auto* item = findStreamItem(streamId);
    if (item) {
        removeStreamItem(item);
        updateEmptyState();
    }
}

void OneSevenLiveMultiRtmpListWidget::updateStream(const OneSevenLiveMultiRtmpConfig& config) {
    auto* item = findStreamItem(config.id);
    if (item) {
        item->updateConfig(config);
    }
}

void OneSevenLiveMultiRtmpListWidget::updateStreamStatus(
    const std::string& streamId, const OneSevenLiveMultiRtmpStreamStatus& status) {
    auto* item = findStreamItem(streamId);
    if (item) {
        item->updateStatus(status);
    }
}

void OneSevenLiveMultiRtmpListWidget::updateStreamStats(
    const std::string& streamId, const OneSevenLiveMultiRtmpStreamStats& stats) {
    auto* item = findStreamItem(streamId);
    if (item) {
        item->updateStats(stats);
    }
}

void OneSevenLiveMultiRtmpListWidget::clearAllStreams() {
    // Remove all stream items
    for (auto* item : m_streamItems) {
        m_streamLayout->removeWidget(item);
        item->deleteLater();
    }

    m_streamItems.clear();
    updateEmptyState();
}

void OneSevenLiveMultiRtmpListWidget::refreshAllStreams() {
    // This would typically reload from manager
    // For now, just update empty state
    updateEmptyState();
}

bool OneSevenLiveMultiRtmpListWidget::hasStream(const std::string& streamId) const {
    return findStreamItem(streamId) != nullptr;
}

size_t OneSevenLiveMultiRtmpListWidget::getStreamCount() const {
    return m_streamItems.size();
}

std::vector<std::string> OneSevenLiveMultiRtmpListWidget::getAllStreamIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_streamItems.size());

    for (const auto* item : m_streamItems) {
        ids.push_back(item->getStreamId());
    }

    return ids;
}

std::vector<std::string> OneSevenLiveMultiRtmpListWidget::getActiveStreamIds() const {
    std::vector<std::string> activeIds;

    for (const auto* item : m_streamItems) {
        if (item->isActive()) {
            activeIds.push_back(item->getStreamId());
        }
    }

    return activeIds;
}

OneSevenLiveMultiRtmpListWidget::StreamStatusStats
OneSevenLiveMultiRtmpListWidget::getStreamStatusStats() const {
    StreamStatusStats stats;
    stats.totalCount = m_streamItems.size();

    for (const auto* item : m_streamItems) {
        if (!item)
            continue;

        const auto& status = item->getStatus();
        switch (status.state) {
        case OneSevenLiveMultiRtmpStreamStatus::State::STREAMING:
            stats.activeCount++;
            break;
        case OneSevenLiveMultiRtmpStreamStatus::State::CONNECTING:
        case OneSevenLiveMultiRtmpStreamStatus::State::RECONNECTING:
            stats.connectingCount++;
            break;
        case OneSevenLiveMultiRtmpStreamStatus::State::STOPPED:
            stats.stoppedCount++;
            break;
        case OneSevenLiveMultiRtmpStreamStatus::State::ERROR_STATE:
            stats.errorCount++;
            break;
        }
    }

    return stats;
}

void OneSevenLiveMultiRtmpListWidget::setManager(OneSevenLiveMultiRtmpManager* manager) {
    m_manager = manager;

    // Update existing stream items
    for (auto* item : m_streamItems) {
        if (item) {
            item->setManager(m_manager);
        }
    }
}

void OneSevenLiveMultiRtmpListWidget::onStreamItemStartClicked(const std::string& streamId) {
    emit streamStartRequested(streamId);
}

void OneSevenLiveMultiRtmpListWidget::onStreamItemStopClicked(const std::string& streamId) {
    emit streamStopRequested(streamId);
}

void OneSevenLiveMultiRtmpListWidget::onStreamItemEditClicked(const std::string& streamId) {
    emit streamEditRequested(streamId);
}

void OneSevenLiveMultiRtmpListWidget::onStreamItemDeleteClicked(const std::string& streamId) {
    emit streamDeleteRequested(streamId);
}

void OneSevenLiveMultiRtmpListWidget::onStreamItemDuplicateClicked(const std::string& streamId) {
    emit streamDuplicateRequested(streamId);
}

void OneSevenLiveMultiRtmpListWidget::updateEmptyState() {
    bool isEmpty = m_streamItems.empty();

    if (isEmpty != m_showEmptyState) {
        m_showEmptyState = isEmpty;

        m_emptyFrame->setVisible(m_showEmptyState);
        m_streamContainer->setVisible(!m_showEmptyState);
    }
}

OneSevenLiveMultiRtmpStreamItem* OneSevenLiveMultiRtmpListWidget::findStreamItem(
    const std::string& streamId) const {
    auto it = std::find_if(m_streamItems.begin(), m_streamItems.end(),
                           [&streamId](const OneSevenLiveMultiRtmpStreamItem* item) {
                               return item && item->getStreamId() == streamId;
                           });

    return (it != m_streamItems.end()) ? *it : nullptr;
}

void OneSevenLiveMultiRtmpListWidget::removeStreamItem(OneSevenLiveMultiRtmpStreamItem* item) {
    if (!item) {
        return;
    }

    // Remove from layout
    m_streamLayout->removeWidget(item);

    // Remove from tracking list
    auto it = std::find(m_streamItems.begin(), m_streamItems.end(), item);
    if (it != m_streamItems.end()) {
        m_streamItems.erase(it);
    }

    // Delete the widget
    item->deleteLater();
}

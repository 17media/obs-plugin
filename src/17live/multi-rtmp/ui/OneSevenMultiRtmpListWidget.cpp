#include "OneSevenMultiRtmpListWidget.hpp"
#include "OneSevenMultiRtmpStreamItem.hpp"
#include <algorithm>

OneSevenMultiRtmpListWidget::OneSevenMultiRtmpListWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_streamLayout(nullptr)
    , m_streamContainer(nullptr)
    , m_emptyFrame(nullptr)
    , m_emptyLayout(nullptr)
    , m_emptyTextLabel(nullptr)
    , m_showEmptyState(true)
{
    setupUI();
    updateEmptyState();
}

OneSevenMultiRtmpListWidget::~OneSevenMultiRtmpListWidget()
{
    clearAllStreams();
}

void OneSevenMultiRtmpListWidget::setupUI()
{
    // Main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(4);
    
    // Stream container
    m_streamContainer = new QWidget();
    m_streamLayout = new QVBoxLayout(m_streamContainer);
    m_streamLayout->setContentsMargins(0, 0, 0, 0);
    m_streamLayout->setSpacing(4);
    m_streamLayout->addStretch(); // Push items to top
    
    // Setup empty state
    setupEmptyState();
    
    // Add to main layout
    m_mainLayout->addWidget(m_streamContainer);
    m_mainLayout->addWidget(m_emptyFrame);
    m_mainLayout->addStretch();
}

void OneSevenMultiRtmpListWidget::setupEmptyState()
{
    m_emptyFrame = new QFrame();
    m_emptyFrame->setStyleSheet(
        "QFrame { "
        "  background-color: transparent; "
        "  border: none; "
        "}"
    );
    
    m_emptyLayout = new QVBoxLayout(m_emptyFrame);
    m_emptyLayout->setContentsMargins(20, 40, 20, 40);
    m_emptyLayout->setSpacing(0);
    
    // Simple text message
    m_emptyTextLabel = new QLabel("请新建推流");
    m_emptyTextLabel->setAlignment(Qt::AlignCenter);
    m_emptyTextLabel->setStyleSheet(
        "font-size: 16px; "
        "color: #999; "
        "font-weight: normal;"
    );
    
    m_emptyLayout->addStretch();
    m_emptyLayout->addWidget(m_emptyTextLabel);
    m_emptyLayout->addStretch();
}

void OneSevenMultiRtmpListWidget::addStream(const OneSevenMultiRtmpConfig& config)
{
    // Check if stream already exists
    if (hasStream(config.id)) {
        updateStream(config);
        return;
    }
    
    // Create new stream item
    auto* streamItem = new OneSevenMultiRtmpStreamItem(config, this);
    
    // Connect signals
    connect(streamItem, &OneSevenMultiRtmpStreamItem::startRequested,
            this, &OneSevenMultiRtmpListWidget::onStreamItemStartClicked);
    connect(streamItem, &OneSevenMultiRtmpStreamItem::stopRequested,
            this, &OneSevenMultiRtmpListWidget::onStreamItemStopClicked);
    connect(streamItem, &OneSevenMultiRtmpStreamItem::editRequested,
            this, &OneSevenMultiRtmpListWidget::onStreamItemEditClicked);
    connect(streamItem, &OneSevenMultiRtmpStreamItem::deleteRequested,
            this, &OneSevenMultiRtmpListWidget::onStreamItemDeleteClicked);
    
    // Add to layout (before stretch)
    int insertIndex = m_streamLayout->count() - 1; // Before stretch
    m_streamLayout->insertWidget(insertIndex, streamItem);
    
    // Add to tracking list
    m_streamItems.push_back(streamItem);
    
    updateEmptyState();
}

void OneSevenMultiRtmpListWidget::removeStream(const std::string& streamId)
{
    auto* item = findStreamItem(streamId);
    if (item) {
        removeStreamItem(item);
        updateEmptyState();
    }
}

void OneSevenMultiRtmpListWidget::updateStream(const OneSevenMultiRtmpConfig& config)
{
    auto* item = findStreamItem(config.id);
    if (item) {
        item->updateConfig(config);
    }
}

void OneSevenMultiRtmpListWidget::updateStreamStatus(const std::string& streamId, const OneSevenMultiRtmpStreamStatus& status)
{
    auto* item = findStreamItem(streamId);
    if (item) {
        item->updateStatus(status);
    }
}

void OneSevenMultiRtmpListWidget::updateStreamStats(const std::string& streamId, const OneSevenMultiRtmpStreamStats& stats)
{
    auto* item = findStreamItem(streamId);
    if (item) {
        item->updateStats(stats);
    }
}

void OneSevenMultiRtmpListWidget::clearAllStreams()
{
    // Remove all stream items
    for (auto* item : m_streamItems) {
        m_streamLayout->removeWidget(item);
        item->deleteLater();
    }
    
    m_streamItems.clear();
    updateEmptyState();
}

void OneSevenMultiRtmpListWidget::refreshAllStreams()
{
    // This would typically reload from manager
    // For now, just update empty state
    updateEmptyState();
}

bool OneSevenMultiRtmpListWidget::hasStream(const std::string& streamId) const
{
    return findStreamItem(streamId) != nullptr;
}

size_t OneSevenMultiRtmpListWidget::getStreamCount() const
{
    return m_streamItems.size();
}

std::vector<std::string> OneSevenMultiRtmpListWidget::getAllStreamIds() const
{
    std::vector<std::string> ids;
    ids.reserve(m_streamItems.size());
    
    for (const auto* item : m_streamItems) {
        ids.push_back(item->getStreamId());
    }
    
    return ids;
}

std::vector<std::string> OneSevenMultiRtmpListWidget::getActiveStreamIds() const
{
    std::vector<std::string> activeIds;
    
    for (const auto* item : m_streamItems) {
        if (item->isActive()) {
            activeIds.push_back(item->getStreamId());
        }
    }
    
    return activeIds;
}

void OneSevenMultiRtmpListWidget::onStreamItemStartClicked(const std::string& streamId)
{
    emit streamStartRequested(streamId);
}

void OneSevenMultiRtmpListWidget::onStreamItemStopClicked(const std::string& streamId)
{
    emit streamStopRequested(streamId);
}

void OneSevenMultiRtmpListWidget::onStreamItemEditClicked(const std::string& streamId)
{
    emit streamEditRequested(streamId);
}

void OneSevenMultiRtmpListWidget::onStreamItemDeleteClicked(const std::string& streamId)
{
    emit streamDeleteRequested(streamId);
}

void OneSevenMultiRtmpListWidget::onStreamItemDuplicateClicked(const std::string& streamId)
{
    emit streamDuplicateRequested(streamId);
}

void OneSevenMultiRtmpListWidget::updateEmptyState()
{
    bool isEmpty = m_streamItems.empty();
    
    if (isEmpty != m_showEmptyState) {
        m_showEmptyState = isEmpty;
        
        m_emptyFrame->setVisible(m_showEmptyState);
        m_streamContainer->setVisible(!m_showEmptyState);
    }
}

OneSevenMultiRtmpStreamItem* OneSevenMultiRtmpListWidget::findStreamItem(const std::string& streamId) const
{
    auto it = std::find_if(m_streamItems.begin(), m_streamItems.end(),
        [&streamId](const OneSevenMultiRtmpStreamItem* item) {
            return item && item->getStreamId() == streamId;
        });
    
    return (it != m_streamItems.end()) ? *it : nullptr;
}

void OneSevenMultiRtmpListWidget::removeStreamItem(OneSevenMultiRtmpStreamItem* item)
{
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

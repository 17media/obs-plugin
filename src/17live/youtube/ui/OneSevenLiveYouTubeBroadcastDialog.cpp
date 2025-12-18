#include "OneSevenLiveYouTubeBroadcastDialog.hpp"
#include "../../OneSevenLiveCoreManager.hpp"
#include <QMessageBox>
#include <QHeaderView>
#include <obs-module.h>

OneSevenLiveYouTubeBroadcastDialog::OneSevenLiveYouTubeBroadcastDialog(QWidget* parent)
    : QDialog(parent), m_isCreating(false) {
    setWindowTitle("YouTube Broadcast Manager");
    resize(600, 400);

    m_client = OneSevenLiveCoreManager::getInstance().getYouTubeApiClient();

    setupUI();
    setupConnections();

    if (m_client) {
        loadBroadcasts();
    }
}

OneSevenLiveYouTubeBroadcastDialog::~OneSevenLiveYouTubeBroadcastDialog() {}

void OneSevenLiveYouTubeBroadcastDialog::setupUI() {
    auto layout = new QVBoxLayout(this);
    m_stackedWidget = new QStackedWidget(this);
    layout->addWidget(m_stackedWidget);

    // --- Page 1: List ---
    m_listPage = new QWidget();
    auto listLayout = new QVBoxLayout(m_listPage);
    
    m_broadcastList = new QListWidget();
    listLayout->addWidget(m_broadcastList);

    auto btnLayout = new QHBoxLayout();
    m_refreshButton = new QPushButton("Refresh");
    m_createButton = new QPushButton("Create New Broadcast");
    m_selectButton = new QPushButton("Select & Start Streaming");
    m_selectButton->setEnabled(false);

    btnLayout->addWidget(m_refreshButton);
    btnLayout->addWidget(m_createButton);
    btnLayout->addStretch();
    btnLayout->addWidget(m_selectButton);
    listLayout->addLayout(btnLayout);

    m_stackedWidget->addWidget(m_listPage);

    // --- Page 2: Create ---
    m_createPage = new QWidget();
    auto createLayout = new QVBoxLayout(m_createPage);
    
    auto formLayout = new QVBoxLayout();
    formLayout->addWidget(new QLabel("Title:"));
    m_titleEdit = new QLineEdit();
    formLayout->addWidget(m_titleEdit);
    
    formLayout->addWidget(new QLabel("Privacy:"));
    m_privacyCombo = new QComboBox();
    m_privacyCombo->addItems({"public", "unlisted", "private"});
    formLayout->addWidget(m_privacyCombo);
    
    createLayout->addLayout(formLayout);
    createLayout->addStretch();

    auto createBtnLayout = new QHBoxLayout();
    m_backButton = new QPushButton("Back");
    m_confirmCreateButton = new QPushButton("Create & Start");
    
    createBtnLayout->addWidget(m_backButton);
    createBtnLayout->addStretch();
    createBtnLayout->addWidget(m_confirmCreateButton);
    createLayout->addLayout(createBtnLayout);

    m_stackedWidget->addWidget(m_createPage);
}

void OneSevenLiveYouTubeBroadcastDialog::setupConnections() {
    connect(m_refreshButton, &QPushButton::clicked, this, &OneSevenLiveYouTubeBroadcastDialog::onRefreshClicked);
    connect(m_createButton, &QPushButton::clicked, this, &OneSevenLiveYouTubeBroadcastDialog::onCreateNewClicked);
    connect(m_selectButton, &QPushButton::clicked, this, &OneSevenLiveYouTubeBroadcastDialog::onSelectClicked);
    connect(m_backButton, &QPushButton::clicked, this, &OneSevenLiveYouTubeBroadcastDialog::onBackClicked);
    connect(m_confirmCreateButton, &QPushButton::clicked, this, &OneSevenLiveYouTubeBroadcastDialog::onCreateConfirmClicked);
    
    connect(m_broadcastList, &QListWidget::itemSelectionChanged, [this]() {
        m_selectButton->setEnabled(!m_broadcastList->selectedItems().isEmpty());
    });

    if (m_client) {
        connect(m_client, &OneSevenLiveYouTubeClient::myLiveBroadcastsReceived, this, &OneSevenLiveYouTubeBroadcastDialog::onBroadcastsReceived);
        connect(m_client, &OneSevenLiveYouTubeClient::myLiveStreamsReceived, this, &OneSevenLiveYouTubeBroadcastDialog::onStreamsReceived);
        connect(m_client, &OneSevenLiveYouTubeClient::liveBroadcastCreated, this, &OneSevenLiveYouTubeBroadcastDialog::onBroadcastCreated);
        connect(m_client, &OneSevenLiveYouTubeClient::liveStreamCreated, this, &OneSevenLiveYouTubeBroadcastDialog::onStreamCreated);
        connect(m_client, &OneSevenLiveYouTubeClient::liveBroadcastBound, this, &OneSevenLiveYouTubeBroadcastDialog::onBroadcastBound);
        connect(m_client, &OneSevenLiveYouTubeClient::errorOccurred, this, &OneSevenLiveYouTubeBroadcastDialog::onError);
    }
}

void OneSevenLiveYouTubeBroadcastDialog::loadBroadcasts() {
    if (!m_client) return;
    m_broadcastList->clear();
    m_broadcastList->addItem("Loading...");
    m_client->getMyLiveBroadcasts("all");
}

void OneSevenLiveYouTubeBroadcastDialog::onRefreshClicked() {
    loadBroadcasts();
}

void OneSevenLiveYouTubeBroadcastDialog::onCreateNewClicked() {
    m_titleEdit->clear();
    m_stackedWidget->setCurrentWidget(m_createPage);
}

void OneSevenLiveYouTubeBroadcastDialog::onBackClicked() {
    m_stackedWidget->setCurrentWidget(m_listPage);
}

void OneSevenLiveYouTubeBroadcastDialog::onBroadcastsReceived(const YouTubeLiveBroadcastListResponse& response) {
    if (m_broadcastList->count() > 0 && m_broadcastList->item(0)->text() == "Loading...") {
        m_broadcastList->clear();
    }

    for (const auto& broadcast : response.items) {
        // Filter out completed broadcasts as they cannot be reused
        if (broadcast.status.lifeCycleStatus == "complete" || broadcast.status.lifeCycleStatus == "completed") {
            continue;
        }

        QString label = QString("%1 (%2) - %3")
            .arg(broadcast.snippet.title)
            .arg(broadcast.status.lifeCycleStatus)
            .arg(broadcast.snippet.scheduledStartTime);
        
        auto item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, broadcast.id);
        item->setData(Qt::UserRole + 1, broadcast.snippet.liveChatId);
        item->setData(Qt::UserRole + 2, broadcast.snippet.title);
        m_broadcastList->addItem(item);
    }
    
    // If we only fetched one type, maybe fetch the other?
    // For now, let's just stop here to avoid complexity of chaining.
    // NOTE: Ideally we should fetch 'upcoming' too.
}

void OneSevenLiveYouTubeBroadcastDialog::onSelectClicked() {
    auto items = m_broadcastList->selectedItems();
    if (items.isEmpty()) return;
    
    auto item = items.first();
    QString bid = item->data(Qt::UserRole).toString();
    QString chatId = item->data(Qt::UserRole + 1).toString();
    QString title = item->data(Qt::UserRole + 2).toString();
    
    processSelection(bid, title, chatId);
}

void OneSevenLiveYouTubeBroadcastDialog::onCreateConfirmClicked() {
    QString title = m_titleEdit->text();
    if (title.isEmpty()) {
        QMessageBox::warning(this, "Error", "Title cannot be empty");
        return;
    }
    
    m_isCreating = true;
    m_selectedTitle = title;
    m_confirmCreateButton->setEnabled(false);
    m_confirmCreateButton->setText("Creating...");
    
    m_client->createLiveBroadcast(title, m_privacyCombo->currentText());
}

void OneSevenLiveYouTubeBroadcastDialog::onBroadcastCreated(const QString& broadcastId) {
    if (!m_isCreating) return;
    
    m_pendingBroadcastId = broadcastId;
    // Now we need a stream. Check if we have any streams.
    m_client->getMyLiveStreams();
}

void OneSevenLiveYouTubeBroadcastDialog::onStreamsReceived(const YouTubeLiveStreamListResponse& response) {
    m_availableStreams = response.items;
    
    if (m_isCreating) {
        // We are in creation flow.
        // Try to find a reusable stream or one with matching title?
        // For simplicity, let's create a NEW stream for this broadcast to ensure unique key/settings.
        // OR use the Default Stream if it exists.
        
        bool found = false;
        QString streamId;
        
        for (const auto& s : m_availableStreams) {
            if (s.snippet.isDefaultStream) {
                streamId = s.id;
                // Save ingestion info
                m_ingestionUrl = s.cdn.ingestionInfo.ingestionAddress;
                m_streamKey = s.cdn.ingestionInfo.streamName;
                found = true;
                break;
            }
        }
        
        if (found) {
            // Bind to default stream
            m_client->bindLiveBroadcast(m_pendingBroadcastId, streamId);
        } else {
            // Create a new stream
            m_client->createLiveStream(m_selectedTitle);
        }
    } else {
        // We are in selection flow (processSelection)
        // We need to bind the selected broadcast to a stream.
        // Reuse logic: find default or first available?
        
        if (m_availableStreams.isEmpty()) {
            // Create one
             m_client->createLiveStream("Default Stream");
             return;
        }
        
        // Pick first one
        auto s = m_availableStreams.first();
        m_ingestionUrl = s.cdn.ingestionInfo.ingestionAddress;
        m_streamKey = s.cdn.ingestionInfo.streamName;
        
        m_client->bindLiveBroadcast(m_selectedBroadcastId, s.id);
    }
}

void OneSevenLiveYouTubeBroadcastDialog::onStreamCreated(const YouTubeLiveStream& stream) {
    // Save ingestion info
    m_ingestionUrl = stream.cdn.ingestionInfo.ingestionAddress;
    m_streamKey = stream.cdn.ingestionInfo.streamName;
    
    if (m_isCreating) {
        m_client->bindLiveBroadcast(m_pendingBroadcastId, stream.id);
    } else {
        m_client->bindLiveBroadcast(m_selectedBroadcastId, stream.id);
    }
}

void OneSevenLiveYouTubeBroadcastDialog::onBroadcastBound(const QString& broadcastId, const QString& streamId) {
    UNUSED_PARAMETER(streamId);
    if (m_isCreating) {
        // We need to fetch the broadcast again to get the Live Chat ID?
        // Or did create return it? 
        // createLiveBroadcast only returns ID in the signal (based on current impl).
        // We need to fetch it to get chat ID.
        m_client->getLiveBroadcastById(broadcastId);
        
        // Actually, we can just finish here and let the caller fetch details if needed, 
        // but we promised to return liveChatId.
        // So let's fetch.
    } else {
        // Selection flow finished binding.
        accept();
    }
}

void OneSevenLiveYouTubeBroadcastDialog::processSelection(const QString& broadcastId, const QString& title, const QString& chatId) {
    m_selectedBroadcastId = broadcastId;
    m_selectedTitle = title;
    m_selectedLiveChatId = chatId;
    m_isCreating = false;
    
    // We need to ensure it's bound and get stream key.
    // Fetch streams to find a candidate to bind.
    m_client->getMyLiveStreams();
}

void OneSevenLiveYouTubeBroadcastDialog::onError(const QString& error, const QString& operation) {
    QMessageBox::critical(this, "Error", QString("Operation %1 failed: %2").arg(operation).arg(error));
    if (m_isCreating) {
        m_confirmCreateButton->setEnabled(true);
        m_confirmCreateButton->setText("Create & Start");
        m_isCreating = false;
    }
}

void OneSevenLiveYouTubeBroadcastDialog::bindAndFinish(const QString& broadcastId, const QString& streamId) {
    UNUSED_PARAMETER(broadcastId);
    UNUSED_PARAMETER(streamId);
    
    // This logic is moved to onBroadcastBound and onStreamsReceived
}

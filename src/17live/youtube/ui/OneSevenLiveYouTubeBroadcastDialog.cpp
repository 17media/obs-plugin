#include "OneSevenLiveYouTubeBroadcastDialog.hpp"
#include "../../OneSevenLiveCoreManager.hpp"
#include <QMessageBox>
#include <QHeaderView>
#include <obs-module.h>
#include "plugin-support.h"

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

    formLayout->addWidget(new QLabel("Latency:"));
    m_latencyCombo = new QComboBox();
    m_latencyCombo->addItem("Normal", "normal");
    m_latencyCombo->addItem("Low", "low");
    m_latencyCombo->addItem("Ultra Low", "ultraLow");
    formLayout->addWidget(m_latencyCombo);

    m_autoStartCheck = new QCheckBox("Auto-start");
    m_autoStartCheck->setToolTip("Automatically start broadcast when stream data is received");
    m_autoStartCheck->setChecked(true);
    formLayout->addWidget(m_autoStartCheck);

    m_autoStopCheck = new QCheckBox("Auto-stop");
    m_autoStopCheck->setToolTip("Automatically stop broadcast when stream stops");
    m_autoStopCheck->setChecked(true);
    formLayout->addWidget(m_autoStopCheck);

    m_dvrCheck = new QCheckBox("Enable DVR");
    m_dvrCheck->setChecked(true);
    formLayout->addWidget(m_dvrCheck);

    m_scheduleCheck = new QCheckBox("Schedule for later");
    formLayout->addWidget(m_scheduleCheck);
    
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
        connect(m_client, &OneSevenLiveYouTubeClient::liveBroadcastReceived, this, &OneSevenLiveYouTubeBroadcastDialog::onSingleBroadcastReceived);
        connect(m_client, &OneSevenLiveYouTubeClient::liveBroadcastTransitioned, this, &OneSevenLiveYouTubeBroadcastDialog::onBroadcastTransitioned);
        connect(m_client, &OneSevenLiveYouTubeClient::errorOccurred, this, &OneSevenLiveYouTubeBroadcastDialog::onError);
        connect(m_client, &OneSevenLiveYouTubeClient::requestCompleted, this,
                [this](const QString& op) {
                    obs_log(LOG_INFO, "[YouTube-Dialog] requestCompleted op=%s",
                            op.toUtf8().constData());
                });
    }
}

void OneSevenLiveYouTubeBroadcastDialog::loadBroadcasts() {
    if (!m_client) return;
    obs_log(LOG_INFO, "[YouTube-Dialog] loadBroadcasts hasClient=%s hasAuth=%s",
            m_client ? "true" : "false",
            (m_client && m_client->hasValidAuth()) ? "true" : "false");
    m_broadcastList->clear();
    m_broadcastList->addItem("Loading...");
    
    // Load both active and upcoming broadcasts
    m_loadingStatuses.clear();
    m_loadingStatuses << "active" << "upcoming";
    fetchNextBroadcastBatch();
}

void OneSevenLiveYouTubeBroadcastDialog::fetchNextBroadcastBatch() {
    if (m_loadingStatuses.isEmpty()) {
        obs_log(LOG_INFO, "[YouTube-Dialog] All broadcast batches loaded");
        if (m_broadcastList->count() == 0) {
            m_broadcastList->addItem("No broadcasts found");
        } else if (m_broadcastList->count() > 0 && m_broadcastList->item(0)->text() == "Loading...") {
            // Remove loading item if it's the only one left and we failed to find anything
            delete m_broadcastList->takeItem(0);
        }
        return;
    }

    m_currentLoadingStatus = m_loadingStatuses.first();
    obs_log(LOG_INFO, "[YouTube-Dialog] Fetching broadcasts status=%s", m_currentLoadingStatus.toUtf8().constData());
    m_client->getMyLiveBroadcasts(m_currentLoadingStatus);
}

void OneSevenLiveYouTubeBroadcastDialog::onRefreshClicked() {
    loadBroadcasts();
}

void OneSevenLiveYouTubeBroadcastDialog::onCreateNewClicked() {
    obs_log(LOG_INFO, "[YouTube-Dialog] onCreateNewClicked()");
    m_titleEdit->clear();
    m_stackedWidget->setCurrentWidget(m_createPage);
}

void OneSevenLiveYouTubeBroadcastDialog::onBackClicked() {
    m_stackedWidget->setCurrentWidget(m_listPage);
}

void OneSevenLiveYouTubeBroadcastDialog::onBroadcastsReceived(const YouTubeLiveBroadcastListResponse& response) {
    obs_log(LOG_INFO, "[YouTube-Dialog] onBroadcastsReceived items=%d status=%s", 
            response.items.size(), m_currentLoadingStatus.toUtf8().constData());
    
    // Clear "Loading..." item if it exists
    if (m_broadcastList->count() > 0 && m_broadcastList->item(0)->text() == "Loading...") {
        delete m_broadcastList->takeItem(0);
    }

    if (response.items.isEmpty()) {
        obs_log(LOG_WARNING, "[YouTube-Dialog] No broadcasts returned for status=%s", m_currentLoadingStatus.toUtf8().constData());
    }

    for (const auto& broadcast : response.items) {
        // ... (logging)
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
    
    // Handle pagination
    if (!response.nextPageToken.isEmpty()) {
        obs_log(LOG_INFO, "[YouTube-Dialog] Fetching next page for status=%s", m_currentLoadingStatus.toUtf8().constData());
        m_client->getMyLiveBroadcasts(m_currentLoadingStatus, response.nextPageToken);
    } else {
        // Finished current status, move to next
        if (!m_loadingStatuses.isEmpty()) {
            m_loadingStatuses.removeFirst();
        }
        fetchNextBroadcastBatch();
    }
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
    obs_log(LOG_INFO, "[YouTube-Dialog] onCreateConfirmClicked()");
    QString title = m_titleEdit->text();
    if (title.isEmpty()) {
        obs_log(LOG_WARNING, "[YouTube-Dialog] Empty title; abort create");
        QMessageBox::warning(this, "Error", "Title cannot be empty");
        return;
    }
    
    m_isCreating = true;
    m_selectedTitle = title;
    m_autoStartEnabled = m_autoStartCheck->isChecked();
    m_confirmCreateButton->setEnabled(false);
    m_confirmCreateButton->setText("Creating...");
    
    QString privacy = m_privacyCombo->currentText();
    QString latency = m_latencyCombo->currentData().toString();
    bool autoStart = m_autoStartCheck->isChecked();
    bool autoStop = m_autoStopCheck->isChecked();
    bool dvr = m_dvrCheck->isChecked();
    bool scheduleLater = m_scheduleCheck->isChecked();

    obs_log(LOG_INFO, "[YouTube-Dialog] Request createLiveBroadcast title=%s privacy=%s latency=%s autoStart=%d",
            title.toUtf8().constData(), privacy.toUtf8().constData(), latency.toUtf8().constData(), autoStart);
    m_client->createLiveBroadcast(title, privacy, latency, autoStart, autoStop, dvr, scheduleLater);
}

void OneSevenLiveYouTubeBroadcastDialog::onBroadcastCreated(const QString& broadcastId) {
    obs_log(LOG_INFO, "[YouTube-Dialog] onBroadcastCreated id=%s",
            broadcastId.toUtf8().constData());
    if (!m_isCreating)
        return;

    if (broadcastId.isEmpty()) {
        obs_log(LOG_ERROR,
                "[YouTube-Dialog] onBroadcastCreated received empty id; aborting creation flow");
        m_isCreating = false;
        if (m_confirmCreateButton) {
            m_confirmCreateButton->setEnabled(true);
            m_confirmCreateButton->setText("Create & Start");
        }
        QMessageBox::critical(this, "Error",
                              "Failed to create YouTube broadcast (no id returned)");
        return;
    }

    m_pendingBroadcastId = broadcastId;
    // Now we need a stream. Check if we have any streams.
    obs_log(LOG_INFO, "[YouTube-Dialog] Fetch my live streams after create");
    m_client->getMyLiveStreams();
}

void OneSevenLiveYouTubeBroadcastDialog::onStreamsReceived(const YouTubeLiveStreamListResponse& response) {
    obs_log(LOG_INFO, "[YouTube-Dialog] onStreamsReceived count=%d isCreating=%s",
            response.items.size(), m_isCreating ? "true" : "false");
    m_availableStreams = response.items;
    
    if (m_isCreating) {
        // We are in creation flow.
        // Try to find a reusable stream or one with matching title?
        // For simplicity, let's create a NEW stream for this broadcast to ensure unique key/settings.
        // OR use the Default Stream if it exists.
        
        bool found = false;
        QString streamId;
        
        for (const auto& s : m_availableStreams) {
            obs_log(LOG_INFO, "[YouTube-Dialog] stream id=%s title=%s isDefault=%s",
                    s.id.toUtf8().constData(), s.snippet.title.toUtf8().constData(),
                    s.snippet.isDefaultStream ? "true" : "false");
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
            obs_log(LOG_INFO, "[YouTube-Dialog] Bind broadcast=%s to default stream=%s",
                    m_pendingBroadcastId.toUtf8().constData(), streamId.toUtf8().constData());
            m_client->bindLiveBroadcast(m_pendingBroadcastId, streamId);
        } else {
            // Create a new stream
            obs_log(LOG_INFO, "[YouTube-Dialog] No default stream found; createLiveStream title=%s",
                    m_selectedTitle.toUtf8().constData());
            m_client->createLiveStream(m_selectedTitle);
        }
    } else {
        // We are in selection flow (processSelection)
        // We need to bind the selected broadcast to a stream.
        // Reuse logic: find default or first available?
        
        if (m_availableStreams.isEmpty()) {
            // Create one
             obs_log(LOG_INFO, "[YouTube-Dialog] No streams available; create default stream for selection flow");
             m_client->createLiveStream("Default Stream");
             return;
        }
        
        // Pick first one
        auto s = m_availableStreams.first();
        m_ingestionUrl = s.cdn.ingestionInfo.ingestionAddress;
        m_streamKey = s.cdn.ingestionInfo.streamName;
        
        obs_log(LOG_INFO, "[YouTube-Dialog] Bind selected broadcast=%s to stream=%s",
                m_selectedBroadcastId.toUtf8().constData(), s.id.toUtf8().constData());
        m_client->bindLiveBroadcast(m_selectedBroadcastId, s.id);
    }
}

void OneSevenLiveYouTubeBroadcastDialog::onStreamCreated(const YouTubeLiveStream& stream) {
    obs_log(LOG_INFO, "[YouTube-Dialog] onStreamCreated id=%s title=%s",
            stream.id.toUtf8().constData(), stream.snippet.title.toUtf8().constData());

    if (stream.id.isEmpty()) {
        obs_log(LOG_ERROR,
                "[YouTube-Dialog] onStreamCreated received stream with empty id; aborting bind");
        if (m_isCreating) {
            m_isCreating = false;
            if (m_confirmCreateButton) {
                m_confirmCreateButton->setEnabled(true);
                m_confirmCreateButton->setText("Create & Start");
            }
            QMessageBox::critical(this, "Error",
                                  "Failed to create YouTube stream (no id returned)");
        }
        return;
    }

    if (m_isCreating && m_pendingBroadcastId.isEmpty()) {
        obs_log(LOG_ERROR,
                "[YouTube-Dialog] onStreamCreated but pending broadcast id is empty; aborting bind");
        m_isCreating = false;
        if (m_confirmCreateButton) {
            m_confirmCreateButton->setEnabled(true);
            m_confirmCreateButton->setText("Create & Start");
        }
        QMessageBox::critical(this, "Error",
                              "YouTube broadcast id is empty; cannot bind stream.");
        return;
    }

    // Save ingestion info
    m_ingestionUrl = stream.cdn.ingestionInfo.ingestionAddress;
    m_streamKey = stream.cdn.ingestionInfo.streamName;
    
    if (m_isCreating) {
        obs_log(LOG_INFO, "[YouTube-Dialog] Bind newly created stream=%s to broadcast=%s",
                stream.id.toUtf8().constData(), m_pendingBroadcastId.toUtf8().constData());
        m_client->bindLiveBroadcast(m_pendingBroadcastId, stream.id);
    } else {
        obs_log(LOG_INFO, "[YouTube-Dialog] Bind newly created stream=%s to selected broadcast=%s",
                stream.id.toUtf8().constData(), m_selectedBroadcastId.toUtf8().constData());
        m_client->bindLiveBroadcast(m_selectedBroadcastId, stream.id);
    }
}

void OneSevenLiveYouTubeBroadcastDialog::onBroadcastBound(const QString& broadcastId, const QString& streamId) {
    obs_log(LOG_INFO, "[YouTube-Dialog] onBroadcastBound broadcast=%s stream=%s isCreating=%s",
            broadcastId.toUtf8().constData(), streamId.toUtf8().constData(), m_isCreating ? "true" : "false");
    UNUSED_PARAMETER(streamId);
    if (m_isCreating) {
        obs_log(LOG_INFO, "[YouTube-Dialog] Fetch broadcast details after bind to get liveChatId");
        m_client->getLiveBroadcastById(broadcastId);
    } else {
        // Selection flow finished binding.
        obs_log(LOG_INFO, "[YouTube-Dialog] Selection flow bound; closing dialog");
        accept();
    }
}

void OneSevenLiveYouTubeBroadcastDialog::onSingleBroadcastReceived(const YouTubeLiveBroadcast& broadcast) {
    obs_log(LOG_INFO, "[YouTube-Dialog] onSingleBroadcastReceived id=%s chatId=%s status=%s",
            broadcast.id.toUtf8().constData(), broadcast.snippet.liveChatId.toUtf8().constData(),
            broadcast.status.lifeCycleStatus.toUtf8().constData());
    if (!m_isCreating) {
        obs_log(LOG_DEBUG, "[YouTube-Dialog] Ignore single broadcast because not creating");
        return;
    }
    m_selectedBroadcastId = broadcast.id;
    m_selectedLiveChatId = broadcast.snippet.liveChatId;
    m_isCreating = false;
    m_confirmCreateButton->setEnabled(true);
    m_confirmCreateButton->setText("Create & Start");
    
    obs_log(LOG_INFO, "[YouTube-Dialog] Creation flow finished; closing dialog. AutoStart=%d", m_autoStartEnabled);
    accept();
}

void OneSevenLiveYouTubeBroadcastDialog::onBroadcastTransitioned(const QString& broadcastId,
                                                                 const QString& status) {
    obs_log(LOG_INFO, "[YouTube-Dialog] onBroadcastTransitioned id=%s status=%s",
            broadcastId.toUtf8().constData(), status.toUtf8().constData());
}

void OneSevenLiveYouTubeBroadcastDialog::processSelection(const QString& broadcastId, const QString& title, const QString& chatId) {
    obs_log(LOG_INFO, "[YouTube-Dialog] processSelection broadcast=%s title=%s chatId=%s",
            broadcastId.toUtf8().constData(), title.toUtf8().constData(), chatId.toUtf8().constData());
    m_selectedBroadcastId = broadcastId;
    m_selectedTitle = title;
    m_selectedLiveChatId = chatId;
    m_isCreating = false;
    
    // We need to ensure it's bound and get stream key.
    // Fetch streams to find a candidate to bind.
    obs_log(LOG_INFO, "[YouTube-Dialog] Fetch my live streams for selection flow");
    m_client->getMyLiveStreams();
}

void OneSevenLiveYouTubeBroadcastDialog::onError(const QString& error, const QString& operation) {
    obs_log(LOG_ERROR, "[YouTube-Dialog] onError op=%s error=%s isCreating=%s",
            operation.toUtf8().constData(), error.toUtf8().constData(),
            m_isCreating ? "true" : "false");
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

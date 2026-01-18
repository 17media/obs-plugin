#include "OneSevenLiveYouTubeBroadcastDialog.hpp"

#include <obs-module.h>

#include <QHeaderView>
#include <QMessageBox>

#include "../../OneSevenLiveCoreManager.hpp"
#include "plugin-support.h"

OneSevenLiveYouTubeBroadcastDialog::OneSevenLiveYouTubeBroadcastDialog(QWidget* parent)
    : QDialog(parent), m_isCreating(false), m_selectionWithBoundStream(false) {
    setWindowTitle(obs_module_text("YouTube.Broadcast.Manager.Title"));
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
    m_refreshButton = new QPushButton(obs_module_text("YouTube.Broadcast.Refresh"));
    m_createButton = new QPushButton(obs_module_text("YouTube.Broadcast.CreateNew"));
    m_selectButton = new QPushButton(obs_module_text("YouTube.Broadcast.SelectAndStart"));
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
    formLayout->addWidget(new QLabel(obs_module_text("YouTube.Broadcast.TitleLabel")));
    m_titleEdit = new QLineEdit();
    formLayout->addWidget(m_titleEdit);

    formLayout->addWidget(new QLabel(obs_module_text("YouTube.Broadcast.PrivacyLabel")));
    m_privacyCombo = new QComboBox();
    m_privacyCombo->addItem(obs_module_text("YouTube.Broadcast.Privacy.Public"), "public");
    m_privacyCombo->addItem(obs_module_text("YouTube.Broadcast.Privacy.Unlisted"), "unlisted");
    m_privacyCombo->addItem(obs_module_text("YouTube.Broadcast.Privacy.Private"), "private");
    formLayout->addWidget(m_privacyCombo);

    formLayout->addWidget(new QLabel(obs_module_text("YouTube.Broadcast.LatencyLabel")));
    m_latencyCombo = new QComboBox();
    m_latencyCombo->addItem(obs_module_text("YouTube.Broadcast.Latency.Normal"), "normal");
    m_latencyCombo->addItem(obs_module_text("YouTube.Broadcast.Latency.Low"), "low");
    m_latencyCombo->addItem(obs_module_text("YouTube.Broadcast.Latency.UltraLow"), "ultraLow");
    formLayout->addWidget(m_latencyCombo);

    m_autoStartCheck = new QCheckBox(obs_module_text("YouTube.Broadcast.AutoStart"));
    m_autoStartCheck->setToolTip(obs_module_text("YouTube.Broadcast.AutoStart.Tooltip"));
    m_autoStartCheck->setChecked(true);
    formLayout->addWidget(m_autoStartCheck);

    m_autoStopCheck = new QCheckBox(obs_module_text("YouTube.Broadcast.AutoStop"));
    m_autoStopCheck->setToolTip(obs_module_text("YouTube.Broadcast.AutoStop.Tooltip"));
    m_autoStopCheck->setChecked(true);
    formLayout->addWidget(m_autoStopCheck);

    m_dvrCheck = new QCheckBox(obs_module_text("YouTube.Broadcast.EnableDVR"));
    m_dvrCheck->setChecked(true);
    formLayout->addWidget(m_dvrCheck);

    m_scheduleCheck = new QCheckBox(obs_module_text("YouTube.Broadcast.ScheduleLater"));
    formLayout->addWidget(m_scheduleCheck);

    createLayout->addLayout(formLayout);
    createLayout->addStretch();

    auto createBtnLayout = new QHBoxLayout();
    m_backButton = new QPushButton(obs_module_text("YouTube.Broadcast.Back"));
    m_confirmCreateButton = new QPushButton(obs_module_text("YouTube.Broadcast.CreateAndStart"));

    createBtnLayout->addWidget(m_backButton);
    createBtnLayout->addStretch();
    createBtnLayout->addWidget(m_confirmCreateButton);
    createLayout->addLayout(createBtnLayout);

    m_stackedWidget->addWidget(m_createPage);
}

void OneSevenLiveYouTubeBroadcastDialog::setupConnections() {
    connect(m_refreshButton, &QPushButton::clicked, this,
            &OneSevenLiveYouTubeBroadcastDialog::onRefreshClicked);
    connect(m_createButton, &QPushButton::clicked, this,
            &OneSevenLiveYouTubeBroadcastDialog::onCreateNewClicked);
    connect(m_selectButton, &QPushButton::clicked, this,
            &OneSevenLiveYouTubeBroadcastDialog::onSelectClicked);
    connect(m_backButton, &QPushButton::clicked, this,
            &OneSevenLiveYouTubeBroadcastDialog::onBackClicked);
    connect(m_confirmCreateButton, &QPushButton::clicked, this,
            &OneSevenLiveYouTubeBroadcastDialog::onCreateConfirmClicked);

    connect(m_broadcastList, &QListWidget::itemSelectionChanged,
            [this]() { m_selectButton->setEnabled(!m_broadcastList->selectedItems().isEmpty()); });

    if (m_client) {
        connect(m_client, &OneSevenLiveYouTubeClient::myLiveBroadcastsReceived, this,
                &OneSevenLiveYouTubeBroadcastDialog::onBroadcastsReceived);
        connect(m_client, &OneSevenLiveYouTubeClient::myLiveStreamsReceived, this,
                &OneSevenLiveYouTubeBroadcastDialog::onStreamsReceived);
        connect(m_client, &OneSevenLiveYouTubeClient::liveBroadcastCreated, this,
                &OneSevenLiveYouTubeBroadcastDialog::onBroadcastCreated);
        connect(m_client, &OneSevenLiveYouTubeClient::liveStreamCreated, this,
                &OneSevenLiveYouTubeBroadcastDialog::onStreamCreated);
        connect(m_client, &OneSevenLiveYouTubeClient::liveBroadcastBound, this,
                &OneSevenLiveYouTubeBroadcastDialog::onBroadcastBound);
        connect(m_client, &OneSevenLiveYouTubeClient::liveBroadcastReceived, this,
                &OneSevenLiveYouTubeBroadcastDialog::onSingleBroadcastReceived);
        connect(m_client, &OneSevenLiveYouTubeClient::liveBroadcastTransitioned, this,
                &OneSevenLiveYouTubeBroadcastDialog::onBroadcastTransitioned);
        connect(m_client, &OneSevenLiveYouTubeClient::errorOccurred, this,
                &OneSevenLiveYouTubeBroadcastDialog::onError);
        connect(m_client, &OneSevenLiveYouTubeClient::requestCompleted, this,
                [this](const QString& op) {
                    obs_log(LOG_INFO, "[YouTube-Dialog] requestCompleted op=%s",
                            op.toUtf8().constData());
                });
    }
}

void OneSevenLiveYouTubeBroadcastDialog::loadBroadcasts() {
    if (!m_client)
        return;
    obs_log(LOG_INFO, "[YouTube-Dialog] loadBroadcasts hasClient=%s hasAuth=%s",
            m_client ? "true" : "false", (m_client && m_client->hasValidAuth()) ? "true" : "false");
    m_broadcastList->clear();
    m_broadcastList->addItem(obs_module_text("YouTube.Broadcast.Loading"));

    // Load both active and upcoming broadcasts
    m_loadingStatuses.clear();
    m_loadingStatuses << "active" << "upcoming";
    fetchNextBroadcastBatch();
}

void OneSevenLiveYouTubeBroadcastDialog::fetchNextBroadcastBatch() {
    if (m_loadingStatuses.isEmpty()) {
        obs_log(LOG_INFO, "[YouTube-Dialog] All broadcast batches loaded");
        if (m_broadcastList->count() == 0) {
            m_broadcastList->addItem(obs_module_text("YouTube.Broadcast.NoBroadcastsFound"));
        } else if (m_broadcastList->count() > 0 &&
                   m_broadcastList->item(0)->text() ==
                       obs_module_text("YouTube.Broadcast.Loading")) {
            // Remove loading item if it's the only one left and we failed to find anything
            delete m_broadcastList->takeItem(0);
        }
        return;
    }

    m_currentLoadingStatus = m_loadingStatuses.first();
    obs_log(LOG_INFO, "[YouTube-Dialog] Fetching broadcasts status=%s",
            m_currentLoadingStatus.toUtf8().constData());
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

void OneSevenLiveYouTubeBroadcastDialog::onBroadcastsReceived(
    const YouTubeLiveBroadcastListResponse& response) {
    obs_log(LOG_INFO, "[YouTube-Dialog] onBroadcastsReceived items=%d status=%s",
            response.items.size(), m_currentLoadingStatus.toUtf8().constData());

    // Clear "Loading..." item if it exists
    if (m_broadcastList->count() > 0 &&
        m_broadcastList->item(0)->text() == obs_module_text("YouTube.Broadcast.Loading")) {
        delete m_broadcastList->takeItem(0);
    }

    if (response.items.isEmpty()) {
        obs_log(LOG_WARNING, "[YouTube-Dialog] No broadcasts returned for status=%s",
                m_currentLoadingStatus.toUtf8().constData());
    }

    for (const auto& broadcast : response.items) {
        // ... (logging)
        // Filter out completed broadcasts as they cannot be reused
        if (broadcast.status.lifeCycleStatus == "complete" ||
            broadcast.status.lifeCycleStatus == "completed") {
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
        item->setData(Qt::UserRole + 3, broadcast.contentDetails.boundStreamId);
        m_broadcastList->addItem(item);
    }

    // Handle pagination
    if (!response.nextPageToken.isEmpty()) {
        obs_log(LOG_INFO, "[YouTube-Dialog] Fetching next page for status=%s",
                m_currentLoadingStatus.toUtf8().constData());
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
    if (items.isEmpty())
        return;

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
        QMessageBox::warning(this, obs_module_text("YouTube.Broadcast.Error.Title"),
                             obs_module_text("YouTube.Broadcast.Error.TitleEmpty"));
        return;
    }

    m_isCreating = true;
    m_selectedTitle = title;
    m_autoStartEnabled = m_autoStartCheck->isChecked();
    m_confirmCreateButton->setEnabled(false);
    m_confirmCreateButton->setText(obs_module_text("YouTube.Broadcast.Creating"));

    QString privacy = m_privacyCombo->currentData().toString();
    QString latency = m_latencyCombo->currentData().toString();
    bool autoStart = m_autoStartCheck->isChecked();
    bool autoStop = m_autoStopCheck->isChecked();
    bool dvr = m_dvrCheck->isChecked();
    bool scheduleLater = m_scheduleCheck->isChecked();

    obs_log(
        LOG_INFO,
        "[YouTube-Dialog] Request createLiveBroadcast title=%s privacy=%s latency=%s autoStart=%d",
        title.toUtf8().constData(), privacy.toUtf8().constData(), latency.toUtf8().constData(),
        autoStart);
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
            m_confirmCreateButton->setText(obs_module_text("YouTube.Broadcast.CreateAndStart"));
        }
        QMessageBox::critical(this, obs_module_text("YouTube.Broadcast.Error.Title"),
                              obs_module_text("YouTube.Broadcast.Error.CreateFailed"));
        return;
    }

    m_pendingBroadcastId = broadcastId;
    // Now we need a stream. Check if we have any streams.
    obs_log(LOG_INFO, "[YouTube-Dialog] Fetch my live streams after create");
    m_client->getMyLiveStreams();
}

void OneSevenLiveYouTubeBroadcastDialog::onStreamsReceived(
    const YouTubeLiveStreamListResponse& response) {
    obs_log(LOG_INFO, "[YouTube-Dialog] onStreamsReceived count=%d isCreating=%s",
            response.items.size(), m_isCreating ? "true" : "false");
    m_availableStreams = response.items;

    if (m_selectionWithBoundStream) {
        bool found = false;
        YouTubeLiveStream target;
        for (const auto& s : m_availableStreams) {
            if (s.id == m_selectedBoundStreamId) {
                target = s;
                found = true;
                break;
            }
        }
        if (found) {
            m_ingestionUrl = target.cdn.ingestionInfo.rtmpsIngestionAddress.isEmpty()
                                 ? target.cdn.ingestionInfo.ingestionAddress
                                 : target.cdn.ingestionInfo.rtmpsIngestionAddress;
            m_streamKey = target.cdn.ingestionInfo.streamName;
            m_client->startBroadcast(m_selectedBroadcastId, m_selectedBoundStreamId);
            auto& core = OneSevenLiveCoreManager::getInstance();
            core.startYouTubeChatPolling(m_selectedLiveChatId);
            obs_log(LOG_INFO,
                    "[YouTube-Dialog] Selection flow reuse bound stream; startBroadcast + start "
                    "chat; closing dialog");
            accept();
            return;
        }
        obs_log(LOG_WARNING,
                "[YouTube-Dialog] Bound stream id not found in received list; fallback to normal "
                "selection");
        m_selectionWithBoundStream = false;
    }

    if (m_isCreating) {
        // We are in creation flow.
        // Try to find a reusable stream or one with matching title?
        // For simplicity, let's create a NEW stream for this broadcast to ensure unique
        // key/settings. OR use the Default Stream if it exists.

        bool found = false;
        QString streamId;
        bool hasTitleDefaultCandidate = false;
        YouTubeLiveStream titleDefaultCandidate;

        for (const auto& s : m_availableStreams) {
            obs_log(LOG_INFO, "[YouTube-Dialog] stream id=%s title=%s isDefault=%s",
                    s.id.toUtf8().constData(), s.snippet.title.toUtf8().constData(),
                    s.snippet.isDefaultStream ? "true" : "false");
            if (s.snippet.isDefaultStream) {
                streamId = s.id;
                m_ingestionUrl = s.cdn.ingestionInfo.rtmpsIngestionAddress.isEmpty()
                                     ? s.cdn.ingestionInfo.ingestionAddress
                                     : s.cdn.ingestionInfo.rtmpsIngestionAddress;
                m_streamKey = s.cdn.ingestionInfo.streamName;
                found = true;
                break;
            }
            if (!hasTitleDefaultCandidate && s.snippet.title == "Default stream key") {
                titleDefaultCandidate = s;
                hasTitleDefaultCandidate = true;
            }
        }

        if (!found && hasTitleDefaultCandidate) {
            const auto& s = titleDefaultCandidate;
            streamId = s.id;
            m_ingestionUrl = s.cdn.ingestionInfo.rtmpsIngestionAddress.isEmpty()
                                 ? s.cdn.ingestionInfo.ingestionAddress
                                 : s.cdn.ingestionInfo.rtmpsIngestionAddress;
            m_streamKey = s.cdn.ingestionInfo.streamName;
            found = true;
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
            obs_log(
                LOG_INFO,
                "[YouTube-Dialog] No streams available; create default stream for selection flow");
            m_client->createLiveStream(obs_module_text("YouTube.Broadcast.DefaultStreamTitle"));
            return;
        }

        // Prefer default stream if available
        YouTubeLiveStream targetStream = m_availableStreams.first();
        bool hasTitleDefaultCandidateSel = false;
        YouTubeLiveStream titleDefaultCandidateSel;
        for (const auto& s : m_availableStreams) {
            if (s.snippet.isDefaultStream) {
                targetStream = s;
                obs_log(LOG_INFO, "[YouTube-Dialog] Found default stream=%s",
                        s.id.toUtf8().constData());
                break;
            }
            if (!hasTitleDefaultCandidateSel && s.snippet.title == "Default stream key") {
                titleDefaultCandidateSel = s;
                hasTitleDefaultCandidateSel = true;
            }
        }
        if (!targetStream.snippet.isDefaultStream && hasTitleDefaultCandidateSel) {
            targetStream = titleDefaultCandidateSel;
            obs_log(LOG_INFO, "[YouTube-Dialog] Fallback to title Default stream key stream=%s",
                    targetStream.id.toUtf8().constData());
        }

        m_ingestionUrl = targetStream.cdn.ingestionInfo.rtmpsIngestionAddress.isEmpty()
                             ? targetStream.cdn.ingestionInfo.ingestionAddress
                             : targetStream.cdn.ingestionInfo.rtmpsIngestionAddress;
        m_streamKey = targetStream.cdn.ingestionInfo.streamName;

        obs_log(LOG_INFO, "[YouTube-Dialog] Bind selected broadcast=%s to stream=%s (key=%s)",
                m_selectedBroadcastId.toUtf8().constData(), targetStream.id.toUtf8().constData(),
                m_streamKey.toUtf8().constData());
        m_client->bindLiveBroadcast(m_selectedBroadcastId, targetStream.id);
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
                m_confirmCreateButton->setText(obs_module_text("YouTube.Broadcast.CreateAndStart"));
            }
            QMessageBox::critical(this, obs_module_text("YouTube.Broadcast.Error.Title"),
                                  obs_module_text("YouTube.Broadcast.Error.StreamCreateFailed"));
        }
        return;
    }

    if (m_isCreating && m_pendingBroadcastId.isEmpty()) {
        obs_log(
            LOG_ERROR,
            "[YouTube-Dialog] onStreamCreated but pending broadcast id is empty; aborting bind");
        m_isCreating = false;
        if (m_confirmCreateButton) {
            m_confirmCreateButton->setEnabled(true);
            m_confirmCreateButton->setText(obs_module_text("YouTube.Broadcast.CreateAndStart"));
        }
        QMessageBox::critical(this, obs_module_text("YouTube.Broadcast.Error.Title"),
                              obs_module_text("YouTube.Broadcast.Error.BroadcastIdEmpty"));
        return;
    }

    m_ingestionUrl = stream.cdn.ingestionInfo.rtmpsIngestionAddress.isEmpty()
                         ? stream.cdn.ingestionInfo.ingestionAddress
                         : stream.cdn.ingestionInfo.rtmpsIngestionAddress;
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

void OneSevenLiveYouTubeBroadcastDialog::onBroadcastBound(const QString& broadcastId,
                                                          const QString& streamId) {
    obs_log(LOG_INFO, "[YouTube-Dialog] onBroadcastBound broadcast=%s stream=%s isCreating=%s",
            broadcastId.toUtf8().constData(), streamId.toUtf8().constData(),
            m_isCreating ? "true" : "false");
    if (m_isCreating) {
        m_client->startBroadcast(broadcastId, streamId);
        obs_log(LOG_INFO, "[YouTube-Dialog] Fetch broadcast details after bind to get liveChatId");
        m_client->getLiveBroadcastById(broadcastId);
    } else {
        m_client->startBroadcast(broadcastId, streamId);
        auto& core = OneSevenLiveCoreManager::getInstance();
        core.startYouTubeChatPolling(m_selectedLiveChatId);
        obs_log(
            LOG_INFO,
            "[YouTube-Dialog] Selection flow bound; startBroadcast + start chat; closing dialog");
        accept();
    }
}

void OneSevenLiveYouTubeBroadcastDialog::onSingleBroadcastReceived(
    const YouTubeLiveBroadcast& broadcast) {
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
    m_confirmCreateButton->setText(obs_module_text("YouTube.Broadcast.CreateAndStart"));
    auto& core = OneSevenLiveCoreManager::getInstance();
    core.startYouTubeChatPolling(m_selectedLiveChatId);

    obs_log(LOG_INFO, "[YouTube-Dialog] Creation flow finished; closing dialog. AutoStart=%d",
            m_autoStartEnabled);
    accept();
}

void OneSevenLiveYouTubeBroadcastDialog::onBroadcastTransitioned(const QString& broadcastId,
                                                                 const QString& status) {
    obs_log(LOG_INFO, "[YouTube-Dialog] onBroadcastTransitioned id=%s status=%s",
            broadcastId.toUtf8().constData(), status.toUtf8().constData());
}

void OneSevenLiveYouTubeBroadcastDialog::processSelection(const QString& broadcastId,
                                                          const QString& title,
                                                          const QString& chatId) {
    obs_log(LOG_INFO, "[YouTube-Dialog] processSelection broadcast=%s title=%s chatId=%s",
            broadcastId.toUtf8().constData(), title.toUtf8().constData(),
            chatId.toUtf8().constData());
    m_selectedBroadcastId = broadcastId;
    m_selectedTitle = title;
    m_selectedLiveChatId = chatId;
    m_isCreating = false;

    auto items = m_broadcastList->selectedItems();
    QString boundId;
    if (!items.isEmpty()) {
        boundId = items.first()->data(Qt::UserRole + 3).toString();
    }
    if (!boundId.isEmpty()) {
        m_selectionWithBoundStream = true;
        m_selectedBoundStreamId = boundId;
        obs_log(LOG_INFO,
                "[YouTube-Dialog] Broadcast already bound to stream=%s; reuse ingestion info",
                boundId.toUtf8().constData());
        m_client->getLiveStreamById(boundId);
        return;
    }
    obs_log(LOG_INFO, "[YouTube-Dialog] Fetch my live streams for selection flow");
    m_client->getMyLiveStreams();
}

void OneSevenLiveYouTubeBroadcastDialog::onError(const QString& error, const QString& operation) {
    obs_log(LOG_ERROR, "[YouTube-Dialog] onError op=%s error=%s isCreating=%s",
            operation.toUtf8().constData(), error.toUtf8().constData(),
            m_isCreating ? "true" : "false");

    // Handle stream conflict (stream already bound to another broadcast)
    if (operation == "bindLiveBroadcast" && !m_isCreating) {
        // Simple heuristic: if error message contains "bound" or "conflict" or specific error
        // message The user reported: "直播间当前直播码已被占用" which likely comes from YouTube API
        // error message We can try to automatically create a NEW stream and bind to it
        obs_log(LOG_WARNING,
                "[YouTube-Dialog] Stream binding conflict detected; attempting to create new "
                "stream as fallback");

        m_isCreating = true;  // Switch to "creating" mode to handle the flow
        m_pendingBroadcastId = m_selectedBroadcastId;  // We are binding for the selected broadcast

        // Use broadcast title for new stream title
        QString streamTitle = m_selectedTitle;
        if (streamTitle.isEmpty())
            streamTitle = obs_module_text("YouTube.Broadcast.FallbackStreamTitle");

        m_client->createLiveStream(streamTitle);
        return;
    }

    QMessageBox::critical(this, obs_module_text("YouTube.Broadcast.Error.Title"),
                          QString(obs_module_text("YouTube.Broadcast.Error.OperationFailed"))
                              .arg(operation)
                              .arg(error));
    if (m_isCreating) {
        m_confirmCreateButton->setEnabled(true);
        m_confirmCreateButton->setText(obs_module_text("YouTube.Broadcast.CreateAndStart"));
        m_isCreating = false;
    }
}

void OneSevenLiveYouTubeBroadcastDialog::bindAndFinish(const QString& broadcastId,
                                                       const QString& streamId) {
    UNUSED_PARAMETER(broadcastId);
    UNUSED_PARAMETER(streamId);

    // This logic is moved to onBroadcastBound and onStreamsReceived
}

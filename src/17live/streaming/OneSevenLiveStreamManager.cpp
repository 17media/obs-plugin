#include "OneSevenLiveStreamManager.hpp"

#include <obs-frontend-api.h>
#include <obs.h>

#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QThread>
#include <QTimer>

#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "api/OneSevenLiveModels.hpp"
#include "moc_OneSevenLiveStreamManager.cpp"
#include "plugin-support.h"
#include "websocket/WebsocketUtils.hpp"
#include "websocket/WsMessage.hpp"

OneSevenLiveStreamManager::OneSevenLiveStreamManager(OneSevenLiveApiWrappers* apiWrapper,
                                                     OneSevenLiveConfigManager* configManager,
                                                     QObject* parent)
    : QObject(parent),
      apiWrapper(apiWrapper),
      configManager(configManager),
      currentStreamingStatus(OneSevenLiveStreamingStatus::NotStarted) {
    // Load current userID
    std::string userID;
    configManager->getConfigValue("UserID", userID);
    currentUserID = userID;

    std::string roomIDStr;
    configManager->getConfigValue("RoomID", roomIDStr);
    currentRoomID = std::stoll(roomIDStr);
    QTimer::singleShot(0, this, [this]() { loadRoomInfo(); });

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(false);
    m_statusTimer->setInterval(10 * 1000);
    connect(m_statusTimer, &QTimer::timeout, this, &OneSevenLiveStreamManager::onStatusTimer);
    m_statusTimer->start();

    obs_log(LOG_INFO, "OneSevenLiveStreamManager initialized");
}

OneSevenLiveStreamManager::~OneSevenLiveStreamManager() {}

bool OneSevenLiveStreamManager::fetchRtmpByProvider(const std::string& provider,
                                                    OneSevenLiveRtmpResponse& response) {
    return apiWrapper && apiWrapper->GetRtmpByProvider(provider, response);
}

QString OneSevenLiveStreamManager::getLastErrorMessage() const {
    return apiWrapper ? apiWrapper->getLastErrorMessage() : QString("Unknown error");
}

bool OneSevenLiveStreamManager::startStreamWithWeb() {
    obs_log(LOG_INFO, "Saving web stream settings");

    if (!roomInfo.rtmpUrls.isEmpty()) {
        QString provider = GetProviderNameByIndex(roomInfo.rtmpUrls[0].provider);
        OneSevenLiveRtmpResponse rtmpResponse;
        if (fetchRtmpByProvider(provider.toStdString(), rtmpResponse)) {
            rtmpResponse.liveStreamID = QString::number(roomInfo.liveStreamID);
            configureStreamingService(rtmpResponse);

            currentLiveStreamID = rtmpResponse.liveStreamID.toStdString();

            OneSevenLiveRtmpRequest request;
            request.userID = QString::fromStdString(currentUserID);
            request.caption = roomInfo.caption;
            request.device = "OBS";

            qint64 selectedEventId = 0;
            for (const auto &evt : roomInfo.eventList) {
                if (evt.type == 2) {
                    selectedEventId = evt.ID;
                    break;
                }
            }
            request.eventID = selectedEventId;

            QStringList tags;
            for (const auto &t : roomInfo.lastUsedHashtags) {
                tags << t.text;
            }
            request.hashtags = tags;

            request.landscape = roomInfo.landscape;
            request.streamerType = roomInfo.streamerType;
            request.subtabID = (roomInfo.subtabs.size() > 0) ? roomInfo.subtabs[0] : QString();
            request.archiveConfig = roomInfo.archiveConfig;

            OneSevenLiveVliverInfo vl;
            vl.vliverModel = configStreamer.lastStreamState.vliverInfo.vliverModel;
            request.vliverInfo = vl;

            OneSevenLiveArmy army{};
            army.enable = false;
            army.requiredArmyRank = 0;
            army.showOnHotPage = false;
            army.armyOnlyPN = false;
            request.armyOnly = army;

            request.enableOBSGroupCall = roomInfo.enableOBSGroupCall;

            currentStreamRequest = request;
            currentStreamResponse = rtmpResponse;
            OneSevenLiveStreamInfo info;
            info.request = request;
            info.categoryName = QString();
            info.createdAt = QDateTime::currentDateTime();
            info.streamUuid = rtmpResponse.streamID;
            currentLiveStreamInfo = info;

        } else {
            obs_log(LOG_ERROR, "Failed to fetch rtmp url for provider %s", provider.toStdString().c_str());
            return false;
        }
    } else {
        obs_log(LOG_ERROR, "Empty rtmpUrl in roomInfo");
        return false;
    }
    
    return true;
}

bool OneSevenLiveStreamManager::createRtmp(const OneSevenLiveRtmpRequest& request) {
    obs_log(LOG_INFO, "Creating live stream");

    OneSevenLiveRtmpRequest modifiedRequest = request;
    modifiedRequest.userID = QString::fromStdString(currentUserID);
    modifiedRequest.streamerType = roomInfo.streamerType;

    // Get room info for streamer type
    // Note: This would need to be passed in or retrieved from somewhere
    // For now, we'll assume it's available through the API wrapper or config manager

    OneSevenLiveRtmpResponse response;
    if (!apiWrapper->CreateRtmp(modifiedRequest, response)) {
        QString errorMsg = apiWrapper->getLastErrorMessage();
        obs_log(LOG_ERROR, "Failed to create stream. UserID: %s, Error: %s, Timestamp: %lld",
                modifiedRequest.userID.toStdString().c_str(),
                errorMsg.isEmpty() ? "Unknown error" : errorMsg.toStdString().c_str(),
                QDateTime::currentMSecsSinceEpoch());
        emit errorOccurred(errorMsg, "createLiveStream");
        return false;
    }

    // Store the current stream ID and user ID
    currentLiveStreamID = response.liveStreamID.toStdString();
    // Store full request/response and snapshot info
    currentStreamRequest = modifiedRequest;
    currentStreamResponse = response;
    OneSevenLiveStreamInfo info;
    info.request = modifiedRequest;
    info.categoryName = QString();
    info.createdAt = QDateTime::currentDateTime();
    info.streamUuid = response.streamID;
    currentLiveStreamInfo = info;

    // Update status
    setCurrentStreamingStatus(OneSevenLiveStreamingStatus::Live);

    // Broadcast Ably chat connected when live is created
    wsBroadcast(QString::fromUtf8(ws::EventAblyChatConnected), nlohmann::json{{"status","connected"}});

    obs_log(LOG_INFO, "Live stream created successfully. LiveStreamID: %s",
            currentLiveStreamID.c_str());
    return true;
}

bool OneSevenLiveStreamManager::startStream() {
    obs_log(LOG_INFO, "Starting streaming");

    // Configure streaming service (RTMP or WHIP)
    configureStreamingService(currentStreamResponse);

    // Start live stream via API
    if (!apiWrapper->StartStream(currentStreamResponse.liveStreamID.toStdString(), currentUserID)) {
        QString errorMsg = apiWrapper->getLastErrorMessage();
        obs_log(LOG_ERROR,
                "Failed to start stream. LiveStreamID: %s, UserID: %s, Error: %s, Timestamp: %lld",
                currentStreamResponse.liveStreamID.toStdString().c_str(), currentUserID.c_str(),
                errorMsg.isEmpty() ? "Unknown error" : errorMsg.toStdString().c_str(),
                QDateTime::currentMSecsSinceEpoch());
        emit errorOccurred(errorMsg, "startStream");
        return false;
    }

    // Enable archive if requested
    if (currentStreamRequest.archiveConfig.autoRecording) {
        enableStreamArchive(currentStreamResponse.liveStreamID.toStdString(), true);
    }

    // Update status
    setCurrentStreamingStatus(OneSevenLiveStreamingStatus::Streaming);

    // Broadcast Ably chat connected when streaming starts
    wsBroadcast(QString::fromUtf8(ws::EventAblyChatConnected), nlohmann::json{{"status","connected"}});

    obs_log(LOG_INFO, "Streaming started successfully");
    return true;
}

bool OneSevenLiveStreamManager::stopStream(bool isAutoClose) {
    obs_log(LOG_INFO, "Stopping streaming");

    // Stop OBS streaming first
    stopOBSStreaming();

    QString endReason = isAutoClose ? "autoClose" : "normalEnd";

    // Send close live stream request
    OneSevenLiveCloseLiveRequest request;
    request.reason = endReason;
    request.userID = QString::fromStdString(currentUserID);

    if (!apiWrapper->StopStream(currentLiveStreamID, request)) {
        QString errorMsg = apiWrapper->getLastErrorMessage();
        obs_log(LOG_ERROR, "Failed to stop stream. LiveStreamID: %s, UserID: %s, Reason: %s",
                currentLiveStreamID.c_str(), currentUserID.c_str(),
                endReason.toStdString().c_str());
        emit errorOccurred(errorMsg, "stopStreaming");
        // Continue with cleanup even if API call fails
    } else {
        obs_log(LOG_INFO,
                "Successfully stopped stream. LiveStreamID: %s, UserID: %s, Reason: %s, "
                "IsAutoClose: %s",
                currentLiveStreamID.c_str(), currentUserID.c_str(), endReason.toStdString().c_str(),
                isAutoClose ? "true" : "false");
    }

    // Clear streaming configuration
    clearStreamingConfiguration();

    // Update status
    setCurrentStreamingStatus(OneSevenLiveStreamingStatus::NotStarted);

    // Broadcast Ably chat break when streaming stops
    wsBroadcast(QString::fromUtf8(ws::EventAblyChatConnected), nlohmann::json{{"status","break"}});

    // Clear current stream info
    currentLiveStreamID.clear();
    currentUserID.clear();
    currentStreamRequest = OneSevenLiveRtmpRequest{};
    currentStreamResponse = OneSevenLiveRtmpResponse{};
    currentLiveStreamInfo = OneSevenLiveStreamInfo{};

    obs_log(LOG_INFO, "Streaming stopped successfully");
    return true;
}

void OneSevenLiveStreamManager::onStatusTimer() {
    if (currentStreamingStatus == OneSevenLiveStreamingStatus::NotStarted) {
        wsBroadcast(QString::fromUtf8(ws::EventAblyChatConnected), nlohmann::json{{"status","break"}});
    }
}

void OneSevenLiveStreamManager::startOBSStreaming() {
    // Start OBS streaming
    obs_frontend_streaming_start();
}

void OneSevenLiveStreamManager::stopOBSStreaming() {
    obs_log(LOG_INFO, "Stopping OBS streaming");

    if (!obs_frontend_streaming_active()) {
        obs_log(LOG_INFO, "OBS streaming is not active");
        return;
    }

    obs_frontend_streaming_stop();
}

bool OneSevenLiveStreamManager::isOBSStreaming() const {
    return obs_frontend_streaming_active();
}

bool OneSevenLiveStreamManager::saveStreamConfiguration(const OneSevenLiveStreamInfo& streamInfo) {
    obs_log(LOG_INFO, "Saving stream configuration");

    if (!configManager->saveLiveConfig(streamInfo)) {
        obs_log(LOG_ERROR, "Failed to save stream info");
        emit errorOccurred("Failed to save stream configuration", "saveStreamConfiguration");
        return false;
    }

    

    emit streamConfigurationSaved();
    obs_log(LOG_INFO, "Stream configuration saved successfully");
    return true;
}

OneSevenLiveStreamingStatus OneSevenLiveStreamManager::getCurrentStreamingStatus() const {
    return currentStreamingStatus;
}

void OneSevenLiveStreamManager::setCurrentStreamingStatus(OneSevenLiveStreamingStatus status) {
    if (currentStreamingStatus != status) {
        currentStreamingStatus = status;
        emit streamStatusChanged(status);
    }
}

std::string OneSevenLiveStreamManager::getCurrentLiveStreamID() const {
    return currentLiveStreamID;
}

std::string OneSevenLiveStreamManager::getCurrentUserID() const {
    return currentUserID;
}

qint64 OneSevenLiveStreamManager::getRoomID() const {
    return configManager->getRoomID();
}

void OneSevenLiveStreamManager::saveStreamingSettings(const std::string& liveStreamID,
                                                      const std::string& streamUrl,
                                                      const std::string& streamKey) {
    obs_log(LOG_INFO, "Saving RTMP streaming settings for stream: %s", liveStreamID.c_str());

    // Get OBS service
    obs_service_t* service = obs_service_create("rtmp_custom", "default_service", NULL, NULL);
    if (!service) {
        obs_log(LOG_ERROR, "Failed to create OBS service");
        return;
    }

    // Set streaming URL and key
    obs_data_t* settings = obs_service_get_settings(service);
    obs_data_set_string(settings, "server", streamUrl.c_str());
    obs_data_set_string(settings, "key", streamKey.c_str());

    // Apply settings
    obs_service_update(service, settings);
    obs_data_release(settings);

    obs_frontend_set_streaming_service(service);
    obs_frontend_save_streaming_service();

    // Release resources
    obs_service_release(service);

    // Store in config manager
    configManager->setStreamingInfo(liveStreamID, streamUrl, streamKey);
    configManager->setWhipMode(false);

    obs_log(LOG_INFO, "RTMP streaming settings saved successfully");
}

void OneSevenLiveStreamManager::saveWhipStreamingSettings(const std::string& liveStreamID,
                                                          const std::string& whipServer,
                                                          const std::string& whipToken) {
    obs_log(LOG_INFO, "Saving WHIP streaming settings for stream: %s", liveStreamID.c_str());

    // Set WHIP server and token
    obs_data_t* settings = obs_data_create();
    obs_data_set_string(settings, "type", "whip_custom");
    obs_data_set_string(settings, "service", "WHIP");
    obs_data_set_string(settings, "server", whipServer.c_str());
    obs_data_set_string(settings, "bearer_token", whipToken.c_str());

    // Get or create WHIP service
    obs_service_t* service = obs_service_create("whip_custom", "whip_service", settings, NULL);
    if (!service) {
        obs_log(LOG_ERROR, "Failed to create WHIP service");
        obs_data_release(settings);
        return;
    }

    // Set as current streaming service
    obs_frontend_set_streaming_service(service);

    obs_service_release(service);
    obs_data_release(settings);

    obs_frontend_save_streaming_service();

    // Store in config manager
    configManager->setWhipStreamingInfo(liveStreamID, whipServer, whipToken);
    configManager->setWhipMode(true);

    obs_log(LOG_INFO, "WHIP streaming settings saved successfully");
}

void OneSevenLiveStreamManager::configureStreamingService(const OneSevenLiveRtmpResponse& response) {
    obs_log(LOG_INFO, "Configuring streaming service");

    // Check if WHIP information is available
    bool hasWhipInfo = !response.whipInfo.server.isEmpty() && !response.whipInfo.token.isEmpty();

    if (hasWhipInfo) {
        // WHIP mode
        obs_log(LOG_INFO, "Using WHIP streaming mode");
        saveWhipStreamingSettings(response.liveStreamID.toStdString(),
                                  response.whipInfo.server.toStdString(),
                                  response.whipInfo.token.toStdString());
    } else {
        // RTMP mode
        obs_log(LOG_INFO, "Using RTMP streaming mode");

        QString streamUrl;
        QString streamKey;

        // Parse RTMP URL using regex
        QRegularExpression re("(^.+://[^/]+/[^/]+)/(.+)$");
        QRegularExpressionMatch match = re.match(response.rtmpURL);
        if (match.hasMatch()) {
            streamUrl = match.captured(1);
            streamKey = match.captured(2);
        } else {
            obs_log(LOG_ERROR, "Failed to parse stream URL");
            emit errorOccurred("Failed to parse stream URL", "configureStreamingService");
            return;
        }

        saveStreamingSettings(response.liveStreamID.toStdString(), streamUrl.toStdString(),
                              streamKey.toStdString());
    }
}

bool OneSevenLiveStreamManager::enableStreamArchive(const std::string& liveStreamID, bool enable) {
    obs_log(LOG_INFO, "%s stream archive for stream: %s", enable ? "Enabling" : "Disabling",
            liveStreamID.c_str());

    if (!apiWrapper->EnableStreamArchive(liveStreamID, enable ? 1 : 0)) {
        QString errorMsg = apiWrapper->getLastErrorMessage();
        obs_log(LOG_ERROR, "Failed to %s archive. LiveStreamID: %s, Error: %s",
                enable ? "enable" : "disable", liveStreamID.c_str(),
                errorMsg.isEmpty() ? "Unknown error" : errorMsg.toStdString().c_str());
        emit errorOccurred(errorMsg, "enableStreamArchive");
        return false;
    }

    obs_log(LOG_INFO, "Stream archive %s successfully", enable ? "enabled" : "disabled");
    return true;
}

void OneSevenLiveStreamManager::clearStreamingConfiguration() {
    obs_log(LOG_INFO, "Clearing streaming configuration");

    // Clear streaming configuration based on current mode
    if (configManager->isWhipMode()) {
        configManager->clearWhipStreamingInfo();
    } else {
        configManager->clearStreamingInfo();
    }
    configManager->setWhipMode(false);

    obs_log(LOG_INFO, "Streaming configuration cleared");
}

void OneSevenLiveStreamManager::configureStreamingSettings(
    const OneSevenLiveRtmpResponse& response) {
    obs_log(LOG_INFO, "Configuring streaming settings");

    // Store the current stream response for later use
    currentStreamResponse = response;

    // Check if WHIP information is available
    bool hasWhipInfo = !response.whipInfo.server.isEmpty() && !response.whipInfo.token.isEmpty();

    if (hasWhipInfo) {
        // WHIP mode
        obs_log(LOG_INFO, "Using WHIP streaming mode");
        saveWhipStreamingSettings(response.liveStreamID.toStdString(),
                                  response.whipInfo.server.toStdString(),
                                  response.whipInfo.token.toStdString());
    } else {
        // RTMP mode
        obs_log(LOG_INFO, "Using RTMP streaming mode");

        QString streamUrl;
        QString streamKey;

        // Parse RTMP URL using regex
        QRegularExpression re("(^.+://[^/]+/[^/]+)/(.+)$");
        QRegularExpressionMatch match = re.match(response.rtmpURL);
        if (match.hasMatch()) {
            streamUrl = match.captured(1);
            streamKey = match.captured(2);
        } else {
            obs_log(LOG_ERROR, "Failed to parse stream URL");
            emit errorOccurred("Failed to parse stream URL", "configureStreamingSettings");
            return;
        }

        saveStreamingSettings(response.liveStreamID.toStdString(), streamUrl.toStdString(),
                              streamKey.toStdString());
    }
}

const OneSevenLiveRtmpResponse& OneSevenLiveStreamManager::getCurrentStreamResponse() const {
    return currentStreamResponse;
}

const OneSevenLiveRtmpRequest& OneSevenLiveStreamManager::getCurrentStreamRequest() const {
    return currentStreamRequest;
}

const OneSevenLiveStreamInfo& OneSevenLiveStreamManager::getCurrentLiveStreamInfo() const {
    return currentLiveStreamInfo;
}

bool OneSevenLiveStreamManager::hasActiveLiveStream() const {
    return !currentLiveStreamID.empty() &&
           currentStreamingStatus != OneSevenLiveStreamingStatus::NotStarted;
}

void OneSevenLiveStreamManager::loadRoomInfo() {
    if (roomInfoLoading)
        return;

    roomInfoLoading = true;
    QThread* workerThread = new QThread(this);

    OneSevenLiveRoomInfo localRoomInfo;
    OneSevenLiveConfigStreamer localConfigStreamer;
    OneSevenLiveUserInfo localUserInfo;
    OneSevenLiveArmySubscriptionLevels localLevels;

    connect(
        workerThread, &QThread::started, this,
        [this, workerThread, localRoomInfo, localConfigStreamer, localUserInfo,
         localLevels]() mutable {
            OneSevenLiveLoadRoomInfoWorker worker(apiWrapper, configManager);
            worker.setDataStructures(&localRoomInfo, &localConfigStreamer, &localUserInfo,
                                     &localLevels);

            OneSevenLiveLoadRoomInfoWorker::LoadResult result =
                worker.loadRoomInfo(static_cast<std::int64_t>(currentRoomID));

            QMetaObject::invokeMethod(
                this,
                [this, result, localRoomInfo, localConfigStreamer, localUserInfo, localLevels]() {
                    roomInfo = localRoomInfo;
                    configStreamer = localConfigStreamer;
                    userInfo = localUserInfo;
                    levels = localLevels;
                    roomInfoLoading = false;

                    emit roomInfoLoaded(result);
                },
                Qt::QueuedConnection);

            workerThread->quit();
        },
        Qt::DirectConnection);

    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    workerThread->start();
}

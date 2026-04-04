#include "OneSevenLiveStreamManager.hpp"

#include <obs-frontend-api.h>
#include <obs.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QMessageBox>
#include <QPointer>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QThread>
#include <QTimer>

#include "../OneSevenLiveCoreManager.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "api/OneSevenLiveModels.hpp"
#include "moc_OneSevenLiveStreamManager.cpp"
#include "plugin-support.h"
#include "utility/Common.hpp"
#include "websocket/OneSevenLiveWebsocketServer.hpp"
#include "websocket/WebsocketUtils.hpp"
#include "websocket/WsMessage.hpp"

// Static callback for OBS frontend events to ensure safe registration/removal
static void ObsFrontendEventCallback(enum obs_frontend_event event, void* private_data) {
    OneSevenLiveStreamManager* manager = static_cast<OneSevenLiveStreamManager*>(private_data);
    if (!manager)
        return;

    if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
        obs_output_t* output = obs_frontend_get_streaming_output();
        if (output) {
            const char* err = obs_output_get_last_error(output);
            manager->handleObsStreamStopped(0, err ? QString(err) : QString());
            obs_output_release(output);
        } else {
            manager->handleObsStreamStopped(0, QString());
        }
    }
}

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

    // Register callback for OBS streaming events
    obs_frontend_add_event_callback(ObsFrontendEventCallback, this);

    obs_log(LOG_INFO, "OneSevenLiveStreamManager initialized");
}

OneSevenLiveStreamManager::~OneSevenLiveStreamManager() {
    obs_frontend_remove_event_callback(ObsFrontendEventCallback, this);
}

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
            for (const auto& evt : roomInfo.eventList) {
                if (evt.type == 2) {
                    selectedEventId = evt.ID;
                    break;
                }
            }
            request.eventID = selectedEventId;

            QStringList tags;
            for (const auto& t : roomInfo.lastUsedHashtags) {
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

            // Do not signal connected here; wait until live starts

        } else {
            obs_log(LOG_ERROR, "Failed to fetch rtmp url for provider %s",
                    provider.toStdString().c_str());
            return false;
        }
    } else {
        obs_log(LOG_ERROR, "Empty rtmpUrl in roomInfo");
        return false;
    }

    return true;
}

void OneSevenLiveStreamManager::startStreamWithWebAsync() {
    obs_log(LOG_INFO, "Saving web stream settings (Async)");

    if (roomInfo.rtmpUrls.isEmpty()) {
        obs_log(LOG_ERROR, "Empty rtmpUrl in roomInfo");
        emit webStreamSettingsLoaded(false);
        return;
    }

    QString provider = GetProviderNameByIndex(roomInfo.rtmpUrls[0].provider);

    auto* api = this->apiWrapper;
    QPointer<OneSevenLiveStreamManager> self = this;
    std::string providerStr = provider.toStdString();

    ScheduleOBSTask([self, api, providerStr]() {
        if (!self)
            return;

        OneSevenLiveRtmpResponse rtmpResponse;
        bool success = false;

        if (api) {
            success = api->GetRtmpByProvider(providerStr, rtmpResponse);
        }

        if (self) {
            QMetaObject::invokeMethod(
                self,
                [self, success, rtmpResponse, providerStr]() {
                    if (success) {
                        OneSevenLiveRtmpResponse resp = rtmpResponse;
                        resp.liveStreamID = QString::number(self->roomInfo.liveStreamID);
                        self->configureStreamingService(resp);

                        self->currentLiveStreamID = resp.liveStreamID.toStdString();

                        OneSevenLiveRtmpRequest request;
                        request.userID = QString::fromStdString(self->currentUserID);
                        request.caption = self->roomInfo.caption;
                        request.device = "OBS";

                        qint64 selectedEventId = 0;
                        for (const auto& evt : self->roomInfo.eventList) {
                            if (evt.type == 2) {
                                selectedEventId = evt.ID;
                                break;
                            }
                        }
                        request.eventID = selectedEventId;

                        QStringList tags;
                        for (const auto& t : self->roomInfo.lastUsedHashtags) {
                            tags << t.text;
                        }
                        request.hashtags = tags;

                        request.landscape = self->roomInfo.landscape;
                        request.streamerType = self->roomInfo.streamerType;
                        request.subtabID = (self->roomInfo.subtabs.size() > 0)
                                               ? self->roomInfo.subtabs[0]
                                               : QString();
                        request.archiveConfig = self->roomInfo.archiveConfig;

                        OneSevenLiveVliverInfo vl;
                        vl.vliverModel =
                            self->configStreamer.lastStreamState.vliverInfo.vliverModel;
                        request.vliverInfo = vl;

                        OneSevenLiveArmy army{};
                        army.enable = false;
                        army.requiredArmyRank = 0;
                        army.showOnHotPage = false;
                        army.armyOnlyPN = false;
                        request.armyOnly = army;

                        request.enableOBSGroupCall = self->roomInfo.enableOBSGroupCall;

                        self->currentStreamRequest = request;
                        self->currentStreamResponse = resp;
                        OneSevenLiveStreamInfo info;
                        info.request = request;
                        info.categoryName = QString();
                        info.createdAt = QDateTime::currentDateTime();
                        info.streamUuid = resp.streamID;
                        self->currentLiveStreamInfo = info;

                        // Do not signal connected here; wait until live starts

                        emit self->webStreamSettingsLoaded(true);
                    } else {
                        obs_log(LOG_ERROR, "Failed to fetch rtmp url for provider %s",
                                providerStr.c_str());
                        emit self->webStreamSettingsLoaded(false);
                    }
                },
                Qt::QueuedConnection);
        }
    });
}

bool OneSevenLiveStreamManager::createRtmp(const OneSevenLiveRtmpRequest& request) {
    obs_log(LOG_INFO, "Creating live stream");

    OneSevenLiveRtmpRequest modifiedRequest = request;
    modifiedRequest.userID = QString::fromStdString(currentUserID);
    modifiedRequest.streamerType = roomInfo.streamerType;

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
    wsBroadcast(QString::fromUtf8(ws::EventAblyChatConnected),
                nlohmann::json{{"status", "connected"}});

    obs_log(LOG_INFO, "Live stream created successfully. LiveStreamID: %s",
            currentLiveStreamID.c_str());
    return true;
}

void OneSevenLiveStreamManager::createRtmpAsync(const OneSevenLiveRtmpRequest& request) {
    obs_log(LOG_INFO, "Creating live stream (Async)");

    OneSevenLiveRtmpRequest modifiedRequest = request;
    modifiedRequest.userID = QString::fromStdString(currentUserID);
    modifiedRequest.streamerType = roomInfo.streamerType;

    // Capture apiWrapper pointer by value. It is owned by CoreManager.
    auto* api = this->apiWrapper;
    QPointer<OneSevenLiveStreamManager> self = this;

    ScheduleOBSTask([self, modifiedRequest, api]() {
        if (!self)
            return;

        OneSevenLiveRtmpResponse response;
        bool success = false;
        QString errorMsg;

        if (api) {
            success = api->CreateRtmp(modifiedRequest, response);
            if (!success) {
                errorMsg = api->getLastErrorMessage();
            }
        } else {
            errorMsg = "API Wrapper not initialized";
        }

        if (self) {
            QMetaObject::invokeMethod(
                self,
                [self, success, errorMsg, modifiedRequest, response]() {
                    if (success) {
                        self->currentLiveStreamID = response.liveStreamID.toStdString();
                        self->currentStreamRequest = modifiedRequest;
                        self->currentStreamResponse = response;

                        OneSevenLiveStreamInfo info;
                        info.request = modifiedRequest;
                        info.categoryName = QString();
                        info.createdAt = QDateTime::currentDateTime();
                        info.streamUuid = response.streamID;
                        self->currentLiveStreamInfo = info;

                        self->setCurrentStreamingStatus(OneSevenLiveStreamingStatus::Live);

                        self->wsBroadcast(QString::fromUtf8(ws::EventAblyChatConnected),
                                          nlohmann::json{{"status", "connected"}});

                        obs_log(LOG_INFO,
                                "Live stream created successfully (Async). LiveStreamID: %s",
                                self->currentLiveStreamID.c_str());

                        emit self->createRtmpFinished(true, QString());
                    } else {
                        obs_log(LOG_ERROR, "Failed to create stream (Async). Error: %s",
                                errorMsg.toStdString().c_str());
                        emit self->errorOccurred(errorMsg, "createLiveStream");
                        emit self->createRtmpFinished(false, errorMsg);
                    }
                },
                Qt::QueuedConnection);
        }
    });
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
    wsBroadcast(QString::fromUtf8(ws::EventAblyChatConnected),
                nlohmann::json{{"status", "connected"}});

    obs_log(LOG_INFO, "Streaming started successfully");
    return true;
}

void OneSevenLiveStreamManager::startStreamAsync() {
    obs_log(LOG_INFO, "Starting streaming (Async)");

    configureStreamingService(currentStreamResponse);

    std::string lid = currentStreamResponse.liveStreamID.toStdString();
    std::string uid = currentUserID;
    bool autoRecord = currentStreamRequest.archiveConfig.autoRecording;

    auto* api = this->apiWrapper;
    QPointer<OneSevenLiveStreamManager> self = this;

    ScheduleOBSTask([self, lid, uid, autoRecord, api]() {
        if (!self)
            return;

        bool success = false;
        QString errorMsg;

        if (api) {
            success = api->StartStream(lid, uid);
            if (!success) {
                errorMsg = api->getLastErrorMessage();
            } else if (autoRecord) {
                if (!api->EnableStreamArchive(lid, 1)) {
                    QString archiveError = api->getLastErrorMessage();
                    obs_log(LOG_ERROR, "Failed to enable archive (Async). Error: %s",
                            archiveError.toStdString().c_str());
                    success = false;
                    errorMsg = archiveError;
                }
            }
        } else {
            errorMsg = "API Wrapper not initialized";
        }

        if (self) {
            QMetaObject::invokeMethod(
                self,
                [self, success, errorMsg]() {
                    if (success) {
                        self->setCurrentStreamingStatus(OneSevenLiveStreamingStatus::Streaming);

                        self->wsBroadcast(QString::fromUtf8(ws::EventAblyChatConnected),
                                          nlohmann::json{{"status", "connected"}});

                        obs_log(LOG_INFO, "Streaming started successfully (Async)");
                        emit self->startStreamFinished(true, QString());
                    } else {
                        obs_log(LOG_ERROR, "Failed to start stream (Async). Error: %s",
                                errorMsg.toStdString().c_str());
                        emit self->errorOccurred(errorMsg, "startStream");
                        emit self->startStreamFinished(false, errorMsg);
                    }
                },
                Qt::QueuedConnection);
        }
    });
}

void OneSevenLiveStreamManager::changeEventAsync(const OneSevenLiveChangeEventRequest& request) {
    obs_log(LOG_INFO, "Changing event (Async) to: %lld", request.eventID);

    auto* api = this->apiWrapper;
    QPointer<OneSevenLiveStreamManager> self = this;

    ScheduleOBSTask([self, request, api]() {
        if (!self)
            return;

        bool success = false;
        QString errorMsg;

        if (api) {
            success = api->ChangeEvent(request);
            if (!success) {
                errorMsg = api->getLastErrorMessage();
            }
        } else {
            errorMsg = "API Wrapper not initialized";
        }

        if (self) {
            QMetaObject::invokeMethod(
                self,
                [self, success, errorMsg, request]() {
                    if (success) {
                        obs_log(LOG_INFO, "Successfully changed event (Async) to: %lld",
                                request.eventID);
                        emit self->changeEventFinished(true, QString());
                    } else {
                        obs_log(LOG_ERROR, "Failed to change event (Async) to: %lld, error: %s",
                                request.eventID, errorMsg.toStdString().c_str());
                        emit self->changeEventFinished(false, errorMsg);
                    }
                },
                Qt::QueuedConnection);
        }
    });
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
    wsBroadcast(QString::fromUtf8(ws::EventAblyChatConnected), nlohmann::json{{"status", "break"}});

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
    wsBroadcast(
        QString::fromUtf8(ws::EventAblyChatConnected),
        nlohmann::json{{"status", currentStreamingStatus == OneSevenLiveStreamingStatus::NotStarted
                                      ? "break"
                                      : "connected"}});
}

void OneSevenLiveStreamManager::startOBSStreaming() {
    obs_video_info vinfo{};
    if (obs_get_video_info(&vinfo)) {
        obs_log(LOG_INFO, "OBS video: base=%ux%u output=%ux%u fps=%u/%u colorspace=%d range=%d",
                vinfo.base_width, vinfo.base_height, vinfo.output_width, vinfo.output_height,
                vinfo.fps_num, vinfo.fps_den, (int) vinfo.colorspace, (int) vinfo.range);
    }
    obs_audio_info ainfo{};
    if (obs_get_audio_info(&ainfo)) {
        obs_log(LOG_INFO, "OBS audio: rate=%u speakers=%d", ainfo.samples_per_sec,
                (int) ainfo.speakers);
    }

    obs_service_t* svc = obs_frontend_get_streaming_service();
    if (svc) {
        const char* stype = obs_service_get_type(svc);
        const char* proto = obs_service_get_protocol(svc);
        ObsDataPtr sset{obs_service_get_settings(svc)};
        const char* server = sset ? obs_data_get_string(sset.get(), "server") : nullptr;
        const char* key = sset ? obs_data_get_string(sset.get(), "key") : nullptr;
        const char* token = sset ? obs_data_get_string(sset.get(), "bearer_token") : nullptr;
        const char** svc_vcodecs = obs_service_get_supported_video_codecs(svc);
        const char** svc_acodecs = obs_service_get_supported_audio_codecs(svc);
        int max_v_bitrate = 0, max_a_bitrate = 0;
        obs_service_get_max_bitrate(svc, &max_v_bitrate, &max_a_bitrate);
        obs_log(LOG_INFO, "OBS service: type=%s proto=%s server=%s key_len=%zu token_len=%zu",
                stype ? stype : "", proto ? proto : "", server ? server : "", key ? strlen(key) : 0,
                token ? strlen(token) : 0);
        if (svc_vcodecs) {
            std::string vlist;
            for (size_t i = 0; svc_vcodecs[i]; ++i) {
                if (!vlist.empty())
                    vlist += ",";
                vlist += svc_vcodecs[i];
            }
            obs_log(LOG_INFO, "OBS service supported video codecs: %s", vlist.c_str());
        }
        if (svc_acodecs) {
            std::string alist;
            for (size_t i = 0; svc_acodecs[i]; ++i) {
                if (!alist.empty())
                    alist += ",";
                alist += svc_acodecs[i];
            }
            obs_log(LOG_INFO, "OBS service supported audio codecs: %s", alist.c_str());
        }
        obs_log(LOG_INFO, "OBS service max bitrate: video=%d audio=%d", max_v_bitrate,
                max_a_bitrate);
    }

    obs_frontend_streaming_start();
    if (!m_streamLogTimer) {
        m_streamLogTimer = new QTimer(this);
        m_streamLogTimer->setSingleShot(true);
        connect(m_streamLogTimer, &QTimer::timeout, this,
                &OneSevenLiveStreamManager::logCurrentObsOutputInfo);
    }
    m_streamLogTimer->start(200);
}

void OneSevenLiveStreamManager::logCurrentObsOutputInfo() {
    obs_output_t* out = obs_frontend_get_streaming_output();
    if (!out) {
        return;
    }
    const char* oid = obs_output_get_id(out);
    uint32_t ow = obs_output_get_width(out);
    uint32_t oh = obs_output_get_height(out);
    obs_encoder_t* venc = obs_output_get_video_encoder(out);
    obs_encoder_t* aenc = obs_output_get_audio_encoder(out, 0);
    const char* v_id = venc ? obs_encoder_get_id(venc) : nullptr;
    const char* v_codec = venc ? obs_encoder_get_codec(venc) : nullptr;
    ObsDataPtr vset{venc ? obs_encoder_get_settings(venc) : nullptr};
    int v_bitrate = vset ? (int) obs_data_get_int(vset.get(), "bitrate") : 0;
    uint32_t v_scaled_w = venc ? obs_encoder_get_width(venc) : 0;
    uint32_t v_scaled_h = venc ? obs_encoder_get_height(venc) : 0;
    uint32_t v_fps_div = venc ? obs_encoder_get_frame_rate_divisor(venc) : 1;
    const char* a_id = aenc ? obs_encoder_get_id(aenc) : nullptr;
    const char* a_codec = aenc ? obs_encoder_get_codec(aenc) : nullptr;
    ObsDataPtr aset{aenc ? obs_encoder_get_settings(aenc) : nullptr};
    int a_bitrate = aset ? (int) obs_data_get_int(aset.get(), "bitrate") : 0;
    uint32_t a_rate = aenc ? obs_encoder_get_sample_rate(aenc) : 0;
    size_t a_mixer = aenc ? obs_encoder_get_mixer_index(aenc) : 0;
    const char* out_v_supported = obs_output_get_supported_video_codecs(out);
    const char* out_a_supported = obs_output_get_supported_audio_codecs(out);
    obs_log(LOG_INFO,
            "OBS output: id=%s size=%ux%u video_encoder=%s codec=%s bitrate=%d scaled=%ux%u "
            "fps_div=%u audio_encoder=%s codec=%s bitrate=%d rate=%u mixer=%zu",
            oid ? oid : "", ow, oh, v_id ? v_id : "", v_codec ? v_codec : "", v_bitrate, v_scaled_w,
            v_scaled_h, v_fps_div, a_id ? a_id : "", a_codec ? a_codec : "", a_bitrate, a_rate,
            a_mixer);
    obs_log(LOG_INFO, "OBS output supported codecs: video=%s audio=%s",
            out_v_supported ? out_v_supported : "", out_a_supported ? out_a_supported : "");
}

void OneSevenLiveStreamManager::stopOBSStreaming() {
    obs_log(LOG_INFO, "Stopping OBS streaming");
    auto& core = OneSevenLiveCoreManager::getInstance();
    if (QThread::currentThread() != core.thread()) {
        QMetaObject::invokeMethod(
            &core, [this]() { this->stopOBSStreaming(); }, Qt::BlockingQueuedConnection);
        return;
    }
    if (!obs_frontend_streaming_active()) {
        obs_log(LOG_INFO, "OBS streaming is not active");
        return;
    }

    if (m_streamLogTimer) {
        m_streamLogTimer->stop();
    }

    obs_frontend_streaming_stop();

    // Removed busy-wait loop. We rely on OBS_FRONTEND_EVENT_STREAMING_STOPPED event.
    obs_log(LOG_INFO, "OBS streaming stop requested");
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
    ObsDataPtr settings{obs_service_get_settings(service)};
    obs_data_set_string(settings.get(), "server", streamUrl.c_str());
    obs_data_set_string(settings.get(), "key", streamKey.c_str());

    // Apply settings
    obs_service_update(service, settings.get());
    settings.reset();

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
    ObsDataPtr settings{obs_data_create()};
    obs_data_set_string(settings.get(), "type", "whip_custom");
    obs_data_set_string(settings.get(), "service", "WHIP");
    obs_data_set_string(settings.get(), "server", whipServer.c_str());
    obs_data_set_string(settings.get(), "bearer_token", whipToken.c_str());

    // Get or create WHIP service
    obs_service_t* service =
        obs_service_create("whip_custom", "whip_service", settings.get(), NULL);
    if (!service) {
        obs_log(LOG_ERROR, "Failed to create WHIP service");
        settings.reset();
        return;
    }

    // Set as current streaming service
    obs_frontend_set_streaming_service(service);

    obs_service_release(service);
    settings.reset();

    obs_frontend_save_streaming_service();

    // Store in config manager
    configManager->setWhipStreamingInfo(liveStreamID, whipServer, whipToken);
    configManager->setWhipMode(true);

    obs_log(LOG_INFO, "WHIP streaming settings saved successfully");
}

void OneSevenLiveStreamManager::configureStreamingService(
    const OneSevenLiveRtmpResponse& response) {
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

void OneSevenLiveStreamManager::handleObsStreamStopped(int code, const QString& lastError) {
    // This is called from OBS callback thread, so we need to invoke on main thread
    QMetaObject::invokeMethod(
        this, [this, code, lastError]() { emit obsStreamStopped(code, lastError); },
        Qt::QueuedConnection);
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

    auto* api = this->apiWrapper;
    auto* cm = this->configManager;
    qint64 rid = currentRoomID;
    QPointer<OneSevenLiveStreamManager> self = this;

    ScheduleOBSTask([self, api, cm, rid]() {
        if (!self)
            return;

        OneSevenLiveRoomInfo localRoomInfo;
        OneSevenLiveConfigStreamer localConfigStreamer;
        OneSevenLiveUserInfo localUserInfo;
        OneSevenLiveArmySubscriptionLevels localLevels;

        OneSevenLiveLoadRoomInfoWorker worker(api, cm);
        worker.setDataStructures(&localRoomInfo, &localConfigStreamer, &localUserInfo,
                                 &localLevels);

        OneSevenLiveLoadRoomInfoWorker::LoadResult result =
            worker.loadRoomInfo(static_cast<std::int64_t>(rid));

        if (self) {
            QMetaObject::invokeMethod(
                self,
                [self, result, localRoomInfo, localConfigStreamer, localUserInfo, localLevels]() {
                    self->roomInfo = localRoomInfo;
                    self->configStreamer = localConfigStreamer;
                    self->userInfo = localUserInfo;
                    self->levels = localLevels;
                    self->roomInfoLoading = false;

                    emit self->roomInfoLoaded(result);
                },
                Qt::QueuedConnection);
        }
    });
}

void OneSevenLiveStreamManager::wsBroadcast(const QString& type, const nlohmann::json& payload) {
    auto& core = OneSevenLiveCoreManager::getInstance();
    if (auto* ws = core.getWebsocketServer()) {
        WsMessage msg;
        msg.type = type.toStdString();
        msg.payload = payload;
        ws->broadcastMessage(msg.dump());
    }
}

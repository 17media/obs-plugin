#include "OneSevenLiveStreamManager.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "api/OneSevenLiveModels.hpp"
#include "plugin-support.h"

#include <QTimer>
#include <QMessageBox>
#include <obs-frontend-api.h>
#include <obs.h>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

OneSevenLiveStreamManager::OneSevenLiveStreamManager(OneSevenLiveApiWrappers* apiWrapper,
                                                     OneSevenLiveConfigManager* configManager,
                                                     QObject* parent)
    : QObject(parent),
      apiWrapper(apiWrapper),
      configManager(configManager),
      currentStreamingStatus(OneSevenLiveStreamingStatus::NotStarted),
      eventCooldownTimer(new QTimer(this)),
      eventCooldownRemaining(0) {
    
    // Connect cooldown timer
    connect(eventCooldownTimer, &QTimer::timeout, this, &OneSevenLiveStreamManager::onEventCooldownTimeout);
    eventCooldownTimer->setInterval(1000); // 1 second intervals
    
    obs_log(LOG_INFO, "OneSevenLiveStreamManager initialized");
}

OneSevenLiveStreamManager::~OneSevenLiveStreamManager() {
    if (eventCooldownTimer->isActive()) {
        eventCooldownTimer->stop();
    }
}

bool OneSevenLiveStreamManager::createLiveStream(const OneSevenLiveRtmpRequest& request) {
    obs_log(LOG_INFO, "Creating live stream");
    
    // Add current userID and streamerType to request
    std::string userID;
    configManager->getConfigValue("UserID", userID);
    
    OneSevenLiveRtmpRequest modifiedRequest = request;
    modifiedRequest.userID = QString::fromStdString(userID);
    
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
    currentUserID = userID;
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
    
    obs_log(LOG_INFO, "Live stream created successfully. LiveStreamID: %s", currentLiveStreamID.c_str());
    return true;
}

bool OneSevenLiveStreamManager::startStreaming(const std::string& userID,
                                             const OneSevenLiveRtmpResponse& response,
                                             bool autoRecording,
                                             bool skip) {
    obs_log(LOG_INFO, "Starting streaming");
    
    // Configure streaming service (RTMP or WHIP)
    configureStreamingService(userID, response);
    
    // Start live stream via API
    if (!skip && !apiWrapper->StartStream(response.liveStreamID.toStdString(), userID)) {
        QString errorMsg = apiWrapper->getLastErrorMessage();
        obs_log(LOG_ERROR,
                "Failed to start stream. LiveStreamID: %s, UserID: %s, Error: %s, Timestamp: %lld",
                response.liveStreamID.toStdString().c_str(), userID.c_str(),
                errorMsg.isEmpty() ? "Unknown error" : errorMsg.toStdString().c_str(),
                QDateTime::currentMSecsSinceEpoch());
        emit errorOccurred(errorMsg, "startStreaming");
        return false;
    }
    
    // Enable archive if requested
    if (!skip && autoRecording) {
        enableStreamArchive(response.liveStreamID.toStdString(), true);
    }
    
    // Update status
    setCurrentStreamingStatus(OneSevenLiveStreamingStatus::Streaming);
    
    // Start event cooldown
    startEventCooldown();
    
    obs_log(LOG_INFO, "Streaming started successfully");
    return true;
}

bool OneSevenLiveStreamManager::stopStreaming(const std::string& userID,
                                            const std::string& liveStreamID,
                                            bool isAutoClose) {
    obs_log(LOG_INFO, "Stopping streaming");
    
    // Stop OBS streaming first
    stopOBSStreaming();
    
    QString endReason = isAutoClose ? "autoClose" : "normalEnd";
    
    // Send close live stream request
    OneSevenLiveCloseLiveRequest request;
    request.reason = endReason;
    request.userID = QString::fromStdString(userID);
    
    if (!apiWrapper->StopStream(liveStreamID, request)) {
        QString errorMsg = apiWrapper->getLastErrorMessage();
        obs_log(LOG_ERROR, "Failed to stop stream. LiveStreamID: %s, Reason: %s",
                liveStreamID.c_str(), endReason.toStdString().c_str());
        emit errorOccurred(errorMsg, "stopStreaming");
        // Continue with cleanup even if API call fails
    } else {
        obs_log(LOG_INFO,
                "Successfully stopped stream. LiveStreamID: %s, Reason: %s, IsAutoClose: %s",
                liveStreamID.c_str(), endReason.toStdString().c_str(),
                isAutoClose ? "true" : "false");
    }
    
    // Clear streaming configuration
    clearStreamingConfiguration();
    
    // Update status
    setCurrentStreamingStatus(OneSevenLiveStreamingStatus::NotStarted);
    
    // Clear current stream info
    currentLiveStreamID.clear();
    currentUserID.clear();
    currentStreamRequest = OneSevenLiveRtmpRequest{};
    currentStreamResponse = OneSevenLiveRtmpResponse{};
    currentLiveStreamInfo = OneSevenLiveStreamInfo{};
    
    obs_log(LOG_INFO, "Streaming stopped successfully");
    return true;
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

bool OneSevenLiveStreamManager::changeEvent(qint64 eventID) {
    obs_log(LOG_INFO, "Changing event to: %lld", eventID);
    
    // Check if we're in cooldown
    if (isEventInCooldown()) {
        obs_log(LOG_INFO, "Event change ignored due to cooldown");
        return false;
    }
    
    // Call ChangeEvent API
    OneSevenLiveChangeEventRequest request;
    request.eventID = eventID;
    
    bool success = apiWrapper->ChangeEvent(request);
    if (success) {
        obs_log(LOG_INFO, "Successfully changed event to: %lld", eventID);
        
        // Start event cooldown
        startEventCooldown();
    } else {
        obs_log(LOG_ERROR, "Failed to change event to: %lld", eventID);
        QString errorMsg = apiWrapper->getLastErrorMessage();
        emit errorOccurred(errorMsg, "changeEvent");
    }
    
    return success;
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

void OneSevenLiveStreamManager::startEventCooldown(int duration) {
    eventCooldownRemaining = duration;
    eventCooldownTimer->start();
    
    emit eventCooldownUpdated(eventCooldownRemaining);
    obs_log(LOG_INFO, "Event cooldown started for %d seconds", duration);
}

bool OneSevenLiveStreamManager::isEventInCooldown() const {
    return eventCooldownTimer->isActive();
}

int OneSevenLiveStreamManager::getEventCooldownRemaining() const {
    return eventCooldownRemaining;
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

void OneSevenLiveStreamManager::onEventCooldownTimeout() {
    if (eventCooldownRemaining > 0) {
        eventCooldownRemaining--;
        emit eventCooldownUpdated(eventCooldownRemaining);
    } else {
        // Cooldown finished
        eventCooldownTimer->stop();
        emit eventCooldownUpdated(0);
        obs_log(LOG_INFO, "Event cooldown finished");
    }
}

void OneSevenLiveStreamManager::configureStreamingService(const std::string& userID,
                                                       const OneSevenLiveRtmpResponse& response) {
    UNUSED_PARAMETER(userID);
    
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
        
        saveStreamingSettings(response.liveStreamID.toStdString(),
                             streamUrl.toStdString(),
                             streamKey.toStdString());
    }
}

bool OneSevenLiveStreamManager::enableStreamArchive(const std::string& liveStreamID, bool enable) {
    obs_log(LOG_INFO, "%s stream archive for stream: %s", enable ? "Enabling" : "Disabling", liveStreamID.c_str());
    
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

bool OneSevenLiveStreamManager::startLiveStream(const std::string& liveStreamID,
                                               const std::string& userID,
                                               bool autoRecording) {
    obs_log(LOG_INFO, "Starting live stream. LiveStreamID: %s, UserID: %s, AutoRecording: %s",
            liveStreamID.c_str(), userID.c_str(), autoRecording ? "true" : "false");
    
    // Start live stream via API
    if (!apiWrapper->StartStream(liveStreamID, userID)) {
        QString errorMsg = apiWrapper->getLastErrorMessage();
        obs_log(LOG_ERROR,
                "Failed to start stream. LiveStreamID: %s, UserID: %s, Error: %s, Timestamp: %lld",
                liveStreamID.c_str(), userID.c_str(),
                errorMsg.isEmpty() ? "Unknown error" : errorMsg.toStdString().c_str(),
                QDateTime::currentMSecsSinceEpoch());
        emit errorOccurred(errorMsg, "startLiveStream");
        return false;
    }
    
    // Enable archive if requested
    if (autoRecording) {
        enableStreamArchive(liveStreamID, true);
    }
    
    obs_log(LOG_INFO, "Live stream started successfully");
    return true;
}

bool OneSevenLiveStreamManager::stopLiveStream(const std::string& liveStreamID,
                                              const OneSevenLiveCloseLiveRequest& request) {
    obs_log(LOG_INFO, "Stopping live stream. LiveStreamID: %s, Reason: %s",
            liveStreamID.c_str(), request.reason.toStdString().c_str());
    
    if (!apiWrapper->StopStream(liveStreamID, request)) {
        QString errorMsg = apiWrapper->getLastErrorMessage();
        obs_log(LOG_ERROR, "Failed to stop stream. LiveStreamID: %s, Reason: %s, Error: %s",
                liveStreamID.c_str(), request.reason.toStdString().c_str(),
                errorMsg.isEmpty() ? "Unknown error" : errorMsg.toStdString().c_str());
        emit errorOccurred(errorMsg, "stopLiveStream");
        return false;
    }
    
    obs_log(LOG_INFO, "Live stream stopped successfully");
    return true;
}

void OneSevenLiveStreamManager::configureStreamingSettings(const OneSevenLiveRtmpResponse& response) {
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
        
        saveStreamingSettings(response.liveStreamID.toStdString(),
                             streamUrl.toStdString(),
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
    return !currentLiveStreamID.empty() && currentStreamingStatus != OneSevenLiveStreamingStatus::NotStarted;
}

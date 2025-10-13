#include "OneSevenMultiRtmpModels.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <ctime>

// OneSevenMultiRtmpServiceConfig implementation
void OneSevenMultiRtmpServiceConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{
        {"serverUrl", serverUrl},
        {"streamKey", streamKey},
        {"useAuthentication", useAuthentication},
        {"authToken", authToken}
    };
}

void OneSevenMultiRtmpServiceConfig::from_json(const nlohmann::json& j) {
    j.at("serverUrl").get_to(serverUrl);
    j.at("streamKey").get_to(streamKey);
    j.at("useAuthentication").get_to(useAuthentication);
    if (j.contains("authToken")) {
        j.at("authToken").get_to(authToken);
    }
}

// OneSevenMultiRtmpOutputConfig implementation
void OneSevenMultiRtmpOutputConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{
        {"bufferSize", bufferSize},
        {"reconnectDelay", reconnectDelay},
        {"autoReconnect", autoReconnect},
        {"bindIP", bindIP}
    };
}

void OneSevenMultiRtmpOutputConfig::from_json(const nlohmann::json& j) {
    j.at("bufferSize").get_to(bufferSize);
    j.at("reconnectDelay").get_to(reconnectDelay);
    j.at("autoReconnect").get_to(autoReconnect);
    j.at("bindIP").get_to(bindIP);
}

// OneSevenMultiRtmpVideoConfig implementation
void OneSevenMultiRtmpVideoConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{
        {"useSharedEncoder", useSharedEncoder},
        {"encoderId", encoderId},
        {"sceneId", sceneId},
        {"outputWidth", outputWidth},
        {"outputHeight", outputHeight},
        {"fpsDenominator", fpsDenominator},
        {"rateControl", rateControl},
        {"bitrate", bitrate},
        {"rateLimiting", rateLimiting},
        {"keyframeInterval", keyframeInterval},
        {"profile", profile},
        {"useBFrames", useBFrames}
    };
}

void OneSevenMultiRtmpVideoConfig::from_json(const nlohmann::json& j) {
    j.at("useSharedEncoder").get_to(useSharedEncoder);
    if (j.contains("encoderId")) {
        j.at("encoderId").get_to(encoderId);
    }
    j.at("sceneId").get_to(sceneId);
    j.at("outputWidth").get_to(outputWidth);
    j.at("outputHeight").get_to(outputHeight);
    j.at("fpsDenominator").get_to(fpsDenominator);
    j.at("rateControl").get_to(rateControl);
    j.at("bitrate").get_to(bitrate);
    j.at("rateLimiting").get_to(rateLimiting);
    j.at("keyframeInterval").get_to(keyframeInterval);
    j.at("profile").get_to(profile);
    j.at("useBFrames").get_to(useBFrames);
}

// OneSevenMultiRtmpAudioConfig implementation
void OneSevenMultiRtmpAudioConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{
        {"useSharedEncoder", useSharedEncoder},
        {"encoderId", encoderId},
        {"mixerId", mixerId},
        {"vodTrack", vodTrack},
        {"sampleRate", sampleRate},
        {"bitrate", bitrate},
        {"useHEAAC", useHEAAC}
    };
}

void OneSevenMultiRtmpAudioConfig::from_json(const nlohmann::json& j) {
    j.at("useSharedEncoder").get_to(useSharedEncoder);
    if (j.contains("encoderId")) {
        j.at("encoderId").get_to(encoderId);
    }
    j.at("mixerId").get_to(mixerId);
    j.at("vodTrack").get_to(vodTrack);
    j.at("sampleRate").get_to(sampleRate);
    j.at("bitrate").get_to(bitrate);
    j.at("useHEAAC").get_to(useHEAAC);
}

// OneSevenMultiRtmpSyncConfig implementation
void OneSevenMultiRtmpSyncConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{
        {"syncStart", syncStart},
        {"syncStop", syncStop}
    };
}

void OneSevenMultiRtmpSyncConfig::from_json(const nlohmann::json& j) {
    j.at("syncStart").get_to(syncStart);
    j.at("syncStop").get_to(syncStop);
}

// OneSevenMultiRtmpConfig implementation
void OneSevenMultiRtmpConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{
        {"id", id},
        {"streamName", streamName},
        {"protocol", protocol},
        {"service", service},
        {"output", output},
        {"video", video},
        {"audio", audio},
        {"sync", sync},
        {"createdAt", createdAt},
        {"updatedAt", updatedAt}
    };
}

void OneSevenMultiRtmpConfig::from_json(const nlohmann::json& j) {
    j.at("id").get_to(id);
    j.at("streamName").get_to(streamName);
    j.at("protocol").get_to(protocol);
    j.at("service").get_to(service);
    j.at("output").get_to(output);
    j.at("video").get_to(video);
    j.at("audio").get_to(audio);
    j.at("sync").get_to(sync);
    if (j.contains("createdAt")) {
        j.at("createdAt").get_to(createdAt);
    }
    if (j.contains("updatedAt")) {
        j.at("updatedAt").get_to(updatedAt);
    }
}

bool OneSevenMultiRtmpConfig::isValid() const {
    if (id.empty() || streamName.empty()) {
        return false;
    }
    if (service.serverUrl.empty() || service.streamKey.empty()) {
        return false;
    }
    if (video.outputWidth <= 0 || video.outputHeight <= 0) {
        return false;
    }
    if (video.bitrate <= 0 || audio.bitrate <= 0) {
        return false;
    }
    return true;
}

std::string OneSevenMultiRtmpConfig::getValidationError() const {
    if (id.empty()) {
        return "Stream ID is required";
    }
    if (streamName.empty()) {
        return "Stream name is required";
    }
    if (service.serverUrl.empty()) {
        return "Server URL is required";
    }
    if (service.streamKey.empty()) {
        return "Stream key is required";
    }
    if (video.outputWidth <= 0 || video.outputHeight <= 0) {
        return "Invalid video resolution";
    }
    if (video.bitrate <= 0) {
        return "Invalid video bitrate";
    }
    if (audio.bitrate <= 0) {
        return "Invalid audio bitrate";
    }
    return "";
}

// OneSevenMultiRtmpStreamStatus implementation
std::string OneSevenMultiRtmpStreamStatus::getStateString() const {
    switch (state) {
        case STOPPED: return "Stopped";
        case CONNECTING: return "Connecting";
        case STREAMING: return "Streaming";
        case RECONNECTING: return "Reconnecting";
        case ERROR: return "Error";
        default: return "Unknown";
    }
}

bool OneSevenMultiRtmpStreamStatus::isActive() const {
    return state == CONNECTING || state == STREAMING || state == RECONNECTING;
}

// OneSevenMultiRtmpStreamStats implementation
std::string OneSevenMultiRtmpStreamStats::getDurationString() const {
    auto totalSeconds = static_cast<int>(duration.count());
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours << ":"
        << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}

double OneSevenMultiRtmpStreamStats::getDroppedFramePercentage() const {
    if (totalFrames == 0) {
        return 0.0;
    }
    return (static_cast<double>(droppedFrames) / totalFrames) * 100.0;
}

// OneSevenMultiRtmpGlobalConfig implementation
void OneSevenMultiRtmpGlobalConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{
        {"version", version},
        {"lastModified", lastModified},
        {"streams", streams}
    };
}

void OneSevenMultiRtmpGlobalConfig::from_json(const nlohmann::json& j) {
    j.at("version").get_to(version);
    if (j.contains("lastModified")) {
        j.at("lastModified").get_to(lastModified);
    }
    if (j.contains("streams")) {
        j.at("streams").get_to(streams);
    }
}

void OneSevenMultiRtmpGlobalConfig::updateLastModified() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    lastModified = oss.str();
}

OneSevenMultiRtmpConfig* OneSevenMultiRtmpGlobalConfig::findStream(const std::string& streamId) {
    auto it = std::find_if(streams.begin(), streams.end(),
        [&streamId](const OneSevenMultiRtmpConfig& config) {
            return config.id == streamId;
        });
    return (it != streams.end()) ? &(*it) : nullptr;
}

const OneSevenMultiRtmpConfig* OneSevenMultiRtmpGlobalConfig::findStream(const std::string& streamId) const {
    auto it = std::find_if(streams.begin(), streams.end(),
        [&streamId](const OneSevenMultiRtmpConfig& config) {
            return config.id == streamId;
        });
    return (it != streams.end()) ? &(*it) : nullptr;
}

bool OneSevenMultiRtmpGlobalConfig::removeStream(const std::string& streamId) {
    auto it = std::remove_if(streams.begin(), streams.end(),
        [&streamId](const OneSevenMultiRtmpConfig& config) {
            return config.id == streamId;
        });
    if (it != streams.end()) {
        streams.erase(it);
        updateLastModified();
        return true;
    }
    return false;
}

void OneSevenMultiRtmpGlobalConfig::addStream(const OneSevenMultiRtmpConfig& config) {
    streams.push_back(config);
    updateLastModified();
}

void OneSevenMultiRtmpGlobalConfig::updateStream(const std::string& streamId, const OneSevenMultiRtmpConfig& config) {
    auto* existingConfig = findStream(streamId);
    if (existingConfig) {
        *existingConfig = config;
        updateLastModified();
    }
}

// Global JSON serialization functions
void to_json(nlohmann::json& j, const OneSevenMultiRtmpServiceConfig& config) {
    config.to_json(j);
}

void from_json(const nlohmann::json& j, OneSevenMultiRtmpServiceConfig& config) {
    config.from_json(j);
}

void to_json(nlohmann::json& j, const OneSevenMultiRtmpOutputConfig& config) {
    config.to_json(j);
}

void from_json(const nlohmann::json& j, OneSevenMultiRtmpOutputConfig& config) {
    config.from_json(j);
}

void to_json(nlohmann::json& j, const OneSevenMultiRtmpVideoConfig& config) {
    config.to_json(j);
}

void from_json(const nlohmann::json& j, OneSevenMultiRtmpVideoConfig& config) {
    config.from_json(j);
}

void to_json(nlohmann::json& j, const OneSevenMultiRtmpAudioConfig& config) {
    config.to_json(j);
}

void from_json(const nlohmann::json& j, OneSevenMultiRtmpAudioConfig& config) {
    config.from_json(j);
}

void to_json(nlohmann::json& j, const OneSevenMultiRtmpSyncConfig& config) {
    config.to_json(j);
}

void from_json(const nlohmann::json& j, OneSevenMultiRtmpSyncConfig& config) {
    config.from_json(j);
}

void to_json(nlohmann::json& j, const OneSevenMultiRtmpConfig& config) {
    config.to_json(j);
}

void from_json(const nlohmann::json& j, OneSevenMultiRtmpConfig& config) {
    config.from_json(j);
}

void to_json(nlohmann::json& j, const OneSevenMultiRtmpGlobalConfig& config) {
    config.to_json(j);
}

void from_json(const nlohmann::json& j, OneSevenMultiRtmpGlobalConfig& config) {
    config.from_json(j);
}

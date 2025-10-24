#include "OneSevenMultiRtmpModels.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <ctime>


// OneSevenMultiRtmpVideoConfig implementation
void OneSevenMultiRtmpVideoConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{
        {"id", id},
        {"encoderId", encoderId},
        {"fpsDenominator", fpsDenominator},
        {"encoderSettings", encoderSettings},
        {"outputScene", outputScene},
        {"resolution", resolution}
    };
}

void OneSevenMultiRtmpVideoConfig::from_json(const nlohmann::json& j) {
    j.at("id").get_to(id);
    if (j.contains("encoderId")) {
        j.at("encoderId").get_to(encoderId);
    }
    if (j.contains("fpsDenominator")) {
        j.at("fpsDenominator").get_to(fpsDenominator);
    }
    if (j.contains("encoderSettings")) {
        j.at("encoderSettings").get_to(encoderSettings);
    }
    if (j.contains("outputScene")) {
        j.at("outputScene").get_to(outputScene);
    }
    if (j.contains("resolution")) {
        j.at("resolution").get_to(resolution);
    }
}

// AudioTrackConfig JSON serialization
void to_json(nlohmann::json& j, const AudioTrackConfig& config) {
    j = nlohmann::json{
        {"mixer_track", config.mixer_track},
        {"output_track", config.output_track}
    };
}

void from_json(const nlohmann::json& j, AudioTrackConfig& config) {
    j.at("mixer_track").get_to(config.mixer_track);
    j.at("output_track").get_to(config.output_track);
}

// OneSevenMultiRtmpAudioConfig implementation
void OneSevenMultiRtmpAudioConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{
        {"id", id},
        {"encoderId", encoderId},
        {"encoderSettings", encoderSettings},
        {"mixerId", mixerId},
        {"audioTracks", audioTracks}
    };
}

void OneSevenMultiRtmpAudioConfig::from_json(const nlohmann::json& j) {
    j.at("id").get_to(id);
    if (j.contains("encoderId")) {
        j.at("encoderId").get_to(encoderId);
    }
    if (j.contains("encoderSettings")) {
        j.at("encoderSettings").get_to(encoderSettings);
    }
    if (j.contains("mixerId")) {
        j.at("mixerId").get_to(mixerId);
    }
    if (j.contains("audioTracks")) {
        j.at("audioTracks").get_to(audioTracks);
    }
}

// OneSevenMultiRtmpConfig implementation
void OneSevenMultiRtmpConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{
        {"id", id},
        {"streamName", streamName},
        {"protocol", protocol},
        {"syncStart", syncStart},
        {"syncStop", syncStop},
        {"serviceSettings", serviceSettings},
        {"outputSettings", outputSettings},
        {"videoConfigId", videoConfigId},
        {"audioConfigId", audioConfigId},
        {"video", {
            {"useSharedEncoder", video.useSharedEncoder},
            {"bitrate", video.bitrate},
            {"outputWidth", video.outputWidth},
            {"outputHeight", video.outputHeight},
            {"encoderId", video.encoderId},
            {"rateControl", video.rateControl},
            {"profile", video.profile},
            {"keyframeInterval", video.keyframeInterval},
            {"rateLimiting", video.rateLimiting},
            {"useBFrames", video.useBFrames}
        }},
        {"audio", {
            {"bitrate", audio.bitrate},
            {"sampleRate", audio.sampleRate},
            {"mixerId", audio.mixerId},
            {"encoderId", audio.encoderId},
            {"useHEAAC", audio.useHEAAC},
            {"useSharedEncoder", audio.useSharedEncoder}
        }},
        {"service", {
            {"serverUrl", service.serverUrl},
            {"streamKey", service.streamKey},
            {"useAuthentication", service.useAuthentication},
            {"authToken", service.authToken}
        }},
        {"output", {
            {"autoReconnect", output.autoReconnect},
            {"reconnectDelay", output.reconnectDelay},
            {"bufferSize", output.bufferSize},
            {"bindIP", output.bindIP}
        }}
    };
}

void OneSevenMultiRtmpConfig::from_json(const nlohmann::json& j) {
    j.at("id").get_to(id);
    if (j.contains("streamName")) {
        j.at("streamName").get_to(streamName);
    }
    if (j.contains("protocol")) {
        j.at("protocol").get_to(protocol);
    }
    if (j.contains("syncStart")) {
        j.at("syncStart").get_to(syncStart);
    }
    if (j.contains("syncStop")) {
        j.at("syncStop").get_to(syncStop);
    }
    if (j.contains("serviceSettings")) {
        j.at("serviceSettings").get_to(serviceSettings);
    }
    if (j.contains("outputSettings")) {
        j.at("outputSettings").get_to(outputSettings);
    }
    if (j.contains("videoConfigId")) {
        j.at("videoConfigId").get_to(videoConfigId);
    }
    if (j.contains("audioConfigId")) {
        j.at("audioConfigId").get_to(audioConfigId);
    }
    
    // Parse legacy compatibility fields
    if (j.contains("video")) {
        const auto& videoJson = j["video"];
        if (videoJson.contains("useSharedEncoder")) {
            video.useSharedEncoder = videoJson["useSharedEncoder"];
        }
        if (videoJson.contains("bitrate")) {
            video.bitrate = videoJson["bitrate"];
        }
        if (videoJson.contains("outputWidth")) {
            video.outputWidth = videoJson["outputWidth"];
        }
        if (videoJson.contains("outputHeight")) {
            video.outputHeight = videoJson["outputHeight"];
        }
        if (videoJson.contains("encoderId")) {
            video.encoderId = videoJson["encoderId"];
        }
        if (videoJson.contains("rateControl")) {
            video.rateControl = videoJson["rateControl"];
        }
        if (videoJson.contains("profile")) {
            video.profile = videoJson["profile"];
        }
        if (videoJson.contains("keyframeInterval")) {
            video.keyframeInterval = videoJson["keyframeInterval"];
        }
        if (videoJson.contains("rateLimiting")) {
            video.rateLimiting = videoJson["rateLimiting"];
        }
        if (videoJson.contains("useBFrames")) {
            video.useBFrames = videoJson["useBFrames"];
        }
    }
    
    if (j.contains("audio")) {
        const auto& audioJson = j["audio"];
        if (audioJson.contains("bitrate")) {
            audio.bitrate = audioJson["bitrate"];
        }
        if (audioJson.contains("sampleRate")) {
            audio.sampleRate = audioJson["sampleRate"];
        }
        if (audioJson.contains("mixerId")) {
            audio.mixerId = audioJson["mixerId"];
        }
        if (audioJson.contains("encoderId")) {
            audio.encoderId = audioJson["encoderId"];
        }
        if (audioJson.contains("useHEAAC")) {
            audio.useHEAAC = audioJson["useHEAAC"];
        }
        if (audioJson.contains("useSharedEncoder")) {
            audio.useSharedEncoder = audioJson["useSharedEncoder"];
        }
    }
    
    if (j.contains("service")) {
        const auto& serviceJson = j["service"];
        if (serviceJson.contains("serverUrl")) {
            service.serverUrl = serviceJson["serverUrl"];
        }
        if (serviceJson.contains("streamKey")) {
            service.streamKey = serviceJson["streamKey"];
        }
        if (serviceJson.contains("useAuthentication")) {
            service.useAuthentication = serviceJson["useAuthentication"];
        }
        if (serviceJson.contains("authToken")) {
            service.authToken = serviceJson["authToken"];
        }
    }
    
    if (j.contains("output")) {
        const auto& outputJson = j["output"];
        if (outputJson.contains("autoReconnect")) {
            output.autoReconnect = outputJson["autoReconnect"];
        }
        if (outputJson.contains("reconnectDelay")) {
            output.reconnectDelay = outputJson["reconnectDelay"];
        }
        if (outputJson.contains("bufferSize")) {
            output.bufferSize = outputJson["bufferSize"];
        }
        if (outputJson.contains("bindIP")) {
            output.bindIP = outputJson["bindIP"];
        }
    }
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
        {"streams", streams},
        {"audioConfigs", audioConfigs},
        {"videoConfigs", videoConfigs}
    };
}

void OneSevenMultiRtmpGlobalConfig::from_json(const nlohmann::json& j) {
    if (j.contains("streams")) {
        j.at("streams").get_to(streams);
    }
    if (j.contains("audioConfigs")) {
        j.at("audioConfigs").get_to(audioConfigs);
    }
    if (j.contains("videoConfigs")) {
        j.at("videoConfigs").get_to(videoConfigs);
    }
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
        return true;
    }
    return false;
}

void OneSevenMultiRtmpGlobalConfig::addStream(const OneSevenMultiRtmpConfig& config) {
    streams.push_back(config);
}

void OneSevenMultiRtmpGlobalConfig::updateStream(const std::string& streamId, const OneSevenMultiRtmpConfig& config) {
    auto* existingConfig = findStream(streamId);
    if (existingConfig) {
        *existingConfig = config;
    }
}

// Global JSON serialization functions
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

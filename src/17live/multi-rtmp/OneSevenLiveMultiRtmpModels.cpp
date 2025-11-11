#include "OneSevenLiveMultiRtmpModels.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

static OneSevenLiveProtocol s_protocolList[] = {
    // protocol, label, output_id, service_id
    {"rtmp", "RTMP", "rtmp_output", "rtmp_custom"}};

// OneSevenLiveMultiRtmpVideoConfig implementation
void OneSevenLiveMultiRtmpVideoConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{{"encoderId", encoderId},
                       {"fpsDenominator", fpsDenominator},
                       {"encoderSettings", encoderSettings},
                       {"outputScene", outputScene},
                       {"resolution", resolution}};
}

void OneSevenLiveMultiRtmpVideoConfig::from_json(const nlohmann::json& j) {
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
    j = nlohmann::json{{"mixer_track", config.mixer_track}, {"output_track", config.output_track}};
}

void from_json(const nlohmann::json& j, AudioTrackConfig& config) {
    j.at("mixer_track").get_to(config.mixer_track);
    j.at("output_track").get_to(config.output_track);
}

// OneSevenLiveMultiRtmpAudioConfig implementation
void OneSevenLiveMultiRtmpAudioConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{{"encoderId", encoderId},
                       {"encoderSettings", encoderSettings},
                       {"mixerId", mixerId},
                       {"audioTracks", audioTracks}};
}

void OneSevenLiveMultiRtmpAudioConfig::from_json(const nlohmann::json& j) {
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

// OneSevenLiveMultiRtmpConfig implementation
void OneSevenLiveMultiRtmpConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{{"id", id},
                       {"streamName", streamName},
                       {"protocol", protocol},
                       {"syncStart", syncStart},
                       {"syncStop", syncStop},
                       {"serviceSettings", serviceSettings},
                       {"outputSettings", outputSettings},
                       {"videoConfig", videoConfig.has_value() ? nlohmann::json(videoConfig.value())
                                                               : nlohmann::json(nullptr)},
                       {"audioConfig", audioConfig.has_value() ? nlohmann::json(audioConfig.value())
                                                               : nlohmann::json(nullptr)}};
}

void OneSevenLiveMultiRtmpConfig::from_json(const nlohmann::json& j) {
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
    if (j.contains("videoConfig")) {
        if (j["videoConfig"].is_null()) {
            videoConfig.reset();
        } else {
            videoConfig = j["videoConfig"].get<OneSevenLiveMultiRtmpVideoConfig>();
        }
    }
    if (j.contains("audioConfig")) {
        if (j["audioConfig"].is_null()) {
            audioConfig.reset();
        } else {
            audioConfig = j["audioConfig"].get<OneSevenLiveMultiRtmpAudioConfig>();
        }
    }
}

// OneSevenLiveMultiRtmpStreamStatus implementation
std::string OneSevenLiveMultiRtmpStreamStatus::getStateString() const {
    switch (state) {
    case STOPPED:
        return "Stopped";
    case CONNECTING:
        return "Connecting";
    case STREAMING:
        return "Streaming";
    case RECONNECTING:
        return "Reconnecting";
    case ERROR:
        return "Error";
    default:
        return "Unknown";
    }
}

bool OneSevenLiveMultiRtmpStreamStatus::isActive() const {
    return state == CONNECTING || state == STREAMING || state == RECONNECTING;
}

// OneSevenLiveMultiRtmpStreamStats implementation
std::string OneSevenLiveMultiRtmpStreamStats::getDurationString() const {
    auto totalSeconds = static_cast<int>(duration.count());
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours << ":" << std::setfill('0') << std::setw(2)
        << minutes << ":" << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}

double OneSevenLiveMultiRtmpStreamStats::getDroppedFramePercentage() const {
    if (totalFrames == 0) {
        return 0.0;
    }
    return (static_cast<double>(droppedFrames) / totalFrames) * 100.0;
}

// OneSevenLiveMultiRtmpGlobalConfig implementation
void OneSevenLiveMultiRtmpGlobalConfig::to_json(nlohmann::json& j) const {
    j = nlohmann::json{{"streams", streams}};
}

void OneSevenLiveMultiRtmpGlobalConfig::from_json(const nlohmann::json& j) {
    if (j.contains("streams")) {
        j.at("streams").get_to(streams);
    }
}

OneSevenLiveMultiRtmpConfig* OneSevenLiveMultiRtmpGlobalConfig::findStream(const std::string& streamId) {
    auto it = std::find_if(
        streams.begin(), streams.end(),
        [&streamId](const OneSevenLiveMultiRtmpConfig& config) { return config.id == streamId; });
    return (it != streams.end()) ? &(*it) : nullptr;
}

const OneSevenLiveMultiRtmpConfig* OneSevenLiveMultiRtmpGlobalConfig::findStream(
    const std::string& streamId) const {
    auto it = std::find_if(
        streams.begin(), streams.end(),
        [&streamId](const OneSevenLiveMultiRtmpConfig& config) { return config.id == streamId; });
    return (it != streams.end()) ? &(*it) : nullptr;
}

bool OneSevenLiveMultiRtmpGlobalConfig::removeStream(const std::string& streamId) {
    auto it = std::remove_if(
        streams.begin(), streams.end(),
        [&streamId](const OneSevenLiveMultiRtmpConfig& config) { return config.id == streamId; });
    if (it != streams.end()) {
        streams.erase(it);
        return true;
    }
    return false;
}

void OneSevenLiveMultiRtmpGlobalConfig::addStream(const OneSevenLiveMultiRtmpConfig& config) {
    streams.push_back(config);
}

void OneSevenLiveMultiRtmpGlobalConfig::updateStream(const std::string& streamId,
                                                     const OneSevenLiveMultiRtmpConfig& config) {
    auto* existingConfig = findStream(streamId);
    if (existingConfig) {
        *existingConfig = config;
    }
}

// Global JSON serialization functions
void to_json(nlohmann::json& j, const OneSevenLiveMultiRtmpVideoConfig& config) {
    config.to_json(j);
}

void from_json(const nlohmann::json& j, OneSevenLiveMultiRtmpVideoConfig& config) {
    config.from_json(j);
}

void to_json(nlohmann::json& j, const OneSevenLiveMultiRtmpAudioConfig& config) {
    config.to_json(j);
}

void from_json(const nlohmann::json& j, OneSevenLiveMultiRtmpAudioConfig& config) {
    config.from_json(j);
}

void to_json(nlohmann::json& j, const OneSevenLiveMultiRtmpConfig& config) {
    config.to_json(j);
}

void from_json(const nlohmann::json& j, OneSevenLiveMultiRtmpConfig& config) {
    config.from_json(j);
}

void to_json(nlohmann::json& j, const OneSevenLiveMultiRtmpGlobalConfig& config) {
    config.to_json(j);
}

void from_json(const nlohmann::json& j, OneSevenLiveMultiRtmpGlobalConfig& config) {
    config.from_json(j);
}

// Protocol helper functions implementation
const OneSevenLiveProtocol* getProtocolList() {
    return s_protocolList;
}

size_t getProtocolCount() {
    return sizeof(s_protocolList) / sizeof(s_protocolList[0]);
}

const OneSevenLiveProtocol* findProtocol(const std::string& protocol) {
    const OneSevenLiveProtocol* protocols = getProtocolList();
    size_t count = getProtocolCount();

    for (size_t i = 0; i < count; ++i) {
        if (protocols[i].protocol == protocol) {
            return &protocols[i];
        }
    }

    return nullptr;  // Protocol not found
}

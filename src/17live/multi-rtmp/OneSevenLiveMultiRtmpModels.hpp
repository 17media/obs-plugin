#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

// Forward declarations
struct OneSevenLiveMultiRtmpVideoConfig;
struct OneSevenLiveMultiRtmpAudioConfig;
struct OneSevenLiveMultiRtmpConfig;
struct OneSevenLiveMultiRtmpStreamStatus;
struct OneSevenLiveMultiRtmpStreamStats;

struct OneSevenLiveProtocol {
    const char* protocol;
    const char* label;
    const char* outputId;
    const char* serviceId;
};

/**
 * Video configuration for RTMP stream
 */
struct OneSevenLiveMultiRtmpVideoConfig {
    std::string encoderId;
    int fpsDenominator = 1;
    nlohmann::json encoderSettings;

    std::string outputScene;
    std::string resolution;

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);
};

/**
 * Audio configuration for RTMP stream
 */

struct AudioTrackConfig {
    int mixer_track;
    int output_track;
};

struct OneSevenLiveMultiRtmpAudioConfig {
    std::string encoderId;
    nlohmann::json encoderSettings;
    int mixerId = 0;
    std::vector<AudioTrackConfig> audioTracks;

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);
};

/**
 * Complete configuration for a single RTMP stream
 */
struct OneSevenLiveMultiRtmpConfig {
    std::string id;
    std::string streamName;
    std::string protocol = "rtmp";
    bool syncStart = true;
    bool syncStop = true;

    nlohmann::json serviceSettings;
    nlohmann::json outputSettings;

    std::optional<OneSevenLiveMultiRtmpVideoConfig> videoConfig;
    std::optional<OneSevenLiveMultiRtmpAudioConfig> audioConfig;

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);
};

/**
 * Stream status information
 */
struct OneSevenLiveMultiRtmpStreamStatus {
    enum State { STOPPED, CONNECTING, STREAMING, RECONNECTING, ERROR_STATE };

    std::string id;
    State state = STOPPED;
    std::string errorMessage;
    std::chrono::steady_clock::time_point startTime;

    // Helper methods
    std::string getStateString() const;
    bool isActive() const;
};

/**
 * Stream statistics information
 */
struct OneSevenLiveMultiRtmpStreamStats {
    std::string id;
    std::chrono::duration<double> duration;
    double currentBitrate = 0.0;
    double averageBitrate = 0.0;
    int currentFPS = 0;
    int droppedFrames = 0;
    int totalFrames = 0;
    double cpuUsage = 0.0;

    // Helper methods
    std::string getDurationString() const;
    double getDroppedFramePercentage() const;
};

/**
 * Multi-RTMP configuration container
 */
struct OneSevenLiveMultiRtmpGlobalConfig {
    std::vector<OneSevenLiveMultiRtmpConfig> streams;

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);

    // Helper methods
    OneSevenLiveMultiRtmpConfig* findStream(const std::string& streamId);
    const OneSevenLiveMultiRtmpConfig* findStream(const std::string& streamId) const;
    bool removeStream(const std::string& streamId);
    void addStream(const OneSevenLiveMultiRtmpConfig& config);
    void updateStream(const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config);
};

// JSON serialization helpers
void to_json(nlohmann::json& j, const OneSevenLiveMultiRtmpVideoConfig& config);
void from_json(const nlohmann::json& j, OneSevenLiveMultiRtmpVideoConfig& config);

void to_json(nlohmann::json& j, const OneSevenLiveMultiRtmpAudioConfig& config);
void from_json(const nlohmann::json& j, OneSevenLiveMultiRtmpAudioConfig& config);

void to_json(nlohmann::json& j, const OneSevenLiveMultiRtmpConfig& config);
void from_json(const nlohmann::json& j, OneSevenLiveMultiRtmpConfig& config);

void to_json(nlohmann::json& j, const OneSevenLiveMultiRtmpGlobalConfig& config);
void from_json(const nlohmann::json& j, OneSevenLiveMultiRtmpGlobalConfig& config);

// Protocol helper functions
const OneSevenLiveProtocol* getProtocolList();
size_t getProtocolCount();
const OneSevenLiveProtocol* findProtocol(const std::string& protocol);

std::string get_protocol_from_settings(const nlohmann::json& j);
std::string get_url_from_settings(const nlohmann::json& j);
std::string get_key_from_settings(const nlohmann::json& j);

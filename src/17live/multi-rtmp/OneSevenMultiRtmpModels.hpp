#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <nlohmann/json.hpp>

// Forward declarations
struct OneSevenMultiRtmpVideoConfig;
struct OneSevenMultiRtmpAudioConfig;
struct OneSevenMultiRtmpConfig;
struct OneSevenMultiRtmpStreamStatus;
struct OneSevenMultiRtmpStreamStats;


struct OneSevenLiveProtocol {
    const char* protocol;
    const char* label;
    const char* outputId;
    const char* serviceId;
};

/**
 * Video configuration for RTMP stream
 */
struct OneSevenMultiRtmpVideoConfig {
    std::string id;
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

struct OneSevenMultiRtmpAudioConfig {
    std::string id;
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
struct OneSevenMultiRtmpConfig {
    std::string id;
    std::string streamName;
    std::string protocol = "rtmp";
    bool syncStart = true;
    bool syncStop = true;

    nlohmann::json serviceSettings;
    nlohmann::json outputSettings;
    
    std::string videoConfigId;
    std::string audioConfigId;

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);
};

/**
 * Stream status information
 */
struct OneSevenMultiRtmpStreamStatus {
    enum State {
        STOPPED,
        CONNECTING,
        STREAMING,
        RECONNECTING,
        ERROR
    };

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
struct OneSevenMultiRtmpStreamStats {
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
struct OneSevenMultiRtmpGlobalConfig {
    std::vector<OneSevenMultiRtmpConfig> streams;
    std::vector<OneSevenMultiRtmpAudioConfig> audioConfigs;
    std::vector<OneSevenMultiRtmpVideoConfig> videoConfigs;

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);

    // Helper methods
    OneSevenMultiRtmpConfig* findStream(const std::string& streamId);
    const OneSevenMultiRtmpConfig* findStream(const std::string& streamId) const;
    bool removeStream(const std::string& streamId);
    void addStream(const OneSevenMultiRtmpConfig& config);
    void updateStream(const std::string& streamId, const OneSevenMultiRtmpConfig& config);
};

// JSON serialization helpers
void to_json(nlohmann::json& j, const OneSevenMultiRtmpVideoConfig& config);
void from_json(const nlohmann::json& j, OneSevenMultiRtmpVideoConfig& config);

void to_json(nlohmann::json& j, const OneSevenMultiRtmpAudioConfig& config);
void from_json(const nlohmann::json& j, OneSevenMultiRtmpAudioConfig& config);

void to_json(nlohmann::json& j, const OneSevenMultiRtmpConfig& config);
void from_json(const nlohmann::json& j, OneSevenMultiRtmpConfig& config);

void to_json(nlohmann::json& j, const OneSevenMultiRtmpGlobalConfig& config);
void from_json(const nlohmann::json& j, OneSevenMultiRtmpGlobalConfig& config);

// Protocol helper functions
const OneSevenLiveProtocol* getProtocolList();
size_t getProtocolCount();

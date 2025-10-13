#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <nlohmann/json.hpp>

// Forward declarations
struct OneSevenMultiRtmpServiceConfig;
struct OneSevenMultiRtmpOutputConfig;
struct OneSevenMultiRtmpVideoConfig;
struct OneSevenMultiRtmpAudioConfig;
struct OneSevenMultiRtmpSyncConfig;
struct OneSevenMultiRtmpConfig;
struct OneSevenMultiRtmpStreamStatus;
struct OneSevenMultiRtmpStreamStats;

/**
 * Service configuration for RTMP stream
 */
struct OneSevenMultiRtmpServiceConfig {
    std::string serverUrl;
    std::string streamKey;
    bool useAuthentication = false;
    std::string authToken;

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);
};

/**
 * Output configuration for RTMP stream
 */
struct OneSevenMultiRtmpOutputConfig {
    int bufferSize = 2048;
    int reconnectDelay = 5;
    bool autoReconnect = true;
    std::string bindIP = "default";

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);
};

/**
 * Video configuration for RTMP stream
 */
struct OneSevenMultiRtmpVideoConfig {
    bool useSharedEncoder = true;
    std::string encoderId;
    std::string sceneId = "default";
    int outputWidth = 1920;
    int outputHeight = 1080;
    int fpsDenominator = 1;
    std::string rateControl = "ABR";
    int bitrate = 6000;
    bool rateLimiting = false;
    int keyframeInterval = 2;
    std::string profile = "high";
    bool useBFrames = true;

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);
};

/**
 * Audio configuration for RTMP stream
 */
struct OneSevenMultiRtmpAudioConfig {
    bool useSharedEncoder = true;
    std::string encoderId;
    int mixerId = 1;
    std::string vodTrack = "default";
    int sampleRate = 48000;
    int bitrate = 128;
    bool useHEAAC = false;

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);
};

/**
 * Synchronization configuration for RTMP stream
 */
struct OneSevenMultiRtmpSyncConfig {
    bool syncStart = true;
    bool syncStop = true;

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
    OneSevenMultiRtmpServiceConfig service;
    OneSevenMultiRtmpOutputConfig output;
    OneSevenMultiRtmpVideoConfig video;
    OneSevenMultiRtmpAudioConfig audio;
    OneSevenMultiRtmpSyncConfig sync;
    std::string createdAt;
    std::string updatedAt;

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);

    // Validation
    bool isValid() const;
    std::string getValidationError() const;
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
    std::string version = "1.0.0";
    std::string lastModified;
    std::vector<OneSevenMultiRtmpConfig> streams;

    // JSON serialization
    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);

    // Helper methods
    void updateLastModified();
    OneSevenMultiRtmpConfig* findStream(const std::string& streamId);
    const OneSevenMultiRtmpConfig* findStream(const std::string& streamId) const;
    bool removeStream(const std::string& streamId);
    void addStream(const OneSevenMultiRtmpConfig& config);
    void updateStream(const std::string& streamId, const OneSevenMultiRtmpConfig& config);
};

// JSON serialization helpers
void to_json(nlohmann::json& j, const OneSevenMultiRtmpServiceConfig& config);
void from_json(const nlohmann::json& j, OneSevenMultiRtmpServiceConfig& config);

void to_json(nlohmann::json& j, const OneSevenMultiRtmpOutputConfig& config);
void from_json(const nlohmann::json& j, OneSevenMultiRtmpOutputConfig& config);

void to_json(nlohmann::json& j, const OneSevenMultiRtmpVideoConfig& config);
void from_json(const nlohmann::json& j, OneSevenMultiRtmpVideoConfig& config);

void to_json(nlohmann::json& j, const OneSevenMultiRtmpAudioConfig& config);
void from_json(const nlohmann::json& j, OneSevenMultiRtmpAudioConfig& config);

void to_json(nlohmann::json& j, const OneSevenMultiRtmpSyncConfig& config);
void from_json(const nlohmann::json& j, OneSevenMultiRtmpSyncConfig& config);

void to_json(nlohmann::json& j, const OneSevenMultiRtmpConfig& config);
void from_json(const nlohmann::json& j, OneSevenMultiRtmpConfig& config);

void to_json(nlohmann::json& j, const OneSevenMultiRtmpGlobalConfig& config);
void from_json(const nlohmann::json& j, OneSevenMultiRtmpGlobalConfig& config);

#pragma once

#include <obs-module.h>
#include <obs.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <QTimer>

#include <QTimer>

#include "OneSevenLiveMultiRtmpModels.hpp"
#include "plugin-support.h"

#include <string>

// Forward declarations for async platform resolution
class OneSevenLiveYouTubeClient;
class OneSevenLiveTwitchClient;

/**
 * Stream Controller for Multi-RTMP functionality
 * Handles ONLY OBS runtime operations (service/output creation and management)
 * Does NOT handle configuration storage or JSON operations
 */
class OneSevenLiveMultiRtmpStreamController {
   public:
    // Callback types for stream events
    using StreamStatusCallback = std::function<void(const std::string& streamId,
                                                    const OneSevenLiveMultiRtmpStreamStatus& status)>;
    using StreamStatsCallback =
        std::function<void(const std::string& streamId, const OneSevenLiveMultiRtmpStreamStats& stats)>;

    OneSevenLiveMultiRtmpStreamController();
    ~OneSevenLiveMultiRtmpStreamController();

    // OBS output lifecycle management
    bool createOutput(const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config);
    bool startOutput(const std::string& streamId);
    bool stopOutput(const std::string& streamId);
    bool destroyOutput(const std::string& streamId);

    // Bulk operations
    bool startAllOutputs();
    bool stopAllOutputs();
    void destroyAllOutputs();

    // Status and statistics
    OneSevenLiveMultiRtmpStreamStatus getStreamStatus(const std::string& streamId) const;
    OneSevenLiveMultiRtmpStreamStats getStreamStats(const std::string& streamId) const;
    std::vector<std::string> getActiveStreamIds() const;
    std::vector<std::string> getAllStreamIds() const;

    // OBS encoder sharing
    obs_encoder_t* getSharedVideoEncoder();
    obs_encoder_t* getSharedAudioEncoder(int mixerId = 1);

    // Stream management
    bool isStreamActive(const std::string& streamId) const;
    bool hasOutput(const std::string& streamId) const;

    // OBS output access
    obs_output_t* getStreamOutput(const std::string& streamId) const;

    // Callback registration
    void setStreamStatusCallback(StreamStatusCallback callback);
    void setStreamStatsCallback(StreamStatsCallback callback);

    // Statistics monitoring
    void startStatsMonitoring();
    void stopStatsMonitoring();

   private:
    // Internal structures
    struct StreamOutput {
        obs_output_t* output = nullptr;
        obs_service_t* service = nullptr;
        obs_encoder_t* videoEncoder = nullptr;
        obs_encoder_t* audioEncoder = nullptr;
        OneSevenLiveMultiRtmpConfig config;
        OneSevenLiveMultiRtmpStreamStatus status;
        OneSevenLiveMultiRtmpStreamStats stats;
        std::chrono::steady_clock::time_point startTime;
        QTimer* connectTimeoutTimer = nullptr;
    };

    // Internal implementation methods
    bool createService(const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config,
                       StreamOutput* streamOutput);
    bool createEncoders(const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config,
                        StreamOutput* streamOutput);
    bool setupOutput(const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config,
                     StreamOutput* streamOutput);
    bool startOutputInternal(const std::string& streamId, StreamOutput* streamOutput);
    bool stopOutputInternal(const std::string& streamId, StreamOutput* streamOutput);

    void destroyService(const std::string& streamId);
    void destroyEncoders(const std::string& streamId);

    void updateStreamStatus(const std::string& streamId, OneSevenLiveMultiRtmpStreamStatus::State state,
                            const std::string& error = "");
    void updateStreamStats(const std::string& streamId);

    // OBS callbacks
    static void outputStartCallback(void* data, calldata_t* cd);
    static void outputStopCallback(void* data, calldata_t* cd);
    static void outputReconnectCallback(void* data, calldata_t* cd);
    static void outputReconnectSuccessCallback(void* data, calldata_t* cd);

    // Statistics monitoring
    void statsMonitoringThread();
    void collectStreamStats(const std::string& streamId, StreamOutput& streamOutput);

    // Helper methods
    std::string getOutputName(const std::string& streamId) const;
    std::string getServiceName(const std::string& streamId) const;
    std::string getVideoEncoderName(const std::string& streamId) const;
    std::string getAudioEncoderName(const std::string& streamId) const;

    obs_data_t* createServiceSettings(const OneSevenLiveMultiRtmpConfig& config) const;
    obs_data_t* createOutputSettings(const OneSevenLiveMultiRtmpConfig& config) const;
    obs_data_t* createVideoEncoderSettings(const OneSevenLiveMultiRtmpConfig& config) const;
    obs_data_t* createAudioEncoderSettings(const OneSevenLiveMultiRtmpConfig& config) const;

    // Async platform resolution
    void resolvePlatformServerKeyAsync(const std::string& streamId,
                                       const OneSevenLiveMultiRtmpConfig& config);
    void finalizeServiceSetupAfterResolve(const std::string& streamId,
                                          const std::string& server,
                                          const std::string& key);

    // Helper methods for getting OBS default encoder settings
    obs_data_t* getObsDefaultVideoEncoderSettings() const;
    obs_data_t* getObsDefaultAudioEncoderSettings() const;
    const char* getObsDefaultVideoEncoderId() const;

    // Member variables
    std::map<std::string, std::unique_ptr<StreamOutput>> m_streamOutputs;
    mutable std::mutex m_outputsMutex;

    // Pending async resolution clients per stream
    std::map<std::string, std::unique_ptr<OneSevenLiveYouTubeClient>> m_pendingYouTubeClients;
    std::map<std::string, std::unique_ptr<OneSevenLiveTwitchClient>> m_pendingTwitchClients;

    // Shared encoders
    obs_encoder_t* m_sharedVideoEncoder = nullptr;
    std::map<int, obs_encoder_t*> m_sharedAudioEncoders;

    // Callbacks
    StreamStatusCallback m_statusCallback;
    StreamStatsCallback m_statsCallback;

    // Statistics monitoring
    std::atomic<bool> m_statsMonitoringActive{false};
    std::thread m_statsThread;
    std::mutex m_statsThreadMutex;

    // Constants
    static constexpr int STATS_UPDATE_INTERVAL_MS = 1000;
    static constexpr int CONNECT_TIMEOUT_MS = 60000;
    static constexpr const char* OUTPUT_ID = "rtmp_output";
    static constexpr const char* SERVICE_ID = "rtmp_common";
    static constexpr const char* VIDEO_ENCODER_ID = "obs_x264";
    static constexpr const char* AUDIO_ENCODER_ID = "ffmpeg_aac";
};

// Logging macros for stream controller
#define MULTI_RTMP_STREAM_LOG(level, format, ...) \
    obs_log(level, "[MultiRTMP-Stream] " format, ##__VA_ARGS__)

#define MULTI_RTMP_STREAM_LOG_INFO(format, ...) \
    MULTI_RTMP_STREAM_LOG(LOG_INFO, format, ##__VA_ARGS__)

#define MULTI_RTMP_STREAM_LOG_WARNING(format, ...) \
    MULTI_RTMP_STREAM_LOG(LOG_WARNING, format, ##__VA_ARGS__)

#define MULTI_RTMP_STREAM_LOG_ERROR(format, ...) \
    MULTI_RTMP_STREAM_LOG(LOG_ERROR, format, ##__VA_ARGS__)

#define MULTI_RTMP_STREAM_LOG_DEBUG(format, ...) \
    MULTI_RTMP_STREAM_LOG(LOG_DEBUG, format, ##__VA_ARGS__)
class QTimer;

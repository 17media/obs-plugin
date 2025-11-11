#pragma once

#include <obs-module.h>

#include <functional>
#include <memory>
#include <mutex>

#include "OneSevenLiveMultiRtmpConfigManager.hpp"
#include "OneSevenLiveMultiRtmpModels.hpp"
#include "OneSevenLiveMultiRtmpStreamController.hpp"
#include "plugin-support.h"

/**
 * Main Manager for Multi-RTMP functionality
 * Coordinates between configuration management and OBS stream operations
 * Provides a unified interface for the UI layer
 */
class OneSevenLiveMultiRtmpManager {
   public:
    // Callback types for UI notifications
    using StreamStatusCallback = std::function<void(const std::string& streamId,
                                                    const OneSevenLiveMultiRtmpStreamStatus& status)>;
    using StreamStatsCallback =
        std::function<void(const std::string& streamId, const OneSevenLiveMultiRtmpStreamStats& stats)>;
    using ConfigChangeCallback =
        std::function<void(const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config)>;
    using ConfigDeleteCallback = std::function<void(const std::string& streamId)>;

    OneSevenLiveMultiRtmpManager();
    ~OneSevenLiveMultiRtmpManager();

    // Singleton access
    static OneSevenLiveMultiRtmpManager* getInstance();
    static void destroyInstance();

    // Initialization and cleanup
    bool initialize();
    void shutdown();

    // Configuration operations (delegates to ConfigManager)
    bool addStreamConfig(const OneSevenLiveMultiRtmpConfig& config);
    bool removeStreamConfig(const std::string& streamId);
    bool updateStreamConfig(const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config);
    std::vector<OneSevenLiveMultiRtmpConfig> getAllStreamConfigs() const;
    OneSevenLiveMultiRtmpConfig getStreamConfig(const std::string& streamId) const;
    bool hasStreamConfig(const std::string& streamId) const;

    // Runtime operations (delegates to StreamController)
    bool startStream(const std::string& streamId);
    bool stopStream(const std::string& streamId);
    bool startAllStreams();
    bool stopAllStreams();

    // Status and statistics monitoring
    OneSevenLiveMultiRtmpStreamStatus getStreamStatus(const std::string& streamId) const;
    OneSevenLiveMultiRtmpStreamStats getStreamStats(const std::string& streamId) const;
    std::vector<std::string> getActiveStreamIds() const;
    std::vector<std::string> getAllStreamIds() const;

    // Utility methods
    std::string generateStreamId() const;
    size_t getStreamCount() const;

    // Configuration file operations
    bool saveConfiguration();
    bool loadConfiguration();
    bool createConfigBackup();
    bool restoreFromBackup();

    // Callback registration for UI updates
    void setStreamStatusCallback(StreamStatusCallback callback);
    void setStreamStatsCallback(StreamStatsCallback callback);
    void setConfigChangeCallback(ConfigChangeCallback callback);
    void setConfigDeleteCallback(ConfigDeleteCallback callback);

    // Stream lifecycle management
    bool createStreamOutput(const std::string& streamId);
    bool destroyStreamOutput(const std::string& streamId);
    void destroyAllStreamOutputs();

    // Bulk operations with synchronization
    bool startAllStreamsWithSync();
    bool stopAllStreamsWithSync();

    // Statistics monitoring control
    void startStatsMonitoring();
    void stopStatsMonitoring();

    // State management
    bool isInitialized() const {
        return m_initialized;
    }

    bool isStreamActive(const std::string& streamId) const;
    bool hasStreamOutput(const std::string& streamId) const;

    // Get stream output for real-time statistics
    obs_output_t* getStreamOutput(const std::string& streamId) const;

   private:
    // Internal callback handlers
    void onConfigChanged(const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config);
    void onConfigDeleted(const std::string& streamId);
    void onStreamStatusChanged(const std::string& streamId,
                               const OneSevenLiveMultiRtmpStreamStatus& status);
    void onStreamStatsUpdated(const std::string& streamId,
                              const OneSevenLiveMultiRtmpStreamStats& stats);

    // Helper methods
    void setupCallbacks();
    void cleanupCallbacks();
    bool ensureStreamOutput(const std::string& streamId);

    // Member variables
    std::unique_ptr<OneSevenLiveMultiRtmpConfigManager> m_configManager;
    std::unique_ptr<OneSevenLiveMultiRtmpStreamController> m_streamController;

    // UI callbacks
    StreamStatusCallback m_statusCallback;
    StreamStatsCallback m_statsCallback;
    ConfigChangeCallback m_configChangeCallback;
    ConfigDeleteCallback m_configDeleteCallback;

    // State
    bool m_initialized = false;

    // Singleton instance
    static OneSevenLiveMultiRtmpManager* s_instance;
    static std::mutex s_instanceMutex;
};

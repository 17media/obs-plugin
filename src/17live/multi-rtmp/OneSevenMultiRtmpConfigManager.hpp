#pragma once

#include "OneSevenMultiRtmpModels.hpp"
#include "plugin-support.h"
#include <obs-module.h>
#include <string>
#include <memory>
#include <functional>

/**
 * Configuration Manager for Multi-RTMP functionality
 * Handles ONLY JSON file operations and configuration CRUD
 * Does NOT handle any OBS runtime operations
 */
class OneSevenMultiRtmpConfigManager {
public:
    // Callback types for configuration changes
    using ConfigChangeCallback = std::function<void(const std::string& streamId, const OneSevenMultiRtmpConfig& config)>;
    using ConfigDeleteCallback = std::function<void(const std::string& streamId)>;

    explicit OneSevenMultiRtmpConfigManager();
    ~OneSevenMultiRtmpConfigManager();

    // Configuration file operations
    bool loadConfiguration();
    bool saveConfiguration();
    std::string getConfigFilePath() const;

    // Stream configuration CRUD operations (JSON only)
    bool addStreamConfig(const OneSevenMultiRtmpConfig& config);
    bool removeStreamConfig(const std::string& streamId);
    bool updateStreamConfig(const std::string& streamId, const OneSevenMultiRtmpConfig& config);
    std::vector<OneSevenMultiRtmpConfig> getStreamConfigs() const;
    OneSevenMultiRtmpConfig getStreamConfig(const std::string& streamId) const;
    bool hasStreamConfig(const std::string& streamId) const;

    // Validation
    bool validateStreamConfig(const OneSevenMultiRtmpConfig& config) const;
    std::string getValidationError(const OneSevenMultiRtmpConfig& config) const;

    // Utility methods
    std::string generateStreamId() const;
    std::string getCurrentTimestamp() const;
    size_t getStreamCount() const;
    std::vector<std::string> getStreamIds() const;

    // Callback registration for configuration changes
    void setConfigChangeCallback(ConfigChangeCallback callback);
    void setConfigDeleteCallback(ConfigDeleteCallback callback);

    // Configuration backup and restore
    bool createBackup() const;
    bool restoreFromBackup();
    std::vector<std::string> getAvailableBackups() const;

private:
    // Helper methods
    bool ensureConfigDirectoryExists() const;
    bool writeConfigToFile(const OneSevenMultiRtmpGlobalConfig& config) const;
    bool readConfigFromFile(OneSevenMultiRtmpGlobalConfig& config) const;
    std::string getBackupFilePath(const std::string& timestamp) const;
    void notifyConfigChange(const std::string& streamId, const OneSevenMultiRtmpConfig& config);
    void notifyConfigDelete(const std::string& streamId);
    
    // Internal methods
    bool saveConfigurationInternal();
    bool loadConfigurationInternal();

    // Member variables
    std::string m_configFilePath;
    std::string m_configDirectory;
    OneSevenMultiRtmpGlobalConfig m_globalConfig;
    
    // Callbacks
    ConfigChangeCallback m_configChangeCallback;
    ConfigDeleteCallback m_configDeleteCallback;

    // Constants
    static constexpr const char* CONFIG_FILE_NAME = "17live_multi_rtmp.json";
    static constexpr const char* CONFIG_BACKUP_PREFIX = "17live_multi_rtmp_backup_";
    static constexpr const char* CONFIG_BACKUP_EXTENSION = ".json";
};

// Logging macros for consistent usage
#define MULTI_RTMP_CONFIG_LOG(level, format, ...) \
    obs_log(level, "[MultiRTMP-Config] " format, ##__VA_ARGS__)

#define MULTI_RTMP_CONFIG_LOG_INFO(format, ...) \
    MULTI_RTMP_CONFIG_LOG(LOG_INFO, format, ##__VA_ARGS__)

#define MULTI_RTMP_CONFIG_LOG_WARNING(format, ...) \
    MULTI_RTMP_CONFIG_LOG(LOG_WARNING, format, ##__VA_ARGS__)

#define MULTI_RTMP_CONFIG_LOG_ERROR(format, ...) \
    MULTI_RTMP_CONFIG_LOG(LOG_ERROR, format, ##__VA_ARGS__)

#define MULTI_RTMP_CONFIG_LOG_DEBUG(format, ...) \
    MULTI_RTMP_CONFIG_LOG(LOG_DEBUG, format, ##__VA_ARGS__)

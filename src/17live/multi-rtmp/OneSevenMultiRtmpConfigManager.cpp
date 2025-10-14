#include "OneSevenMultiRtmpConfigManager.hpp"
#include <util/config-file.h>
#include <fstream>
#include <filesystem>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

OneSevenMultiRtmpConfigManager::OneSevenMultiRtmpConfigManager() {
    // Get the configuration directory from OBS
    char* configDir = obs_module_config_path("");
    if (configDir) {
        m_configDirectory = std::string(configDir);
        bfree(configDir);
    } else {
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to get OBS config directory");
        m_configDirectory = ".";
    }

    m_configFilePath = m_configDirectory + "/" + CONFIG_FILE_NAME;
    
    MULTI_RTMP_CONFIG_LOG_INFO("Configuration manager initialized with path: %s", m_configFilePath.c_str());
    
    // Ensure config directory exists
    ensureConfigDirectoryExists();
    
    // Load existing configuration
    loadConfiguration();
}

OneSevenMultiRtmpConfigManager::~OneSevenMultiRtmpConfigManager() {
    // Save configuration on destruction
    saveConfiguration();
    MULTI_RTMP_CONFIG_LOG_INFO("Configuration manager destroyed");
}

bool OneSevenMultiRtmpConfigManager::loadConfiguration() {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return loadConfigurationInternal();
}

bool OneSevenMultiRtmpConfigManager::saveConfiguration() {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    m_globalConfig.updateLastModified();
    
    if (!writeConfigToFile(m_globalConfig)) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to save configuration to file: %s", m_configFilePath.c_str());
        return false;
    }

    MULTI_RTMP_CONFIG_LOG_INFO("Configuration saved successfully");
    return true;
}

std::string OneSevenMultiRtmpConfigManager::getConfigFilePath() const {
    return m_configFilePath;
}

bool OneSevenMultiRtmpConfigManager::addStreamConfig(const OneSevenMultiRtmpConfig& config)
{
    MULTI_RTMP_CONFIG_LOG_DEBUG("addStreamConfig called for stream ID: %s", config.id.c_str());
    
    if (!validateStreamConfig(config)) {
        std::string error = getValidationError(config);
        MULTI_RTMP_CONFIG_LOG_ERROR("Configuration validation failed: %s", error.c_str());
        return false;
    }
    
    OneSevenMultiRtmpConfig newConfig = config;
    
    // Critical section - hold lock only for data modification
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        
        // Check if stream already exists
        if (m_globalConfig.findStream(config.id) != nullptr) {
            MULTI_RTMP_CONFIG_LOG_ERROR("Stream with ID already exists: %s", config.id.c_str());
            return false;
        }
        
        MULTI_RTMP_CONFIG_LOG_DEBUG("Adding stream to global config");
        
        // Add timestamps
        std::string timestamp = getCurrentTimestamp();
        newConfig.createdAt = timestamp;
        newConfig.updatedAt = timestamp;
        
        // Add to global config
        m_globalConfig.streams.push_back(newConfig);
        
        MULTI_RTMP_CONFIG_LOG_INFO("Stream configuration added: %s (%s)", 
                                   newConfig.streamName.c_str(), newConfig.id.c_str());
    }
    // Lock released here
    
    // Save configuration outside of lock
    if (!saveConfigurationInternal()) {
        // Rollback: remove the added stream
        std::lock_guard<std::mutex> lock(m_configMutex);
        auto it = std::find_if(m_globalConfig.streams.begin(), m_globalConfig.streams.end(),
                               [&config](const OneSevenMultiRtmpConfig& stream) {
                                   return stream.id == config.id;
                               });
        if (it != m_globalConfig.streams.end()) {
            m_globalConfig.streams.erase(it);
        }
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to save configuration, rolled back stream addition");
        return false;
    }
    
    // Notify callback outside of lock to prevent deadlock
    notifyConfigChange(newConfig.id, newConfig);
    
    MULTI_RTMP_CONFIG_LOG_DEBUG("Stream configuration added and saved successfully");
    return true;
}

bool OneSevenMultiRtmpConfigManager::removeStreamConfig(const std::string& streamId) {
    OneSevenMultiRtmpConfig removedConfig;
    bool found = false;
    
    // Critical section - hold lock only for data modification
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        
        // Find and store the config before removal for potential rollback
        auto* configToRemove = m_globalConfig.findStream(streamId);
        if (!configToRemove) {
            MULTI_RTMP_CONFIG_LOG_ERROR("Stream with ID %s not found", streamId.c_str());
            return false;
        }
        
        removedConfig = *configToRemove;
        found = true;
        
        // Remove from global config
        auto it = std::find_if(m_globalConfig.streams.begin(), m_globalConfig.streams.end(),
                               [&streamId](const OneSevenMultiRtmpConfig& stream) {
                                   return stream.id == streamId;
                               });
        if (it != m_globalConfig.streams.end()) {
            m_globalConfig.streams.erase(it);
        }

        MULTI_RTMP_CONFIG_LOG_INFO("Stream configuration removed: %s", streamId.c_str());
    }
    // Lock released here
    
    if (!found) {
        return false;
    }
    
    // Save configuration outside of lock
    if (!saveConfigurationInternal()) {
        // Rollback: restore the removed stream
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_globalConfig.streams.push_back(removedConfig);
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to save configuration, rolled back stream removal");
        return false;
    }
    
    // Notify callback outside of lock to prevent deadlock
    notifyConfigDelete(streamId);
    
    return true;
}

bool OneSevenMultiRtmpConfigManager::updateStreamConfig(const std::string& streamId, const OneSevenMultiRtmpConfig& config) {
    MULTI_RTMP_CONFIG_LOG_DEBUG("updateStreamConfig called for stream ID: %s", streamId.c_str());
    
    if (!validateStreamConfig(config)) {
        std::string error = getValidationError(config);
        MULTI_RTMP_CONFIG_LOG_ERROR("Configuration validation failed: %s", error.c_str());
        return false;
    }
    
    OneSevenMultiRtmpConfig updatedConfig = config;
    OneSevenMultiRtmpConfig originalConfig;
    bool found = false;
    
    // Critical section - hold lock only for data modification
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        
        auto* existingConfig = m_globalConfig.findStream(streamId);
        if (!existingConfig) {
            MULTI_RTMP_CONFIG_LOG_ERROR("Stream configuration not found for update: %s", streamId.c_str());
            return false;
        }
        
        // Store original config for potential rollback
        originalConfig = *existingConfig;
        found = true;
        
        // Update timestamps
        updatedConfig.createdAt = existingConfig->createdAt; // Keep original creation time
        updatedConfig.updatedAt = getCurrentTimestamp();
        
        // Update the configuration
        *existingConfig = updatedConfig;
        
        MULTI_RTMP_CONFIG_LOG_INFO("Stream configuration updated: %s (%s)", 
                                   updatedConfig.streamName.c_str(), streamId.c_str());
    }
    // Lock released here
    
    if (!found) {
        return false;
    }
    
    // Save configuration outside of lock
    if (!saveConfigurationInternal()) {
        // Rollback: restore original configuration
        std::lock_guard<std::mutex> lock(m_configMutex);
        auto* configToRestore = m_globalConfig.findStream(streamId);
        if (configToRestore) {
            *configToRestore = originalConfig;
        }
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to save configuration, rolled back stream update");
        return false;
    }
    
    // Notify callback outside of lock to prevent deadlock
    notifyConfigChange(streamId, updatedConfig);
    
    MULTI_RTMP_CONFIG_LOG_DEBUG("Stream configuration updated and saved successfully");
    return true;
}

std::vector<OneSevenMultiRtmpConfig> OneSevenMultiRtmpConfigManager::getStreamConfigs() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_globalConfig.streams;
}

OneSevenMultiRtmpConfig OneSevenMultiRtmpConfigManager::getStreamConfig(const std::string& streamId) const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    const auto* config = m_globalConfig.findStream(streamId);
    if (config) {
        return *config;
    }
    
    MULTI_RTMP_CONFIG_LOG_WARNING("Stream configuration not found: %s", streamId.c_str());
    return OneSevenMultiRtmpConfig();
}

bool OneSevenMultiRtmpConfigManager::hasStreamConfig(const std::string& streamId) const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_globalConfig.findStream(streamId) != nullptr;
}

bool OneSevenMultiRtmpConfigManager::validateStreamConfig(const OneSevenMultiRtmpConfig& config) const {
    return config.isValid();
}

std::string OneSevenMultiRtmpConfigManager::getValidationError(const OneSevenMultiRtmpConfig& config) const {
    return config.getValidationError();
}

std::string OneSevenMultiRtmpConfigManager::generateStreamId() const {
    // Generate a UUID-like string
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4"; // Version 4 UUID
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << (8 + (dis(gen) & 3)); // Variant bits
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; i++) {
        ss << dis(gen);
    }
    
    return ss.str();
}

std::string OneSevenMultiRtmpConfigManager::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

size_t OneSevenMultiRtmpConfigManager::getStreamCount() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_globalConfig.streams.size();
}

std::vector<std::string> OneSevenMultiRtmpConfigManager::getStreamIds() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    std::vector<std::string> ids;
    ids.reserve(m_globalConfig.streams.size());
    
    for (const auto& stream : m_globalConfig.streams) {
        ids.push_back(stream.id);
    }
    
    return ids;
}

void OneSevenMultiRtmpConfigManager::setConfigChangeCallback(ConfigChangeCallback callback) {
    m_configChangeCallback = callback;
}

void OneSevenMultiRtmpConfigManager::setConfigDeleteCallback(ConfigDeleteCallback callback) {
    m_configDeleteCallback = callback;
}

bool OneSevenMultiRtmpConfigManager::createBackup() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    std::string timestamp = getCurrentTimestamp();
    std::replace(timestamp.begin(), timestamp.end(), ':', '-');
    std::string backupPath = getBackupFilePath(timestamp);
    
    try {
        std::filesystem::copy_file(m_configFilePath, backupPath);
        MULTI_RTMP_CONFIG_LOG_INFO("Configuration backup created: %s", backupPath.c_str());
        return true;
    } catch (const std::exception& e) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to create backup: %s", e.what());
        return false;
    }
}

bool OneSevenMultiRtmpConfigManager::restoreFromBackup() {
    // Find the most recent backup
    auto backups = getAvailableBackups();
    if (backups.empty()) {
        MULTI_RTMP_CONFIG_LOG_ERROR("No backups available");
        return false;
    }
    
    std::string latestBackup = backups.back(); // Assuming sorted by timestamp
    std::string backupPath = m_configDirectory + "/" + latestBackup;
    
    try {
        std::filesystem::copy_file(backupPath, m_configFilePath, std::filesystem::copy_options::overwrite_existing);
        MULTI_RTMP_CONFIG_LOG_INFO("Configuration restored from backup: %s", latestBackup.c_str());
        return loadConfiguration();
    } catch (const std::exception& e) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to restore from backup: %s", e.what());
        return false;
    }
}

std::vector<std::string> OneSevenMultiRtmpConfigManager::getAvailableBackups() const {
    std::vector<std::string> backups;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_configDirectory)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.substr(0, strlen(CONFIG_BACKUP_PREFIX)) == CONFIG_BACKUP_PREFIX && 
                    filename.size() >= strlen(CONFIG_BACKUP_EXTENSION) &&
                    filename.substr(filename.size() - strlen(CONFIG_BACKUP_EXTENSION)) == CONFIG_BACKUP_EXTENSION) {
                    backups.push_back(filename);
                }
            }
        }
        
        // Sort backups by timestamp (filename contains timestamp)
        std::sort(backups.begin(), backups.end());
    } catch (const std::exception& e) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to list backups: %s", e.what());
    }
    
    return backups;
}

bool OneSevenMultiRtmpConfigManager::ensureConfigDirectoryExists() const {
    try {
        if (!std::filesystem::exists(m_configDirectory)) {
            std::filesystem::create_directories(m_configDirectory);
            MULTI_RTMP_CONFIG_LOG_INFO("Created configuration directory: %s", m_configDirectory.c_str());
        }
        return true;
    } catch (const std::exception& e) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to create configuration directory: %s", e.what());
        return false;
    }
}

bool OneSevenMultiRtmpConfigManager::writeConfigToFile(const OneSevenMultiRtmpGlobalConfig& config) const {
    try {
        nlohmann::json j = config;
        std::ofstream file(m_configFilePath);
        if (!file.is_open()) {
            MULTI_RTMP_CONFIG_LOG_ERROR("Failed to open config file for writing: %s", m_configFilePath.c_str());
            return false;
        }
        
        file << j.dump(4); // Pretty print with 4 spaces
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to write configuration: %s", e.what());
        return false;
    }
}

bool OneSevenMultiRtmpConfigManager::readConfigFromFile(OneSevenMultiRtmpGlobalConfig& config) const {
    try {
        std::ifstream file(m_configFilePath);
        if (!file.is_open()) {
            MULTI_RTMP_CONFIG_LOG_ERROR("Failed to open config file for reading: %s", m_configFilePath.c_str());
            return false;
        }
        
        nlohmann::json j;
        file >> j;
        file.close();
        
        config = j.get<OneSevenMultiRtmpGlobalConfig>();
        
        return true;
    } catch (const std::exception& e) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to read configuration: %s", e.what());
        return false;
    }
}

std::string OneSevenMultiRtmpConfigManager::getBackupFilePath(const std::string& timestamp) const {
    return m_configDirectory + "/" + CONFIG_BACKUP_PREFIX + timestamp + CONFIG_BACKUP_EXTENSION;
}

void OneSevenMultiRtmpConfigManager::notifyConfigChange(const std::string& streamId, const OneSevenMultiRtmpConfig& config) {
    if (m_configChangeCallback) {
        m_configChangeCallback(streamId, config);
    }
}

void OneSevenMultiRtmpConfigManager::notifyConfigDelete(const std::string& streamId) {
    if (m_configDeleteCallback) {
        m_configDeleteCallback(streamId);
    }
}

bool OneSevenMultiRtmpConfigManager::saveConfigurationInternal() {
    // This method can be called without holding the mutex
    // It will acquire its own lock for thread safety
    std::lock_guard<std::mutex> lock(m_configMutex);
    return saveConfigurationInternalLocked();
}

bool OneSevenMultiRtmpConfigManager::saveConfigurationInternalLocked() {
    // This method assumes the mutex is already locked by the caller
    m_globalConfig.updateLastModified();
    
    if (!writeConfigToFile(m_globalConfig)) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to save configuration to file: %s", m_configFilePath.c_str());
        return false;
    }

    MULTI_RTMP_CONFIG_LOG_INFO("Configuration saved successfully");
    return true;
}

bool OneSevenMultiRtmpConfigManager::loadConfigurationInternal() {
    // This method assumes the mutex is already locked by the caller
    if (!std::filesystem::exists(m_configFilePath)) {
        MULTI_RTMP_CONFIG_LOG_INFO("Configuration file does not exist, creating new one");
        m_globalConfig = OneSevenMultiRtmpGlobalConfig();
        m_globalConfig.updateLastModified();
        return saveConfigurationInternal(); // Use internal method to avoid deadlock
    }

    if (!readConfigFromFile(m_globalConfig)) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to read configuration from file: %s", m_configFilePath.c_str());
        return false;
    }

    MULTI_RTMP_CONFIG_LOG_INFO("Configuration loaded successfully, %zu streams found", m_globalConfig.streams.size());
    return true;
}

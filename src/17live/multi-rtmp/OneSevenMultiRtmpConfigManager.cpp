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
    
    if (!std::filesystem::exists(m_configFilePath)) {
        MULTI_RTMP_CONFIG_LOG_INFO("Configuration file does not exist, creating new one");
        m_globalConfig = OneSevenMultiRtmpGlobalConfig();
        m_globalConfig.updateLastModified();
        return saveConfiguration();
    }

    if (!readConfigFromFile(m_globalConfig)) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Failed to read configuration from file: %s", m_configFilePath.c_str());
        return false;
    }

    MULTI_RTMP_CONFIG_LOG_INFO("Configuration loaded successfully, %zu streams found", m_globalConfig.streams.size());
    return true;
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

bool OneSevenMultiRtmpConfigManager::addStreamConfig(const OneSevenMultiRtmpConfig& config) {
    if (!validateStreamConfig(config)) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Invalid stream configuration: %s", getValidationError(config).c_str());
        return false;
    }

    std::lock_guard<std::mutex> lock(m_configMutex);
    
    // Check if stream ID already exists
    if (m_globalConfig.findStream(config.id) != nullptr) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Stream with ID %s already exists", config.id.c_str());
        return false;
    }

    // Add the stream configuration
    OneSevenMultiRtmpConfig newConfig = config;
    newConfig.createdAt = getCurrentTimestamp();
    newConfig.updatedAt = newConfig.createdAt;
    
    m_globalConfig.addStream(newConfig);
    
    MULTI_RTMP_CONFIG_LOG_INFO("Stream configuration added: %s (%s)", 
                               newConfig.streamName.c_str(), newConfig.id.c_str());
    
    // Notify callback
    notifyConfigChange(newConfig.id, newConfig);
    
    return true;
}

bool OneSevenMultiRtmpConfigManager::removeStreamConfig(const std::string& streamId) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    if (!m_globalConfig.removeStream(streamId)) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Stream with ID %s not found", streamId.c_str());
        return false;
    }

    MULTI_RTMP_CONFIG_LOG_INFO("Stream configuration removed: %s", streamId.c_str());
    
    // Notify callback
    notifyConfigDelete(streamId);
    
    return true;
}

bool OneSevenMultiRtmpConfigManager::updateStreamConfig(const std::string& streamId, const OneSevenMultiRtmpConfig& config) {
    if (!validateStreamConfig(config)) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Invalid stream configuration: %s", getValidationError(config).c_str());
        return false;
    }

    std::lock_guard<std::mutex> lock(m_configMutex);
    
    auto* existingConfig = m_globalConfig.findStream(streamId);
    if (!existingConfig) {
        MULTI_RTMP_CONFIG_LOG_ERROR("Stream with ID %s not found", streamId.c_str());
        return false;
    }

    // Update the configuration
    OneSevenMultiRtmpConfig updatedConfig = config;
    updatedConfig.id = streamId; // Ensure ID doesn't change
    updatedConfig.createdAt = existingConfig->createdAt; // Preserve creation time
    updatedConfig.updatedAt = getCurrentTimestamp();
    
    m_globalConfig.updateStream(streamId, updatedConfig);
    
    MULTI_RTMP_CONFIG_LOG_INFO("Stream configuration updated: %s (%s)", 
                               updatedConfig.streamName.c_str(), streamId.c_str());
    
    // Notify callback
    notifyConfigChange(streamId, updatedConfig);
    
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

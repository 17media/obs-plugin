#include "OneSevenLiveMultiRtmpConfigManager.hpp"

#include <obs-module.h>
#include <plugin-support.h>
#include <util/config-file.h>

#include <QDir>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

OneSevenLiveMultiRtmpConfigManager::OneSevenLiveMultiRtmpConfigManager() {
    obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] ConfigManager initialized");

    // Initialize configuration directory path - use same directory as OneSevenLiveConfigManager
    QString homeDir = QDir::homePath();
    QString configDir = homeDir + "/.17Live";
    QDir dir(configDir);

    // If directory doesn't exist, create it
    if (!dir.exists()) {
        if (!dir.mkpath(configDir)) {
            obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Failed to create config directory: %s",
                    configDir.toStdString().c_str());
            m_configDirectory = "./config/multi-rtmp";  // fallback
        } else {
            m_configDirectory = configDir.toStdString();
        }
    } else {
        m_configDirectory = configDir.toStdString();
    }

    m_configFilePath = m_configDirectory + "/" + CONFIG_FILE_NAME;

    obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Config directory: %s", m_configDirectory.c_str());
    obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Config file path: %s", m_configFilePath.c_str());

    // Ensure config directory exists
    ensureConfigDirectoryExists();
}

OneSevenLiveMultiRtmpConfigManager::~OneSevenLiveMultiRtmpConfigManager() {
    // Save configuration on destruction
    saveConfiguration();
    obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Configuration manager destroyed");
}

bool OneSevenLiveMultiRtmpConfigManager::loadConfiguration() {
    return loadConfigurationInternal();
}

bool OneSevenLiveMultiRtmpConfigManager::saveConfiguration() {
    if (!writeConfigToFile(m_globalConfig)) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Failed to save configuration to file: %s",
                m_configFilePath.c_str());
        return false;
    }

    obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Configuration saved successfully");
    return true;
}

bool OneSevenLiveMultiRtmpConfigManager::forceSave() {
    obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Force saving configuration");
    return saveConfigurationInternal();
}

std::string OneSevenLiveMultiRtmpConfigManager::getConfigFilePath() const {
    return m_configFilePath;
}

bool OneSevenLiveMultiRtmpConfigManager::addStreamConfig(const OneSevenLiveMultiRtmpConfig& config) {
    obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Adding stream config: %s",
            config.streamName.c_str());

    // Check if stream with same ID already exists
    if (m_globalConfig.findStream(config.id)) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Stream with ID %s already exists",
                config.id.c_str());
        return false;
    }

    // Add to global config
    m_globalConfig.streams.push_back(config);

    // Notify callback
    notifyConfigChange(config.id, config);

    return true;
}

bool OneSevenLiveMultiRtmpConfigManager::removeStreamConfig(const std::string& streamId) {
    obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Removing stream config: %s", streamId.c_str());

    // Find the config to remove
    auto* configToRemove = m_globalConfig.findStream(streamId);
    if (!configToRemove) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Stream with ID %s not found",
                streamId.c_str());
        return false;
    }

    // Remove from global config
    auto it = std::find_if(
        m_globalConfig.streams.begin(), m_globalConfig.streams.end(),
        [&streamId](const OneSevenLiveMultiRtmpConfig& stream) { return stream.id == streamId; });
    if (it != m_globalConfig.streams.end()) {
        m_globalConfig.streams.erase(it);
    }

    // Notify callback
    notifyConfigDelete(streamId);

    return true;
}

bool OneSevenLiveMultiRtmpConfigManager::updateStreamConfig(const std::string& streamId,
                                                        const OneSevenLiveMultiRtmpConfig& config) {
    obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Updating stream config: %s", streamId.c_str());

    OneSevenLiveMultiRtmpConfig updatedConfig = config;

    auto* existingConfig = m_globalConfig.findStream(streamId);
    if (!existingConfig) {
        obs_log(LOG_ERROR,
                "[MultiRTMP-ConfigManager] Stream configuration not found for update: %s",
                streamId.c_str());
        return false;
    }

    // Update the configuration
    *existingConfig = updatedConfig;

    // Notify callback
    notifyConfigChange(streamId, updatedConfig);

    return true;
}

std::vector<OneSevenLiveMultiRtmpConfig> OneSevenLiveMultiRtmpConfigManager::getStreamConfigs() const {
    return m_globalConfig.streams;
}

OneSevenLiveMultiRtmpConfig OneSevenLiveMultiRtmpConfigManager::getStreamConfig(
    const std::string& streamId) const {
    const auto* config = m_globalConfig.findStream(streamId);
    if (config) {
        return *config;
    }

    obs_log(LOG_WARNING, "[MultiRTMP-ConfigManager] Stream configuration not found: %s",
            streamId.c_str());
    return OneSevenLiveMultiRtmpConfig();
}

bool OneSevenLiveMultiRtmpConfigManager::hasStreamConfig(const std::string& streamId) const {
    return m_globalConfig.findStream(streamId) != nullptr;
}

std::string OneSevenLiveMultiRtmpConfigManager::generateStreamId() const {
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
    ss << "-4";  // Version 4 UUID
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << (8 + (dis(gen) & 3));  // Variant bits
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; i++) {
        ss << dis(gen);
    }

    return ss.str();
}

std::string OneSevenLiveMultiRtmpConfigManager::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

size_t OneSevenLiveMultiRtmpConfigManager::getStreamCount() const {
    return m_globalConfig.streams.size();
}

std::vector<std::string> OneSevenLiveMultiRtmpConfigManager::getStreamIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_globalConfig.streams.size());

    for (const auto& stream : m_globalConfig.streams) {
        ids.push_back(stream.id);
    }

    return ids;
}

void OneSevenLiveMultiRtmpConfigManager::setConfigChangeCallback(ConfigChangeCallback callback) {
    m_configChangeCallback = callback;
}

void OneSevenLiveMultiRtmpConfigManager::setConfigDeleteCallback(ConfigDeleteCallback callback) {
    m_configDeleteCallback = callback;
}

bool OneSevenLiveMultiRtmpConfigManager::createBackup() const {
    std::string timestamp = getCurrentTimestamp();
    std::replace(timestamp.begin(), timestamp.end(), ':', '-');
    std::string backupPath = getBackupFilePath(timestamp);

    try {
        std::filesystem::copy_file(m_configFilePath, backupPath);
        return true;
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Failed to create backup: %s", e.what());
        return false;
    }
}

bool OneSevenLiveMultiRtmpConfigManager::restoreFromBackup() {
    // Find the most recent backup
    auto backups = getAvailableBackups();
    if (backups.empty()) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] No backups available");
        return false;
    }

    std::string latestBackup = backups.back();  // Assuming sorted by timestamp
    std::string backupPath = m_configDirectory + "/" + latestBackup;

    try {
        std::filesystem::copy_file(backupPath, m_configFilePath,
                                   std::filesystem::copy_options::overwrite_existing);
        return loadConfiguration();
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Failed to restore from backup: %s", e.what());
        return false;
    }
}

std::vector<std::string> OneSevenLiveMultiRtmpConfigManager::getAvailableBackups() const {
    std::vector<std::string> backups;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_configDirectory)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.substr(0, strlen(CONFIG_BACKUP_PREFIX)) == CONFIG_BACKUP_PREFIX &&
                    filename.size() >= strlen(CONFIG_BACKUP_EXTENSION) &&
                    filename.substr(filename.size() - strlen(CONFIG_BACKUP_EXTENSION)) ==
                        CONFIG_BACKUP_EXTENSION) {
                    backups.push_back(filename);
                }
            }
        }

        // Sort backups by timestamp (filename contains timestamp)
        std::sort(backups.begin(), backups.end());
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Failed to list backups: %s", e.what());
    }

    return backups;
}

bool OneSevenLiveMultiRtmpConfigManager::ensureConfigDirectoryExists() const {
    try {
        if (!std::filesystem::exists(m_configDirectory)) {
            std::filesystem::create_directories(m_configDirectory);
        }
        return true;
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Failed to create configuration directory: %s",
                e.what());
        return false;
    }
}

bool OneSevenLiveMultiRtmpConfigManager::writeConfigToFile(
    const OneSevenLiveMultiRtmpGlobalConfig& config) const {
    try {
        obs_log(LOG_INFO,
                "[MultiRTMP-ConfigManager] Preparing to write config with %zu streams to: %s",
                config.streams.size(), m_configFilePath.c_str());
        nlohmann::json j = config;

        obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] JSON content to write: %s", j.dump().c_str());

        std::ofstream file(m_configFilePath);
        if (!file.is_open()) {
            obs_log(LOG_ERROR,
                    "[MultiRTMP-ConfigManager] Failed to open config file for writing: %s",
                    m_configFilePath.c_str());
            return false;
        }

        file << j.dump(4);  // Pretty print with 4 spaces
        file.close();

        obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Successfully wrote config file: %s",
                m_configFilePath.c_str());
        return true;
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Failed to write configuration: %s", e.what());
        return false;
    }
}

bool OneSevenLiveMultiRtmpConfigManager::readConfigFromFile(
    OneSevenLiveMultiRtmpGlobalConfig& config) const {
    try {
        obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Opening config file for reading: %s",
                m_configFilePath.c_str());
        std::ifstream file(m_configFilePath);
        if (!file.is_open()) {
            obs_log(LOG_ERROR,
                    "[MultiRTMP-ConfigManager] Failed to open config file for reading: %s",
                    m_configFilePath.c_str());
            return false;
        }

        obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Reading JSON content from config file");
        nlohmann::json j;
        file >> j;
        file.close();

        obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] JSON content: %s", j.dump().c_str());

        config = j.get<OneSevenLiveMultiRtmpGlobalConfig>();

        obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Successfully parsed config with %zu streams",
                config.streams.size());
        return true;
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Failed to read configuration: %s", e.what());
        return false;
    }
}

std::string OneSevenLiveMultiRtmpConfigManager::getBackupFilePath(const std::string& timestamp) const {
    return m_configDirectory + "/" + CONFIG_BACKUP_PREFIX + timestamp + CONFIG_BACKUP_EXTENSION;
}

void OneSevenLiveMultiRtmpConfigManager::notifyConfigChange(const std::string& streamId,
                                                        const OneSevenLiveMultiRtmpConfig& config) {
    if (m_configChangeCallback) {
        m_configChangeCallback(streamId, config);
    }
}

void OneSevenLiveMultiRtmpConfigManager::notifyConfigDelete(const std::string& streamId) {
    if (m_configDeleteCallback) {
        m_configDeleteCallback(streamId);
    }
}

bool OneSevenLiveMultiRtmpConfigManager::saveConfigurationInternal() {
    // Create backup of existing config file before saving new one
    if (std::filesystem::exists(m_configFilePath)) {
        std::string timestamp = getCurrentTimestamp();
        // Replace colons with dashes to ensure valid filename on all operating systems
        std::replace(timestamp.begin(), timestamp.end(), ':', '-');
        std::string backupPath = getBackupFilePath(timestamp);

        try {
            std::filesystem::copy_file(m_configFilePath, backupPath);
            obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Created backup of existing config: %s",
                    backupPath.c_str());
        } catch (const std::filesystem::filesystem_error& e) {
            obs_log(LOG_WARNING,
                    "[MultiRTMP-ConfigManager] Failed to create backup before saving: %s",
                    e.what());
            // Continue with save operation even if backup fails
        }
    }

    if (!writeConfigToFile(m_globalConfig)) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Failed to save configuration to file: %s",
                m_configFilePath.c_str());
        return false;
    }

    return true;
}

bool OneSevenLiveMultiRtmpConfigManager::loadConfigurationInternal() {
    obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Attempting to load configuration from: %s",
            m_configFilePath.c_str());

    if (!std::filesystem::exists(m_configFilePath)) {
        obs_log(LOG_INFO,
                "[MultiRTMP-ConfigManager] Config file does not exist, creating new empty "
                "configuration");
        m_globalConfig = OneSevenLiveMultiRtmpGlobalConfig();
        return saveConfigurationInternal();
    }

    obs_log(LOG_INFO, "[MultiRTMP-ConfigManager] Config file exists, attempting to read");

    if (!readConfigFromFile(m_globalConfig)) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigManager] Failed to read configuration from file: %s",
                m_configFilePath.c_str());
        return false;
    }

    obs_log(LOG_INFO,
            "[MultiRTMP-ConfigManager] Configuration loaded successfully, %zu streams found",
            m_globalConfig.streams.size());
    return true;
}

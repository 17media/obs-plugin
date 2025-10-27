#include "OneSevenMultiRtmpManager.hpp"
#include <algorithm>
#include <exception>

// Static member initialization
OneSevenMultiRtmpManager* OneSevenMultiRtmpManager::s_instance = nullptr;
std::mutex OneSevenMultiRtmpManager::s_instanceMutex;

OneSevenMultiRtmpManager::OneSevenMultiRtmpManager()
    : m_configManager(nullptr)
    , m_streamController(nullptr)
    , m_initialized(false)
{
    obs_log(LOG_INFO, "[MultiRTMP-Manager] Creating MultiRTMP Manager");
}

OneSevenMultiRtmpManager::~OneSevenMultiRtmpManager()
{
    obs_log(LOG_INFO, "[MultiRTMP-Manager] Destroying MultiRTMP Manager");
    shutdown();
}

OneSevenMultiRtmpManager* OneSevenMultiRtmpManager::getInstance()
{
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = new OneSevenMultiRtmpManager();
    }
    return s_instance;
}

void OneSevenMultiRtmpManager::destroyInstance()
{
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

bool OneSevenMultiRtmpManager::initialize()
{
    if (m_initialized) {
        obs_log(LOG_WARNING, "[MultiRTMP-Manager] Manager already initialized");
        return true;
    }

    obs_log(LOG_INFO, "[MultiRTMP-Manager] Initializing MultiRTMP Manager");

    try {
        // Create configuration manager
        m_configManager = std::make_unique<OneSevenMultiRtmpConfigManager>();
        if (!m_configManager) {
            obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to create configuration manager");
            return false;
        }

        // Create stream controller
        m_streamController = std::make_unique<OneSevenMultiRtmpStreamController>();
        if (!m_streamController) {
            obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to create stream controller");
            return false;
        }

        // Setup callbacks between components
        setupCallbacks();

        // Mark as initialized before loading configuration
        m_initialized = true;

        // Load existing configuration
        obs_log(LOG_INFO, "[MultiRTMP-Manager] Attempting to load configuration...");
        if (!loadConfiguration()) {
            obs_log(LOG_WARNING, "[MultiRTMP-Manager] Failed to load configuration, starting with empty config");
            obs_log(LOG_INFO, "[MultiRTMP-Manager] Config manager state: %s", m_configManager ? "valid" : "null");
        } else {
            obs_log(LOG_INFO, "[MultiRTMP-Manager] Configuration loaded successfully");
        }
        obs_log(LOG_INFO, "[MultiRTMP-Manager] MultiRTMP Manager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Exception during initialization: %s", e.what());
        m_initialized = false;  // Reset initialization flag on failure
        return false;
    }
}

void OneSevenMultiRtmpManager::shutdown()
{
    if (!m_initialized) {
        return;
    }

    obs_log(LOG_INFO, "[MultiRTMP-Manager] Shutting down MultiRTMP Manager");

    try {
        // Stop all streams first
        stopAllStreams();

        // Destroy all outputs
        destroyAllStreamOutputs();

        // Save configuration
        saveConfiguration();

        // Cleanup callbacks
        cleanupCallbacks();

        // Reset components
        m_streamController.reset();
        m_configManager.reset();

        m_initialized = false;
        obs_log(LOG_INFO, "[MultiRTMP-Manager] MultiRTMP Manager shutdown complete");

    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Exception during shutdown: %s", e.what());
    }
}

// Configuration operations
bool OneSevenMultiRtmpManager::addStreamConfig(const OneSevenMultiRtmpConfig& config)
{
    obs_log(LOG_INFO, "[MultiRTMP-Manager] Adding stream config: %s", config.streamName.c_str());
    
    if (!m_initialized) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }
    
    if (!m_configManager) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Config manager is null");
        return false;
    }
    
    bool result = m_configManager->addStreamConfig(config);
    
    if (!result) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Config manager failed to add stream config");
        return false;
    }
    
    // Save configuration to file immediately after adding
    obs_log(LOG_INFO, "[MultiRTMP-Manager] Saving configuration after adding stream: %s", config.streamName.c_str());
    if (!saveConfiguration()) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to save configuration after adding stream");
        // Note: We don't return false here as the config was added to memory successfully
    } else {
        obs_log(LOG_INFO, "[MultiRTMP-Manager] Configuration saved successfully after adding stream");
    }
    
    return result;
}

bool OneSevenMultiRtmpManager::removeStreamConfig(const std::string& streamId)
{
    if (!m_initialized || !m_configManager) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }

    // Stop stream if it's running
    if (isStreamActive(streamId)) {
        stopStream(streamId);
    }

    // Destroy output if it exists
    destroyStreamOutput(streamId);

    bool result = m_configManager->removeStreamConfig(streamId);
    
    if (result) {
        // Save configuration to file immediately after removing
        obs_log(LOG_INFO, "[MultiRTMP-Manager] Saving configuration after removing stream: %s", streamId.c_str());
        if (!saveConfiguration()) {
            obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to save configuration after removing stream");
            // Note: We don't return false here as the config was removed from memory successfully
        } else {
            obs_log(LOG_INFO, "[MultiRTMP-Manager] Configuration saved successfully after removing stream");
        }
    }
    
    return result;
}

bool OneSevenMultiRtmpManager::updateStreamConfig(const std::string& streamId, const OneSevenMultiRtmpConfig& config)
{
    if (!m_initialized || !m_configManager) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }

    // If stream is active, we need to restart it with new config
    bool wasActive = isStreamActive(streamId);
    if (wasActive) {
        stopStream(streamId);
        destroyStreamOutput(streamId);
    }

    bool result = m_configManager->updateStreamConfig(streamId, config);

    if (result) {
        // Save configuration to file immediately after updating
        obs_log(LOG_INFO, "[MultiRTMP-Manager] Saving configuration after updating stream: %s", streamId.c_str());
        if (!saveConfiguration()) {
            obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to save configuration after updating stream");
            // Note: We don't return false here as the config was updated in memory successfully
        } else {
            obs_log(LOG_INFO, "[MultiRTMP-Manager] Configuration saved successfully after updating stream");
        }
    }

    // Restart if it was active
    if (result && wasActive) {
        createStreamOutput(streamId);
        startStream(streamId);
    }

    return result;
}

std::vector<OneSevenMultiRtmpConfig> OneSevenMultiRtmpManager::getAllStreamConfigs() const
{
    if (!m_configManager) {
        return {};
    }
    return m_configManager->getStreamConfigs();
}

OneSevenMultiRtmpConfig OneSevenMultiRtmpManager::getStreamConfig(const std::string& streamId) const
{
    if (!m_initialized || !m_configManager) {
        return {};
    }
    return m_configManager->getStreamConfig(streamId);
}

bool OneSevenMultiRtmpManager::hasStreamConfig(const std::string& streamId) const
{
    if (!m_initialized || !m_configManager) {
        return false;
    }
    return m_configManager->hasStreamConfig(streamId);
}

// Runtime operations
bool OneSevenMultiRtmpManager::startStream(const std::string& streamId)
{
    if (!m_initialized || !m_streamController) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }

    if (!hasStreamConfig(streamId)) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Stream configuration not found: %s", streamId.c_str());
        return false;
    }

    // Ensure output exists
    if (!ensureStreamOutput(streamId)) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to create stream output: %s", streamId.c_str());
        return false;
    }

    return m_streamController->startOutput(streamId);
}

bool OneSevenMultiRtmpManager::stopStream(const std::string& streamId)
{
    if (!m_initialized || !m_streamController) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }

    return m_streamController->stopOutput(streamId);
}

bool OneSevenMultiRtmpManager::startAllStreams()
{
    if (!m_initialized || !m_streamController) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }

    auto configs = getAllStreamConfigs();
    bool allSuccess = true;

    for (const auto& config : configs) {
        if (!startStream(config.id)) {
            obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to start stream: %s", config.id.c_str());
            allSuccess = false;
        }
    }

    return allSuccess;
}

bool OneSevenMultiRtmpManager::stopAllStreams()
{
    if (!m_initialized || !m_streamController) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }

    return m_streamController->stopAllOutputs();
}

// Status and statistics
OneSevenMultiRtmpStreamStatus OneSevenMultiRtmpManager::getStreamStatus(const std::string& streamId) const
{
    if (!m_initialized || !m_streamController) {
        return {};
    }
    return m_streamController->getStreamStatus(streamId);
}

OneSevenMultiRtmpStreamStats OneSevenMultiRtmpManager::getStreamStats(const std::string& streamId) const
{
    if (!m_initialized || !m_streamController) {
        return {};
    }
    return m_streamController->getStreamStats(streamId);
}

std::vector<std::string> OneSevenMultiRtmpManager::getActiveStreamIds() const
{
    if (!m_initialized || !m_streamController) {
        return {};
    }
    return m_streamController->getActiveStreamIds();
}

std::vector<std::string> OneSevenMultiRtmpManager::getAllStreamIds() const
{
    if (!m_initialized || !m_configManager) {
        return {};
    }

    auto configs = m_configManager->getStreamConfigs();
    std::vector<std::string> ids;
    ids.reserve(configs.size());

    for (const auto& config : configs) {
        ids.push_back(config.id);
    }

    return ids;
}

// Utility methods
std::string OneSevenMultiRtmpManager::generateStreamId() const
{
    if (!m_initialized || !m_configManager) {
        return "";
    }
    return m_configManager->generateStreamId();
}

size_t OneSevenMultiRtmpManager::getStreamCount() const
{
    if (!m_initialized || !m_configManager) {
        return 0;
    }
    return m_configManager->getStreamCount();
}

// Configuration file operations
bool OneSevenMultiRtmpManager::saveConfiguration()
{
    if (!m_initialized || !m_configManager) {
        return false;
    }
    return m_configManager->saveConfiguration();
}

bool OneSevenMultiRtmpManager::loadConfiguration()
{
    if (!m_initialized || !m_configManager) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] loadConfiguration() failed - initialized: %s, configManager: %s", 
                m_initialized ? "true" : "false", m_configManager ? "valid" : "null");
        return false;
    }
    
    obs_log(LOG_INFO, "[MultiRTMP-Manager] Calling configManager->loadConfiguration()");
    bool result = m_configManager->loadConfiguration();
    obs_log(LOG_INFO, "[MultiRTMP-Manager] configManager->loadConfiguration() returned: %s", result ? "true" : "false");
    return result;
}

bool OneSevenMultiRtmpManager::createConfigBackup()
{
    if (!m_initialized || !m_configManager) {
        return false;
    }
    return m_configManager->createBackup();
}

bool OneSevenMultiRtmpManager::restoreFromBackup()
{
    if (!m_initialized || !m_configManager) {
        return false;
    }
    return m_configManager->restoreFromBackup();
}

// Callback registration
void OneSevenMultiRtmpManager::setStreamStatusCallback(StreamStatusCallback callback)
{
    m_statusCallback = callback;
}

void OneSevenMultiRtmpManager::setStreamStatsCallback(StreamStatsCallback callback)
{
    m_statsCallback = callback;
}

void OneSevenMultiRtmpManager::setConfigChangeCallback(ConfigChangeCallback callback)
{
    m_configChangeCallback = callback;
}

void OneSevenMultiRtmpManager::setConfigDeleteCallback(ConfigDeleteCallback callback)
{
    m_configDeleteCallback = callback;
}

// Stream lifecycle management
bool OneSevenMultiRtmpManager::createStreamOutput(const std::string& streamId)
{
    if (!m_initialized || !m_streamController) {
        return false;
    }

    auto config = getStreamConfig(streamId);
    if (config.id.empty()) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Stream configuration not found: %s", streamId.c_str());
        return false;
    }

    return m_streamController->createOutput(streamId, config);
}

bool OneSevenMultiRtmpManager::destroyStreamOutput(const std::string& streamId)
{
    if (!m_initialized || !m_streamController) {
        return false;
    }
    return m_streamController->destroyOutput(streamId);
}

void OneSevenMultiRtmpManager::destroyAllStreamOutputs()
{
    if (!m_initialized || !m_streamController) {
        return;
    }
    m_streamController->destroyAllOutputs();
}

// Bulk operations with synchronization
bool OneSevenMultiRtmpManager::startAllStreamsWithSync()
{
    if (!m_initialized || !m_streamController) {
        return false;
    }

    auto configs = getAllStreamConfigs();
    
    // Create all outputs first
    for (const auto& config : configs) {
        if (!ensureStreamOutput(config.id)) {
            obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to create output for stream: %s", config.id.c_str());
            return false;
        }
    }

    // Start all streams with synchronization
    return m_streamController->startAllOutputs();
}

bool OneSevenMultiRtmpManager::stopAllStreamsWithSync()
{
    if (!m_initialized || !m_streamController) {
        return false;
    }
    return m_streamController->stopAllOutputs();
}

// Statistics monitoring control
void OneSevenMultiRtmpManager::startStatsMonitoring()
{
    if (!m_initialized || !m_streamController) {
        return;
    }
    m_streamController->startStatsMonitoring();
}

void OneSevenMultiRtmpManager::stopStatsMonitoring()
{
    if (!m_initialized || !m_streamController) {
        return;
    }
    m_streamController->stopStatsMonitoring();
}

// State management
bool OneSevenMultiRtmpManager::isStreamActive(const std::string& streamId) const
{
    if (!m_initialized || !m_streamController) {
        return false;
    }
    return m_streamController->isStreamActive(streamId);
}

bool OneSevenMultiRtmpManager::hasStreamOutput(const std::string& streamId) const
{
    if (!m_initialized || !m_streamController) {
        return false;
    }
    return m_streamController->hasOutput(streamId);
}

obs_output_t* OneSevenMultiRtmpManager::getStreamOutput(const std::string& streamId) const
{
    if (!m_initialized || !m_streamController) {
        return nullptr;
    }
    return m_streamController->getStreamOutput(streamId);
}

// Private methods
void OneSevenMultiRtmpManager::onConfigChanged(const std::string& streamId, const OneSevenMultiRtmpConfig& config)
{
    if (m_configChangeCallback) {
        m_configChangeCallback(streamId, config);
    }
}

void OneSevenMultiRtmpManager::onConfigDeleted(const std::string& streamId)
{
    if (m_configDeleteCallback) {
        m_configDeleteCallback(streamId);
    }
}

void OneSevenMultiRtmpManager::onStreamStatusChanged(const std::string& streamId, const OneSevenMultiRtmpStreamStatus& status)
{
    if (m_statusCallback) {
        m_statusCallback(streamId, status);
    }
}

void OneSevenMultiRtmpManager::onStreamStatsUpdated(const std::string& streamId, const OneSevenMultiRtmpStreamStats& stats)
{
    if (m_statsCallback) {
        m_statsCallback(streamId, stats);
    }
}

void OneSevenMultiRtmpManager::setupCallbacks()
{
    if (m_configManager) {
        m_configManager->setConfigChangeCallback(
            [this](const std::string& streamId, const OneSevenMultiRtmpConfig& config) {
                onConfigChanged(streamId, config);
            }
        );

        m_configManager->setConfigDeleteCallback(
            [this](const std::string& streamId) {
                onConfigDeleted(streamId);
            }
        );
    }

    if (m_streamController) {
        m_streamController->setStreamStatusCallback(
            [this](const std::string& streamId, const OneSevenMultiRtmpStreamStatus& status) {
                onStreamStatusChanged(streamId, status);
            }
        );

        m_streamController->setStreamStatsCallback(
            [this](const std::string& streamId, const OneSevenMultiRtmpStreamStats& stats) {
                onStreamStatsUpdated(streamId, stats);
            }
        );
    }
}

void OneSevenMultiRtmpManager::cleanupCallbacks()
{
    if (m_configManager) {
        m_configManager->setConfigChangeCallback(nullptr);
        m_configManager->setConfigDeleteCallback(nullptr);
    }

    if (m_streamController) {
        m_streamController->setStreamStatusCallback(nullptr);
        m_streamController->setStreamStatsCallback(nullptr);
    }

    m_statusCallback = nullptr;
    m_statsCallback = nullptr;
    m_configChangeCallback = nullptr;
    m_configDeleteCallback = nullptr;
}

bool OneSevenMultiRtmpManager::ensureStreamOutput(const std::string& streamId)
{
    if (hasStreamOutput(streamId)) {
        return true;
    }
    return createStreamOutput(streamId);
}

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
    MULTI_RTMP_MANAGER_LOG_INFO("Creating MultiRTMP Manager");
}

OneSevenMultiRtmpManager::~OneSevenMultiRtmpManager()
{
    MULTI_RTMP_MANAGER_LOG_INFO("Destroying MultiRTMP Manager");
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
        MULTI_RTMP_MANAGER_LOG_WARNING("Manager already initialized");
        return true;
    }

    MULTI_RTMP_MANAGER_LOG_INFO("Initializing MultiRTMP Manager");

    try {
        // Create configuration manager
        m_configManager = std::make_unique<OneSevenMultiRtmpConfigManager>();
        if (!m_configManager) {
            MULTI_RTMP_MANAGER_LOG_ERROR("Failed to create configuration manager");
            return false;
        }

        // Create stream controller
        m_streamController = std::make_unique<OneSevenMultiRtmpStreamController>();
        if (!m_streamController) {
            MULTI_RTMP_MANAGER_LOG_ERROR("Failed to create stream controller");
            return false;
        }

        // Setup callbacks between components
        setupCallbacks();

        // Load existing configuration
        if (!loadConfiguration()) {
            MULTI_RTMP_MANAGER_LOG_WARNING("Failed to load configuration, starting with empty config");
        }

        m_initialized = true;
        MULTI_RTMP_MANAGER_LOG_INFO("MultiRTMP Manager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        MULTI_RTMP_MANAGER_LOG_ERROR("Exception during initialization: %s", e.what());
        return false;
    }
}

void OneSevenMultiRtmpManager::shutdown()
{
    if (!m_initialized) {
        return;
    }

    MULTI_RTMP_MANAGER_LOG_INFO("Shutting down MultiRTMP Manager");

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
        MULTI_RTMP_MANAGER_LOG_INFO("MultiRTMP Manager shutdown complete");

    } catch (const std::exception& e) {
        MULTI_RTMP_MANAGER_LOG_ERROR("Exception during shutdown: %s", e.what());
    }
}

// Configuration operations
bool OneSevenMultiRtmpManager::addStreamConfig(const OneSevenMultiRtmpConfig& config)
{
    obs_log(LOG_INFO, "[MultiRTMP-Manager] addStreamConfig called for stream ID: %s", config.id.c_str());
    
    if (!m_initialized) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }
    
    if (!m_configManager) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Config manager is null");
        return false;
    }
    
    // Validate configuration
    if (!validateStreamConfig(config)) {
        std::string error = getValidationError(config);
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Stream config validation failed: %s", error.c_str());
        return false;
    }
    
    obs_log(LOG_INFO, "[MultiRTMP-Manager] Configuration validation passed, delegating to config manager");
    bool result = m_configManager->addStreamConfig(config);
    
    if (result) {
        obs_log(LOG_INFO, "[MultiRTMP-Manager] Stream config added successfully");
    } else {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Config manager failed to add stream config");
    }
    
    return result;
}

bool OneSevenMultiRtmpManager::removeStreamConfig(const std::string& streamId)
{
    if (!m_initialized || !m_configManager) {
        MULTI_RTMP_MANAGER_LOG_ERROR("Manager not initialized");
        return false;
    }

    // Stop stream if it's running
    if (isStreamActive(streamId)) {
        stopStream(streamId);
    }

    // Destroy output if it exists
    destroyStreamOutput(streamId);

    return m_configManager->removeStreamConfig(streamId);
}

bool OneSevenMultiRtmpManager::updateStreamConfig(const std::string& streamId, const OneSevenMultiRtmpConfig& config)
{
    if (!m_initialized || !m_configManager) {
        MULTI_RTMP_MANAGER_LOG_ERROR("Manager not initialized");
        return false;
    }

    if (!validateStreamConfig(config)) {
        MULTI_RTMP_MANAGER_LOG_ERROR("Invalid stream configuration: %s", getValidationError(config).c_str());
        return false;
    }

    // If stream is active, we need to restart it with new config
    bool wasActive = isStreamActive(streamId);
    if (wasActive) {
        stopStream(streamId);
        destroyStreamOutput(streamId);
    }

    bool result = m_configManager->updateStreamConfig(streamId, config);

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
        MULTI_RTMP_MANAGER_LOG_ERROR("Manager not initialized");
        return false;
    }

    if (!hasStreamConfig(streamId)) {
        MULTI_RTMP_MANAGER_LOG_ERROR("Stream configuration not found: %s", streamId.c_str());
        return false;
    }

    // Ensure output exists
    if (!ensureStreamOutput(streamId)) {
        MULTI_RTMP_MANAGER_LOG_ERROR("Failed to create stream output: %s", streamId.c_str());
        return false;
    }

    return m_streamController->startOutput(streamId);
}

bool OneSevenMultiRtmpManager::stopStream(const std::string& streamId)
{
    if (!m_initialized || !m_streamController) {
        MULTI_RTMP_MANAGER_LOG_ERROR("Manager not initialized");
        return false;
    }

    return m_streamController->stopOutput(streamId);
}

bool OneSevenMultiRtmpManager::startAllStreams()
{
    if (!m_initialized || !m_streamController) {
        MULTI_RTMP_MANAGER_LOG_ERROR("Manager not initialized");
        return false;
    }

    auto configs = getAllStreamConfigs();
    bool allSuccess = true;

    for (const auto& config : configs) {
        if (!startStream(config.id)) {
            MULTI_RTMP_MANAGER_LOG_ERROR("Failed to start stream: %s", config.id.c_str());
            allSuccess = false;
        }
    }

    return allSuccess;
}

bool OneSevenMultiRtmpManager::stopAllStreams()
{
    if (!m_initialized || !m_streamController) {
        MULTI_RTMP_MANAGER_LOG_ERROR("Manager not initialized");
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
        return false;
    }
    return m_configManager->loadConfiguration();
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
        MULTI_RTMP_MANAGER_LOG_ERROR("Stream configuration not found: %s", streamId.c_str());
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
            MULTI_RTMP_MANAGER_LOG_ERROR("Failed to create output for stream: %s", config.id.c_str());
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

// Private methods
void OneSevenMultiRtmpManager::onConfigChanged(const std::string& streamId, const OneSevenMultiRtmpConfig& config)
{
    MULTI_RTMP_MANAGER_LOG_DEBUG("Configuration changed for stream: %s", streamId.c_str());
    if (m_configChangeCallback) {
        m_configChangeCallback(streamId, config);
    }
}

void OneSevenMultiRtmpManager::onConfigDeleted(const std::string& streamId)
{
    MULTI_RTMP_MANAGER_LOG_DEBUG("Configuration deleted for stream: %s", streamId.c_str());
    if (m_configDeleteCallback) {
        m_configDeleteCallback(streamId);
    }
}

void OneSevenMultiRtmpManager::onStreamStatusChanged(const std::string& streamId, const OneSevenMultiRtmpStreamStatus& status)
{
    MULTI_RTMP_MANAGER_LOG_DEBUG("Status changed for stream %s: %s", streamId.c_str(), status.getStateString().c_str());
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

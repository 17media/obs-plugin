#include "OneSevenLiveMultiRtmpManager.hpp"

#include <QMessageBox>
#include <QTimer>
#include <QThread>
#include <algorithm>
#include <exception>

#include "OneSevenLiveCoreManager.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "streaming/OneSevenLiveStreamManager.hpp"

// Static member initialization
OneSevenLiveMultiRtmpManager* OneSevenLiveMultiRtmpManager::s_instance = nullptr;
std::mutex OneSevenLiveMultiRtmpManager::s_instanceMutex;

OneSevenLiveMultiRtmpManager::OneSevenLiveMultiRtmpManager()
    : m_configManager(nullptr), m_streamController(nullptr), m_initialized(false) {
    obs_log(LOG_INFO, "[MultiRTMP-Manager] Creating MultiRTMP Manager");
}

OneSevenLiveMultiRtmpManager::~OneSevenLiveMultiRtmpManager() {
    obs_log(LOG_INFO, "[MultiRTMP-Manager] Destroying MultiRTMP Manager");
    shutdown();
}

OneSevenLiveMultiRtmpManager* OneSevenLiveMultiRtmpManager::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = new OneSevenLiveMultiRtmpManager();
    }
    return s_instance;
}

OneSevenLiveMultiRtmpManager* OneSevenLiveMultiRtmpManager::peekInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    return s_instance;
}

void OneSevenLiveMultiRtmpManager::destroyInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

bool OneSevenLiveMultiRtmpManager::initialize() {
    if (m_initialized) {
        obs_log(LOG_WARNING, "[MultiRTMP-Manager] Manager already initialized");
        return true;
    }

    if (OneSevenLiveCoreManager::getInstance().isShuttingDown()) {
        obs_log(LOG_WARNING, "[MultiRTMP-Manager] Skipping initialize: shutting down");
        return false;
    }

    obs_log(LOG_INFO, "[MultiRTMP-Manager] Initializing MultiRTMP Manager");

    try {
        // Create configuration manager
        m_configManager = std::make_unique<OneSevenLiveMultiRtmpConfigManager>();
        if (!m_configManager) {
            obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to create configuration manager");
            return false;
        }

        // Create stream controller
        m_streamController = std::make_unique<OneSevenLiveMultiRtmpStreamController>();
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
            obs_log(LOG_WARNING,
                    "[MultiRTMP-Manager] Failed to load configuration, starting with empty config");
            obs_log(LOG_INFO, "[MultiRTMP-Manager] Config manager state: %s",
                    m_configManager ? "valid" : "null");
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

void OneSevenLiveMultiRtmpManager::shutdown() {
    if (!m_initialized) {
        return;
    }

    obs_log(LOG_INFO, "[MultiRTMP-Manager] Shutting down MultiRTMP Manager");

    try {
        auto& core = OneSevenLiveCoreManager::getInstance();

        if (m_streamController) {
            if (QThread::currentThread() == core.thread()) {
                m_streamController->beginShutdown();
                m_streamController->stopStatsMonitoring();
                (void) m_streamController->stopAllOutputs();
                m_streamController->destroyAllOutputs();
            } else {
                QMetaObject::invokeMethod(
                    &core,
                    [this]() {
                        if (!m_streamController)
                            return;
                        m_streamController->beginShutdown();
                        m_streamController->stopStatsMonitoring();
                        (void) m_streamController->stopAllOutputs();
                        m_streamController->destroyAllOutputs();
                    },
                    Qt::BlockingQueuedConnection);
            }
        }

        if (!OneSevenLiveCoreManager::getInstance().isShuttingDown()) {
            saveConfiguration();
        } else {
            obs_log(LOG_INFO, "[MultiRTMP-Manager] Skipping save during shutdown");
        }

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
bool OneSevenLiveMultiRtmpManager::addStreamConfig(const OneSevenLiveMultiRtmpConfig& config) {
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
    obs_log(LOG_INFO, "[MultiRTMP-Manager] Saving configuration after adding stream: %s",
            config.streamName.c_str());
    if (!saveConfiguration()) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to save configuration after adding stream");
        // Note: We don't return false here as the config was added to memory successfully
    } else {
        obs_log(LOG_INFO,
                "[MultiRTMP-Manager] Configuration saved successfully after adding stream");
    }

    return result;
}

bool OneSevenLiveMultiRtmpManager::removeStreamConfig(const std::string& streamId) {
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

    OneSevenLiveMultiRtmpConfig cfg = m_configManager->getStreamConfig(streamId);
    if (!cfg.id.empty()) {
        OneSevenLiveConfigManager* cm = OneSevenLiveCoreManager::getInstance().getConfigManager();
        if (cm && cm->initialize()) {
            const std::string platform = cfg.streamName;
            if (platform == "YouTube") {
                (void) cm->clearYouTubeAccessToken();
                (void) cm->clearYouTubeRefreshToken();
                obs_log(LOG_INFO, "[MultiRTMP-Manager] Cleared YouTube tokens on delete: %s",
                        streamId.c_str());
            } else if (platform == "Twitch") {
                (void) cm->clearTwitchTokens();
                (void) cm->clearTwitchUserInfo();
                obs_log(LOG_INFO, "[MultiRTMP-Manager] Cleared Twitch tokens on delete: %s",
                        streamId.c_str());
            }
        }
    }

    bool result = m_configManager->removeStreamConfig(streamId);

    if (result) {
        // Save configuration to file immediately after removing
        obs_log(LOG_INFO, "[MultiRTMP-Manager] Saving configuration after removing stream: %s",
                streamId.c_str());
        if (!saveConfiguration()) {
            obs_log(LOG_ERROR,
                    "[MultiRTMP-Manager] Failed to save configuration after removing stream");
            // Note: We don't return false here as the config was removed from memory successfully
        } else {
            obs_log(LOG_INFO,
                    "[MultiRTMP-Manager] Configuration saved successfully after removing stream");
        }
    }

    return result;
}

bool OneSevenLiveMultiRtmpManager::updateStreamConfig(const std::string& streamId,
                                                      const OneSevenLiveMultiRtmpConfig& config) {
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
        obs_log(LOG_INFO, "[MultiRTMP-Manager] Saving configuration after updating stream: %s",
                streamId.c_str());
        if (!saveConfiguration()) {
            obs_log(LOG_ERROR,
                    "[MultiRTMP-Manager] Failed to save configuration after updating stream");
            // Note: We don't return false here as the config was updated in memory successfully
        } else {
            obs_log(LOG_INFO,
                    "[MultiRTMP-Manager] Configuration saved successfully after updating stream");
        }
    }

    // Restart if it was active
    if (result && wasActive) {
        createStreamOutput(streamId);
        startStream(streamId);
    }

    return result;
}

std::vector<OneSevenLiveMultiRtmpConfig> OneSevenLiveMultiRtmpManager::getAllStreamConfigs() const {
    if (!m_configManager) {
        return {};
    }
    return m_configManager->getStreamConfigs();
}

OneSevenLiveMultiRtmpConfig OneSevenLiveMultiRtmpManager::getStreamConfig(
    const std::string& streamId) const {
    if (!m_initialized || !m_configManager) {
        return {};
    }
    return m_configManager->getStreamConfig(streamId);
}

bool OneSevenLiveMultiRtmpManager::hasStreamConfig(const std::string& streamId) const {
    if (!m_initialized || !m_configManager) {
        return false;
    }
    return m_configManager->hasStreamConfig(streamId);
}

// Runtime operations
bool OneSevenLiveMultiRtmpManager::startStream(const std::string& streamId) {
    if (!m_initialized || !m_streamController) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }

    if (!hasStreamConfig(streamId)) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Stream configuration not found: %s",
                streamId.c_str());
        return false;
    }

    QTimer::singleShot(0, [this, streamId]() {
        if (!ensureStreamOutput(streamId)) {
            obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to create stream output: %s",
                    streamId.c_str());
            return;
        }
        if (!m_streamController->startOutput(streamId)) {
            obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to start stream: %s", streamId.c_str());
        }
    });

    return true;
}

bool OneSevenLiveMultiRtmpManager::stopStream(const std::string& streamId) {
    if (!m_initialized || !m_streamController) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }
    auto& core = OneSevenLiveCoreManager::getInstance();
    if (QThread::currentThread() == core.thread()) {
        return m_streamController->stopOutput(streamId);
    } else {
        bool result = false;
        QMetaObject::invokeMethod(&core, [this, &streamId, &result]() {
            result = m_streamController->stopOutput(streamId);
        }, Qt::BlockingQueuedConnection);
        return result;
    }
}

bool OneSevenLiveMultiRtmpManager::startAllStreams() {
    if (!m_initialized || !m_streamController) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }

    auto configs = getAllStreamConfigs();
    for (const auto& config : configs) {
        (void) startStream(config.id);
    }

    return true;
}

bool OneSevenLiveMultiRtmpManager::stopAllStreams() {
    if (!m_initialized || !m_streamController) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Manager not initialized");
        return false;
    }
    auto& core = OneSevenLiveCoreManager::getInstance();
    if (QThread::currentThread() == core.thread()) {
        return m_streamController->stopAllOutputs();
    } else {
        bool result = false;
        QMetaObject::invokeMethod(&core, [this, &result]() {
            result = m_streamController->stopAllOutputs();
        }, Qt::BlockingQueuedConnection);
        return result;
    }
}

// Status and statistics
OneSevenLiveMultiRtmpStreamStatus OneSevenLiveMultiRtmpManager::getStreamStatus(
    const std::string& streamId) const {
    if (!m_initialized || !m_streamController) {
        return {};
    }
    return m_streamController->getStreamStatus(streamId);
}

OneSevenLiveMultiRtmpStreamStatus OneSevenLiveMultiRtmpManager::getStreamStatusByName(
    const std::string& streamName) const {
    OneSevenLiveMultiRtmpStreamStatus status;
    status.state = OneSevenLiveMultiRtmpStreamStatus::STOPPED;
    if (!m_initialized || !m_streamController || !m_configManager)
        return status;
    auto configs = m_configManager->getStreamConfigs();
    for (const auto& cfg : configs) {
        if (cfg.streamName == streamName) {
            return m_streamController->getStreamStatus(cfg.id);
        }
    }
    return status;
}

bool OneSevenLiveMultiRtmpManager::isPlatformStreaming(const std::string& streamName) const {
    auto st = getStreamStatusByName(streamName);
    return st.state == OneSevenLiveMultiRtmpStreamStatus::STREAMING;
}

OneSevenLiveMultiRtmpStreamStats OneSevenLiveMultiRtmpManager::getStreamStats(
    const std::string& streamId) const {
    if (!m_initialized || !m_streamController) {
        return {};
    }
    return m_streamController->getStreamStats(streamId);
}

std::vector<std::string> OneSevenLiveMultiRtmpManager::getActiveStreamIds() const {
    if (!m_initialized || !m_streamController) {
        return {};
    }
    return m_streamController->getActiveStreamIds();
}

std::vector<std::string> OneSevenLiveMultiRtmpManager::getAllStreamIds() const {
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
std::string OneSevenLiveMultiRtmpManager::generateStreamId() const {
    if (!m_initialized || !m_configManager) {
        return "";
    }
    return m_configManager->generateStreamId();
}

size_t OneSevenLiveMultiRtmpManager::getStreamCount() const {
    if (!m_initialized || !m_configManager) {
        return 0;
    }
    return m_configManager->getStreamCount();
}

// Configuration file operations
bool OneSevenLiveMultiRtmpManager::saveConfiguration() {
    if (!m_initialized || !m_configManager) {
        return false;
    }
    return m_configManager->saveConfiguration();
}

bool OneSevenLiveMultiRtmpManager::loadConfiguration() {
    if (!m_initialized || !m_configManager) {
        obs_log(
            LOG_ERROR,
            "[MultiRTMP-Manager] loadConfiguration() failed - initialized: %s, configManager: %s",
            m_initialized ? "true" : "false", m_configManager ? "valid" : "null");
        return false;
    }

    obs_log(LOG_INFO, "[MultiRTMP-Manager] Calling configManager->loadConfiguration()");
    bool result = m_configManager->loadConfiguration();
    obs_log(LOG_INFO, "[MultiRTMP-Manager] configManager->loadConfiguration() returned: %s",
            result ? "true" : "false");
    return result;
}

bool OneSevenLiveMultiRtmpManager::createConfigBackup() {
    if (!m_initialized || !m_configManager) {
        return false;
    }
    return m_configManager->createBackup();
}

bool OneSevenLiveMultiRtmpManager::restoreFromBackup() {
    if (!m_initialized || !m_configManager) {
        return false;
    }
    return m_configManager->restoreFromBackup();
}

// Callback registration
void OneSevenLiveMultiRtmpManager::setStreamStatusCallback(StreamStatusCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_statusCallback = std::move(callback);
}

void OneSevenLiveMultiRtmpManager::setStreamStatsCallback(StreamStatsCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_statsCallback = std::move(callback);
}

void OneSevenLiveMultiRtmpManager::setConfigChangeCallback(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_configChangeCallback = std::move(callback);
}

void OneSevenLiveMultiRtmpManager::setConfigDeleteCallback(ConfigDeleteCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_configDeleteCallback = std::move(callback);
}

// Stream lifecycle management
bool OneSevenLiveMultiRtmpManager::createStreamOutput(const std::string& streamId) {
    if (!m_initialized || !m_streamController) {
        return false;
    }
    auto config = getStreamConfig(streamId);
    if (config.id.empty()) {
        obs_log(LOG_ERROR, "[MultiRTMP-Manager] Stream configuration not found: %s",
                streamId.c_str());
        return false;
    }

    return m_streamController->createOutput(streamId, config);
}

bool OneSevenLiveMultiRtmpManager::destroyStreamOutput(const std::string& streamId) {
    if (!m_initialized || !m_streamController) {
        return false;
    }
    auto& core = OneSevenLiveCoreManager::getInstance();
    if (QThread::currentThread() == core.thread()) {
        return m_streamController->destroyOutput(streamId);
    } else {
        bool result = false;
        QMetaObject::invokeMethod(&core, [this, &streamId, &result]() {
            result = m_streamController->destroyOutput(streamId);
        }, Qt::BlockingQueuedConnection);
        return result;
    }
}

void OneSevenLiveMultiRtmpManager::destroyAllStreamOutputs() {
    if (!m_initialized || !m_streamController) {
        return;
    }
    auto& core = OneSevenLiveCoreManager::getInstance();
    if (QThread::currentThread() == core.thread()) {
        m_streamController->destroyAllOutputs();
    } else {
        QMetaObject::invokeMethod(&core, [this]() {
            m_streamController->destroyAllOutputs();
        }, Qt::BlockingQueuedConnection);
    }
}

// Bulk operations with synchronization
bool OneSevenLiveMultiRtmpManager::startAllStreamsWithSync() {
    if (!m_initialized || !m_streamController) {
        return false;
    }

    auto configs = getAllStreamConfigs();

    // Create all outputs first
    for (const auto& config : configs) {
        if (!ensureStreamOutput(config.id)) {
            obs_log(LOG_ERROR, "[MultiRTMP-Manager] Failed to create output for stream: %s",
                    config.id.c_str());
            return false;
        }
    }

    // Start all streams with synchronization
    return m_streamController->startAllOutputs();
}

bool OneSevenLiveMultiRtmpManager::stopAllStreamsWithSync() {
    if (!m_initialized || !m_streamController) {
        return false;
    }
    return m_streamController->stopAllOutputs();
}

// Statistics monitoring control
void OneSevenLiveMultiRtmpManager::startStatsMonitoring() {
    if (!m_initialized || !m_streamController) {
        return;
    }
    m_streamController->startStatsMonitoring();
}

void OneSevenLiveMultiRtmpManager::stopStatsMonitoring() {
    if (!m_initialized || !m_streamController) {
        return;
    }
    m_streamController->stopStatsMonitoring();
}

// State management
bool OneSevenLiveMultiRtmpManager::isStreamActive(const std::string& streamId) const {
    if (!m_initialized || !m_streamController) {
        return false;
    }
    return m_streamController->isStreamActive(streamId);
}

bool OneSevenLiveMultiRtmpManager::hasStreamOutput(const std::string& streamId) const {
    if (!m_initialized || !m_streamController) {
        return false;
    }
    return m_streamController->hasOutput(streamId);
}

obs_output_t* OneSevenLiveMultiRtmpManager::getStreamOutput(const std::string& streamId) const {
    if (!m_initialized || !m_streamController) {
        return nullptr;
    }
    return m_streamController->getStreamOutput(streamId);
}

// Private methods
void OneSevenLiveMultiRtmpManager::onConfigChanged(const std::string& streamId,
                                                   const OneSevenLiveMultiRtmpConfig& config) {
    ConfigChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_configChangeCallback;
    }
    if (cb) {
        cb(streamId, config);
    }
}

void OneSevenLiveMultiRtmpManager::onConfigDeleted(const std::string& streamId) {
    ConfigDeleteCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_configDeleteCallback;
    }
    if (cb) {
        cb(streamId);
    }
}

void OneSevenLiveMultiRtmpManager::onStreamStatusChanged(
    const std::string& streamId, const OneSevenLiveMultiRtmpStreamStatus& status) {
    StreamStatusCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_statusCallback;
    }
    if (cb) {
        cb(streamId, status);
    }
}

void OneSevenLiveMultiRtmpManager::onStreamStatsUpdated(
    const std::string& streamId, const OneSevenLiveMultiRtmpStreamStats& stats) {
    StreamStatsCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_statsCallback;
    }
    if (cb) {
        cb(streamId, stats);
    }
}

void OneSevenLiveMultiRtmpManager::setupCallbacks() {
    if (m_configManager) {
        m_configManager->setConfigChangeCallback(
            [this](const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config) {
                onConfigChanged(streamId, config);
            });

        m_configManager->setConfigDeleteCallback(
            [this](const std::string& streamId) { onConfigDeleted(streamId); });
    }

    if (m_streamController) {
        m_streamController->setStreamStatusCallback(
            [this](const std::string& streamId, const OneSevenLiveMultiRtmpStreamStatus& status) {
                onStreamStatusChanged(streamId, status);
            });

        m_streamController->setStreamStatsCallback(
            [this](const std::string& streamId, const OneSevenLiveMultiRtmpStreamStats& stats) {
                onStreamStatsUpdated(streamId, stats);
            });
    }
}

void OneSevenLiveMultiRtmpManager::cleanupCallbacks() {
    if (m_configManager) {
        m_configManager->setConfigChangeCallback(nullptr);
        m_configManager->setConfigDeleteCallback(nullptr);
    }

    if (m_streamController) {
        m_streamController->setStreamStatusCallback(nullptr);
        m_streamController->setStreamStatsCallback(nullptr);
    }

    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_statusCallback = nullptr;
        m_statsCallback = nullptr;
        m_configChangeCallback = nullptr;
        m_configDeleteCallback = nullptr;
    }
}

bool OneSevenLiveMultiRtmpManager::ensureStreamOutput(const std::string& streamId) {
    if (hasStreamOutput(streamId)) {
        return true;
    }
    return createStreamOutput(streamId);
}

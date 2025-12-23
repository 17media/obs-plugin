#include "OneSevenLiveMultiRtmpStreamController.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <chrono>
#include <thread>

#include "OneSevenLiveCoreManager.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "plugin-support.h"
#include "streaming/OneSevenLiveStreamManager.hpp"
#include "utility/Common.hpp"


OneSevenLiveMultiRtmpStreamController::OneSevenLiveMultiRtmpStreamController() {
    MULTI_RTMP_STREAM_LOG_INFO("Creating MultiRTMP Stream Controller");
}

OneSevenLiveMultiRtmpStreamController::~OneSevenLiveMultiRtmpStreamController() {
    MULTI_RTMP_STREAM_LOG_INFO("Destroying MultiRTMP Stream Controller");

    // Stop statistics monitoring
    stopStatsMonitoring();

    // Destroy all outputs
    destroyAllOutputs();

    // Release shared encoders
    if (m_sharedVideoEncoder) {
        obs_encoder_release(m_sharedVideoEncoder);
        m_sharedVideoEncoder = nullptr;
    }

    for (auto& [mixerId, encoder] : m_sharedAudioEncoders) {
        if (encoder) {
            obs_encoder_release(encoder);
        }
    }
    m_sharedAudioEncoders.clear();
}

bool OneSevenLiveMultiRtmpStreamController::createOutput(
    const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config) {
    MULTI_RTMP_STREAM_LOG_INFO("Creating output for stream: %s", streamId.c_str());

    // Check if output already exists
    if (m_streamOutputs.find(streamId) != m_streamOutputs.end()) {
        MULTI_RTMP_STREAM_LOG_WARNING("Output already exists for stream: %s", streamId.c_str());
        return true;
    }

    // Create stream output structure
    auto streamOutput = std::make_unique<StreamOutput>();
    streamOutput->config = config;
    streamOutput->status.id = streamId;
    streamOutput->status.state = OneSevenLiveMultiRtmpStreamStatus::STOPPED;
    streamOutput->stats.id = streamId;

    // Store early to allow async resolution to populate
    m_streamOutputs[streamId] = std::move(streamOutput);
    auto* storedOutput = m_streamOutputs[streamId].get();

    // Create service
    if (!createService(streamId, config, storedOutput)) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to create service for stream: %s", streamId.c_str());
        return false;
    }

    // Create encoders
    if (!createEncoders(streamId, config, storedOutput)) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to create encoders for stream: %s", streamId.c_str());
        return false;
    }

    // Setup output
    if (storedOutput->service) {
        if (!setupOutput(streamId, config, storedOutput)) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to setup output for stream: %s", streamId.c_str());
            return false;
        }
    } else {
        MULTI_RTMP_STREAM_LOG_ERROR("Service creation failed for stream: %s", streamId.c_str());
        return false;
    }

    return true;
}

bool OneSevenLiveMultiRtmpStreamController::startOutput(const std::string& streamId) {
    MULTI_RTMP_STREAM_LOG_INFO("=== STARTING OUTPUT FOR STREAM: %s ===", streamId.c_str());

    auto it = m_streamOutputs.find(streamId);
    if (it == m_streamOutputs.end()) {
        MULTI_RTMP_STREAM_LOG_ERROR("Stream output not found: %s", streamId.c_str());
        return false;
    }

    if (!it->second->output) {
        MULTI_RTMP_STREAM_LOG_ERROR("Output not ready for stream: %s", streamId.c_str());
        return false;
    }

    return startOutputInternal(streamId, it->second.get());
}

bool OneSevenLiveMultiRtmpStreamController::startOutputInternal(const std::string& streamId,
                                                                StreamOutput* streamOutput) {
    MULTI_RTMP_STREAM_LOG_INFO("Starting output internal for stream: %s", streamId.c_str());

    if (!streamOutput || !streamOutput->output) {
        MULTI_RTMP_STREAM_LOG_ERROR("Invalid stream output for: %s", streamId.c_str());
        return false;
    }

    if (obs_output_active(streamOutput->output)) {
        MULTI_RTMP_STREAM_LOG_WARNING("Output already active for stream: %s", streamId.c_str());
        return true;
    }

    MULTI_RTMP_STREAM_LOG_INFO("Calling obs_output_start for stream: %s", streamId.c_str());

    // Start the output
    if (!obs_output_start(streamOutput->output)) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to start output for stream: %s", streamId.c_str());
        updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE, "StartFailed");
        return false;
    }

    MULTI_RTMP_STREAM_LOG_INFO("obs_output_start succeeded for stream: %s", streamId.c_str());

    // Update status
    updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::CONNECTING);
    if (streamOutput->startTime.time_since_epoch().count() == 0) {
        streamOutput->startTime = std::chrono::steady_clock::now();
    }

    // Setup connect timeout timer
    if (streamOutput->connectTimeoutTimer) {
        QTimer* t = streamOutput->connectTimeoutTimer;
        QMetaObject::invokeMethod(
            t,
            [t]() {
                t->stop();
                t->deleteLater();
            },
            Qt::QueuedConnection);
        streamOutput->connectTimeoutTimer = nullptr;
    }
    streamOutput->connectTimeoutTimer = new QTimer(QCoreApplication::instance());
    streamOutput->connectTimeoutTimer->setSingleShot(true);
    QObject::connect(streamOutput->connectTimeoutTimer, &QTimer::timeout, [this, streamId]() {
        auto it = m_streamOutputs.find(streamId);
        if (it != m_streamOutputs.end()) {
            StreamOutput* so = it->second.get();
            if (so && (so->status.state == OneSevenLiveMultiRtmpStreamStatus::CONNECTING ||
                       so->status.state == OneSevenLiveMultiRtmpStreamStatus::RECONNECTING)) {
                MULTI_RTMP_STREAM_LOG_WARNING("Connect timeout for stream: %s", streamId.c_str());
                if (so->output) {
                    obs_output_stop(so->output);
                }
                updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE,
                                   "NetworkError:RTMP:Timeout");
            }
        }
    });
    QMetaObject::invokeMethod(
        streamOutput->connectTimeoutTimer,
        [timer = streamOutput->connectTimeoutTimer]() { timer->start(CONNECT_TIMEOUT_MS); },
        Qt::QueuedConnection);

    MULTI_RTMP_STREAM_LOG_INFO("startOutputInternal completed for stream: %s", streamId.c_str());
    return true;
}

bool OneSevenLiveMultiRtmpStreamController::stopOutput(const std::string& streamId) {
    // TEMPORARILY REMOVED LOCK FOR DEBUGGING - DEADLOCK PREVENTION
    // std::lock_guard<std::mutex> lock(m_outputsMutex);

    auto it = m_streamOutputs.find(streamId);
    if (it == m_streamOutputs.end()) {
        MULTI_RTMP_STREAM_LOG_ERROR("Stream output not found: %s", streamId.c_str());
        return false;
    }

    return stopOutputInternal(streamId, it->second.get());
}

bool OneSevenLiveMultiRtmpStreamController::stopOutputInternal(const std::string& streamId,
                                                               StreamOutput* streamOutput) {
    if (!streamOutput) {
        MULTI_RTMP_STREAM_LOG_ERROR("Invalid stream output structure for: %s", streamId.c_str());
        return false;
    }

    // Cancel pending connect timeout and async resolution if any
    if (streamOutput->connectTimeoutTimer) {
        QTimer* t = streamOutput->connectTimeoutTimer;
        QMetaObject::invokeMethod(
            t,
            [t]() {
                t->stop();
                t->deleteLater();
            },
            Qt::QueuedConnection);
        streamOutput->connectTimeoutTimer = nullptr;
    }

    if (!streamOutput->output) {
        updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::STOPPED);
        return true;
    }

    if (!obs_output_active(streamOutput->output)) {
        updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::STOPPED);
        return true;
    }

    obs_output_stop(streamOutput->output);
    updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::STOPPED);

    return true;
}

bool OneSevenLiveMultiRtmpStreamController::destroyOutput(const std::string& streamId) {
    // TEMPORARILY REMOVED LOCK FOR DEBUGGING - DEADLOCK PREVENTION
    // std::lock_guard<std::mutex> lock(m_outputsMutex);

    auto it = m_streamOutputs.find(streamId);
    if (it == m_streamOutputs.end()) {
        return true;
    }

    auto& streamOutput = it->second;

    if (streamOutput->connectTimeoutTimer) {
        QTimer* t = streamOutput->connectTimeoutTimer;
        QMetaObject::invokeMethod(
            t,
            [t]() {
                t->stop();
                t->deleteLater();
            },
            Qt::QueuedConnection);
        streamOutput->connectTimeoutTimer = nullptr;
    }

    if (streamOutput->output && obs_output_active(streamOutput->output)) {
        obs_output_stop(streamOutput->output);
        QElapsedTimer t;
        t.start();
        while (obs_output_active(streamOutput->output) && t.elapsed() < 5000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
    }

    if (streamOutput->output) {
        signal_handler_t* handler = obs_output_get_signal_handler(streamOutput->output);
        if (handler) {
            signal_handler_disconnect(handler, "start", outputStartCallback, this);
            signal_handler_disconnect(handler, "stop", outputStopCallback, this);
            signal_handler_disconnect(handler, "reconnect", outputReconnectCallback, this);
            signal_handler_disconnect(handler, "reconnect_success", outputReconnectSuccessCallback,
                                      this);
        }
    }
    if (streamOutput->output) {
        obs_output_release(streamOutput->output);
    }
    if (streamOutput->service) {
        obs_service_release(streamOutput->service);
    }
    if (streamOutput->videoEncoder && streamOutput->config.videoConfig.has_value()) {
        obs_encoder_release(streamOutput->videoEncoder);
    }
    if (streamOutput->audioEncoder && streamOutput->config.audioConfig.has_value()) {
        obs_encoder_release(streamOutput->audioEncoder);
    }

    m_streamOutputs.erase(it);

    return true;
}

bool OneSevenLiveMultiRtmpStreamController::startAllOutputs() {
    MULTI_RTMP_STREAM_LOG_INFO("Starting all outputs");

    bool allStarted = true;
    for (const auto& [streamId, streamOutput] : m_streamOutputs) {
        if (!obs_output_active(streamOutput->output)) {
            if (!startOutputInternal(streamId, streamOutput.get())) {
                allStarted = false;
                MULTI_RTMP_STREAM_LOG_ERROR("Failed to start output for stream: %s",
                                            streamId.c_str());
            }
        }
    }

    return allStarted;
}

bool OneSevenLiveMultiRtmpStreamController::stopAllOutputs() {
    bool allStopped = true;
    for (const auto& [streamId, streamOutput] : m_streamOutputs) {
        // Stop if stream is technically active (Connecting, Streaming, Reconnecting)
        // OR if the OBS output is active (fallback check)
        bool shouldStop =
            streamOutput->status.state != OneSevenLiveMultiRtmpStreamStatus::STOPPED &&
            streamOutput->status.state != OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE;

        if (streamOutput->output && obs_output_active(streamOutput->output)) {
            shouldStop = true;
        }

        if (shouldStop) {
            if (!stopOutputInternal(streamId, streamOutput.get())) {
                allStopped = false;
                MULTI_RTMP_STREAM_LOG_ERROR("Failed to stop output for stream: %s",
                                            streamId.c_str());
            }
        }
    }
    return allStopped;
}

void OneSevenLiveMultiRtmpStreamController::destroyAllOutputs() {
    // TEMPORARILY REMOVED LOCK FOR DEBUGGING - DEADLOCK PREVENTION
    // std::lock_guard<std::mutex> lock(m_outputsMutex);

    for (auto& [streamId, streamOutput] : m_streamOutputs) {
        if (streamOutput->connectTimeoutTimer) {
            QTimer* t = streamOutput->connectTimeoutTimer;
            QMetaObject::invokeMethod(
                t,
                [t]() {
                    t->stop();
                    t->deleteLater();
                },
                Qt::QueuedConnection);
            streamOutput->connectTimeoutTimer = nullptr;
        }
        if (streamOutput->output && obs_output_active(streamOutput->output)) {
            obs_output_stop(streamOutput->output);
            QElapsedTimer t;
            t.start();
            while (obs_output_active(streamOutput->output) && t.elapsed() < 5000) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            }
        }
        if (streamOutput->output) {
            signal_handler_t* handler = obs_output_get_signal_handler(streamOutput->output);
            if (handler) {
                signal_handler_disconnect(handler, "start", outputStartCallback, this);
                signal_handler_disconnect(handler, "stop", outputStopCallback, this);
                signal_handler_disconnect(handler, "reconnect", outputReconnectCallback, this);
                signal_handler_disconnect(handler, "reconnect_success",
                                          outputReconnectSuccessCallback, this);
            }
        }
        if (streamOutput->output) {
            obs_output_release(streamOutput->output);
        }
        if (streamOutput->service) {
            obs_service_release(streamOutput->service);
        }
        if (streamOutput->videoEncoder && streamOutput->config.videoConfig.has_value()) {
            obs_encoder_release(streamOutput->videoEncoder);
        }
        if (streamOutput->audioEncoder && streamOutput->config.audioConfig.has_value()) {
            obs_encoder_release(streamOutput->audioEncoder);
        }
    }

    m_streamOutputs.clear();
}

OneSevenLiveMultiRtmpStreamStatus OneSevenLiveMultiRtmpStreamController::getStreamStatus(
    const std::string& streamId) const {
    // TEMPORARILY REMOVED LOCK FOR DEBUGGING - DEADLOCK PREVENTION
    // std::lock_guard<std::mutex> lock(m_outputsMutex);

    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end()) {
        return it->second->status;
    }

    OneSevenLiveMultiRtmpStreamStatus status;
    status.id = streamId;
    status.state = OneSevenLiveMultiRtmpStreamStatus::STOPPED;
    return status;
}

OneSevenLiveMultiRtmpStreamStats OneSevenLiveMultiRtmpStreamController::getStreamStats(
    const std::string& streamId) const {
    // TEMPORARILY REMOVED LOCK FOR DEBUGGING - DEADLOCK PREVENTION
    // std::lock_guard<std::mutex> lock(m_outputsMutex);

    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end()) {
        return it->second->stats;
    }

    OneSevenLiveMultiRtmpStreamStats stats;
    stats.id = streamId;
    return stats;
}

std::vector<std::string> OneSevenLiveMultiRtmpStreamController::getActiveStreamIds() const {
    // TEMPORARILY REMOVED LOCK FOR DEBUGGING - DEADLOCK PREVENTION
    // std::lock_guard<std::mutex> lock(m_outputsMutex);

    std::vector<std::string> activeIds;
    for (const auto& [streamId, streamOutput] : m_streamOutputs) {
        if (streamOutput->output && obs_output_active(streamOutput->output)) {
            activeIds.push_back(streamId);
        }
    }

    return activeIds;
}

std::vector<std::string> OneSevenLiveMultiRtmpStreamController::getAllStreamIds() const {
    // TEMPORARILY REMOVED LOCK FOR DEBUGGING - DEADLOCK PREVENTION
    // std::lock_guard<std::mutex> lock(m_outputsMutex);

    std::vector<std::string> allIds;
    allIds.reserve(m_streamOutputs.size());

    for (const auto& [streamId, streamOutput] : m_streamOutputs) {
        allIds.push_back(streamId);
    }

    return allIds;
}

obs_encoder_t* OneSevenLiveMultiRtmpStreamController::getSharedVideoEncoder() {
    MULTI_RTMP_STREAM_LOG_INFO(
        "Shared video encoder disabled; using dedicated encoders per output");
    return nullptr;
}

obs_encoder_t* OneSevenLiveMultiRtmpStreamController::getSharedAudioEncoder(int mixerId) {
    (void) mixerId;
    MULTI_RTMP_STREAM_LOG_INFO(
        "Shared audio encoder disabled; using dedicated encoders per output");
    return nullptr;
}

bool OneSevenLiveMultiRtmpStreamController::isStreamActive(const std::string& streamId) const {
    // TEMPORARILY REMOVED LOCK FOR DEBUGGING - DEADLOCK PREVENTION
    // std::lock_guard<std::mutex> lock(m_outputsMutex);

    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end()) {
        return it->second->output && obs_output_active(it->second->output);
    }

    return false;
}

bool OneSevenLiveMultiRtmpStreamController::hasOutput(const std::string& streamId) const {
    // TEMPORARILY REMOVED LOCK FOR DEBUGGING - DEADLOCK PREVENTION
    // std::lock_guard<std::mutex> lock(m_outputsMutex);
    return m_streamOutputs.find(streamId) != m_streamOutputs.end();
}

obs_output_t* OneSevenLiveMultiRtmpStreamController::getStreamOutput(
    const std::string& streamId) const {
    std::lock_guard<std::mutex> lock(m_outputsMutex);

    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end()) {
        return it->second->output;
    }

    return nullptr;
}

void OneSevenLiveMultiRtmpStreamController::setStreamStatusCallback(StreamStatusCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_statusCallback = std::move(callback);
}

void OneSevenLiveMultiRtmpStreamController::setStreamStatsCallback(StreamStatsCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_statsCallback = std::move(callback);
}

void OneSevenLiveMultiRtmpStreamController::startStatsMonitoring() {
    std::lock_guard<std::mutex> lock(m_statsThreadMutex);

    if (m_statsMonitoringActive) {
        return;
    }

    m_statsMonitoringActive = true;
    m_statsThread =
        std::thread(&OneSevenLiveMultiRtmpStreamController::statsMonitoringThread, this);

    MULTI_RTMP_STREAM_LOG_INFO("Statistics monitoring started");
}

void OneSevenLiveMultiRtmpStreamController::stopStatsMonitoring() {
    {
        std::lock_guard<std::mutex> lock(m_statsThreadMutex);
        m_statsMonitoringActive = false;
    }

    if (m_statsThread.joinable()) {
        m_statsThread.join();
    }

    MULTI_RTMP_STREAM_LOG_INFO("Statistics monitoring stopped");
}

bool OneSevenLiveMultiRtmpStreamController::createService(const std::string& streamId,
                                                          const OneSevenLiveMultiRtmpConfig& config,
                                                          StreamOutput* streamOutput) {
    if (!streamOutput) {
        MULTI_RTMP_STREAM_LOG_ERROR("StreamOutput is null for stream: %s", streamId.c_str());
        return false;
    }
    ObsDataPtr serviceSettings{createServiceSettings(config)};
    if (!serviceSettings) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to create base service settings for stream: %s",
                                    streamId.c_str());
        return false;
    }

    const char* server = obs_data_get_string(serviceSettings.get(), "server");
    const char* key = obs_data_get_string(serviceSettings.get(), "key");
    const bool hasServer = server && *server;
    const bool hasKey = key && *key;

    if (hasServer && hasKey) {
        streamOutput->service = obs_service_create(SERVICE_ID, getServiceName(streamId).c_str(),
                                                   serviceSettings.get(), nullptr);
        serviceSettings.reset();
        if (!streamOutput->service) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create service for stream: %s",
                                        streamId.c_str());
            return false;
        }
        return true;
    }

    serviceSettings.reset();
    MULTI_RTMP_STREAM_LOG_ERROR("Server/key missing for stream: %s", streamId.c_str());
    return false;
}

bool OneSevenLiveMultiRtmpStreamController::createEncoders(
    const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config,
    StreamOutput* streamOutput) {
    if (!streamOutput) {
        MULTI_RTMP_STREAM_LOG_ERROR("StreamOutput is null for stream: %s", streamId.c_str());
        return false;
    }

    // Video encoder: always create dedicated encoder
    {
        ObsDataPtr videoSettings{createVideoEncoderSettings(config)};
        if (!videoSettings) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create video encoder settings for stream: %s",
                                        streamId.c_str());
            return false;
        }
        const char* videoEncoderId =
            config.videoConfig.has_value() && !config.videoConfig->encoderId.empty()
                ? config.videoConfig->encoderId.c_str()
                : getObsDefaultVideoEncoderId();
        streamOutput->videoEncoder = obs_video_encoder_create(
            videoEncoderId, getVideoEncoderName(streamId).c_str(), videoSettings.get(), nullptr);
        videoSettings.reset();
        if (!streamOutput->videoEncoder) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create video encoder for stream: %s",
                                        streamId.c_str());
            return false;
        }
    }

    // Audio encoder: always create dedicated encoder
    {
        ObsDataPtr audioSettings{createAudioEncoderSettings(config)};
        if (!audioSettings) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create audio encoder settings for stream: %s",
                                        streamId.c_str());
            return false;
        }
        const char* audioEncoderId =
            (config.audioConfig.has_value() && !config.audioConfig->encoderId.empty())
                ? config.audioConfig->encoderId.c_str()
                : AUDIO_ENCODER_ID;
        streamOutput->audioEncoder = obs_audio_encoder_create(
            audioEncoderId, getAudioEncoderName(streamId).c_str(), audioSettings.get(), 0, nullptr);
        audioSettings.reset();
        if (!streamOutput->audioEncoder) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create audio encoder for stream: %s",
                                        streamId.c_str());
            return false;
        }
    }

    // Final validation to ensure both encoders are available
    if (!streamOutput->videoEncoder) {
        MULTI_RTMP_STREAM_LOG_ERROR("Video encoder is null after creation for stream: %s",
                                    streamId.c_str());
        return false;
    }

    if (!streamOutput->audioEncoder) {
        MULTI_RTMP_STREAM_LOG_ERROR("Audio encoder is null after creation for stream: %s",
                                    streamId.c_str());
        return false;
    }

    // Connect encoders to video and audio sources
    obs_encoder_set_video(streamOutput->videoEncoder, obs_get_video());
    obs_encoder_set_audio(streamOutput->audioEncoder, obs_get_audio());

    return true;
}

bool OneSevenLiveMultiRtmpStreamController::setupOutput(const std::string& streamId,
                                                        const OneSevenLiveMultiRtmpConfig& config,
                                                        StreamOutput* streamOutput) {
    if (!streamOutput) {
        MULTI_RTMP_STREAM_LOG_ERROR("StreamOutput is null for stream: %s", streamId.c_str());
        return false;
    }

    // Validate that all required components are available
    if (!streamOutput->service) {
        MULTI_RTMP_STREAM_LOG_ERROR("Service is null for stream: %s", streamId.c_str());
        return false;
    }

    if (!streamOutput->videoEncoder) {
        MULTI_RTMP_STREAM_LOG_ERROR("Video encoder is null for stream: %s", streamId.c_str());
        return false;
    }

    if (!streamOutput->audioEncoder) {
        MULTI_RTMP_STREAM_LOG_ERROR("Audio encoder is null for stream: %s", streamId.c_str());
        return false;
    }

    ObsDataPtr outputSettings{createOutputSettings(config)};
    if (!outputSettings) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to create output settings for stream: %s",
                                    streamId.c_str());
        return false;
    }

    streamOutput->output = obs_output_create(OUTPUT_ID, getOutputName(streamId).c_str(),
                                             outputSettings.get(), nullptr);
    outputSettings.reset();

    if (!streamOutput->output) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to create output for stream: %s", streamId.c_str());
        return false;
    }

    // Set service and encoders
    obs_output_set_service(streamOutput->output, streamOutput->service);
    obs_output_set_video_encoder(streamOutput->output, streamOutput->videoEncoder);
    obs_output_set_audio_encoder(streamOutput->output, streamOutput->audioEncoder, 0);

    // Set callbacks
    signal_handler_t* handler = obs_output_get_signal_handler(streamOutput->output);
    signal_handler_connect(handler, "start", outputStartCallback, this);
    signal_handler_connect(handler, "stop", outputStopCallback, this);
    signal_handler_connect(handler, "reconnect", outputReconnectCallback, this);
    signal_handler_connect(handler, "reconnect_success", outputReconnectSuccessCallback, this);

    MULTI_RTMP_STREAM_LOG_INFO("Output setup completed successfully for stream: %s",
                               streamId.c_str());
    return true;
}

void OneSevenLiveMultiRtmpStreamController::updateStreamStatus(
    const std::string& streamId, OneSevenLiveMultiRtmpStreamStatus::State state,
    const std::string& error) {
    // NOTE: NOT ADDING MUTEX LOCK HERE FOR DEBUGGING - POTENTIAL DEADLOCK SOURCE
    // This method is called from OBS callbacks which may already hold locks
    // std::lock_guard<std::mutex> lock(m_outputsMutex);

    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end()) {
        it->second->status.state = state;
        it->second->status.errorMessage = error;
        StreamStatusCallback cb;
        OneSevenLiveMultiRtmpStreamStatus statusCopy = it->second->status;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            cb = m_statusCallback;
        }
        if (cb) {
            cb(streamId, statusCopy);
        }
    }
}

// Static callback implementations
void OneSevenLiveMultiRtmpStreamController::outputStartCallback(void* data, calldata_t* cd) {
    auto* controller = static_cast<OneSevenLiveMultiRtmpStreamController*>(data);
    obs_output_t* output = static_cast<obs_output_t*>(calldata_ptr(cd, "output"));

    MULTI_RTMP_STREAM_LOG_INFO("=== OUTPUT START CALLBACK TRIGGERED ===");

    // Find stream ID by output - TEMPORARILY REMOVED LOCK FOR DEBUGGING
    // std::lock_guard<std::mutex> lock(controller->m_outputsMutex);
    for (const auto& [streamId, streamOutput] : controller->m_streamOutputs) {
        if (streamOutput->output == output) {
            MULTI_RTMP_STREAM_LOG_INFO("Found matching stream in callback: %s", streamId.c_str());
            if (streamOutput->connectTimeoutTimer) {
                QTimer* t = streamOutput->connectTimeoutTimer;
                QMetaObject::invokeMethod(
                    t,
                    [t]() {
                        t->stop();
                        t->deleteLater();
                    },
                    Qt::QueuedConnection);
                streamOutput->connectTimeoutTimer = nullptr;
            }
            if (streamOutput->startTime.time_since_epoch().count() == 0) {
                streamOutput->startTime = std::chrono::steady_clock::now();
            }
            controller->updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::STREAMING);
            {
                std::string platform = streamOutput->config.streamName;
                std::transform(platform.begin(), platform.end(), platform.begin(), ::tolower);
                if (platform.find("twitch") != std::string::npos) {
                    auto& core = OneSevenLiveCoreManager::getInstance();
                    QMetaObject::invokeMethod(
                        &core, [&core]() { core.connectTwitchChatClient(QString()); },
                        Qt::QueuedConnection);
                }
                if (platform.find("youtube") != std::string::npos) {
                    auto& core = OneSevenLiveCoreManager::getInstance();
                    QMetaObject::invokeMethod(
                        &core,
                        [&core]() {
                            auto* cfg = core.getConfigManager();
                            if (!cfg)
                                return;
                            QString bid;
                            QString chat;
                            if (cfg->getYouTubeBroadcastInfo(bid, chat) && !chat.isEmpty()) {
                                core.startYouTubeChatPolling(chat);
                            }
                        },
                        Qt::QueuedConnection);
                }
            }
            MULTI_RTMP_STREAM_LOG_INFO("Stream started: %s", streamId.c_str());
            break;
        }
    }
}

void OneSevenLiveMultiRtmpStreamController::outputStopCallback(void* data, calldata_t* cd) {
    auto* controller = static_cast<OneSevenLiveMultiRtmpStreamController*>(data);
    obs_output_t* output = static_cast<obs_output_t*>(calldata_ptr(cd, "output"));

    std::lock_guard<std::mutex> lock(controller->m_outputsMutex);
    for (const auto& [streamId, streamOutput] : controller->m_streamOutputs) {
        if (streamOutput->output == output) {
            if (streamOutput->status.state == OneSevenLiveMultiRtmpStreamStatus::CONNECTING ||
                streamOutput->status.state == OneSevenLiveMultiRtmpStreamStatus::RECONNECTING) {
                std::string detail;
                const char* lastErr = nullptr;
                const char* reason = nullptr;
                int code = 0;
                // Attempt to read error fields from calldata if present
                lastErr = calldata_string(cd, "last_error");
                if (!lastErr)
                    lastErr = calldata_string(cd, "error");
                reason = calldata_string(cd, "reason");
                code = calldata_int(cd, "code");
                if (lastErr && *lastErr) {
                    detail = lastErr;
                } else if (reason && *reason) {
                    detail = reason;
                } else if (code != 0) {
                    detail = std::string("code=") + std::to_string(code);
                }
                std::string errMsg;
                std::string dlow = detail;
                std::transform(dlow.begin(), dlow.end(), dlow.begin(), ::tolower);
                bool isNet = dlow.find("tls") != std::string::npos ||
                             dlow.find("ssl") != std::string::npos ||
                             dlow.find("timeout") != std::string::npos ||
                             dlow.find("connection") != std::string::npos ||
                             dlow.find("recv") != std::string::npos ||
                             dlow.find("reset") != std::string::npos ||
                             dlow.find("handshake") != std::string::npos ||
                             dlow.find("network") != std::string::npos ||
                             dlow.find("code=-2") != std::string::npos;
                errMsg = isNet ? "NetworkError:RTMP" : "ConnectFailed";
                if (!detail.empty())
                    errMsg += ":" + detail;
                controller->updateStreamStatus(
                    streamId, OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE, errMsg);
            } else {
                controller->updateStreamStatus(streamId,
                                               OneSevenLiveMultiRtmpStreamStatus::STOPPED);
            }
            {
                std::string platform = streamOutput->config.streamName;
                std::transform(platform.begin(), platform.end(), platform.begin(), ::tolower);
                if (platform.find("twitch") != std::string::npos) {
                    auto& core = OneSevenLiveCoreManager::getInstance();
                    QMetaObject::invokeMethod(
                        &core, [&core]() { core.disconnectTwitchChatClient(); },
                        Qt::QueuedConnection);
                }
                if (platform.find("youtube") != std::string::npos) {
                    auto& core = OneSevenLiveCoreManager::getInstance();
                    QMetaObject::invokeMethod(
                        &core, [&core]() { core.stopYouTubeChatPolling(); }, Qt::QueuedConnection);
                }
            }
            MULTI_RTMP_STREAM_LOG_INFO("Stream stopped: %s", streamId.c_str());
            break;
        }
    }
}

void OneSevenLiveMultiRtmpStreamController::outputReconnectCallback(void* data, calldata_t* cd) {
    auto* controller = static_cast<OneSevenLiveMultiRtmpStreamController*>(data);
    obs_output_t* output = static_cast<obs_output_t*>(calldata_ptr(cd, "output"));

    std::lock_guard<std::mutex> lock(controller->m_outputsMutex);
    for (const auto& [streamId, streamOutput] : controller->m_streamOutputs) {
        if (streamOutput->output == output) {
            if (streamOutput->connectTimeoutTimer) {
                QTimer* t = streamOutput->connectTimeoutTimer;
                QMetaObject::invokeMethod(
                    t,
                    [t]() {
                        t->stop();
                        t->deleteLater();
                    },
                    Qt::QueuedConnection);
                streamOutput->connectTimeoutTimer = nullptr;
            }
            controller->updateStreamStatus(streamId,
                                           OneSevenLiveMultiRtmpStreamStatus::RECONNECTING);
            MULTI_RTMP_STREAM_LOG_INFO("Stream reconnecting: %s", streamId.c_str());
            break;
        }
    }
}

void OneSevenLiveMultiRtmpStreamController::outputReconnectSuccessCallback(void* data,
                                                                           calldata_t* cd) {
    auto* controller = static_cast<OneSevenLiveMultiRtmpStreamController*>(data);
    obs_output_t* output = static_cast<obs_output_t*>(calldata_ptr(cd, "output"));

    std::lock_guard<std::mutex> lock(controller->m_outputsMutex);
    for (const auto& [streamId, streamOutput] : controller->m_streamOutputs) {
        if (streamOutput->output == output) {
            if (streamOutput->connectTimeoutTimer) {
                QTimer* t = streamOutput->connectTimeoutTimer;
                QMetaObject::invokeMethod(
                    t,
                    [t]() {
                        t->stop();
                        t->deleteLater();
                    },
                    Qt::QueuedConnection);
                streamOutput->connectTimeoutTimer = nullptr;
            }
            controller->updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::STREAMING);
            MULTI_RTMP_STREAM_LOG_INFO("Stream reconnected successfully: %s", streamId.c_str());
            break;
        }
    }
}

void OneSevenLiveMultiRtmpStreamController::statsMonitoringThread() {
    while (m_statsMonitoringActive) {
        {
            std::lock_guard<std::mutex> lock(m_outputsMutex);
            for (auto& [streamId, streamOutput] : m_streamOutputs) {
                if (streamOutput->output &&
                    (streamOutput->status.state == OneSevenLiveMultiRtmpStreamStatus::CONNECTING ||
                     streamOutput->status.state == OneSevenLiveMultiRtmpStreamStatus::STREAMING ||
                     streamOutput->status.state ==
                         OneSevenLiveMultiRtmpStreamStatus::RECONNECTING)) {
                    collectStreamStats(streamId, *streamOutput);

                    if (m_statsCallback) {
                        m_statsCallback(streamId, streamOutput->stats);
                    }
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(STATS_UPDATE_INTERVAL_MS));
    }
}

void OneSevenLiveMultiRtmpStreamController::collectStreamStats(const std::string& streamId,
                                                               StreamOutput& streamOutput) {
    (void) streamId;  // Suppress unused parameter warning

    if (!streamOutput.output) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    streamOutput.stats.duration = now - streamOutput.startTime;

    uint64_t totalBytes = obs_output_get_total_bytes(streamOutput.output);
    uint64_t totalFrames =
        static_cast<uint64_t>(obs_output_get_total_frames(streamOutput.output));

    streamOutput.stats.totalFrames = static_cast<int>(totalFrames);
    streamOutput.stats.droppedFrames =
        static_cast<int>(obs_output_get_frames_dropped(streamOutput.output));

    if (streamOutput.lastStatsTime.time_since_epoch().count() == 0) {
        streamOutput.lastStatsTime = now;
        streamOutput.lastBytes = totalBytes;
        streamOutput.lastFrames = totalFrames;
        streamOutput.stats.currentBitrate = 0.0;
        streamOutput.stats.currentFPS = 0;
        return;
    }

    using namespace std::chrono;

    double interval =
        duration_cast<duration<double>>(now - streamOutput.lastStatsTime).count();
    if (interval <= 0.0) {
        return;
    }

    double currentBitrate = streamOutput.stats.currentBitrate;
    double currentFPS = static_cast<double>(streamOutput.stats.currentFPS);

    if (totalBytes >= streamOutput.lastBytes) {
        uint64_t byteDiff = totalBytes - streamOutput.lastBytes;
        double instantBitrateKbps = (byteDiff * 8.0) / (interval * 1000.0);

        if (instantBitrateKbps < 0.0 || instantBitrateKbps > 100000.0) {
            instantBitrateKbps = 0.0;
        }

        if (currentBitrate > 0.0 && instantBitrateKbps > 0.0) {
            double maxUp = currentBitrate * 1.5;
            double maxDown = currentBitrate * 0.5;
            if (instantBitrateKbps > maxUp) {
                instantBitrateKbps = maxUp;
            } else if (instantBitrateKbps < maxDown) {
                instantBitrateKbps = maxDown;
            }
        }

        const double alphaBitrate = 0.2;
        if (streamOutput.smoothedBitrateKbps <= 0.0) {
            streamOutput.smoothedBitrateKbps = instantBitrateKbps;
        } else {
            streamOutput.smoothedBitrateKbps =
                streamOutput.smoothedBitrateKbps * (1.0 - alphaBitrate) +
                instantBitrateKbps * alphaBitrate;
        }

        streamOutput.stats.currentBitrate =
            streamOutput.smoothedBitrateKbps > 0.0 ? streamOutput.smoothedBitrateKbps : 0.0;
    } else {
        streamOutput.lastBytes = totalBytes;
    }

    if (totalFrames >= streamOutput.lastFrames) {
        uint64_t frameDiff = totalFrames - streamOutput.lastFrames;
        double instantFPS = interval > 0.0 ? static_cast<double>(frameDiff) / interval : 0.0;

        if (instantFPS < 0.0 || instantFPS > 120.0) {
            instantFPS = 0.0;
        }

        if (currentFPS > 0.0 && instantFPS > 0.0) {
            double maxUp = currentFPS * 1.5;
            double maxDown = currentFPS * 0.5;
            if (instantFPS > maxUp) {
                instantFPS = maxUp;
            } else if (instantFPS < maxDown) {
                instantFPS = maxDown;
            }
        }

        const double alphaFPS = 0.3;
        if (streamOutput.smoothedFPS <= 0.0) {
            streamOutput.smoothedFPS = instantFPS;
        } else {
            streamOutput.smoothedFPS =
                streamOutput.smoothedFPS * (1.0 - alphaFPS) + instantFPS * alphaFPS;
        }

        if (streamOutput.smoothedFPS > 0.0) {
            streamOutput.stats.currentFPS =
                static_cast<int>(std::round(streamOutput.smoothedFPS));
        } else {
            streamOutput.stats.currentFPS = 0;
        }
    } else {
        streamOutput.lastFrames = totalFrames;
    }

    streamOutput.lastStatsTime = now;
    streamOutput.lastBytes = totalBytes;
    streamOutput.lastFrames = totalFrames;

    streamOutput.stats.cpuUsage = 0.0;        // Would need actual implementation

    if (streamOutput.stats.totalFrames > 0) {
        streamOutput.stats.averageBitrate = streamOutput.stats.currentBitrate;
    }
}

// Helper method implementations
std::string OneSevenLiveMultiRtmpStreamController::getOutputName(
    const std::string& streamId) const {
    return "MultiRTMP_Output_" + streamId;
}

std::string OneSevenLiveMultiRtmpStreamController::getServiceName(
    const std::string& streamId) const {
    return "MultiRTMP_Service_" + streamId;
}

std::string OneSevenLiveMultiRtmpStreamController::getVideoEncoderName(
    const std::string& streamId) const {
    return "MultiRTMP_VideoEncoder_" + streamId;
}

std::string OneSevenLiveMultiRtmpStreamController::getAudioEncoderName(
    const std::string& streamId) const {
    return "MultiRTMP_AudioEncoder_" + streamId;
}

obs_data_t* OneSevenLiveMultiRtmpStreamController::createServiceSettings(
    const OneSevenLiveMultiRtmpConfig& config) const {
    obs_log(LOG_INFO, "createServiceSettings (non-blocking)");
    obs_data_t* settings = ObsDataFromJson(config.serviceSettings);
    if (!settings) {
        settings = obs_data_create();
    }

    const std::string platform = config.streamName;
    if (platform == "YouTube") {
        obs_data_set_string(settings, "service", "YouTube - RTMPS");
    } else if (platform == "Twitch") {
        obs_data_set_string(settings, "service", "Twitch");
    }

    // Return settings as-is; server/key must be present in config
    return settings;
}

obs_data_t* OneSevenLiveMultiRtmpStreamController::createOutputSettings(
    const OneSevenLiveMultiRtmpConfig& config) const {
    obs_log(LOG_INFO, "createOutputSettings");
    obs_data_t* settings = ObsDataFromJson(config.outputSettings);
    // TODO: Add any additional output settings here
    return settings;
}

obs_data_t* OneSevenLiveMultiRtmpStreamController::createVideoEncoderSettings(
    const OneSevenLiveMultiRtmpConfig& config) const {
    obs_log(LOG_INFO, "createVideoEncoderSettings");

    // Use custom settings when videoConfig is provided; otherwise use OBS defaults
    if (config.videoConfig.has_value()) {
        const nlohmann::json& j = config.videoConfig->encoderSettings;
        if (!j.is_null()) {
            obs_data_t* settings = ObsDataFromJson(j);
            if (settings) {
                MULTI_RTMP_STREAM_LOG_DEBUG("Using custom video encoder settings from JSON");
                return settings;
            }
        }
        MULTI_RTMP_STREAM_LOG_DEBUG(
            "Video encoderSettings empty or invalid JSON, using empty settings");
        return obs_data_create();
    }

    MULTI_RTMP_STREAM_LOG_DEBUG(
        "No custom video config provided, using OBS default video encoder settings");
    return getObsDefaultVideoEncoderSettings();
}

obs_data_t* OneSevenLiveMultiRtmpStreamController::createAudioEncoderSettings(
    const OneSevenLiveMultiRtmpConfig& config) const {
    obs_log(LOG_INFO, "createAudioEncoderSettings");
    // Use custom settings when audioConfig is provided; otherwise use OBS defaults
    if (config.audioConfig.has_value()) {
        const nlohmann::json& j = config.audioConfig->encoderSettings;
        if (!j.is_null()) {
            obs_data_t* settings = ObsDataFromJson(j);
            if (settings) {
                MULTI_RTMP_STREAM_LOG_DEBUG("Using custom audio encoder settings from JSON");
                return settings;
            }
        }
        MULTI_RTMP_STREAM_LOG_DEBUG(
            "Audio encoderSettings empty or invalid JSON, using empty settings");
        return obs_data_create();
    }

    MULTI_RTMP_STREAM_LOG_DEBUG(
        "No custom audio config provided, using OBS default audio encoder settings");
    return getObsDefaultAudioEncoderSettings();
}




void OneSevenLiveMultiRtmpStreamController::destroyService(const std::string& streamId) {
    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end() && it->second->service) {
        obs_service_release(it->second->service);
        it->second->service = nullptr;
        MULTI_RTMP_STREAM_LOG_DEBUG("Service destroyed for stream: %s", streamId.c_str());
    }
}

void OneSevenLiveMultiRtmpStreamController::destroyEncoders(const std::string& streamId) {
    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end()) {
        if (it->second->videoEncoder) {
            obs_encoder_release(it->second->videoEncoder);
            it->second->videoEncoder = nullptr;
        }
        if (it->second->audioEncoder) {
            obs_encoder_release(it->second->audioEncoder);
            it->second->audioEncoder = nullptr;
        }
        MULTI_RTMP_STREAM_LOG_DEBUG("Encoders destroyed for stream: %s", streamId.c_str());
    }
}

void OneSevenLiveMultiRtmpStreamController::updateStreamStats(const std::string& streamId) {
    OneSevenLiveMultiRtmpStreamStats statsCopy;
    {
        std::lock_guard<std::mutex> lock(m_outputsMutex);
        auto it = m_streamOutputs.find(streamId);
        if (it == m_streamOutputs.end()) {
            return;
        }
        collectStreamStats(streamId, *it->second);
        statsCopy = it->second->stats;
    }
    StreamStatsCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_statsCallback;
    }
    if (cb) {
        cb(streamId, statsCopy);
    }
}

const char* OneSevenLiveMultiRtmpStreamController::getObsDefaultVideoEncoderId() const {
    // Try to get the encoder ID from the main streaming output
    obs_output_t* streamingOutput = obs_frontend_get_streaming_output();
    if (streamingOutput) {
        obs_encoder_t* videoEncoder = obs_output_get_video_encoder(streamingOutput);
        if (videoEncoder) {
            const char* encoderId = obs_encoder_get_id(videoEncoder);
            obs_output_release(streamingOutput);
            return encoderId ? encoderId : VIDEO_ENCODER_ID;
        }
        obs_output_release(streamingOutput);
    }

    // Fallback to default x264 encoder
    return VIDEO_ENCODER_ID;
}

obs_data_t* OneSevenLiveMultiRtmpStreamController::getObsDefaultVideoEncoderSettings() const {
    obs_data_t* settings = obs_data_create();

    // Try to get settings from the main streaming output
    obs_output_t* streamingOutput = obs_frontend_get_streaming_output();
    if (streamingOutput) {
        obs_encoder_t* videoEncoder = obs_output_get_video_encoder(streamingOutput);
        if (videoEncoder) {
            ObsDataPtr encoderSettings{obs_encoder_get_settings(videoEncoder)};
            if (encoderSettings) {
                // Copy the settings
                obs_data_apply(settings, encoderSettings.get());
                obs_output_release(streamingOutput);
                MULTI_RTMP_STREAM_LOG_DEBUG("Using OBS default video encoder settings");
                return settings;
            }
        }
        obs_output_release(streamingOutput);
    }

    // Fallback to reasonable defaults if OBS settings are not available
    obs_data_set_int(settings, "bitrate", 2500);
    obs_data_set_int(settings, "keyint_sec", 2);
    obs_data_set_string(settings, "rate_control", "CBR");
    obs_data_set_string(settings, "profile", "main");
    obs_data_set_bool(settings, "use_bufsize", true);
    obs_data_set_bool(settings, "bframes", false);

    MULTI_RTMP_STREAM_LOG_DEBUG("Using fallback video encoder settings");
    return settings;
}

obs_data_t* OneSevenLiveMultiRtmpStreamController::getObsDefaultAudioEncoderSettings() const {
    obs_data_t* settings = obs_data_create();

    // Try to get settings from the main streaming output
    obs_output_t* streamingOutput = obs_frontend_get_streaming_output();
    if (streamingOutput) {
        obs_encoder_t* audioEncoder = obs_output_get_audio_encoder(streamingOutput, 0);
        if (audioEncoder) {
            ObsDataPtr encoderSettings{obs_encoder_get_settings(audioEncoder)};
            if (encoderSettings) {
                // Copy the settings
                obs_data_apply(settings, encoderSettings.get());
                obs_output_release(streamingOutput);
                MULTI_RTMP_STREAM_LOG_DEBUG("Using OBS default audio encoder settings");
                return settings;
            }
        }
        obs_output_release(streamingOutput);
    }

    // Fallback to reasonable defaults if OBS settings are not available
    obs_data_set_int(settings, "bitrate", 128);
    obs_data_set_int(settings, "rate", 44100);

    MULTI_RTMP_STREAM_LOG_DEBUG("Using fallback audio encoder settings");
    return settings;
}

#include "utility/Common.hpp"


// removed global stop aggregation; rely on manager to orchestrate destroy after stop
void OneSevenLiveMultiRtmpStreamController::beginShutdown() {
    m_shuttingDown.store(true);
}

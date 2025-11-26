#include "OneSevenLiveMultiRtmpStreamController.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>

#include <QEventLoop>
#include <QString>
#include <QTimer>
#include <QPointer>
#include <QObject>
#include <chrono>
#include <thread>

#include "OneSevenLiveCoreManager.hpp"
#include "plugin-support.h"
#include "twitch/OneSevenLiveTwitchAuth.hpp"
#include "twitch/OneSevenLiveTwitchClient.hpp"
#include "utility/Common.hpp"
#include "youtube/OneSevenLiveYouTubeAuth.hpp"
#include "youtube/OneSevenLiveYouTubeClient.hpp"

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
        MULTI_RTMP_STREAM_LOG_INFO(
            "Service not yet available for stream: %s; awaiting async resolution",
            streamId.c_str());
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
        MULTI_RTMP_STREAM_LOG_INFO(
            "Output not ready for stream: %s; waiting for async service/key resolution",
            streamId.c_str());
        updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::CONNECTING);
        return true;
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

    // Setup connect timeout timer
    if (streamOutput->connectTimeoutTimer) {
        streamOutput->connectTimeoutTimer->stop();
        streamOutput->connectTimeoutTimer->deleteLater();
        streamOutput->connectTimeoutTimer = nullptr;
    }
    streamOutput->connectTimeoutTimer = new QTimer();
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
    streamOutput->connectTimeoutTimer->start(CONNECT_TIMEOUT_MS);

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
    if (!streamOutput || !streamOutput->output) {
        MULTI_RTMP_STREAM_LOG_ERROR("Invalid stream output for: %s", streamId.c_str());
        return false;
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
        streamOutput->connectTimeoutTimer->stop();
        streamOutput->connectTimeoutTimer->deleteLater();
        streamOutput->connectTimeoutTimer = nullptr;
    }

    // Stop output if active
    if (streamOutput->output && obs_output_active(streamOutput->output)) {
        obs_output_stop(streamOutput->output);
    }

    // Destroy OBS objects
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
        if (obs_output_active(streamOutput->output)) {
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
        // Stop output if active
        if (streamOutput->output && obs_output_active(streamOutput->output)) {
            obs_output_stop(streamOutput->output);
        }

        // Destroy OBS objects
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
    if (!m_sharedVideoEncoder) {
        // Get the main video encoder from OBS
        m_sharedVideoEncoder =
            obs_frontend_get_streaming_output()
                ? obs_output_get_video_encoder(obs_frontend_get_streaming_output())
                : nullptr;

        if (m_sharedVideoEncoder) {
            obs_encoder_get_ref(m_sharedVideoEncoder);
        } else {
            MULTI_RTMP_STREAM_LOG_WARNING("No shared video encoder available");
        }
    }

    return m_sharedVideoEncoder;
}

obs_encoder_t* OneSevenLiveMultiRtmpStreamController::getSharedAudioEncoder(int mixerId) {
    auto it = m_sharedAudioEncoders.find(mixerId);
    if (it != m_sharedAudioEncoders.end()) {
        return it->second;
    }

    // Get the main audio encoder from OBS
    obs_encoder_t* audioEncoder =
        obs_frontend_get_streaming_output()
            ? obs_output_get_audio_encoder(obs_frontend_get_streaming_output(), 0)
            : nullptr;

    if (audioEncoder) {
        obs_encoder_get_ref(audioEncoder);
        m_sharedAudioEncoders[mixerId] = audioEncoder;
    } else {
        MULTI_RTMP_STREAM_LOG_WARNING("No shared audio encoder available for mixer %d", mixerId);
    }

    return audioEncoder;
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
    m_statusCallback = callback;
}

void OneSevenLiveMultiRtmpStreamController::setStreamStatsCallback(StreamStatsCallback callback) {
    m_statsCallback = callback;
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
    MULTI_RTMP_STREAM_LOG_INFO("Server/key missing for stream: %s; resolving asynchronously",
                               streamId.c_str());
    resolvePlatformServerKeyAsync(streamId, config);
    return true;
}

bool OneSevenLiveMultiRtmpStreamController::createEncoders(
    const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config,
    StreamOutput* streamOutput) {
    if (!streamOutput) {
        MULTI_RTMP_STREAM_LOG_ERROR("StreamOutput is null for stream: %s", streamId.c_str());
        return false;
    }

    // Video encoder
    if (!config.videoConfig.has_value()) {
        // Use shared encoder when no custom video config provided
        streamOutput->videoEncoder = getSharedVideoEncoder();

        // Fallback to creating independent encoder if shared encoder is not available
        if (!streamOutput->videoEncoder) {
            MULTI_RTMP_STREAM_LOG_WARNING(
                "Shared video encoder not available, creating independent encoder for stream: %s",
                streamId.c_str());

            ObsDataPtr videoSettings{createVideoEncoderSettings(config)};
            if (!videoSettings) {
                MULTI_RTMP_STREAM_LOG_ERROR(
                    "Failed to create video encoder settings for stream: %s", streamId.c_str());
                return false;
            }

            // Determine the video encoder ID to use
            const char* videoEncoderId = getObsDefaultVideoEncoderId();

            streamOutput->videoEncoder = obs_video_encoder_create(
                videoEncoderId, getVideoEncoderName(streamId).c_str(), videoSettings.get(), nullptr);
            videoSettings.reset();

            if (!streamOutput->videoEncoder) {
                MULTI_RTMP_STREAM_LOG_ERROR(
                    "Failed to create fallback video encoder for stream: %s", streamId.c_str());
                return false;
            }
        }
    } else {
        // Create dedicated encoder with custom settings
        ObsDataPtr videoSettings{createVideoEncoderSettings(config)};
        if (!videoSettings) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create video encoder settings for stream: %s",
                                        streamId.c_str());
            return false;
        }

        // Determine the video encoder ID to use
        const char* videoEncoderId = config.videoConfig->encoderId.empty()
                                         ? getObsDefaultVideoEncoderId()
                                         : config.videoConfig->encoderId.c_str();

        streamOutput->videoEncoder = obs_video_encoder_create(
            videoEncoderId, getVideoEncoderName(streamId).c_str(), videoSettings.get(), nullptr);
        videoSettings.reset();

        if (!streamOutput->videoEncoder) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create video encoder for stream: %s",
                                        streamId.c_str());
            return false;
        }
    }

    // Audio encoder
    if (!config.audioConfig.has_value()) {
        // Use shared audio encoder when no custom audio config provided
        streamOutput->audioEncoder = getSharedAudioEncoder(0);

        // Fallback to creating independent encoder if shared encoder is not available
        if (!streamOutput->audioEncoder) {
            MULTI_RTMP_STREAM_LOG_WARNING(
                "Shared audio encoder not available, creating independent encoder for stream: %s",
                streamId.c_str());

            ObsDataPtr audioSettings{createAudioEncoderSettings(config)};
            if (!audioSettings) {
                MULTI_RTMP_STREAM_LOG_ERROR(
                    "Failed to create audio encoder settings for stream: %s", streamId.c_str());
                return false;
            }

            // Determine the audio encoder ID to use
            const char* audioEncoderId = AUDIO_ENCODER_ID;

            streamOutput->audioEncoder = obs_audio_encoder_create(
                audioEncoderId, getAudioEncoderName(streamId).c_str(), audioSettings.get(), 0, nullptr);
            audioSettings.reset();

            if (!streamOutput->audioEncoder) {
                MULTI_RTMP_STREAM_LOG_ERROR(
                    "Failed to create fallback audio encoder for stream: %s", streamId.c_str());
                return false;
            }
        }
    } else {
        ObsDataPtr audioSettings{createAudioEncoderSettings(config)};
        if (!audioSettings) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create audio encoder settings for stream: %s",
                                        streamId.c_str());
            return false;
        }

        // Determine the audio encoder ID to use
        const char* audioEncoderId = config.audioConfig->encoderId.empty()
                                         ? AUDIO_ENCODER_ID
                                         : config.audioConfig->encoderId.c_str();

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

    streamOutput->output =
        obs_output_create(OUTPUT_ID, getOutputName(streamId).c_str(), outputSettings.get(), nullptr);
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

        if (m_statusCallback) {
            m_statusCallback(streamId, it->second->status);
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
                streamOutput->connectTimeoutTimer->stop();
                streamOutput->connectTimeoutTimer->deleteLater();
                streamOutput->connectTimeoutTimer = nullptr;
            }
            controller->updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::STREAMING);
            MULTI_RTMP_STREAM_LOG_INFO("Stream started: %s", streamId.c_str());
            break;
        }
    }
}

void OneSevenLiveMultiRtmpStreamController::outputStopCallback(void* data, calldata_t* cd) {
    auto* controller = static_cast<OneSevenLiveMultiRtmpStreamController*>(data);
    obs_output_t* output = static_cast<obs_output_t*>(calldata_ptr(cd, "output"));

    // Find stream ID by output - TEMPORARILY REMOVED LOCK FOR DEBUGGING
    // std::lock_guard<std::mutex> lock(controller->m_outputsMutex);
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
            MULTI_RTMP_STREAM_LOG_INFO("Stream stopped: %s", streamId.c_str());
            break;
        }
    }
}

void OneSevenLiveMultiRtmpStreamController::outputReconnectCallback(void* data, calldata_t* cd) {
    auto* controller = static_cast<OneSevenLiveMultiRtmpStreamController*>(data);
    obs_output_t* output = static_cast<obs_output_t*>(calldata_ptr(cd, "output"));

    // Find stream ID by output - TEMPORARILY REMOVED LOCK FOR DEBUGGING
    // std::lock_guard<std::mutex> lock(controller->m_outputsMutex);
    for (const auto& [streamId, streamOutput] : controller->m_streamOutputs) {
        if (streamOutput->output == output) {
            if (streamOutput->connectTimeoutTimer) {
                streamOutput->connectTimeoutTimer->stop();
                streamOutput->connectTimeoutTimer->deleteLater();
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

    // Find stream ID by output - TEMPORARILY REMOVED LOCK FOR DEBUGGING
    // std::lock_guard<std::mutex> lock(controller->m_outputsMutex);
    for (const auto& [streamId, streamOutput] : controller->m_streamOutputs) {
        if (streamOutput->output == output) {
            if (streamOutput->connectTimeoutTimer) {
                streamOutput->connectTimeoutTimer->stop();
                streamOutput->connectTimeoutTimer->deleteLater();
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
                if (streamOutput->output && obs_output_active(streamOutput->output)) {
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

    // Update duration
    auto now = std::chrono::steady_clock::now();
    streamOutput.stats.duration = now - streamOutput.startTime;

    // Get output statistics
    streamOutput.stats.totalFrames =
        static_cast<int>(obs_output_get_total_frames(streamOutput.output));
    streamOutput.stats.droppedFrames =
        static_cast<int>(obs_output_get_frames_dropped(streamOutput.output));

    // Calculate bitrate and FPS (these would need to be implemented based on OBS API)
    // For now, we'll use placeholder values
    streamOutput.stats.currentBitrate = 0.0;  // Would need actual implementation
    streamOutput.stats.currentFPS = 0;        // Would need actual implementation
    streamOutput.stats.cpuUsage = 0.0;        // Would need actual implementation

    if (streamOutput.stats.totalFrames > 0) {
        streamOutput.stats.averageBitrate = streamOutput.stats.currentBitrate;  // Simplified
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

    // Return settings as-is; async resolution will fill server/key if missing
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

void OneSevenLiveMultiRtmpStreamController::resolvePlatformServerKeyAsync(
    const std::string& streamId, const OneSevenLiveMultiRtmpConfig& config) {
    QTimer::singleShot(0, [this, streamId, config]() {
        std::string platform;
        try {
            if (config.serviceSettings.contains("service") &&
                config.serviceSettings["service"].is_string()) {
                platform = config.serviceSettings["service"].get<std::string>();
            }
        } catch (...) {
        }
        if (platform.empty()) {
            platform = config.streamName;
        }

        auto contains_ci = [](const std::string& s, const std::string& needle) {
            std::string hs = s, hn = needle;
            std::transform(hs.begin(), hs.end(), hs.begin(), ::tolower);
            std::transform(hn.begin(), hn.end(), hn.begin(), ::tolower);
            return hs.find(hn) != std::string::npos;
        };

        if (contains_ci(platform, "youtube")) {
            auto* ytAuth = OneSevenLiveCoreManager::getInstance().getYouTubeAuth();
            if (!ytAuth || !ytAuth->hasValidToken()) {
                MULTI_RTMP_STREAM_LOG_WARNING("YouTube auth not available; cannot resolve for %s",
                                              streamId.c_str());
                updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE,
                                   "AuthInvalid:YouTube");
                return;
            }

            auto client = std::make_unique<OneSevenLiveYouTubeClient>();
            client->setAccessToken(ytAuth->getAccessToken());

            QPointer<QTimer> timeout = new QTimer(client.get());
            timeout->setSingleShot(true);
            QObject::connect(timeout, &QTimer::timeout, [this, streamId, timeout]() {
                MULTI_RTMP_STREAM_LOG_WARNING("YouTube resolve timeout for stream: %s",
                                              streamId.c_str());
                updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE,
                                   "NetworkError:YouTube:Timeout");
                m_pendingYouTubeClients.erase(streamId);
                if (timeout)
                    timeout->deleteLater();
            });

            QObject::connect(
                client.get(), &OneSevenLiveYouTubeClient::myLiveStreamsReceived,
                [this, streamId, timeout](const YouTubeLiveStreamListResponse& resp) {
                    if (timeout)
                        timeout->stop();
                    QString resolvedServer;
                    QString resolvedKey;
                    for (const auto& s : resp.items) {
                        const auto& info = s.cdn.ingestionInfo;
                        const QString serverCandidate = !info.rtmpsIngestionAddress.isEmpty()
                                                            ? info.rtmpsIngestionAddress
                                                            : info.ingestionAddress;
                        const QString keyCandidate = info.streamName;
                        if (!serverCandidate.isEmpty() && !keyCandidate.isEmpty()) {
                            resolvedServer = serverCandidate;
                            resolvedKey = keyCandidate;
                            break;
                        }
                    }
                    if (!resolvedServer.isEmpty() && !resolvedKey.isEmpty()) {
                        finalizeServiceSetupAfterResolve(streamId,
                                                         resolvedServer.toUtf8().constData(),
                                                         resolvedKey.toUtf8().constData());
                    } else {
                        MULTI_RTMP_STREAM_LOG_WARNING(
                            "YouTube resolve returned empty server/key for %s", streamId.c_str());
                        updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE,
                                           "MissingServerKey:YouTube");
                    }
                });

            QObject::connect(
                client.get(), &OneSevenLiveYouTubeClient::errorOccurred,
                [this, streamId, timeout](const QString& err, const QString&) {
                    if (timeout)
                        timeout->stop();
                    MULTI_RTMP_STREAM_LOG_WARNING("YouTube resolve error for stream: %s",
                                                  streamId.c_str());
                    QString e = err.toLower();
                    bool isNet = e.contains("recv failure") || e.contains("connection reset") ||
                                 e.contains("timeout") || e.contains("could not resolve") ||
                                 e.contains("dns") || e.contains("tls") || e.contains("ssl") ||
                                 e.contains("handshake") || e.contains("network");
                    std::string msg =
                        std::string(isNet ? "NetworkError:YouTube:" : "APIError:YouTube:") +
                        err.toUtf8().constData();
                    updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE,
                                       msg.c_str());
                });

            m_pendingYouTubeClients[streamId] = std::move(client);
            timeout->start(5000);
            m_pendingYouTubeClients[streamId]->getMyLiveStreams();
        } else if (contains_ci(platform, "twitch")) {
            auto* twAuth = OneSevenLiveCoreManager::getInstance().getTwitchAuth();
            OneSevenLiveTwitchClient* client = nullptr;
            if (twAuth) {
                client = twAuth->getTwitchClient();
            }
            if (!client) {
                m_pendingTwitchClients[streamId] = std::make_unique<OneSevenLiveTwitchClient>();
                client = m_pendingTwitchClients[streamId].get();
            }

            if (!twAuth || !twAuth->hasValidToken()) {
                MULTI_RTMP_STREAM_LOG_WARNING("Twitch auth not available; cannot resolve for %s",
                                              streamId.c_str());
                updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE,
                                   "AuthInvalid:Twitch");
                return;
            }

            if (!client->hasValidAuth()) {
                client->setAuthData(twAuth->getAccessToken(), QString(TWITCH_API_CLIENT_ID));
            }

            QPointer<QTimer> timeout = new QTimer(client);
            timeout->setSingleShot(true);
            QObject::connect(timeout, &QTimer::timeout, [this, streamId, timeout]() {
                MULTI_RTMP_STREAM_LOG_WARNING("Twitch resolve timeout for stream: %s",
                                              streamId.c_str());
                updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE,
                                   "NetworkError:Twitch:Timeout");
                m_pendingTwitchClients.erase(streamId);
                if (timeout)
                    timeout->deleteLater();
            });

            const QMetaObject::Connection userConn = QObject::connect(
                client, &OneSevenLiveTwitchClient::userInfoReceived,
                [client, userConn](const TwitchUserInfo& user) {
                    if (client) client->getStreamKey(user.id);
                    QObject::disconnect(userConn);
                });

            const QMetaObject::Connection keyConn = QObject::connect(
                client, &OneSevenLiveTwitchClient::streamKeyReceived,
                [this, streamId, timeout, keyConn](const QString& keyVal) {
                    if (timeout)
                        timeout->stop();
                    const QString serverUrl = OneSevenLiveTwitchClient::TWITCH_RTMP_SERVER;
                    finalizeServiceSetupAfterResolve(streamId, serverUrl.toUtf8().constData(),
                                                     keyVal.toUtf8().constData());
                    QObject::disconnect(keyConn);
                });

            QObject::connect(
                client, &OneSevenLiveTwitchClient::errorOccurred,
                [this, streamId, timeout](const QString& err) {
                    if (timeout)
                        timeout->stop();
                    MULTI_RTMP_STREAM_LOG_WARNING("Twitch resolve error for stream: %s",
                                                  streamId.c_str());
                    QString e = err.toLower();
                    bool isNet = e.contains("recv failure") || e.contains("connection reset") ||
                                 e.contains("timeout") || e.contains("could not resolve") ||
                                 e.contains("dns") || e.contains("tls") || e.contains("ssl") ||
                                 e.contains("handshake") || e.contains("network");
                    std::string msg =
                        std::string(isNet ? "NetworkError:Twitch:" : "APIError:Twitch:") +
                        err.toUtf8().constData();
                    updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE,
                                       msg.c_str());
                });

            timeout->start(5000);
            client->getCurrentUser();
        } else {
            MULTI_RTMP_STREAM_LOG_WARNING("Unknown platform for stream: %s", streamId.c_str());
            updateStreamStatus(streamId, OneSevenLiveMultiRtmpStreamStatus::ERROR_STATE,
                               "UnknownPlatform");
        }
    });
}

void OneSevenLiveMultiRtmpStreamController::finalizeServiceSetupAfterResolve(
    const std::string& streamId, const std::string& server, const std::string& key) {
    auto it = m_streamOutputs.find(streamId);
    if (it == m_streamOutputs.end()) {
        MULTI_RTMP_STREAM_LOG_ERROR("StreamOutput missing during finalize for: %s",
                                    streamId.c_str());
        return;
    }

    auto* streamOutput = it->second.get();
    const std::string platform = streamOutput->config.streamName;

    ObsDataPtr settings{ObsDataFromJson(streamOutput->config.serviceSettings)};
    if (!settings)
        settings.reset(obs_data_create());
    if (platform == "YouTube") {
    obs_data_set_string(settings.get(), "service", "YouTube - RTMPS");
    } else if (platform == "Twitch") {
        obs_data_set_string(settings.get(), "service", "Twitch");
    }
    obs_data_set_string(settings.get(), "server", server.c_str());
    obs_data_set_string(settings.get(), "key", key.c_str());

    streamOutput->service =
        obs_service_create(SERVICE_ID, getServiceName(streamId).c_str(), settings.get(), nullptr);
    settings.reset();
    if (!streamOutput->service) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to create service after resolve for: %s",
                                    streamId.c_str());
        return;
    }

    if (!streamOutput->videoEncoder || !streamOutput->audioEncoder) {
        if (!createEncoders(streamId, streamOutput->config, streamOutput)) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create encoders after resolve for: %s",
                                        streamId.c_str());
            return;
        }
    }

    if (!streamOutput->output) {
        if (!setupOutput(streamId, streamOutput->config, streamOutput)) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to setup output after resolve for: %s",
                                        streamId.c_str());
            return;
        }
    }

    // Cleanup pending clients if any
    m_pendingYouTubeClients.erase(streamId);
    m_pendingTwitchClients.erase(streamId);

    (void) startOutputInternal(streamId, streamOutput);
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
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end()) {
        collectStreamStats(streamId, *it->second);
        if (m_statsCallback) {
            m_statsCallback(streamId, it->second->stats);
        }
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

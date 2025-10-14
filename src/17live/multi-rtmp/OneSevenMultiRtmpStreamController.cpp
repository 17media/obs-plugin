#include "OneSevenMultiRtmpStreamController.hpp"
#include <obs-frontend-api.h>
#include <chrono>
#include <thread>

OneSevenMultiRtmpStreamController::OneSevenMultiRtmpStreamController() {
    MULTI_RTMP_STREAM_LOG_INFO("Stream controller initialized");
}

OneSevenMultiRtmpStreamController::~OneSevenMultiRtmpStreamController() {
    stopStatsMonitoring();
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
    
    MULTI_RTMP_STREAM_LOG_INFO("Stream controller destroyed");
}

bool OneSevenMultiRtmpStreamController::createOutput(const std::string& streamId, const OneSevenMultiRtmpConfig& config) {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
    if (m_streamOutputs.find(streamId) != m_streamOutputs.end()) {
        MULTI_RTMP_STREAM_LOG_ERROR("Output already exists for stream: %s", streamId.c_str());
        return false;
    }
    
    auto streamOutput = std::make_unique<StreamOutput>();
    streamOutput->config = config;
    streamOutput->status.id = streamId;
    streamOutput->status.state = OneSevenMultiRtmpStreamStatus::STOPPED;
    
    // Create service
    if (!createService(streamId, config, streamOutput.get())) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to create service for stream: %s", streamId.c_str());
        return false;
    }
    
    // Create encoders
    if (!createEncoders(streamId, config, streamOutput.get())) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to create encoders for stream: %s", streamId.c_str());
        destroyService(streamId);
        return false;
    }
    
    // Setup output
    if (!setupOutput(streamId, config, streamOutput.get())) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to setup output for stream: %s", streamId.c_str());
        destroyEncoders(streamId);
        destroyService(streamId);
        return false;
    }
    
    m_streamOutputs[streamId] = std::move(streamOutput);
    
    MULTI_RTMP_STREAM_LOG_INFO("Output created successfully for stream: %s (%s)", 
                               config.streamName.c_str(), streamId.c_str());
    return true;
}

bool OneSevenMultiRtmpStreamController::startOutput(const std::string& streamId) {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
    auto it = m_streamOutputs.find(streamId);
    if (it == m_streamOutputs.end()) {
        MULTI_RTMP_STREAM_LOG_ERROR("Output not found for stream: %s", streamId.c_str());
        return false;
    }
    
    auto& streamOutput = it->second;
    if (!streamOutput->output) {
        MULTI_RTMP_STREAM_LOG_ERROR("Output object is null for stream: %s", streamId.c_str());
        return false;
    }
    
    if (obs_output_active(streamOutput->output)) {
        MULTI_RTMP_STREAM_LOG_WARNING("Output already active for stream: %s", streamId.c_str());
        return true;
    }
    
    updateStreamStatus(streamId, OneSevenMultiRtmpStreamStatus::CONNECTING);
    
    if (!obs_output_start(streamOutput->output)) {
        std::string error = obs_output_get_last_error(streamOutput->output);
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to start output for stream %s: %s", streamId.c_str(), error.c_str());
        updateStreamStatus(streamId, OneSevenMultiRtmpStreamStatus::ERROR, error);
        return false;
    }
    
    streamOutput->startTime = std::chrono::steady_clock::now();
    
    MULTI_RTMP_STREAM_LOG_INFO("Output started for stream: %s", streamId.c_str());
    return true;
}

bool OneSevenMultiRtmpStreamController::stopOutput(const std::string& streamId) {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
    auto it = m_streamOutputs.find(streamId);
    if (it == m_streamOutputs.end()) {
        MULTI_RTMP_STREAM_LOG_ERROR("Output not found for stream: %s", streamId.c_str());
        return false;
    }
    
    auto& streamOutput = it->second;
    if (!streamOutput->output) {
        MULTI_RTMP_STREAM_LOG_ERROR("Output object is null for stream: %s", streamId.c_str());
        return false;
    }
    
    if (!obs_output_active(streamOutput->output)) {
        MULTI_RTMP_STREAM_LOG_WARNING("Output already inactive for stream: %s", streamId.c_str());
        updateStreamStatus(streamId, OneSevenMultiRtmpStreamStatus::STOPPED);
        return true;
    }
    
    updateStreamStatus(streamId, OneSevenMultiRtmpStreamStatus::STOPPED);
    obs_output_stop(streamOutput->output);
    
    MULTI_RTMP_STREAM_LOG_INFO("Output stopped for stream: %s", streamId.c_str());
    return true;
}

bool OneSevenMultiRtmpStreamController::destroyOutput(const std::string& streamId) {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
    auto it = m_streamOutputs.find(streamId);
    if (it == m_streamOutputs.end()) {
        MULTI_RTMP_STREAM_LOG_WARNING("Output not found for stream: %s", streamId.c_str());
        return false;
    }
    
    auto& streamOutput = it->second;
    
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
    if (streamOutput->videoEncoder && !streamOutput->config.video.useSharedEncoder) {
        obs_encoder_release(streamOutput->videoEncoder);
    }
    if (streamOutput->audioEncoder && !streamOutput->config.audio.useSharedEncoder) {
        obs_encoder_release(streamOutput->audioEncoder);
    }
    
    m_streamOutputs.erase(it);
    
    MULTI_RTMP_STREAM_LOG_INFO("Output destroyed for stream: %s", streamId.c_str());
    return true;
}

bool OneSevenMultiRtmpStreamController::startAllOutputs() {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
    bool allStarted = true;
    for (const auto& [streamId, streamOutput] : m_streamOutputs) {
        if (!obs_output_active(streamOutput->output)) {
            if (!startOutput(streamId)) {
                allStarted = false;
                MULTI_RTMP_STREAM_LOG_ERROR("Failed to start output for stream: %s", streamId.c_str());
            }
        }
    }
    
    MULTI_RTMP_STREAM_LOG_INFO("Start all outputs completed, success: %s", allStarted ? "true" : "false");
    return allStarted;
}

bool OneSevenMultiRtmpStreamController::stopAllOutputs() {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
    bool allStopped = true;
    for (const auto& [streamId, streamOutput] : m_streamOutputs) {
        if (obs_output_active(streamOutput->output)) {
            if (!stopOutput(streamId)) {
                allStopped = false;
                MULTI_RTMP_STREAM_LOG_ERROR("Failed to stop output for stream: %s", streamId.c_str());
            }
        }
    }
    
    MULTI_RTMP_STREAM_LOG_INFO("Stop all outputs completed, success: %s", allStopped ? "true" : "false");
    return allStopped;
}

void OneSevenMultiRtmpStreamController::destroyAllOutputs() {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
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
        if (streamOutput->videoEncoder && !streamOutput->config.video.useSharedEncoder) {
            obs_encoder_release(streamOutput->videoEncoder);
        }
        if (streamOutput->audioEncoder && !streamOutput->config.audio.useSharedEncoder) {
            obs_encoder_release(streamOutput->audioEncoder);
        }
    }
    
    m_streamOutputs.clear();
    MULTI_RTMP_STREAM_LOG_INFO("All outputs destroyed");
}

OneSevenMultiRtmpStreamStatus OneSevenMultiRtmpStreamController::getStreamStatus(const std::string& streamId) const {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end()) {
        return it->second->status;
    }
    
    OneSevenMultiRtmpStreamStatus status;
    status.id = streamId;
    status.state = OneSevenMultiRtmpStreamStatus::STOPPED;
    return status;
}

OneSevenMultiRtmpStreamStats OneSevenMultiRtmpStreamController::getStreamStats(const std::string& streamId) const {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end()) {
        return it->second->stats;
    }
    
    OneSevenMultiRtmpStreamStats stats;
    stats.id = streamId;
    return stats;
}

std::vector<std::string> OneSevenMultiRtmpStreamController::getActiveStreamIds() const {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
    std::vector<std::string> activeIds;
    for (const auto& [streamId, streamOutput] : m_streamOutputs) {
        if (streamOutput->output && obs_output_active(streamOutput->output)) {
            activeIds.push_back(streamId);
        }
    }
    
    return activeIds;
}

std::vector<std::string> OneSevenMultiRtmpStreamController::getAllStreamIds() const {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
    std::vector<std::string> allIds;
    allIds.reserve(m_streamOutputs.size());
    
    for (const auto& [streamId, streamOutput] : m_streamOutputs) {
        allIds.push_back(streamId);
    }
    
    return allIds;
}

obs_encoder_t* OneSevenMultiRtmpStreamController::getSharedVideoEncoder() {
    if (!m_sharedVideoEncoder) {
        // Get the main video encoder from OBS
        m_sharedVideoEncoder = obs_frontend_get_streaming_output() ? 
            obs_output_get_video_encoder(obs_frontend_get_streaming_output()) : nullptr;
        
        if (m_sharedVideoEncoder) {
            obs_encoder_get_ref(m_sharedVideoEncoder);
            MULTI_RTMP_STREAM_LOG_INFO("Using shared video encoder from main stream");
        } else {
            MULTI_RTMP_STREAM_LOG_WARNING("No shared video encoder available");
        }
    }
    
    return m_sharedVideoEncoder;
}

obs_encoder_t* OneSevenMultiRtmpStreamController::getSharedAudioEncoder(int mixerId) {
    auto it = m_sharedAudioEncoders.find(mixerId);
    if (it != m_sharedAudioEncoders.end()) {
        return it->second;
    }
    
    // Get the main audio encoder from OBS
    obs_encoder_t* audioEncoder = obs_frontend_get_streaming_output() ? 
        obs_output_get_audio_encoder(obs_frontend_get_streaming_output(), 0) : nullptr;
    
    if (audioEncoder) {
        obs_encoder_get_ref(audioEncoder);
        m_sharedAudioEncoders[mixerId] = audioEncoder;
        MULTI_RTMP_STREAM_LOG_INFO("Using shared audio encoder for mixer %d", mixerId);
    } else {
        MULTI_RTMP_STREAM_LOG_WARNING("No shared audio encoder available for mixer %d", mixerId);
    }
    
    return audioEncoder;
}

bool OneSevenMultiRtmpStreamController::isStreamActive(const std::string& streamId) const {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    
    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end()) {
        return it->second->output && obs_output_active(it->second->output);
    }
    
    return false;
}

bool OneSevenMultiRtmpStreamController::hasOutput(const std::string& streamId) const {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    return m_streamOutputs.find(streamId) != m_streamOutputs.end();
}

void OneSevenMultiRtmpStreamController::setStreamStatusCallback(StreamStatusCallback callback) {
    m_statusCallback = callback;
}

void OneSevenMultiRtmpStreamController::setStreamStatsCallback(StreamStatsCallback callback) {
    m_statsCallback = callback;
}

void OneSevenMultiRtmpStreamController::startStatsMonitoring() {
    std::lock_guard<std::mutex> lock(m_statsThreadMutex);
    
    if (m_statsMonitoringActive) {
        return;
    }
    
    m_statsMonitoringActive = true;
    m_statsThread = std::thread(&OneSevenMultiRtmpStreamController::statsMonitoringThread, this);
    
    MULTI_RTMP_STREAM_LOG_INFO("Statistics monitoring started");
}

void OneSevenMultiRtmpStreamController::stopStatsMonitoring() {
    {
        std::lock_guard<std::mutex> lock(m_statsThreadMutex);
        m_statsMonitoringActive = false;
    }
    
    if (m_statsThread.joinable()) {
        m_statsThread.join();
    }
    
    MULTI_RTMP_STREAM_LOG_INFO("Statistics monitoring stopped");
}

bool OneSevenMultiRtmpStreamController::createService(const std::string& streamId, const OneSevenMultiRtmpConfig& config, StreamOutput* streamOutput) {
    if (!streamOutput) {
        MULTI_RTMP_STREAM_LOG_ERROR("StreamOutput is null for stream: %s", streamId.c_str());
        return false;
    }
    
    obs_data_t* serviceSettings = createServiceSettings(config);
    if (!serviceSettings) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to create service settings for stream: %s", streamId.c_str());
        return false;
    }
    
    streamOutput->service = obs_service_create(SERVICE_ID, getServiceName(streamId).c_str(), serviceSettings, nullptr);
    obs_data_release(serviceSettings);
    
    if (!streamOutput->service) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to create service for stream: %s", streamId.c_str());
        return false;
    }
    
    MULTI_RTMP_STREAM_LOG_INFO("Service created successfully for stream: %s", streamId.c_str());
    return true;
}

bool OneSevenMultiRtmpStreamController::createEncoders(const std::string& streamId, const OneSevenMultiRtmpConfig& config, StreamOutput* streamOutput) {
    if (!streamOutput) {
        MULTI_RTMP_STREAM_LOG_ERROR("StreamOutput is null for stream: %s", streamId.c_str());
        return false;
    }
    
    // Video encoder
    if (config.video.useSharedEncoder) {
        streamOutput->videoEncoder = getSharedVideoEncoder();
        
        // Fallback to creating independent encoder if shared encoder is not available
        if (!streamOutput->videoEncoder) {
            MULTI_RTMP_STREAM_LOG_WARNING("Shared video encoder not available, creating independent encoder for stream: %s", streamId.c_str());
            
            obs_data_t* videoSettings = createVideoEncoderSettings(config);
            if (!videoSettings) {
                MULTI_RTMP_STREAM_LOG_ERROR("Failed to create video encoder settings for stream: %s", streamId.c_str());
                return false;
            }
            
            // Determine the video encoder ID to use
            const char* videoEncoderId = config.video.encoderId.empty() ? 
                getObsDefaultVideoEncoderId() : config.video.encoderId.c_str();
            
            streamOutput->videoEncoder = obs_video_encoder_create(videoEncoderId, 
                getVideoEncoderName(streamId).c_str(), videoSettings, nullptr);
            obs_data_release(videoSettings);
            
            if (!streamOutput->videoEncoder) {
                MULTI_RTMP_STREAM_LOG_ERROR("Failed to create fallback video encoder for stream: %s", streamId.c_str());
                return false;
            }
        }
    } else {
        obs_data_t* videoSettings = createVideoEncoderSettings(config);
        if (!videoSettings) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create video encoder settings for stream: %s", streamId.c_str());
            return false;
        }
        
        // Determine the video encoder ID to use
        const char* videoEncoderId = config.video.encoderId.empty() ? 
            getObsDefaultVideoEncoderId() : config.video.encoderId.c_str();
        
        streamOutput->videoEncoder = obs_video_encoder_create(videoEncoderId, 
            getVideoEncoderName(streamId).c_str(), videoSettings, nullptr);
        obs_data_release(videoSettings);
        
        if (!streamOutput->videoEncoder) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create video encoder for stream: %s", streamId.c_str());
            return false;
        }
    }
    
    // Audio encoder
    if (config.audio.useSharedEncoder) {
        streamOutput->audioEncoder = getSharedAudioEncoder(config.audio.mixerId);
        
        // Fallback to creating independent encoder if shared encoder is not available
        if (!streamOutput->audioEncoder) {
            MULTI_RTMP_STREAM_LOG_WARNING("Shared audio encoder not available, creating independent encoder for stream: %s", streamId.c_str());
            
            obs_data_t* audioSettings = createAudioEncoderSettings(config);
            if (!audioSettings) {
                MULTI_RTMP_STREAM_LOG_ERROR("Failed to create audio encoder settings for stream: %s", streamId.c_str());
                return false;
            }
            
            // Determine the audio encoder ID to use
            const char* audioEncoderId = config.audio.encoderId.empty() ? 
                AUDIO_ENCODER_ID : config.audio.encoderId.c_str();
            
            streamOutput->audioEncoder = obs_audio_encoder_create(audioEncoderId, 
                getAudioEncoderName(streamId).c_str(), audioSettings, 0, nullptr);
            obs_data_release(audioSettings);
            
            if (!streamOutput->audioEncoder) {
                MULTI_RTMP_STREAM_LOG_ERROR("Failed to create fallback audio encoder for stream: %s", streamId.c_str());
                return false;
            }
        }
    } else {
        obs_data_t* audioSettings = createAudioEncoderSettings(config);
        if (!audioSettings) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create audio encoder settings for stream: %s", streamId.c_str());
            return false;
        }
        
        // Determine the audio encoder ID to use
        const char* audioEncoderId = config.audio.encoderId.empty() ? 
            AUDIO_ENCODER_ID : config.audio.encoderId.c_str();
        
        streamOutput->audioEncoder = obs_audio_encoder_create(audioEncoderId, 
            getAudioEncoderName(streamId).c_str(), audioSettings, 0, nullptr);
        obs_data_release(audioSettings);
        
        if (!streamOutput->audioEncoder) {
            MULTI_RTMP_STREAM_LOG_ERROR("Failed to create audio encoder for stream: %s", streamId.c_str());
            return false;
        }
    }
    
    // Final validation to ensure both encoders are available
    if (!streamOutput->videoEncoder) {
        MULTI_RTMP_STREAM_LOG_ERROR("Video encoder is null after creation for stream: %s", streamId.c_str());
        return false;
    }
    
    if (!streamOutput->audioEncoder) {
        MULTI_RTMP_STREAM_LOG_ERROR("Audio encoder is null after creation for stream: %s", streamId.c_str());
        return false;
    }
    
    MULTI_RTMP_STREAM_LOG_INFO("Encoders created successfully for stream: %s", streamId.c_str());
    return true;
}

bool OneSevenMultiRtmpStreamController::setupOutput(const std::string& streamId, const OneSevenMultiRtmpConfig& config, StreamOutput* streamOutput) {
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
    
    obs_data_t* outputSettings = createOutputSettings(config);
    if (!outputSettings) {
        MULTI_RTMP_STREAM_LOG_ERROR("Failed to create output settings for stream: %s", streamId.c_str());
        return false;
    }
    
    streamOutput->output = obs_output_create(OUTPUT_ID, getOutputName(streamId).c_str(), outputSettings, nullptr);
    obs_data_release(outputSettings);
    
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
    
    MULTI_RTMP_STREAM_LOG_INFO("Output setup completed successfully for stream: %s", streamId.c_str());
    return true;
}

void OneSevenMultiRtmpStreamController::updateStreamStatus(const std::string& streamId, 
    OneSevenMultiRtmpStreamStatus::State state, const std::string& error) {
    
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
void OneSevenMultiRtmpStreamController::outputStartCallback(void* data, calldata_t* cd) {
    auto* controller = static_cast<OneSevenMultiRtmpStreamController*>(data);
    obs_output_t* output = static_cast<obs_output_t*>(calldata_ptr(cd, "output"));
    
    // Find stream ID by output
    std::lock_guard<std::mutex> lock(controller->m_outputsMutex);
    for (const auto& [streamId, streamOutput] : controller->m_streamOutputs) {
        if (streamOutput->output == output) {
            controller->updateStreamStatus(streamId, OneSevenMultiRtmpStreamStatus::STREAMING);
            MULTI_RTMP_STREAM_LOG_INFO("Stream started: %s", streamId.c_str());
            break;
        }
    }
}

void OneSevenMultiRtmpStreamController::outputStopCallback(void* data, calldata_t* cd) {
    auto* controller = static_cast<OneSevenMultiRtmpStreamController*>(data);
    obs_output_t* output = static_cast<obs_output_t*>(calldata_ptr(cd, "output"));
    
    // Find stream ID by output
    std::lock_guard<std::mutex> lock(controller->m_outputsMutex);
    for (const auto& [streamId, streamOutput] : controller->m_streamOutputs) {
        if (streamOutput->output == output) {
            controller->updateStreamStatus(streamId, OneSevenMultiRtmpStreamStatus::STOPPED);
            MULTI_RTMP_STREAM_LOG_INFO("Stream stopped: %s", streamId.c_str());
            break;
        }
    }
}

void OneSevenMultiRtmpStreamController::outputReconnectCallback(void* data, calldata_t* cd) {
    auto* controller = static_cast<OneSevenMultiRtmpStreamController*>(data);
    obs_output_t* output = static_cast<obs_output_t*>(calldata_ptr(cd, "output"));
    
    // Find stream ID by output
    std::lock_guard<std::mutex> lock(controller->m_outputsMutex);
    for (const auto& [streamId, streamOutput] : controller->m_streamOutputs) {
        if (streamOutput->output == output) {
            controller->updateStreamStatus(streamId, OneSevenMultiRtmpStreamStatus::RECONNECTING);
            MULTI_RTMP_STREAM_LOG_INFO("Stream reconnecting: %s", streamId.c_str());
            break;
        }
    }
}

void OneSevenMultiRtmpStreamController::outputReconnectSuccessCallback(void* data, calldata_t* cd) {
    auto* controller = static_cast<OneSevenMultiRtmpStreamController*>(data);
    obs_output_t* output = static_cast<obs_output_t*>(calldata_ptr(cd, "output"));
    
    // Find stream ID by output
    std::lock_guard<std::mutex> lock(controller->m_outputsMutex);
    for (const auto& [streamId, streamOutput] : controller->m_streamOutputs) {
        if (streamOutput->output == output) {
            controller->updateStreamStatus(streamId, OneSevenMultiRtmpStreamStatus::STREAMING);
            MULTI_RTMP_STREAM_LOG_INFO("Stream reconnected successfully: %s", streamId.c_str());
            break;
        }
    }
}

void OneSevenMultiRtmpStreamController::statsMonitoringThread() {
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

void OneSevenMultiRtmpStreamController::collectStreamStats(const std::string& streamId, StreamOutput& streamOutput) {
    (void)streamId; // Suppress unused parameter warning
    
    if (!streamOutput.output) {
        return;
    }
    
    // Update duration
    auto now = std::chrono::steady_clock::now();
    streamOutput.stats.duration = now - streamOutput.startTime;
    
    // Get output statistics
    streamOutput.stats.totalFrames = static_cast<int>(obs_output_get_total_frames(streamOutput.output));
    streamOutput.stats.droppedFrames = static_cast<int>(obs_output_get_frames_dropped(streamOutput.output));
    
    // Calculate bitrate and FPS (these would need to be implemented based on OBS API)
    // For now, we'll use placeholder values
    streamOutput.stats.currentBitrate = 0.0; // Would need actual implementation
    streamOutput.stats.currentFPS = 0; // Would need actual implementation
    streamOutput.stats.cpuUsage = 0.0; // Would need actual implementation
    
    if (streamOutput.stats.totalFrames > 0) {
        streamOutput.stats.averageBitrate = streamOutput.stats.currentBitrate; // Simplified
    }
}

// Helper method implementations
std::string OneSevenMultiRtmpStreamController::getOutputName(const std::string& streamId) const {
    return "MultiRTMP_Output_" + streamId;
}

std::string OneSevenMultiRtmpStreamController::getServiceName(const std::string& streamId) const {
    return "MultiRTMP_Service_" + streamId;
}

std::string OneSevenMultiRtmpStreamController::getVideoEncoderName(const std::string& streamId) const {
    return "MultiRTMP_VideoEncoder_" + streamId;
}

std::string OneSevenMultiRtmpStreamController::getAudioEncoderName(const std::string& streamId) const {
    return "MultiRTMP_AudioEncoder_" + streamId;
}

obs_data_t* OneSevenMultiRtmpStreamController::createServiceSettings(const OneSevenMultiRtmpConfig& config) const {
    obs_data_t* settings = obs_data_create();
    obs_data_set_string(settings, "server", config.service.serverUrl.c_str());
    obs_data_set_string(settings, "key", config.service.streamKey.c_str());
    obs_data_set_bool(settings, "use_auth", config.service.useAuthentication);
    if (config.service.useAuthentication) {
        obs_data_set_string(settings, "username", "");
        obs_data_set_string(settings, "password", config.service.authToken.c_str());
    }
    return settings;
}

obs_data_t* OneSevenMultiRtmpStreamController::createOutputSettings(const OneSevenMultiRtmpConfig& config) const {
    obs_data_t* settings = obs_data_create();
    obs_data_set_int(settings, "buffer_size", config.output.bufferSize);
    obs_data_set_int(settings, "reconnect_delay_sec", config.output.reconnectDelay);
    obs_data_set_bool(settings, "auto_reconnect", config.output.autoReconnect);
    if (config.output.bindIP != "default") {
        obs_data_set_string(settings, "bind_ip", config.output.bindIP.c_str());
    }
    return settings;
}

obs_data_t* OneSevenMultiRtmpStreamController::createVideoEncoderSettings(const OneSevenMultiRtmpConfig& config) const {
    // Check if user has set custom video encoder settings
    // If bitrate is default (6000) and rate control is default ("ABR"), use OBS defaults
    bool useObsDefaults = (config.video.bitrate == 6000 && 
                          config.video.rateControl == "ABR" && 
                          config.video.encoderId.empty());
    
    if (useObsDefaults) {
        MULTI_RTMP_STREAM_LOG_DEBUG("Using OBS default video encoder settings for stream");
        return getObsDefaultVideoEncoderSettings();
    }
    
    // Use user-specified settings
    obs_data_t* settings = obs_data_create();
    obs_data_set_int(settings, "bitrate", config.video.bitrate);
    obs_data_set_int(settings, "keyint_sec", config.video.keyframeInterval);
    obs_data_set_string(settings, "rate_control", config.video.rateControl.c_str());
    obs_data_set_string(settings, "profile", config.video.profile.c_str());
    obs_data_set_bool(settings, "use_bufsize", config.video.rateLimiting);
    obs_data_set_bool(settings, "bframes", config.video.useBFrames);
    
    MULTI_RTMP_STREAM_LOG_DEBUG("Using custom video encoder settings for stream - bitrate: %d, rate_control: %s", 
                               config.video.bitrate, config.video.rateControl.c_str());
    return settings;
}

obs_data_t* OneSevenMultiRtmpStreamController::createAudioEncoderSettings(const OneSevenMultiRtmpConfig& config) const {
    // Check if user has set custom audio encoder settings
    // If bitrate is default (128) and sample rate is default (48000), use OBS defaults
    bool useObsDefaults = (config.audio.bitrate == 128 && 
                          config.audio.sampleRate == 48000 && 
                          config.audio.encoderId.empty() &&
                          !config.audio.useHEAAC);
    
    if (useObsDefaults) {
        MULTI_RTMP_STREAM_LOG_DEBUG("Using OBS default audio encoder settings for stream");
        return getObsDefaultAudioEncoderSettings();
    }
    
    // Use user-specified settings
    obs_data_t* settings = obs_data_create();
    obs_data_set_int(settings, "bitrate", config.audio.bitrate);
    obs_data_set_int(settings, "rate", config.audio.sampleRate);
    if (config.audio.useHEAAC) {
        obs_data_set_string(settings, "codec", "HE-AAC");
    }
    
    MULTI_RTMP_STREAM_LOG_DEBUG("Using custom audio encoder settings for stream - bitrate: %d, sample_rate: %d", 
                               config.audio.bitrate, config.audio.sampleRate);
    return settings;
}

void OneSevenMultiRtmpStreamController::destroyService(const std::string& streamId) {
    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end() && it->second->service) {
        obs_service_release(it->second->service);
        it->second->service = nullptr;
        MULTI_RTMP_STREAM_LOG_DEBUG("Service destroyed for stream: %s", streamId.c_str());
    }
}

void OneSevenMultiRtmpStreamController::destroyEncoders(const std::string& streamId) {
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

void OneSevenMultiRtmpStreamController::updateStreamStats(const std::string& streamId) {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    auto it = m_streamOutputs.find(streamId);
    if (it != m_streamOutputs.end()) {
        collectStreamStats(streamId, *it->second);
        if (m_statsCallback) {
            m_statsCallback(streamId, it->second->stats);
        }
    }
}

const char* OneSevenMultiRtmpStreamController::getObsDefaultVideoEncoderId() const {
    // Try to get the encoder ID from the main streaming output
    obs_output_t* streamingOutput = obs_frontend_get_streaming_output();
    if (streamingOutput) {
        obs_encoder_t* videoEncoder = obs_output_get_video_encoder(streamingOutput);
        if (videoEncoder) {
            const char* encoderId = obs_encoder_get_id(videoEncoder);
            obs_output_release(streamingOutput);
            return encoderId;
        }
        obs_output_release(streamingOutput);
    }
    
    // Fallback to default x264 encoder
    return VIDEO_ENCODER_ID;
}

obs_data_t* OneSevenMultiRtmpStreamController::getObsDefaultVideoEncoderSettings() const {
    obs_data_t* settings = obs_data_create();
    
    // Try to get settings from the main streaming output
    obs_output_t* streamingOutput = obs_frontend_get_streaming_output();
    if (streamingOutput) {
        obs_encoder_t* videoEncoder = obs_output_get_video_encoder(streamingOutput);
        if (videoEncoder) {
            obs_data_t* encoderSettings = obs_encoder_get_settings(videoEncoder);
            if (encoderSettings) {
                // Copy the settings
                obs_data_apply(settings, encoderSettings);
                obs_data_release(encoderSettings);
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

obs_data_t* OneSevenMultiRtmpStreamController::getObsDefaultAudioEncoderSettings() const {
    obs_data_t* settings = obs_data_create();
    
    // Try to get settings from the main streaming output
    obs_output_t* streamingOutput = obs_frontend_get_streaming_output();
    if (streamingOutput) {
        obs_encoder_t* audioEncoder = obs_output_get_audio_encoder(streamingOutput, 0);
        if (audioEncoder) {
            obs_data_t* encoderSettings = obs_encoder_get_settings(audioEncoder);
            if (encoderSettings) {
                // Copy the settings
                obs_data_apply(settings, encoderSettings);
                obs_data_release(encoderSettings);
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

#pragma once

#include <QObject>
#include <memory>
#include <string>
#include <QTimer>

#include "api/OneSevenLiveModels.hpp"

// Forward declarations
class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;

/**
 * @brief OneSevenLiveStreamManager class manages 17Live stream configuration and control
 *
 * This class is responsible for managing all aspects of 17Live streaming including:
 * - Stream configuration and settings
 * - Stream creation and management
 * - Playback control (start/stop)
 * - Integration with OBS streaming service
 * - Event management during streaming
 */
class OneSevenLiveStreamManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param apiWrapper API wrapper instance for making HTTP requests
     * @param configManager Configuration manager for storing settings
     * @param parent Parent QObject
     */
    explicit OneSevenLiveStreamManager(OneSevenLiveApiWrappers* apiWrapper,
                                       OneSevenLiveConfigManager* configManager,
                                       QObject* parent = nullptr);
    ~OneSevenLiveStreamManager();

    /**
     * @brief Create a new live stream
     * @param request RTMP request containing stream configuration
     * @return bool True if stream creation was successful
     */
    bool createLiveStream(const OneSevenLiveRtmpRequest& request);

    /**
     * @brief Start streaming with the given configuration
     * @param userID User ID for the stream
     * @param response RTMP response containing stream credentials
     * @param autoRecording Whether to enable automatic recording
     * @param skip Skip OBS streaming start (for testing purposes)
     * @return bool True if streaming started successfully
     */
    bool startStreaming(const std::string& userID,
                       const OneSevenLiveRtmpResponse& response,
                       bool autoRecording,
                       bool skip = false);

    /**
     * @brief Stop the current stream
     * @param userID User ID for the stream
     * @param liveStreamID Live stream ID to stop
     * @param isAutoClose Whether this is an automatic close
     * @return bool True if stream was stopped successfully
     */
    bool stopStreaming(const std::string& userID,
                        const std::string& liveStreamID,
                        bool isAutoClose = false);

    /**
     * @brief Start live stream with the given response
     * @param liveStreamID Live stream ID
     * @param userID User ID
     * @param autoRecording Whether to enable automatic recording
     * @return bool True if stream was started successfully
     */
    bool startLiveStream(const std::string& liveStreamID,
                        const std::string& userID,
                        bool autoRecording = false);

    /**
     * @brief Stop live stream with the given request
     * @param liveStreamID Live stream ID to stop
     * @param request Close live request
     * @return bool True if stream was stopped successfully
     */
    bool stopLiveStream(const std::string& liveStreamID,
                       const OneSevenLiveCloseLiveRequest& request);

    /**
     * @brief Configure streaming settings based on response (WHIP or RTMP)
     * @param response RTMP response containing stream credentials
     */
    void configureStreamingSettings(const OneSevenLiveRtmpResponse& response);

    /**
     * @brief Get current stream response
     * @return OneSevenLiveRtmpResponse Current stream response
     */
    const OneSevenLiveRtmpResponse& getCurrentStreamResponse() const;

    /**
     * @brief Get current stream request used for creation
     * @return OneSevenLiveRtmpRequest Current stream request
     */
    const OneSevenLiveRtmpRequest& getCurrentStreamRequest() const;

    /**
     * @brief Get current live stream info snapshot
     * @return OneSevenLiveStreamInfo Current live stream info
     */
    const OneSevenLiveStreamInfo& getCurrentLiveStreamInfo() const;

    /**
     * @brief Check if there is an active live stream
     * @return bool True if a live stream is active
     */
    bool hasActiveLiveStream() const;

    /**
     * @brief Stop OBS streaming (frontend control)
     */
    void stopOBSStreaming();

    /**
     * @brief Check if OBS is currently streaming
     * @return bool True if OBS is streaming
     */
    bool isOBSStreaming() const;

    /**
     * @brief Save stream configuration
     * @param streamInfo Stream information to save
     * @return bool True if configuration was saved successfully
     */
    bool saveStreamConfiguration(const OneSevenLiveStreamInfo& streamInfo);

    /**
     * @brief Change event during streaming
     * @param eventID New event ID
     * @return bool True if event change was successful
     */
    bool changeEvent(qint64 eventID);

    /**
     * @brief Get current streaming status
     * @return OneSevenLiveStreamingStatus Current status
     */
    OneSevenLiveStreamingStatus getCurrentStreamingStatus() const;

    /**
     * @brief Set current streaming status
     * @param status New streaming status
     */
    void setCurrentStreamingStatus(OneSevenLiveStreamingStatus status);

    /**
     * @brief Get current live stream ID
     * @return std::string Current live stream ID
     */
    std::string getCurrentLiveStreamID() const;

    /**
     * @brief Get current user ID
     * @return std::string Current user ID
     */
    std::string getCurrentUserID() const;

    /**
     * @brief Get current room ID
     * @return qint64 Current room ID
     */
    qint64 getRoomID() const;

    /**
     * @brief Start event cooldown timer
     * @param duration Cooldown duration in seconds (default 300 = 5 minutes)
     */
    void startEventCooldown(int duration = 300);

    /**
     * @brief Check if event change is in cooldown
     * @return bool True if cooldown is active
     */
    bool isEventInCooldown() const;

    /**
     * @brief Get remaining cooldown time
     * @return int Remaining cooldown time in seconds
     */
    int getEventCooldownRemaining() const;

    /**
     * @brief Save RTMP streaming settings to OBS
     * @param liveStreamID Live stream ID
     * @param streamUrl RTMP server URL
     * @param streamKey Stream key
     */
    void saveStreamingSettings(const std::string& liveStreamID,
                              const std::string& streamUrl,
                              const std::string& streamKey);

    /**
     * @brief Save WHIP streaming settings to OBS
     * @param liveStreamID Live stream ID
     * @param whipServer WHIP server URL
     * @param whipToken WHIP authentication token
     */
    void saveWhipStreamingSettings(const std::string& liveStreamID,
                                  const std::string& whipServer,
                                  const std::string& whipToken);

    /**
     * @brief Clear streaming configuration from OBS
     */
public:
    void clearStreamingConfiguration();

signals:
    /**
     * @brief Emitted when stream status changes
     * @param status New streaming status
     */
    void streamStatusChanged(OneSevenLiveStreamingStatus status);

    /**
     * @brief Emitted when stream configuration is saved
     */
    void streamConfigurationSaved();

    /**
     * @brief Emitted when event cooldown starts or updates
     * @param remainingTime Remaining cooldown time in seconds
     */
    void eventCooldownUpdated(int remainingTime);

    /**
     * @brief Emitted when an error occurs
     * @param errorMessage Error message
     * @param operation Operation that failed
     */
    void errorOccurred(const QString& errorMessage, const QString& operation);

private slots:
    /**
     * @brief Handle event cooldown timer timeout
     */
    void onEventCooldownTimeout();

private:
    /**
     * @brief Configure OBS streaming service
     * @param liveStreamID Live stream ID
     * @param response RTMP response containing credentials
     */
    void configureStreamingService(const std::string& liveStreamID,
                                  const OneSevenLiveRtmpResponse& response);

    /**
     * @brief Enable stream archive
     * @param liveStreamID Live stream ID
     * @param enable Whether to enable archive
     * @return bool True if archive was enabled successfully
     */
    bool enableStreamArchive(const std::string& liveStreamID, bool enable);

    // Member variables
    OneSevenLiveApiWrappers* apiWrapper;
    OneSevenLiveConfigManager* configManager;
    
    OneSevenLiveStreamingStatus currentStreamingStatus;
    std::string currentLiveStreamID;
    std::string currentUserID;
    
    OneSevenLiveRtmpResponse currentStreamResponse;  // Store current stream response
    OneSevenLiveRtmpRequest currentStreamRequest;    // Store current stream request
    OneSevenLiveStreamInfo currentLiveStreamInfo;    // Snapshot info for current live
    
    // Event cooldown management
    QTimer* eventCooldownTimer;
    int eventCooldownRemaining;
    static constexpr int DEFAULT_COOLDOWN_DURATION = 300; // 5 minutes
};

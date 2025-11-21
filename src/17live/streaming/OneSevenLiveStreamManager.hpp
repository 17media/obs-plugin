#pragma once

#include <QObject>
#include <QTimer>
#include <memory>
#include <string>

#include "OneSevenLiveLoadRoomInfoWorker.hpp"
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
    bool createRtmp(const OneSevenLiveRtmpRequest& request);

    /**
     * @brief Start streaming with the given configuration
     * @return bool True if streaming started successfully
     */
    bool startStream();

    /**
     * @brief Stop the current stream
     * @param isAutoClose Whether this is an automatic close
     * @return bool True if stream was stopped successfully
     */
    bool stopStream(bool isAutoClose = false);

    /**
     * @brief Start live stream with the given response
     * @param liveStreamID Live stream ID
     * @param userID User ID
     * @param autoRecording Whether to enable automatic recording
     * @return bool True if stream was started successfully
     */

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

    void startOBSStreaming();

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

    void loadRoomInfo();

    const OneSevenLiveRoomInfo& getRoomInfo() const {
        return roomInfo;
    }

    const OneSevenLiveConfigStreamer& getConfigStreamer() const {
        return configStreamer;
    }

    const OneSevenLiveUserInfo& getUserInfo() const {
        return userInfo;
    }

    const OneSevenLiveArmySubscriptionLevels& getArmyLevels() const {
        return levels;
    }

    bool isRoomInfoLoading() const {
        return roomInfoLoading;
    }

    bool fetchRtmpByProvider(const std::string& provider, OneSevenLiveRtmpResponse& response);
    QString getLastErrorMessage() const;
    bool startStreamWithWeb();

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
     * @brief Save RTMP streaming settings to OBS
     * @param liveStreamID Live stream ID
     * @param streamUrl RTMP server URL
     * @param streamKey Stream key
     */
    void saveStreamingSettings(const std::string& liveStreamID, const std::string& streamUrl,
                               const std::string& streamKey);

    /**
     * @brief Save WHIP streaming settings to OBS
     * @param liveStreamID Live stream ID
     * @param whipServer WHIP server URL
     * @param whipToken WHIP authentication token
     */
    void saveWhipStreamingSettings(const std::string& liveStreamID, const std::string& whipServer,
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
     * @brief Emitted when an error occurs
     * @param errorMessage Error message
     * @param operation Operation that failed
     */
    void errorOccurred(const QString& errorMessage, const QString& operation);

    void roomInfoLoaded(const OneSevenLiveLoadRoomInfoWorker::LoadResult& result);

   private:
    /**
     * @brief Configure OBS streaming service
     * @param response RTMP response containing credentials
     */
    void configureStreamingService(const OneSevenLiveRtmpResponse& response);

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
    qint64 currentRoomID;

    OneSevenLiveRtmpResponse currentStreamResponse;  // Store current stream response
    OneSevenLiveRtmpRequest currentStreamRequest;    // Store current stream request
    OneSevenLiveStreamInfo currentLiveStreamInfo;    // Snapshot info for current live

    // Loaded data for room info
    OneSevenLiveRoomInfo roomInfo;
    OneSevenLiveConfigStreamer configStreamer;
    OneSevenLiveUserInfo userInfo;
    OneSevenLiveArmySubscriptionLevels levels;
    bool roomInfoLoading = false;

    QTimer* m_statusTimer{nullptr};
    void onStatusTimer();
};

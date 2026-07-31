#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>

#include "../utility/Result.hpp"
#include "OneSevenLiveModels.hpp"

// for local http server proxy request
/*
{
  "action": "getAblyToken",
  "params": {
    ...
  },
}
*/

#define ACTION_GETABLYTOKEN "getAblyToken"
#define ACTION_GETGIFTTABS "getGiftTabs"
#define ACTION_GETGIFTS "getGifts"
#define ACTION_GETGIFT "getGift"
#define ACTION_GETROOMINFO "getRoomInfo"
#define ACTION_GETENTERANIMATIONFILES "getEnterAnimationFiles"
#define ACTION_GETI18NCONFIG "getI18nConfig"

#define MAX_CONSECUTIVE_FAILURES 10  // Maximum consecutive failure count

using Json = nlohmann::json;

class OneSevenLiveApiWrappers : public QObject {
    Q_OBJECT

    bool TryInsertCommand(const char *url, const char *content_type, std::string request_type,
                          const char *data, Json &ret, long *error_code = nullptr,
                          int data_size = 0, bool token_required = true,
                          const std::vector<std::string> extraHeaders = {});
    bool UpdateAccessToken();
    bool InsertCommand(const char *url, const char *content_type, std::string request_type,
                       const char *data, Json &ret, int data_size = 0, bool token_required = true,
                       const std::vector<std::string> extraHeaders = {});

   public:
    OneSevenLiveApiWrappers();
    OneSevenLiveApiWrappers(std::string token_);
    ~OneSevenLiveApiWrappers();

    bool Login(const QString &username, const QString &password, OneSevenLiveLoginData &loginData);

    bool GetSelfInfo(OneSevenLiveLoginData &loginData);
    bool CommonRequest(const std::string action, Json &json_out);

    bool GetRoomInfo(const qint64 roomID, OneSevenLiveRoomInfo &roomInfo);
    bool CreateRtmp(const OneSevenLiveRtmpRequest &request, OneSevenLiveRtmpResponse &response);
    bool StartStream(const std::string &liveStreamID, const std::string &userID);
    bool EnableStreamArchive(const std::string &liveStreamID, int enableArchive);
    bool StopStream(const std::string &liveStreamID, const OneSevenLiveCloseLiveRequest &request);
    bool CheckStream(const std::string &liveStreamID);
    bool GetConfigStreamer(const std::string region, const std::string language,
                           OneSevenLiveConfigStreamer &response);
    bool GetAblyToken(const std::string &liveStreamID, Json &response);
    bool GetGiftTabs(const std::string &roomID, const std::string language, Json &response);
    bool GetGifts(const std::string language, Json &response);
    bool GetFilesList(Json &response);
    bool GetI18nConfig(const std::string &language, Json &response);
    bool GetRockViewers(const std::string &roomID, Json &response);
    bool GetUserInfo(const std::string userID, const std::string region, const std::string language,
                     OneSevenLiveUserInfo &response);
    bool GetUserNote(const std::string userID, OneSevenLiveUserNote &response);
    bool SetUserNote(const std::string userID, const QString &content);
    bool GetConfig(const std::string region, const std::string language, Json &response);
    bool GetArmySubscriptionLevels(const std::string region, const std::string language,
                                   OneSevenLiveArmySubscriptionLevels &levels);
    bool GetRtmpByProvider(const std::string provider, OneSevenLiveRtmpResponse &response);
    bool CreateCustomEvent(const OneSevenLiveCustomEvent &request,
                           OneSevenLiveCustomEvent &response);
    // ChangeCustomEventStatus
    // OneSevenLiveCustomEventStatusRequest.status = 2: stop event = 3: close event
    bool ChangeCustomEventStatus(const std::string &eventID,
                                 const OneSevenLiveCustomEventStatusRequest &request);
    // GetCustomEvent
    // Get custom event information by userID
    bool GetCustomEvent(const std::string &userID, OneSevenLiveCustomEvent &response);
    // GetArmyName
    // Get army name information by userID
    bool GetArmyName(const std::string &userID, OneSevenLiveArmyNameResponse &response);

    // PokeOne
    // Send a poke to a user
    bool PokeOne(const OneSevenLivePokeRequest &request, OneSevenLivePokeResponse &response);

    // PokeAll
    // Send a poke to all users in a group
    bool PokeAll(const OneSevenLivePokeAllRequest &request, OneSevenLivePokeResponse &response);

    // ChangeEvent
    // Change event for live stream
    bool ChangeEvent(const OneSevenLiveChangeEventRequest &request);

    bool CreateLiveEngagements(const std::string &liveStreamID,
                               const std::vector<OneSevenLiveEngagementCreate> &engagements,
                               std::vector<OneSevenLiveEngagementCreateResult> &results);
    bool DeleteLiveEngagements(const std::string &liveStreamID,
                               const std::vector<std::string> &engageIDs);
    bool GetLiveEngagementProgress(const std::string &liveStreamID,
                                   std::vector<OneSevenLiveEngagementProgress> &engagements);

    bool ReportObsCrashEvent(const std::string &liveStreamID, int64_t crashTimestampSec);
    bool UploadObsLogsFile(const std::string &zipPath);
    bool UploadObsLogsFile(const std::string &zipPath, std::function<void(double)> onProgress);
    bool UploadObsLogsFile(const std::string &zipPath, std::function<void(double)> onProgress,
                           std::atomic<bool> *cancelFlag);

    /**
     * @brief Perform MD5 encryption on string
     * @param str String to be encrypted
     * @return Returns MD5 encrypted string (hexadecimal format)
     */
    static QString md5(const QString &str);

    /**
     * @brief Get current time in millisecond timestamp
     * @return int64_t Returns milliseconds since 1970-01-01 00:00:00 UTC
     */
    static int64_t getCurrentTimestampMs();

    QString getLastErrorMessage() const {
        std::lock_guard<std::mutex> lock(stateMutex);
        return QString::fromStdString(lastError_.message);
    }

    ResultError getLastError() const {
        std::lock_guard<std::mutex> lock(stateMutex);
        return lastError_;
    }

    /**
     * @brief Set authentication token
     * @param token_ The authentication token to set
     */
    void setToken(const std::string &token_) {
        std::lock_guard<std::mutex> lock(stateMutex);
        token = token_;
    }

    /**
     * @brief Get current authentication token
     * @return Returns the current authentication token
     */
    std::string getToken() const {
        std::lock_guard<std::mutex> lock(stateMutex);
        return token;
    }

    /**
     * @brief Set the cancel flag for network requests
     * @param flag Pointer to atomic bool flag
     */
    void setCancelFlag(std::atomic<bool> *flag) {
        m_cancelFlag = flag;
    }

    void shutdown();

   protected:
    std::string refresh_token;
    std::string token;
    bool implicit = false;
    uint64_t expire_time = 0;
    int currentScopeVer = 0;
    std::atomic<bool> *m_cancelFlag = nullptr;

   private:
    ResultError lastError_;

    std::string currentOS;
    std::string currentOSVersion;
    std::string currentPlatformUUID;

    // Mutex for thread-safe access to shared state
    mutable std::mutex stateMutex;

    void setLastError(ResultError error);
    void clearLastError();
    void initializeApiWrapper();
};

#pragma once

#include <QObject>
#include <QString>
#include <mutex>
#include <nlohmann/json.hpp>

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
#define ACTION_GETROOMINFO "getRoomInfo"

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
    bool GetRockViewers(const std::string &roomID, Json &response);
    bool GetUserInfo(const std::string userID, const std::string region, const std::string language,
                     OneSevenLiveUserInfo &response);
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
        return lastErrorMessage;
    }

   protected:
    std::string refresh_token;
    std::string token;
    bool implicit = false;
    uint64_t expire_time = 0;
    int currentScopeVer = 0;

   private:
    QString lastErrorMessage;

    std::string currentOS;
    std::string currentOSVersion;
    std::string currentPlatformUUID;

    // Mutex for thread-safe access to shared state
    mutable std::mutex stateMutex;

    // Thread-safe helper methods for error message management
    void setLastErrorMessage(const QString &message);
    void clearLastErrorMessage();
};

#pragma once

#include "json11.hpp"

#include <QString>
#include <QObject>

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


using namespace json11;

class OneSevenLiveApiWrappers : public QObject {
  Q_OBJECT

  bool TryInsertCommand(const char *url, const char *content_type, std::string request_type, const char *data,
      json11::Json &ret, long *error_code = nullptr, int data_size = 0, bool token_required = true, const std::vector<std::string> extraHeaders = {});
  bool UpdateAccessToken();
  bool InsertCommand(const char *url, const char *content_type, std::string request_type, const char *data,
   json11::Json &ret, int data_size = 0, bool token_required = true, const std::vector<std::string> extraHeaders = {});


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
  bool GetConfigStreamer(const std::string region, const std::string language, OneSevenLiveConfigStreamer &response);
  bool GetAblyToken(const std::string &liveStreamID, Json &response);
  bool GetGiftTabs(const std::string &liveStreamID, const std::string language, Json &response);
  bool GetGifts(const std::string language, Json &response);
  bool GetUserInfo(const std::string userID, const std::string region, const std::string language, OneSevenLiveUserInfo &response);
  bool GetConfig(const std::string region, const std::string language, Json &response);
  bool GetArmySubscriptionLevels(const std::string region, const std::string language, OneSevenLiveArmySubscriptionLevels &levels);
  bool GetRtmpByProvider(const std::string provider, OneSevenLiveRtmpResponse &response);

  /**
   * @brief 对字符串进行MD5加密
   * @param str 需要加密的字符串
   * @return 返回MD5加密后的字符串（16进制格式）
   */
  static QString md5(const QString& str);

  /**
   * @brief 获取当前时间的毫秒级时间戳
   * @return int64_t 返回自 1970-01-01 00:00:00 UTC 以来的毫秒数
   */
  static int64_t getCurrentTimestampMs();

  QString getLastErrorMessage() const { return lastErrorMessage; }

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
};

#include "SeventeenLiveApiWrappers.hpp"

#include <QFile>
#include <QMimeDatabase>
#include <QUrl>
#include <QCryptographicHash>

#include "../utility/RemoteTextThread.hpp"
#include "../utility/Common.hpp"

#include <obs-module.h>
#include "plugin-support.h"

using namespace json11;
using namespace std;

extern const char* service;

// 登录接口: SEVENTEENLIVE_API_URL + "/api/v1/auth/loginAction"
const string SEVENTEENLIVE_LOGIN_URL = string(SEVENTEENLIVE_API_URL) + "/api/v1/auth/loginAction";

const string SEVENTEENLIVE_APIGATEWAY_URL = string(SEVENTEENLIVE_API_URL) + "/apiGateWay";

const string SEVENTEENLIVE_GET_ROOM_INFO_URL = string(SEVENTEENLIVE_API_URL) + "/api/v1/lives/%1/info";

const string SEVENTEENLIVE_CREATE_RTMP_URL = string(SEVENTEENLIVE_API_URL) + "/api/v1/rtmp";

const string SEVENTEENLIVE_STREAM_URL = string(SEVENTEENLIVE_API_URL) + "/api/v1/lives/%1";

const string SEVENTEENLIVE_ARCHIVE_URL = string(SEVENTEENLIVE_API_URL) + "/api/v1/lives/%1/archive/recording?enable=%2";

const string SEVENTEENLIVE_GET_CONFIG_STREAMER_URL = string(SEVENTEENLIVE_API_URL) + "/api/v1/liveStreams/config/streamer";

const string SEVENTEENLIVE_GET_ABLY_TOKEN_URL = string(SEVENTEENLIVE_API_URL) + "/api/v1/messenger/token?type=3&roomID=%1";

const string SEVENTEENLIVE_GET_GIFTTABS_URL = string(SEVENTEENLIVE_API_URL) + "/api/v1/lives/%1/giftTabs?filter=0";

const string SEVENTEENLIVE_GET_GIFTS_URL = string(SEVENTEENLIVE_API_URL) + "/api/v1/gifts";

SeventeenLiveApiWrappers::SeventeenLiveApiWrappers() : token("") {}

SeventeenLiveApiWrappers::SeventeenLiveApiWrappers(std::string token_) : token(token_) {}

bool SeventeenLiveApiWrappers::TryInsertCommand(const char *url, const char *content_type, std::string request_type,
  const char *data, Json &json_out, long *error_code, int data_size, bool token_required, const std::vector<std::string> extraHeaders)
{
  long httpStatusCode = 0;

#ifdef _DEBUG
  obs_log(LOG_DEBUG, "17Live API command URL: %s", url);
  
  if (data && data[0] == '{') // only log JSON data
    obs_log(LOG_DEBUG, "17Live API command data: %s", data);
#endif

  std::vector<std::string> headers;

  if (token_required && token.empty())
    return false;

  if (token_required)
    headers.push_back("Authorization: Bearer " + token);

  headers.push_back("Devicetype: WEB");

  // add plugin version
  headers.push_back("version: " + std::string(PLUGIN_VERSION));

  // add OS version
  headers.push_back("OSVersion: " + GetCurrentOSVersion());

  // add HW
  headers.push_back("hardware: " + GetCurrentOS());

  headers.push_back("deviceName: OBSPlugin");
  headers.push_back("deviceModel: OBSPlugin");

  headers.push_back("deviceId: " + GetCurrentPlatformUUID());

  // debug output headers
  // for (const auto &header : headers) {
  //   obs_log(LOG_INFO, "17Live API command header: %s", header.c_str());
  // }

  for (const auto &header : extraHeaders) {
    headers.push_back(header);
  }

  std::string output;
  std::string error;
  // Increase timeout by the time it takes to transfer `data_size` at 1 Mbps
  int timeout = 60 + data_size / 125000;
  bool success = GetRemoteFile(url, output, error, &httpStatusCode, content_type,request_type, data, headers, nullptr, timeout, false, data_size);
  if (error_code)
    *error_code = httpStatusCode;

  if (!success || output.empty()) {
    if (!error.empty())
      obs_log(LOG_WARNING, "17Live API request failed: %s", error.c_str());
    return false;
  }

  json_out = Json::parse(output, error);
#ifdef _DEBUG
  obs_log(LOG_DEBUG, "17Live API command answer: %s", json_out.dump().c_str());
#endif
  if (!error.empty()) {
    return false;
  }
  return httpStatusCode < 400;
}

bool SeventeenLiveApiWrappers::UpdateAccessToken()
{
  obs_log(LOG_INFO, "Updating access token");
  // TODO: implement
  return false;
}

bool SeventeenLiveApiWrappers::InsertCommand(const char *url, const char *content_type, std::string request_type, const char *data, Json &json_out, int data_size, bool token_required, const std::vector<std::string> extraHeaders)
{
  long error_code;
  std::string error;
  bool success = TryInsertCommand(url, content_type, request_type, data, json_out, &error_code, data_size, token_required, extraHeaders);

  if (error_code == 401) {
    // Attempt to update access token and try again
    if (!UpdateAccessToken())
      return false;
    success = TryInsertCommand(url, content_type, request_type, data, json_out, &error_code, data_size);
  }

  if (json_out.object_items().find("error") != json_out.object_items().end()) {
    obs_log(LOG_ERROR, "17Live API error:\n\tHTTP status: %ld\n\tURL: %s\n\tJSON: %s", error_code, url, json_out.dump().c_str());

    Json json_out_data = Json::parse(json_out["data"].string_value(), error);
    lastErrorMessage = QString::fromStdString(json_out_data["message"].string_value());
    
    // The existence of an error implies non-success even if the HTTP status code disagrees.
    success = false;
  } else if (json_out.object_items().find("errorCode")!= json_out.object_items().end()) {
    obs_log(LOG_ERROR, "17Live API error:\n\tHTTP status: %ld\n\tURL: %s\n\tJSON: %s", error_code, url, json_out.dump().c_str());

    lastErrorMessage = QString::fromStdString(json_out["errorCode"].string_value()) + " " + QString::fromStdString(json_out["errorMessage"].string_value());
    // The existence of an error implies non-success even if the HTTP status code disagrees.
    success = false;
  }

  return success;
}

bool SeventeenLiveApiWrappers::Login(const QString &username, const QString &password, SeventeenLiveLoginData &loginData)
{
	lastErrorMessage.clear();

	const QByteArray url = SEVENTEENLIVE_LOGIN_URL.c_str();
  // TODO: language
  // const char *obs_get_locale(void)
	const Json data = Json::object{
    {"language", GetCurrentLanguage()},
    {"openID", username.toStdString()},
    {"password", md5(password).toStdString()},
  };
  std::string error;
	Json json_out;
	if (!InsertCommand(url, "application/json", "", data.dump().c_str(), json_out, 0, false)) {
		return false;
	}
  obs_log(LOG_INFO, "Login success");
  obs_log(LOG_INFO, "Login data: %s", json_out.dump().c_str());
    
  // transform string json_out["data"] to Json
  Json json_out_data = Json::parse(json_out["data"].string_value(), error);
  if (!error.empty()) {
    obs_log(LOG_ERROR, "Failed to parse login response data: %s", error.c_str());
    return false;
  }

  // check if json_out_data contains "result" key
  auto items = json_out_data.object_items();
  if (items.find("result") != items.end()) {
    if (json_out_data["result"].string_value() == "fail") {
      lastErrorMessage = QString::fromStdString(json_out_data["message"].string_value());
      return false;
    }
  } else {
    obs_log(LOG_WARNING, "Login response missing result field: %s", json_out_data.dump().c_str());
    lastErrorMessage = QString::fromStdString(json_out.dump().c_str());
    return false;
  }

  // Check for required fields
  if (!json_out_data["jwtAccessToken"].is_string() || json_out_data["jwtAccessToken"].string_value().empty()) {
    obs_log(LOG_ERROR, "Login response missing jwtAccessToken");
    lastErrorMessage = QString::fromStdString(json_out.dump().c_str());
    return false;
  }

  JsonToSeventeenLiveLoginData(json_out_data, loginData);

  // save token to next call
  token = loginData.jwtAccessToken.toStdString();

  return !loginData.jwtAccessToken.isEmpty();  
}

bool SeventeenLiveApiWrappers::SeventeenLiveApiWrappers::GetSelfInfo(SeventeenLiveLoginData &loginData)
{
  Json json_out;
  if (!CommonRequest("getSelfInfo", json_out))
    return false;

  // check if json_out_data contains "openID"
  auto items = json_out.object_items();
  if (items.find("openID")== items.end()) {
    obs_log(LOG_ERROR, "GetSelfInfo response missing openID field: %s", json_out.dump().c_str());
    lastErrorMessage = "GetSelfInfo response missing openID field";
    return false;
  }

  loginData.userInfo.openID = QString::fromStdString(json_out["openID"].string_value());
  loginData.userInfo.displayName = QString::fromStdString(json_out["displayName"].string_value());
  loginData.userInfo.roomID = json_out["roomID"].int_value();
  loginData.userInfo.userID = QString::fromStdString(json_out["userID"].string_value());
  return true;
}
  
bool SeventeenLiveApiWrappers::CommonRequest(const std::string action, Json &json_out)
{
  lastErrorMessage.clear();

	const QByteArray url = SEVENTEENLIVE_APIGATEWAY_URL.c_str();
  
  const Json data = Json::object{
    {"nonce", "nonce-17live-" + std::to_string(getCurrentTimestampMs())},
    {"action", action},
  };
  
  // 将 JSON 转换为字符串并进行 URL 编码
  std::string jsonStr = data.dump();
  QString encodedData = QUrl::toPercentEncoding(QString::fromStdString(jsonStr));
  
  // 构建最终的 post data
  std::string postData = "cypher=0_v2&data=" + encodedData.toStdString();
  

  std::string error;
	Json json_out_resp;
	
	if (!InsertCommand(url, "application/x-www-form-urlencoded", "", postData.c_str(), json_out_resp)) {
		return false;
	}
  obs_log(LOG_INFO, "apiGateWay success");

  // Check if exist errorCode field
  if (json_out_resp.object_items().find("errorCode")!= json_out_resp.object_items().end()) {
  	obs_log(LOG_ERROR, "apiGateWay error: %s", json_out_resp.dump().c_str());
    // lastErrorMessage = errorCode + errorMessage
    lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].string_value()) + " " + QString::fromStdString(json_out_resp["errorMessage"].string_value());
    return false;
  }
    
  // transform string json_out["data"] to Json
  json_out = Json::parse(json_out_resp["data"].string_value(), error);
  if (!error.empty()) {
    obs_log(LOG_ERROR, "Failed to parse apiGateWay response data: %s", error.c_str());
    return false;
  }

  return true;
}


bool SeventeenLiveApiWrappers::GetRoomInfo(const qint64 roomID, SeventeenLiveRoomInfo &roomInfo)
{
  lastErrorMessage.clear();

  // 构建请求URL
  QString urlStr = QString::fromStdString(SEVENTEENLIVE_GET_ROOM_INFO_URL).arg(roomID);
  QByteArray url = urlStr.toUtf8();

  Json json_out;
  if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out, 0, true)) {
    obs_log(LOG_ERROR, "GetRoomInfo failed %s", json_out.dump().c_str());
    lastErrorMessage = QString::fromStdString("GetRoomInfo failed %s").arg(json_out.dump().c_str()).toUtf8().constData();

    return false;
  }

  // 使用 JsonToSeventeenLiveRoomInfo 函数解析数据到结构体
  if (!JsonToSeventeenLiveRoomInfo(json_out, roomInfo)) {
    obs_log(LOG_ERROR, "Failed to parse room info data");
    lastErrorMessage = "Failed to parse room info data";
    return false;
  }

  return true;
}

QString SeventeenLiveApiWrappers::md5(const QString& str)
{
  QByteArray input = str.toUtf8();
  QByteArray hash = QCryptographicHash::hash(input, QCryptographicHash::Md5);
  return QString(hash.toHex());
}

// 添加生成毫秒时间戳的函数
int64_t SeventeenLiveApiWrappers::getCurrentTimestampMs()
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

bool SeventeenLiveApiWrappers::CreateRtmp(const SeventeenLiveRtmpRequest &request, SeventeenLiveRtmpResponse &response)
{
  obs_log(LOG_INFO, "CreateRtmp start");

  lastErrorMessage.clear();

  const QByteArray url = SEVENTEENLIVE_CREATE_RTMP_URL.c_str();

  Json requestData;
  if (!SeventeenLiveRtmpRequestToJson(request, requestData)) {
    obs_log(LOG_ERROR, "Failed to convert request to JSON");
    lastErrorMessage = "Failed to convert request to JSON";
    return false;
  }

  std::string postData = requestData.dump();

  std::string error;
	Json json_out;
	
	if (!InsertCommand(url, "application/json", "", postData.c_str(), json_out)) {
		return false;
	}

  obs_log(LOG_INFO, "CreateRtmp success");
  obs_log(LOG_INFO, "rtmp info %s", json_out.dump().c_str());

  // Check if exist errorCode field
  if (json_out.object_items().find("errorCode")!= json_out.object_items().end()) {
  	obs_log(LOG_ERROR, "CreateRtmp error: %s", json_out.dump().c_str());
    // lastErrorMessage = errorCode + errorMessage
    lastErrorMessage = QString::fromStdString(json_out["errorCode"].string_value()) + " " + QString::fromStdString(json_out["errorMessage"].string_value());
    return false;
  }

  if (!JsonToSeventeenLiveRtmpResponse(json_out, response)) {
    obs_log(LOG_ERROR, "Failed to convert response to struct");
    lastErrorMessage = "Failed to convert response to struct";
    return false;
  }

  return true;
}

bool SeventeenLiveApiWrappers::StartStream(const std::string &liveStreamID, const std::string &userID)
{
  obs_log(LOG_INFO, "StartStream start");
  lastErrorMessage.clear();
  QString urlStr = QString::fromStdString(SEVENTEENLIVE_STREAM_URL).arg(liveStreamID.c_str());
    QByteArray url = urlStr.toUtf8();

  Json requestData = Json::object{
    {"userID", userID},
  };
  std::string postData = requestData.dump();

  std::string error;
	Json json_out_resp;

	if (!InsertCommand(url.constData(), "application/json", "PATCH", postData.c_str(), json_out_resp)) {
    obs_log(LOG_ERROR, "StartStream error: %s", json_out_resp.dump().c_str());
    lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].string_value()) + " " + QString::fromStdString(json_out_resp["errorMessage"].string_value());
		return false;
	}

  obs_log(LOG_INFO, "StartStream success");
  return true;
}

bool SeventeenLiveApiWrappers::EnableStreamArchive(const std::string &liveStreamID, int enableArchive)
{
  obs_log(LOG_INFO, "EnableStreamArchive start");
  lastErrorMessage.clear();
  QString urlStr = QString::fromStdString(SEVENTEENLIVE_ARCHIVE_URL).arg(liveStreamID.c_str(), enableArchive);
    QByteArray url = urlStr.toUtf8();
    
  std::string error;
	Json json_out_resp;
	if (!InsertCommand(url.constData(), "application/json", "", nullptr, json_out_resp)) {
    obs_log(LOG_ERROR, "EnableStreamArchive error: %s", json_out_resp.dump().c_str());
    lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].string_value()) + " " + QString::fromStdString(json_out_resp["errorMessage"].string_value());
		return false;
	}
  obs_log(LOG_INFO, "EnableStreamArchive success");
  return true;
}

bool SeventeenLiveApiWrappers::StopStream(const std::string &liveStreamID, const SeventeenLiveCloseLiveRequest &request)
{
  obs_log(LOG_INFO, "StopStream start");
  lastErrorMessage.clear();
  QString urlStr = QString::fromStdString(SEVENTEENLIVE_STREAM_URL).arg(liveStreamID.c_str());
  QByteArray url = urlStr.toUtf8();

  Json requestData;
  if (!SeventeenLiveCloseLiveRequestToJson(request, requestData)) {
    obs_log(LOG_ERROR, "Failed to convert request to JSON");
    lastErrorMessage = "Failed to convert request to JSON";
    return false;
  }
  std::string postData = requestData.dump();
    
  std::string error;
	Json json_out_resp;
	if (!InsertCommand(url.constData(), "application/json", "DELETE", postData.c_str(), json_out_resp)) {
    obs_log(LOG_ERROR, "StopStream error: %s", json_out_resp.dump().c_str());
    lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].string_value()) + " " + QString::fromStdString(json_out_resp["errorMessage"].string_value());
		return false;
	}

  obs_log(LOG_INFO, "StopStream success");
  return true;
}

bool SeventeenLiveApiWrappers::GetConfigStreamer(const std::string region, const std::string language, SeventeenLiveConfigStreamer &response)
{
  obs_log(LOG_INFO, "GetConfigStreamer");

  lastErrorMessage.clear();
  QString urlStr = QString::fromStdString(SEVENTEENLIVE_GET_CONFIG_STREAMER_URL);
  QByteArray url = urlStr.toUtf8();

  std::vector<std::string> extraHeaders = {
  	"Userselectedregion: " + region,
    "Language: " + language
  };
  
  std::string error;
	Json json_out_resp;
	if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp, 0, true, extraHeaders)) {
    obs_log(LOG_ERROR, "GetConfigStreamer error: %s", json_out_resp.dump().c_str());
    lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].string_value()) + " " + QString::fromStdString(json_out_resp["errorMessage"].string_value());
		return false;
	}

  if (!JsonToSeventeenLiveConfigStreamer(json_out_resp, response)) {
    obs_log(LOG_ERROR, "Failed to convert response to struct");
    lastErrorMessage = "Failed to convert response to struct";
    return false;
  }

  obs_log(LOG_INFO, "GetConfigStreamer success");
  return true;
}

bool SeventeenLiveApiWrappers::GetAblyToken(const std::string &liveStreamID, Json &json_out)
{
  obs_log(LOG_INFO, "GetAblyToken");
  lastErrorMessage.clear();
  QString urlStr = QString::fromStdString(SEVENTEENLIVE_GET_ABLY_TOKEN_URL).arg(liveStreamID.c_str());
  QByteArray url = urlStr.toUtf8();

  if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out, 0, true)) {
    obs_log(LOG_ERROR, "GetAblyToken error: %s", json_out.dump().c_str());
    lastErrorMessage = QString::fromStdString(json_out["errorCode"].string_value()) + " " + QString::fromStdString(json_out["errorMessage"].string_value());
    return false;
  }
  
  obs_log(LOG_INFO, "GetAblyToken success: %s", json_out.dump().c_str());
  
  return true;
}

bool SeventeenLiveApiWrappers::GetGiftTabs(const std::string &liveStreamID, const std::string language, Json &json_out_resp)
{
  obs_log(LOG_INFO, "GetGiftTabs");
  
  lastErrorMessage.clear();
  
  QString urlStr = QString::fromStdString(SEVENTEENLIVE_GET_GIFTTABS_URL).arg(liveStreamID.c_str());
  QByteArray url = urlStr.toUtf8();
  
  std::vector<std::string> extraHeaders = {
    "Language: " + language
  };

  if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp, 0, true, extraHeaders)) {
    obs_log(LOG_ERROR, "GetConfigStreamer error: %s", json_out_resp.dump().c_str());
    lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].string_value()) + " " + QString::fromStdString(json_out_resp["errorMessage"].string_value());
		return false;
	}

  return true;
}

bool SeventeenLiveApiWrappers::GetGifts(const std::string language, Json &json_out_resp)
{
  obs_log(LOG_INFO, "GetGifts");

  lastErrorMessage.clear();

    QByteArray url = SEVENTEENLIVE_GET_GIFTS_URL.c_str();

  std::vector<std::string> extraHeaders = {
    "Language: " + language
  };

  if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp, 0, true, extraHeaders)) {
    obs_log(LOG_ERROR, "GetConfigStreamer error: %s", json_out_resp.dump().c_str());
    lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].string_value()) + " " + QString::fromStdString(json_out_resp["errorMessage"].string_value());
		return false;
	}

  obs_log(LOG_INFO, "GetGifts success %d", json_out_resp["gifts"].array_items().size());

  return true;
}

#include "OneSevenLiveApiWrappers.hpp"

#include <obs-module.h>

#include <QCryptographicHash>
#include <QFile>
#include <QMimeDatabase>
#include <QUrl>

#include "../utility/Common.hpp"
#include "../utility/RemoteTextThread.hpp"
#include "plugin-support.h"

using namespace std;

extern const char *service;

// Optimized URL constants - avoid repeated string concatenations
namespace {
    const string BASE_API_URL = string(ONESEVENLIVE_API_URL);

    // Helper function to build URLs efficiently
    inline string buildApiUrl(const char *endpoint) {
        return BASE_API_URL + endpoint;
    }
}  // namespace

// Login API: ONESEVENLIVE_API_URL + "/api/v1/auth/loginAction"
const string ONESEVENLIVE_LOGIN_URL = buildApiUrl("/api/v1/auth/loginAction");

const string ONESEVENLIVE_APIGATEWAY_URL = buildApiUrl("/apiGateWay");

const string ONESEVENLIVE_GET_ROOM_INFO_URL = buildApiUrl("/api/v1/lives/%1/info");

const string ONESEVENLIVE_CREATE_RTMP_URL = buildApiUrl("/api/v1/rtmp");

const string ONESEVENLIVE_STREAM_URL = buildApiUrl("/api/v1/lives/%1");

const string ONESEVENLIVE_ALIVE_URL = buildApiUrl("/api/v1/lives/%1/alive");

const string ONESEVENLIVE_ARCHIVE_URL = buildApiUrl("/api/v1/lives/%1/archive/recording?enable=%2");

const string ONESEVENLIVE_GET_CONFIG_STREAMER_URL =
    buildApiUrl("/api/v1/liveStreams/config/streamer");

const string ONESEVENLIVE_GET_RTMP_URL = buildApiUrl("/api/v1/rtmp?rtmp-provider=%1");

const string ONESEVENLIVE_GET_ARMYSUBSCRIPIONLEVELS_URL =
    buildApiUrl("/api/v1/army/subscriptionLVs");

const string ONESEVENLIVE_GET_CONFIG_URL = buildApiUrl("/api/v1/config");

const string ONESEVENLIVE_GET_USERINFO_URL = buildApiUrl("/api/v1/users/%1/info?onLive=1");

const string ONESEVENLIVE_CREATE_CUSTOMEVENT_URL = buildApiUrl("/api/v1/event/customEvent");

const string ONESEVENLIVE_GET_CUSTOMEVENT_URL = buildApiUrl("/api/v1/event/customEventV2");

const string ONESEVENLIVE_CHANGE_CUSTOMEVENT_STATUS_URL =
    buildApiUrl("/api/v1/event/customEvent/%1");

const string ONESEVENLIVE_GET_ABLY_TOKEN_URL =
    buildApiUrl("/api/v1/messenger/token?type=3&roomID=%1");

const string ONESEVENLIVE_GET_GIFTTABS_URL = buildApiUrl("/api/v1/lives/%1/giftTabs?filter=0");

const string ONESEVENLIVE_GET_GIFTS_URL = buildApiUrl("/api/v1/gifts");

const string ONESEVENLIVE_GET_ROCKVIEWERS_URL =
    buildApiUrl("/api/v1/lives/%1/streamer/rockviewers?type=0&count=50&filterEmpty=true");

const string ONESEVENLIVE_GET_ARMYNAME_URL = buildApiUrl("/api/v1/army/custom/%1/name");

const string ONESEVENLIVE_POKE_URL = buildApiUrl("/api/v1/pokes");

const string ONESEVENLIVE_POKE_ALL_URL = buildApiUrl("/api/v1/pokes/pokeAll");

const string ONESEVENLIVE_CHANGE_EVENT_URL = buildApiUrl("/api/v1/liveStreams/event");

OneSevenLiveApiWrappers::OneSevenLiveApiWrappers() : token("") {
    currentOS = GetCurrentOS();
    currentOSVersion = GetCurrentOSVersion();
    currentPlatformUUID = GetCurrentPlatformUUID();
}

OneSevenLiveApiWrappers::OneSevenLiveApiWrappers(std::string token_) : token(token_) {
    currentOS = GetCurrentOS();
    currentOSVersion = GetCurrentOSVersion();
    currentPlatformUUID = GetCurrentPlatformUUID();
}

void OneSevenLiveApiWrappers::setLastErrorMessage(const QString &message) {
    std::lock_guard<std::mutex> lock(stateMutex);
    lastErrorMessage = message;
}

void OneSevenLiveApiWrappers::clearLastErrorMessage() {
    std::lock_guard<std::mutex> lock(stateMutex);
    lastErrorMessage.clear();
}

bool OneSevenLiveApiWrappers::TryInsertCommand(const char *url, const char *content_type,
                                               std::string request_type, const char *data,
                                               Json &json_out, long *error_code, int data_size,
                                               bool token_required,
                                               const std::vector<std::string> extraHeaders) {
    long httpStatusCode = 0;

#ifdef _DEBUG
    obs_log(LOG_DEBUG, "17Live API command URL: %s", url);

    if (data && data[0] == '{')  // only log JSON data
        obs_log(LOG_DEBUG, "17Live API command data: %s", data);
#endif

    std::vector<std::string> headers;

    // Thread-safe access to token
    std::string currentToken;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (token_required && token.empty())
            return false;
        currentToken = token;
    }

    if (token_required)
        headers.push_back("Authorization: Bearer " + currentToken);

    headers.push_back("Devicetype: WEB");

    // add plugin version
    headers.push_back("version: " + std::string(PLUGIN_VERSION));

    // add OS version
    headers.push_back("OSVersion: " + currentOSVersion);

    // add HW
    headers.push_back("hardware: " + currentOS);

    headers.push_back("deviceName: OBSPlugin");
    headers.push_back("deviceModel: OBSPlugin");

    headers.push_back("deviceId: " + currentPlatformUUID);

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
    bool success = GetRemoteFile(url, output, error, &httpStatusCode, content_type, request_type,
                                 data, headers, nullptr, timeout, false, data_size);
    if (error_code)
        *error_code = httpStatusCode;

    if (!success || output.empty()) {
        if (!error.empty())
            obs_log(LOG_WARNING, "17Live API request failed: %s", error.c_str());
        return false;
    }

    try {
        json_out = Json::parse(output);
#ifdef _DEBUG
        obs_log(LOG_DEBUG, "17Live API command answer: %s", json_out.dump().c_str());
#endif
    } catch (const Json::parse_error &e) {
        obs_log(LOG_ERROR, "Failed to parse JSON response: %s", e.what());
        return false;
    }
    return httpStatusCode < 400;
}

bool OneSevenLiveApiWrappers::UpdateAccessToken() {
    obs_log(LOG_INFO, "Updating access token");
    // TODO: implement
    return false;
}

bool OneSevenLiveApiWrappers::InsertCommand(const char *url, const char *content_type,
                                            std::string request_type, const char *data,
                                            Json &json_out, int data_size, bool token_required,
                                            const std::vector<std::string> extraHeaders) {
    long error_code;
    std::string error;
    bool success = TryInsertCommand(url, content_type, request_type, data, json_out, &error_code,
                                    data_size, token_required, extraHeaders);

    if (error_code == 401) {
        // Attempt to update access token and try again
        if (!UpdateAccessToken())
            return false;
        success = TryInsertCommand(url, content_type, request_type, data, json_out, &error_code,
                                   data_size);
    }

    try {
        if (json_out.contains("error")) {
            obs_log(LOG_ERROR, "17Live API error:\n\tHTTP status: %ld\n\tURL: %s\n\tJSON: %s",
                    error_code, url, json_out.dump().c_str());

            try {
                const Json json_out_data = Json::parse(json_out["data"].get<std::string>());
                setLastErrorMessage(
                    QString::fromStdString(json_out_data["message"].get<std::string>()));
            } catch (const Json::parse_error &e) {
                obs_log(LOG_ERROR, "Failed to parse error data JSON: %s", e.what());
                setLastErrorMessage(QString("API error with invalid error data"));
            }

            // The existence of an error implies non-success even if the HTTP status code disagrees.
            success = false;
        } else if (json_out.contains("errorCode")) {
            obs_log(LOG_ERROR, "17Live API error:\n\tHTTP status: %ld\n\tURL: %s\n\tJSON: %s",
                    error_code, url, json_out.dump().c_str());

            const std::string errorCode = json_out["errorCode"].get<std::string>();
            const std::string errorMessage = json_out["errorMessage"].get<std::string>();
            setLastErrorMessage(QString::fromStdString(errorCode + " " + errorMessage));
            // The existence of an error implies non-success even if the HTTP status code disagrees.
            success = false;
        }
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "Exception processing API response: %s", e.what());
        setLastErrorMessage(QString("Exception processing API response: %1").arg(e.what()));
        success = false;
    }

    return success;
}

bool OneSevenLiveApiWrappers::Login(const QString &username, const QString &password,
                                    OneSevenLiveLoginData &loginData) {
    clearLastErrorMessage();

    const QByteArray url = ONESEVENLIVE_LOGIN_URL.c_str();

    // Pre-convert strings to avoid repeated conversions
    const std::string usernameStd = username.toStdString();
    const std::string passwordMd5Std = md5(password).toStdString();

    // TODO: language
    // const char *obs_get_locale(void)
    const Json data = Json{
        {"language", GetCurrentLanguage()},
        {"openID", usernameStd},
        {"password", passwordMd5Std},
    };

    const std::string postData = data.dump();
    std::string error;
    Json json_out;

    if (!InsertCommand(url, "application/json", "", postData.c_str(), json_out, 0, false)) {
        return false;
    }
    obs_log(LOG_INFO, "Login success");
    obs_log(LOG_INFO, "Login data: %s", json_out.dump().c_str());

    try {
        // transform string json_out["data"] to Json
        const std::string dataStr = json_out["data"].get<std::string>();
        Json json_out_data = Json::parse(dataStr);

        // check if json_out_data contains "result" key
        if (json_out_data.contains("result")) {
            if (json_out_data["result"].get<std::string>() == "fail") {
                const std::string messageStr = json_out_data["message"].get<std::string>();
                setLastErrorMessage(QString::fromStdString(messageStr));
                return false;
            }
        } else {
            obs_log(LOG_WARNING, "Login response missing result field: %s",
                    json_out_data.dump().c_str());
            setLastErrorMessage("Login response missing result field");
            return false;
        }

        // Check for required fields
        if (!json_out_data.contains("jwtAccessToken") ||
            !json_out_data["jwtAccessToken"].is_string() ||
            json_out_data["jwtAccessToken"].get<std::string>().empty()) {
            obs_log(LOG_ERROR, "Login response missing jwtAccessToken");
            setLastErrorMessage("Login response missing jwtAccessToken");
            return false;
        }

        if (!JsonToOneSevenLiveLoginData(json_out_data, loginData)) {
            obs_log(LOG_ERROR, "Failed to convert response to struct");
            setLastErrorMessage("Failed to convert response to struct");
            return false;
        }
    } catch (const Json::parse_error &e) {
        obs_log(LOG_ERROR, "Failed to parse login response data: %s", e.what());
        setLastErrorMessage("Failed to parse login response data");
        return false;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "Exception during login data processing: %s", e.what());
        setLastErrorMessage("Internal error during login processing");
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "Unknown exception during login data processing");
        setLastErrorMessage("Unknown error during login processing");
        return false;
    }

    // save token to next call
    token = loginData.jwtAccessToken.toStdString();

    return !loginData.jwtAccessToken.isEmpty();
}

bool OneSevenLiveApiWrappers::OneSevenLiveApiWrappers::GetSelfInfo(
    OneSevenLiveLoginData &loginData) {
    clearLastErrorMessage();
    Json json_out;
    if (!CommonRequest("getSelfInfo", json_out))
        return false;

    // check if json_out_data contains "openID"
    if (!json_out.contains("openID")) {
        obs_log(LOG_ERROR, "GetSelfInfo response missing openID field: %s",
                json_out.dump().c_str());
        lastErrorMessage = "GetSelfInfo response missing openID field";
        return false;
    }

    // Pre-convert strings to avoid repeated conversions
    const std::string openIDStr = json_out["openID"].get<std::string>();
    const std::string displayNameStr = json_out["displayName"].get<std::string>();
    const std::string userIDStr = json_out["userID"].get<std::string>();
    const std::string regionStr = json_out["region"].get<std::string>();

    loginData.userInfo.openID = QString::fromStdString(openIDStr);
    loginData.userInfo.displayName = QString::fromStdString(displayNameStr);
    loginData.userInfo.roomID = json_out["roomID"].get<int>();
    loginData.userInfo.region = QString::fromStdString(regionStr);

    return true;
}

bool OneSevenLiveApiWrappers::ChangeEvent(const OneSevenLiveChangeEventRequest &request) {
    obs_log(LOG_INFO, "ChangeEvent start");

    clearLastErrorMessage();

    QByteArray url = QByteArray(ONESEVENLIVE_CHANGE_EVENT_URL.c_str());
    obs_log(LOG_INFO, "ChangeEvent url: %s", ONESEVENLIVE_CHANGE_EVENT_URL.c_str());

    Json requestData;
    if (!OneSevenLiveChangeEventRequestToJson(request, requestData)) {
        obs_log(LOG_ERROR, "Failed to convert request to JSON");
        lastErrorMessage = "Failed to convert request to JSON";
        return false;
    }

    const std::string postData = requestData.dump();
    obs_log(LOG_INFO, "ChangeEvent requestData: %s", postData.c_str());

    std::string error;
    Json json_out;

    if (!InsertCommand(url.constData(), "application/json", "POST", postData.c_str(), json_out)) {
        obs_log(LOG_ERROR, "ChangeEvent error: %s", json_out.dump().c_str());
        // Pre-convert error strings to avoid repeated conversions
        const std::string errorCodeStr = json_out["errorCode"].get<std::string>();
        const std::string errorMessageStr = json_out["errorMessage"].get<std::string>();
        lastErrorMessage =
            QString::fromStdString(errorCodeStr) + " " + QString::fromStdString(errorMessageStr);
        return false;
    }

    // obs_log(LOG_INFO, "ChangeEvent success");
    // obs_log(LOG_INFO, "change event response: %s", json_out.dump().c_str());

    // Check if errorCode field exists
    if (json_out.contains("errorCode")) {
        obs_log(LOG_ERROR, "ChangeEvent error: %s", json_out.dump().c_str());
        // lastErrorMessage = errorCode + errorMessage
        lastErrorMessage = QString::fromStdString(json_out["errorCode"].get<std::string>()) + " " +
                           QString::fromStdString(json_out["errorMessage"].get<std::string>());
        return false;
    }

    return true;
}

bool OneSevenLiveApiWrappers::CommonRequest(const std::string action, Json &json_out) {
    clearLastErrorMessage();

    const QByteArray url = ONESEVENLIVE_APIGATEWAY_URL.c_str();

    const Json data = Json{
        {"nonce", "nonce-17live-" + std::to_string(getCurrentTimestampMs())},
        {"action", action},
    };

    // Convert JSON to string and perform URL encoding
    std::string jsonStr = data.dump();
    QString encodedData = QUrl::toPercentEncoding(QString::fromStdString(jsonStr));

    // Build final post data
    std::string postData = "cypher=0_v2&data=" + encodedData.toStdString();

    std::string error;
    Json json_out_resp;

    if (!InsertCommand(url, "application/x-www-form-urlencoded", "", postData.c_str(),
                       json_out_resp)) {
        return false;
    }
    obs_log(LOG_INFO, "apiGateWay success");

    // Check if exist errorCode field
    if (json_out_resp.contains("errorCode")) {
        obs_log(LOG_ERROR, "apiGateWay error: %s", json_out_resp.dump().c_str());
        // lastErrorMessage = errorCode + errorMessage
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    // transform string json_out["data"] to Json
    try {
        json_out = Json::parse(json_out_resp["data"].get<std::string>());
    } catch (const Json::parse_error &e) {
        obs_log(LOG_ERROR, "Failed to parse apiGateWay response data: %s", e.what());
        return false;
    }

    return true;
}

bool OneSevenLiveApiWrappers::GetRoomInfo(const qint64 roomID, OneSevenLiveRoomInfo &roomInfo) {
    clearLastErrorMessage();

    // Build request URL
    QString urlStr = QString::fromStdString(ONESEVENLIVE_GET_ROOM_INFO_URL).arg(roomID);
    QByteArray url = urlStr.toUtf8();

    Json json_out;
    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out, 0, true)) {
        obs_log(LOG_ERROR, "GetRoomInfo failed %s", json_out.dump().c_str());
        lastErrorMessage = QString::fromStdString("GetRoomInfo failed %s")
                               .arg(json_out.dump().c_str())
                               .toUtf8()
                               .constData();

        return false;
    }

    // Use JsonToOneSevenLiveRoomInfo function to parse data to struct
    if (!JsonToOneSevenLiveRoomInfo(json_out, roomInfo)) {
        obs_log(LOG_ERROR, "Failed to parse room info data");
        lastErrorMessage = "Failed to parse room info data";
        return false;
    }

    return true;
}

QString OneSevenLiveApiWrappers::md5(const QString &str) {
    QByteArray input = str.toUtf8();
    QByteArray hash = QCryptographicHash::hash(input, QCryptographicHash::Md5);
    return QString(hash.toHex());
}

// Add function to generate millisecond timestamp
int64_t OneSevenLiveApiWrappers::getCurrentTimestampMs() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

bool OneSevenLiveApiWrappers::CreateRtmp(const OneSevenLiveRtmpRequest &request,
                                         OneSevenLiveRtmpResponse &response) {
    obs_log(LOG_INFO, "CreateRtmp start");
    clearLastErrorMessage();

    const QByteArray url = ONESEVENLIVE_CREATE_RTMP_URL.c_str();

    Json requestData;
    if (!OneSevenLiveRtmpRequestToJson(request, requestData)) {
        obs_log(LOG_ERROR, "Failed to convert request to JSON");
        lastErrorMessage = "Failed to convert request to JSON";
        return false;
    }

    std::string postData = requestData.dump();

    obs_log(LOG_INFO, "CreateRtmp requestData: %s", postData.c_str());

    std::string error;
    Json json_out;

    if (!InsertCommand(url, "application/json", "", postData.c_str(), json_out)) {
        return false;
    }

    obs_log(LOG_INFO, "CreateRtmp success");
    obs_log(LOG_INFO, "rtmp info %s", json_out.dump().c_str());

    // Check if exist errorCode field
    if (json_out.contains("errorCode")) {
        obs_log(LOG_ERROR, "CreateRtmp error: %s", json_out.dump().c_str());
        // lastErrorMessage = errorCode + errorMessage
        lastErrorMessage = QString::fromStdString(json_out["errorCode"].get<std::string>()) + " " +
                           QString::fromStdString(json_out["errorMessage"].get<std::string>());
        return false;
    }

    if (!JsonToOneSevenLiveRtmpResponse(json_out, response)) {
        obs_log(LOG_ERROR, "Failed to convert response to struct");
        lastErrorMessage = "Failed to convert response to struct";
        return false;
    }

    return true;
}

bool OneSevenLiveApiWrappers::StartStream(const std::string &liveStreamID,
                                          const std::string &userID) {
    obs_log(LOG_INFO, "StartStream start");
    clearLastErrorMessage();
    QString urlStr = QString::fromStdString(ONESEVENLIVE_STREAM_URL).arg(liveStreamID.c_str());
    QByteArray url = urlStr.toUtf8();

    Json requestData = Json{
        {"userID", userID},
    };
    std::string postData = requestData.dump();

    std::string error;
    Json json_out_resp;

    if (!InsertCommand(url.constData(), "application/json", "PATCH", postData.c_str(),
                       json_out_resp)) {
        obs_log(LOG_ERROR, "StartStream error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    obs_log(LOG_INFO, "StartStream success");
    return true;
}

bool OneSevenLiveApiWrappers::EnableStreamArchive(const std::string &liveStreamID,
                                                  int enableArchive) {
    obs_log(LOG_INFO, "EnableStreamArchive start");
    clearLastErrorMessage();
    QString urlStr =
        QString::fromStdString(ONESEVENLIVE_ARCHIVE_URL).arg(liveStreamID.c_str(), enableArchive);
    QByteArray url = urlStr.toUtf8();

    std::string error;
    Json json_out_resp;
    // null post data, explicitly set request type as POST
    if (!InsertCommand(url.constData(), "application/json", "POST", nullptr, json_out_resp)) {
        obs_log(LOG_ERROR, "EnableStreamArchive error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }
    obs_log(LOG_INFO, "EnableStreamArchive success");
    return true;
}

bool OneSevenLiveApiWrappers::StopStream(const std::string &liveStreamID,
                                         const OneSevenLiveCloseLiveRequest &request) {
    obs_log(LOG_INFO, "StopStream start");
    clearLastErrorMessage();
    QString urlStr = QString::fromStdString(ONESEVENLIVE_STREAM_URL).arg(liveStreamID.c_str());
    QByteArray url = urlStr.toUtf8();

    Json requestData;
    if (!OneSevenLiveCloseLiveRequestToJson(request, requestData)) {
        obs_log(LOG_ERROR, "Failed to convert request to JSON");
        lastErrorMessage = "Failed to convert request to JSON";
        return false;
    }
    std::string postData = requestData.dump();

    std::string error;
    Json json_out_resp;
    if (!InsertCommand(url.constData(), "application/json", "DELETE", postData.c_str(),
                       json_out_resp)) {
        obs_log(LOG_ERROR, "StopStream error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    obs_log(LOG_INFO, "StopStream success");
    return true;
}

bool OneSevenLiveApiWrappers::CreateCustomEvent(const OneSevenLiveCustomEvent &request,
                                                OneSevenLiveCustomEvent &response) {
    obs_log(LOG_INFO, "CreateCustomEvent start");

    clearLastErrorMessage();

    const QByteArray url = ONESEVENLIVE_CREATE_CUSTOMEVENT_URL.c_str();

    try {
        Json requestData;
        if (!OneSevenLiveCustomEventToJson(request, requestData)) {
            obs_log(LOG_ERROR, "Failed to convert request to JSON");
            setLastErrorMessage("Failed to convert request to JSON");
            return false;
        }

        const std::string postData = requestData.dump();

        obs_log(LOG_INFO, "CreateCustomEvent requestData: %s", postData.c_str());

        std::string error;
        Json json_out;

        if (!InsertCommand(url, "application/json", "", postData.c_str(), json_out)) {
            return false;
        }

        obs_log(LOG_INFO, "CreateCustomEvent success");
        obs_log(LOG_INFO, "custom event info %s", json_out.dump().c_str());

        // Check if errorCode field exists
        if (json_out.contains("errorCode")) {
            obs_log(LOG_ERROR, "CreateCustomEvent error: %s", json_out.dump().c_str());
            // Pre-convert error strings to avoid repeated conversions
            const std::string errorCodeStr = json_out["errorCode"].get<std::string>();
            const std::string errorMessageStr = json_out["errorMessage"].get<std::string>();
            setLastErrorMessage(QString::fromStdString(errorCodeStr) + " " +
                                QString::fromStdString(errorMessageStr));
            return false;
        }

        if (!JsonToOneSevenLiveCustomEvent(json_out, response)) {
            obs_log(LOG_ERROR, "Failed to convert response to struct");
            setLastErrorMessage("Failed to convert response to struct");
            return false;
        }
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "Exception during CreateCustomEvent: %s", e.what());
        setLastErrorMessage("Internal error during custom event creation");
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "Unknown exception during CreateCustomEvent");
        setLastErrorMessage("Unknown error during custom event creation");
        return false;
    }

    return true;
}

bool OneSevenLiveApiWrappers::ChangeCustomEventStatus(
    const std::string &eventID, const OneSevenLiveCustomEventStatusRequest &request) {
    obs_log(LOG_INFO, "ChangeCustomEventStatus start");

    clearLastErrorMessage();

    // Replace %1 with eventID in the URL
    QString urlStr =
        QString::fromStdString(ONESEVENLIVE_CHANGE_CUSTOMEVENT_STATUS_URL).arg(eventID.c_str());
    obs_log(LOG_INFO, "ChangeCustomEventStatus url: %s", urlStr.toStdString().c_str());
    QByteArray url = urlStr.toUtf8();

    Json requestData;
    if (!OneSevenLiveChangeCustomEventStatusRequestToJson(request, requestData)) {
        obs_log(LOG_ERROR, "Failed to convert request to JSON");
        lastErrorMessage = "Failed to convert request to JSON";
        return false;
    }

    std::string patchData = requestData.dump();

    obs_log(LOG_INFO, "ChangeCustomEventStatus requestData: %s", patchData.c_str());

    Json json_out;

    if (!InsertCommand(url, "application/json", "PATCH", patchData.c_str(), json_out)) {
        // Check if errorCode field exists
        if (json_out.contains("errorCode")) {
            obs_log(LOG_ERROR, "ChangeCustomEventStatus error: %s", json_out.dump().c_str());
            // lastErrorMessage = errorCode + errorMessage
            setLastErrorMessage(
                QString::fromStdString(json_out["errorCode"].get<std::string>()) + " " +
                QString::fromStdString(json_out["errorMessage"].get<std::string>()));
        }
        return false;
    }

    obs_log(LOG_INFO, "ChangeCustomEventStatus success");

    // For ChangeCustomEventStatus, we only check if status code is 200, no need to parse response
    return true;
}

bool OneSevenLiveApiWrappers::CheckStream(const std::string &liveStreamID) {
    // obs_log(LOG_INFO, "CheckStream start");
    clearLastErrorMessage();
    QString urlStr = QString::fromStdString(ONESEVENLIVE_ALIVE_URL).arg(liveStreamID.c_str());
    QByteArray url = urlStr.toUtf8();

    std::string error;
    Json json_out_resp;
    if (!InsertCommand(url.constData(), "application/json", "POST", nullptr, json_out_resp)) {
        obs_log(LOG_ERROR, "CheckStream error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    // obs_log(LOG_INFO, "CheckStream success");
    return true;
}

bool OneSevenLiveApiWrappers::GetConfigStreamer(const std::string region,
                                                const std::string language,
                                                OneSevenLiveConfigStreamer &response) {
    obs_log(LOG_INFO, "GetConfigStreamer");

    lastErrorMessage.clear();
    QString urlStr = QString::fromStdString(ONESEVENLIVE_GET_CONFIG_STREAMER_URL);
    QByteArray url = urlStr.toUtf8();

    std::vector<std::string> extraHeaders = {"Userselectedregion: " + region,
                                             "Language: " + language};

    std::string error;
    Json json_out_resp;
    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp, 0, true,
                       extraHeaders)) {
        obs_log(LOG_ERROR, "GetConfigStreamer error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    if (!JsonToOneSevenLiveConfigStreamer(json_out_resp, response)) {
        obs_log(LOG_ERROR, "Failed to convert response to struct");
        lastErrorMessage = "Failed to convert response to struct";
        return false;
    }

    // obs_log(LOG_INFO, "GetConfigStreamer success");
    return true;
}

bool OneSevenLiveApiWrappers::GetRtmpByProvider(const std::string provider,
                                                OneSevenLiveRtmpResponse &response) {
    obs_log(LOG_INFO, "GetRtmpByProvider");

    lastErrorMessage.clear();
    QString urlStr = QString::fromStdString(ONESEVENLIVE_GET_RTMP_URL).arg(provider.c_str());
    QByteArray url = urlStr.toUtf8();

    std::string error;
    Json json_out_resp;
    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp)) {
        obs_log(LOG_ERROR, "GetRtmpByProvider error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    if (!JsonToOneSevenLiveRtmpResponse(json_out_resp, response)) {
        obs_log(LOG_ERROR, "Failed to convert response to struct");
        lastErrorMessage = "Failed to convert response to struct";
        return false;
    }

    return true;
}

bool OneSevenLiveApiWrappers::GetArmySubscriptionLevels(
    const std::string region, const std::string language,
    OneSevenLiveArmySubscriptionLevels &response) {
    obs_log(LOG_INFO, "GetArmySubscriptionLevels");

    lastErrorMessage.clear();
    QString urlStr = QString::fromStdString(ONESEVENLIVE_GET_ARMYSUBSCRIPIONLEVELS_URL);
    QByteArray url = urlStr.toUtf8();

    std::vector<std::string> extraHeaders = {"Userselectedregion: " + region,
                                             "Language: " + language};

    std::string error;
    Json json_out_resp;
    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp, 0, true,
                       extraHeaders)) {
        obs_log(LOG_ERROR, "GetArmySubscriptionLevels error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    if (!JsonToOneSevenLiveArmySubscriptionLevels(json_out_resp, response)) {
        obs_log(LOG_ERROR, "Failed to convert response to struct");
        lastErrorMessage = "Failed to convert response to struct";
        return false;
    }

    // obs_log(LOG_INFO, "GetConfigStreamer success");
    return true;
}

bool OneSevenLiveApiWrappers::GetConfig(const std::string region, const std::string language,
                                        Json &json_out_resp) {
    obs_log(LOG_INFO, "GetConfig");

    lastErrorMessage.clear();
    QString urlStr = QString::fromStdString(ONESEVENLIVE_GET_CONFIG_URL);
    QByteArray url = urlStr.toUtf8();

    std::vector<std::string> extraHeaders = {"Userselectedregion: " + region,
                                             "Language: " + language};

    std::string error;

    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp, 0, true,
                       extraHeaders)) {
        obs_log(LOG_ERROR, "GetConfig error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    return true;
}

bool OneSevenLiveApiWrappers::GetUserInfo(const std::string userID, const std::string region,
                                          const std::string language,
                                          OneSevenLiveUserInfo &response) {
    obs_log(LOG_INFO, "GetUserInfo");

    lastErrorMessage.clear();
    QString urlStr = QString::fromStdString(ONESEVENLIVE_GET_USERINFO_URL).arg(userID.c_str());
    QByteArray url = urlStr.toUtf8();

    std::vector<std::string> extraHeaders = {"Userselectedregion: " + region,
                                             "Language: " + language};

    std::string error;
    Json json_out_resp;
    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp, 0, true,
                       extraHeaders)) {
        obs_log(LOG_ERROR, "GetUserInfo error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    if (!JsonToOneSevenLiveUserInfo(json_out_resp, response)) {
        obs_log(LOG_ERROR, "Failed to convert response to struct");
        lastErrorMessage = "Failed to convert response to struct";
        return false;
    }

    // obs_log(LOG_INFO, "GetUserInfo success");
    return true;
}

bool OneSevenLiveApiWrappers::GetAblyToken(const std::string &liveStreamID, Json &json_out) {
    // obs_log(LOG_INFO, "GetAblyToken");
    lastErrorMessage.clear();
    QString urlStr =
        QString::fromStdString(ONESEVENLIVE_GET_ABLY_TOKEN_URL).arg(liveStreamID.c_str());
    QByteArray url = urlStr.toUtf8();

    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out, 0, true)) {
        obs_log(LOG_ERROR, "GetAblyToken error: %s", json_out.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out["errorCode"].get<std::string>()) + " " +
                           QString::fromStdString(json_out["errorMessage"].get<std::string>());
        return false;
    }

    // obs_log(LOG_INFO, "GetAblyToken success: %s", json_out.dump().c_str());

    return true;
}

bool OneSevenLiveApiWrappers::GetGiftTabs(const std::string &roomID, const std::string language,
                                          Json &json_out_resp) {
    obs_log(LOG_INFO, "GetGiftTabs");

    lastErrorMessage.clear();

    QString urlStr = QString::fromStdString(ONESEVENLIVE_GET_GIFTTABS_URL).arg(roomID.c_str());
    QByteArray url = urlStr.toUtf8();

    std::vector<std::string> extraHeaders = {"Language: " + language};

    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp, 0, true,
                       extraHeaders)) {
        obs_log(LOG_ERROR, "GetConfigStreamer error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    return true;
}

bool OneSevenLiveApiWrappers::GetGifts(const std::string language, Json &json_out_resp) {
    obs_log(LOG_INFO, "GetGifts");

    lastErrorMessage.clear();

    QByteArray url = ONESEVENLIVE_GET_GIFTS_URL.c_str();

    std::vector<std::string> extraHeaders = {"Language: " + language};

    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp, 0, true,
                       extraHeaders)) {
        obs_log(LOG_ERROR, "GetConfigStreamer error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    obs_log(LOG_INFO, "GetGifts success %d", json_out_resp["gifts"].size());

    return true;
}

bool OneSevenLiveApiWrappers::GetRockViewers(const std::string &roomID, Json &json_out_resp) {
    // obs_log(LOG_INFO, "GetRockViewers");

    lastErrorMessage.clear();
    QString urlStr = QString::fromStdString(ONESEVENLIVE_GET_ROCKVIEWERS_URL).arg(roomID.c_str());
    QByteArray url = urlStr.toUtf8();

    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp, 0,
                       true)) {
        obs_log(LOG_ERROR, "GetRockViewers error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    // obs_log(LOG_INFO, "GetRockViewers success");
    return true;
}

bool OneSevenLiveApiWrappers::GetCustomEvent(const std::string &userID,
                                             OneSevenLiveCustomEvent &response) {
    obs_log(LOG_INFO, "GetCustomEvent start");

    lastErrorMessage.clear();

    // Build request URL with query parameter
    QString urlStr = QString::fromStdString(ONESEVENLIVE_GET_CUSTOMEVENT_URL) +
                     "?userID=" + QString::fromStdString(userID);
    QByteArray url = urlStr.toUtf8();

    Json json_out;
    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out, 0, true)) {
        obs_log(LOG_ERROR, "GetCustomEvent failed %s", json_out.dump().c_str());
        lastErrorMessage = QString::fromStdString("GetCustomEvent failed %s")
                               .arg(json_out.dump().c_str())
                               .toUtf8()
                               .constData();
        return false;
    }

    // Use JsonToOneSevenLiveCustomEvent function to parse data to struct
    if (!JsonToOneSevenLiveCustomEvent(json_out, response)) {
        obs_log(LOG_ERROR, "Failed to parse custom event data");
        lastErrorMessage = "Failed to parse custom event data";
        return false;
    }

    obs_log(LOG_INFO, "GetCustomEvent success");
    return true;
}

bool OneSevenLiveApiWrappers::GetArmyName(const std::string &userID,
                                          OneSevenLiveArmyNameResponse &response) {
    obs_log(LOG_INFO, "GetArmyName start");
    lastErrorMessage.clear();
    QString urlStr = QString::fromStdString(ONESEVENLIVE_GET_ARMYNAME_URL).arg(userID.c_str());
    QByteArray url = urlStr.toUtf8();

    std::string error;
    Json json_out_resp;

    if (!InsertCommand(url.constData(), "application/json", "GET", nullptr, json_out_resp)) {
        obs_log(LOG_ERROR, "GetArmyName error: %s", json_out_resp.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) +
                           " " +
                           QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        return false;
    }

    if (!JsonToOneSevenLiveArmyNameResponse(json_out_resp, response)) {
        obs_log(LOG_ERROR, "Failed to convert response to struct");
        lastErrorMessage = "Failed to convert response to struct";
        return false;
    }

    obs_log(LOG_INFO, "GetArmyName success");
    return true;
}

bool OneSevenLiveApiWrappers::PokeOne(const OneSevenLivePokeRequest &request,
                                      OneSevenLivePokeResponse &response) {
    obs_log(LOG_INFO, "PokeOne start");

    lastErrorMessage.clear();

    QByteArray url = QByteArray(ONESEVENLIVE_POKE_URL.c_str());
    obs_log(LOG_INFO, "PokeOne url: %s", ONESEVENLIVE_POKE_URL.c_str());

    Json requestData;
    if (!OneSevenLivePokeRequestToJson(request, requestData)) {
        obs_log(LOG_ERROR, "Failed to convert request to JSON");
        lastErrorMessage = "Failed to convert request to JSON";
        return false;
    }

    std::string postData = requestData.dump();
    obs_log(LOG_INFO, "PokeOne requestData: %s", postData.c_str());

    std::string error;
    Json json_out;

    if (!InsertCommand(url.constData(), "application/json", "POST", postData.c_str(), json_out)) {
        obs_log(LOG_ERROR, "PokeOne error: %s", json_out.dump().c_str());
        lastErrorMessage = QString::fromStdString(json_out["errorCode"].get<std::string>()) + " " +
                           QString::fromStdString(json_out["errorMessage"].get<std::string>());
        return false;
    }

    // obs_log(LOG_INFO, "PokeOne success");
    // obs_log(LOG_INFO, "poke response: %s", json_out.dump().c_str());

    // Check if errorCode field exists
    if (json_out.contains("errorCode")) {
        obs_log(LOG_ERROR, "PokeOne error: %s", json_out.dump().c_str());
        // lastErrorMessage = errorCode + errorMessage
        lastErrorMessage = QString::fromStdString(json_out["errorCode"].get<std::string>()) + " " +
                           QString::fromStdString(json_out["errorMessage"].get<std::string>());
        return false;
    }

    if (!JsonToOneSevenLivePokeResponse(json_out, response)) {
        obs_log(LOG_ERROR, "Failed to convert response to struct");
        lastErrorMessage = "Failed to convert response to struct";
        return false;
    }

    return true;
}

bool OneSevenLiveApiWrappers::PokeAll(const OneSevenLivePokeAllRequest &request,
                                      OneSevenLivePokeResponse &response) {
    obs_log(LOG_INFO, "PokeAll start");

    lastErrorMessage.clear();

    const QByteArray url = QByteArray(ONESEVENLIVE_POKE_ALL_URL.c_str());
    obs_log(LOG_INFO, "PokeAll url: %s", ONESEVENLIVE_POKE_ALL_URL.c_str());

    Json requestData;
    if (!OneSevenLivePokeAllRequestToJson(request, requestData)) {
        obs_log(LOG_ERROR, "Failed to convert request to JSON");
        lastErrorMessage = "Failed to convert request to JSON";
        return false;
    }

    const std::string postData = requestData.dump();
    obs_log(LOG_INFO, "PokeAll requestData: %s", postData.c_str());

    std::string error;
    Json json_out;

    if (!InsertCommand(url.constData(), "application/json", "POST", postData.c_str(), json_out)) {
        obs_log(LOG_ERROR, "PokeAll error: %s", json_out.dump().c_str());
        // Pre-convert error strings to avoid repeated conversions
        const std::string errorCodeStr = json_out["errorCode"].get<std::string>();
        const std::string errorMessageStr = json_out["errorMessage"].get<std::string>();
        lastErrorMessage =
            QString::fromStdString(errorCodeStr) + " " + QString::fromStdString(errorMessageStr);
        return false;
    }

    // obs_log(LOG_INFO, "PokeAll success");
    // obs_log(LOG_INFO, "poke all response: %s", json_out.dump().c_str());

    // Check if errorCode field exists
    if (json_out.contains("errorCode")) {
        obs_log(LOG_ERROR, "PokeAll error: %s", json_out.dump().c_str());
        // Pre-convert error strings to avoid repeated conversions
        const std::string errorCodeStr = json_out["errorCode"].get<std::string>();
        const std::string errorMessageStr = json_out["errorMessage"].get<std::string>();
        lastErrorMessage =
            QString::fromStdString(errorCodeStr) + " " + QString::fromStdString(errorMessageStr);
        return false;
    }

    if (!JsonToOneSevenLivePokeResponse(json_out, response)) {
        obs_log(LOG_ERROR, "Failed to convert response to struct");
        lastErrorMessage = "Failed to convert response to struct";
        return false;
    }

    return true;
}

#include "OneSevenLiveConfigManager.hpp"

#include <obs-module.h>
#include <util/config-file.h>

#include <QDir>
#include <QFile>
#include <QString>
#include <fstream>

#include "api/OneSevenLiveApiWrappers.hpp"
#include "plugin-support.h"

const char *service = "OneSevenLive";

#define CONFIG_PATH ".17Live"
#define CONFIG_NAME "config.ini"

OneSevenLiveConfigManager::OneSevenLiveConfigManager() : initialized(false) {}

bool OneSevenLiveConfigManager::initialize() {
    // Prevent duplicate initialization
    if (initialized) {
        return true;
    }

    // .17Live directory under current user's home directory, using Qt method
    QString homeDir = QDir::homePath();
    QString configDir = homeDir + "/" + CONFIG_PATH;
    QDir dir(configDir);
    // If directory doesn't exist, create it
    if (!dir.exists()) {
        if (!dir.mkpath(configDir)) {
            obs_log(LOG_ERROR, "Failed to create config directory");
            return false;
        }
    }

    // Configuration file path
    QString configFilePath = configDir + "/" + CONFIG_NAME;

    configPath = configDir.toStdString();

    int ret = config_open(&config, configFilePath.toStdString().c_str(), CONFIG_OPEN_ALWAYS);
    if (ret != CONFIG_SUCCESS) {
        obs_log(LOG_ERROR, "Failed to open config file");
        return false;
    }

    initialized = true;

    return true;
}

bool OneSevenLiveConfigManager::getDockVisibility(const std::string &dockName) {
    if (!initialized) {
        return false;
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    std::string key = "DockVisibility_" + dockName;
    const char *visibilityChar = config_get_string(config, service, key.c_str());
    if (!visibilityChar) {
        return false;  // Default to false if not found
    }

    std::string visibility = visibilityChar;
    return visibility == "true";
}

bool OneSevenLiveConfigManager::setDockVisibility(const std::string &dockName, bool visible) {
    if (!initialized) {
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    std::string key = "DockVisibility_" + dockName;
    std::string value = visible ? "true" : "false";

    config_set_string(config, service, key.c_str(), value.c_str());
    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save dock visibility config");
        return false;
    }

    return true;
}

bool OneSevenLiveConfigManager::getConfigValue(const std::string &key, std::string &value) {
    if (!initialized) {
        return false;
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    const char *valueChar = config_get_string(config, service, key.c_str());
    if (!valueChar) {
        return false;
    }
    value = valueChar;
    return true;
}

qint64 OneSevenLiveConfigManager::getRoomID() {
    if (!initialized) {
        return 0;
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return 0;
    }

    return static_cast<qint64>(config_get_uint(config, service, "RoomID"));
}

bool OneSevenLiveConfigManager::getLoginData(OneSevenLiveLoginData &loginData) {
    if (!initialized) {
        return false;
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    const char *jwtTokenChar = config_get_string(config, service, "JwtToken");
    const char *openIdChar = config_get_string(config, service, "OpenID");
    const char *displayNameChar = config_get_string(config, service, "DisplayName");
    const char *userIdChar = config_get_string(config, service, "UserID");
    const char *regionChar = config_get_string(config, service, "Region");

    std::string jwtToken = jwtTokenChar ? jwtTokenChar : "";
    std::string openId = openIdChar ? openIdChar : "";
    std::string userId = userIdChar ? userIdChar : "";
    std::string displayName = displayNameChar ? displayNameChar : "";
    std::string region = regionChar ? regionChar : "";

    loginData.jwtAccessToken = QString::fromStdString(jwtToken);
    loginData.userInfo.openID = QString::fromStdString(openId);
    loginData.userInfo.displayName = QString::fromStdString(displayName);
    loginData.userInfo.roomID = config_get_uint(config, service, "RoomID");
    loginData.userInfo.userID = QString::fromStdString(userId);
    loginData.userInfo.region = QString::fromStdString(region);

    return true;
}

bool OneSevenLiveConfigManager::setLoginData(const OneSevenLiveLoginData &loginData) {
    if (!initialized) {
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    // Convert to std::string and maintain reference
    std::string userID = loginData.userInfo.userID.toStdString();
    std::string openID = loginData.userInfo.openID.toStdString();
    std::string displayName = loginData.userInfo.displayName.toStdString();
    std::string jwtToken = loginData.jwtAccessToken.toStdString();
    std::string region = loginData.userInfo.region.toStdString();

    config_set_string(config, service, "UserID", userID.c_str());
    config_set_string(config, service, "OpenID", openID.c_str());
    config_set_string(config, service, "DisplayName", displayName.c_str());
    config_set_string(config, service, "JwtToken", jwtToken.c_str());
    config_set_string(config, service, "Region", region.c_str());
    config_set_uint(config, service, "RoomID", loginData.userInfo.roomID);

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save config");
        return false;
    }

    obs_log(LOG_DEBUG, "Login data saved to config.");

    return true;
}

void OneSevenLiveConfigManager::clearLoginData() {
    if (!initialized) {
        return;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return;
    }

    config_set_string(config, service, "UserID", "");
    config_set_string(config, service, "OpenID", "");
    config_set_string(config, service, "DisplayName", "");
    config_set_string(config, service, "JwtToken", "");
    config_set_uint(config, service, "RoomID", 0);
    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save config");
    }
}

QByteArray OneSevenLiveConfigManager::getDockState() {
    if (!initialized) {
        return QByteArray();
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return QByteArray();
    }

    const char *dockStateChar = config_get_string(config, service, "DockState");
    if (!dockStateChar) {
        return QByteArray();
    }

    std::string dockStateStr = dockStateChar;

    return QByteArray::fromBase64(QString::fromStdString(dockStateStr).toUtf8());
}

bool OneSevenLiveConfigManager::setDockState(const QByteArray &state) {
    if (!initialized) {
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    QString encoded = state.toBase64();

    config_set_string(config, service, "DockState", encoded.toStdString().c_str());
    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save config");
        return false;
    }

    return true;
}

bool OneSevenLiveConfigManager::setStreamingInfo(const std::string &liveStreamID,
                                                 const std::string &streamUrl,
                                                 const std::string &streamKey) {
    if (!initialized) {
        return false;
    }

    if (!config) {
        return false;
    }

    config_set_string(config, service, "LiveStreamID", liveStreamID.c_str());
    config_set_string(config, service, "StreamUrl", streamUrl.c_str());
    config_set_string(config, service, "StreamKey", streamKey.c_str());

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save config");
        return false;
    }

    return true;
}

bool OneSevenLiveConfigManager::getStreamingInfo(std::string &liveStreamID, std::string &streamUrl,
                                                 std::string &streamKey) {
    if (!initialized) {
        return false;
    }

    if (!config) {
        return false;
    }
    const char *liveStreamIDChar = config_get_string(config, service, "LiveStreamID");
    const char *streamUrlChar = config_get_string(config, service, "StreamUrl");
    const char *streamKeyChar = config_get_string(config, service, "StreamKey");
    if (!liveStreamIDChar || !streamUrlChar || !streamKeyChar) {
        return false;
    }
    liveStreamID = liveStreamIDChar;
    streamUrl = streamUrlChar;
    streamKey = streamKeyChar;
    return true;
}

bool OneSevenLiveConfigManager::clearStreamingInfo() {
    return setStreamingInfo("", "", "");
}

void OneSevenLiveConfigManager::setStreamingPullUrl(const std::string &streamPullUrl) {
    if (!initialized) {
        return;
    }
    if (!config) {
        return;
    }
    config_set_string(config, service, "StreamPullUrl", streamPullUrl.c_str());
    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save config");
    }
}

bool OneSevenLiveConfigManager::getStreamingPullUrl(std::string &streamPullUrl) {
    if (!initialized) {
        return false;
    }
    if (!config) {
        return false;
    }
    const char *streamPullUrlChar = config_get_string(config, service, "StreamPullUrl");
    if (!streamPullUrlChar) {
        return false;
    }
    streamPullUrl = streamPullUrlChar;
    return true;
}

void OneSevenLiveConfigManager::clearStreamingPullUrl() {
    if (!initialized) {
        return;
    }
    if (!config) {
        return;
    }
    config_set_string(config, service, "StreamPullUrl", "");
    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save config");
    }
}

bool OneSevenLiveConfigManager::setWhipStreamingInfo(const std::string &liveStreamID,
                                                     const std::string &whipServer,
                                                     const std::string &whipToken) {
    if (!initialized) {
        return false;
    }

    if (!config) {
        return false;
    }

    config_set_string(config, service, "LiveStreamID", liveStreamID.c_str());
    config_set_string(config, service, "WhipServer", whipServer.c_str());
    config_set_string(config, service, "WhipToken", whipToken.c_str());

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save config");
        return false;
    }

    return true;
}

bool OneSevenLiveConfigManager::getWhipStreamingInfo(std::string &liveStreamID,
                                                     std::string &whipServer,
                                                     std::string &whipToken) {
    if (!initialized) {
        return false;
    }

    if (!config) {
        return false;
    }

    const char *liveStreamIDChar = config_get_string(config, service, "LiveStreamID");
    const char *whipServerChar = config_get_string(config, service, "WhipServer");
    const char *whipTokenChar = config_get_string(config, service, "WhipToken");

    if (!liveStreamIDChar || !whipServerChar || !whipTokenChar) {
        return false;
    }

    liveStreamID = liveStreamIDChar;
    whipServer = whipServerChar;
    whipToken = whipTokenChar;
    return true;
}

bool OneSevenLiveConfigManager::clearWhipStreamingInfo() {
    return setWhipStreamingInfo("", "", "");
}

bool OneSevenLiveConfigManager::isWhipMode() {
    if (!initialized || !config) {
        return false;
    }

    const char *whipModeChar = config_get_string(config, service, "WhipMode");
    if (!whipModeChar) {
        return false;
    }

    return std::string(whipModeChar) == "true";
}

void OneSevenLiveConfigManager::setWhipMode(bool isWhip) {
    if (!initialized || !config) {
        return;
    }

    config_set_string(config, service, "WhipMode", isWhip ? "true" : "false");
    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save config");
    }
}

bool OneSevenLiveConfigManager::saveLiveConfig(const OneSevenLiveStreamInfo &streamInfo) {
    obs_log(LOG_INFO, "Saving live config to live_info.json");

    if (!initialized) {
        return false;
    }

    std::vector<OneSevenLiveStreamInfo> streamInfoList;
    loadAllLiveConfig(streamInfoList);

    bool found = false;

    for (auto &info : streamInfoList) {
        if (info.streamUuid == streamInfo.streamUuid) {
            // Replace with new streamInfo
            info = streamInfo;
            found = true;
            break;
        }
    }

    if (!found) {
        streamInfoList.push_back(streamInfo);
    }

    // Save maximum 10 entries
    if (streamInfoList.size() > 10) {
        streamInfoList.erase(streamInfoList.begin());
    }
    saveAllLiveConfig(streamInfoList);
    return true;
}

bool OneSevenLiveConfigManager::loadAllLiveConfig(std::vector<OneSevenLiveStreamInfo> &streamInfo) {
    if (!initialized) {
        return false;
    }

    QString liveListFile = QString::fromStdString(configPath) + "/" + "live_list.json";
    QFile file(liveListFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream in(&file);
    QString jsonString = in.readAll();
    file.close();
    try {
        json jsonData = json::parse(jsonString.toStdString());

        if (!jsonData.is_array()) {
            obs_log(LOG_ERROR, "live_list.json is not an array");
            return false;
        }

        for (const auto &item : jsonData) {
            OneSevenLiveStreamInfo info;
            JsonToOneSevenLiveStreamInfo(item, info);
            streamInfo.push_back(info);
        }

        return true;
    } catch (const json::parse_error &e) {
        obs_log(LOG_ERROR, "Failed to parse live_list.json: %s", e.what());
        return false;
    }
}

bool OneSevenLiveConfigManager::saveAllLiveConfig(
    const std::vector<OneSevenLiveStreamInfo> &streamInfoList) {
    if (!initialized) {
        return false;
    }

    json json_array = json::array();
    for (const auto &item : streamInfoList) {
        json json_item;
        OneSevenLiveStreamInfoToJson(item, json_item);
        json_array.push_back(json_item);
    }
    json json_data = json_array;
    QString liveListFile = QString::fromStdString(configPath) + "/" + "live_list.json";
    QFile file(liveListFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << QString::fromStdString(json_data.dump());
    file.close();
    return true;
}

bool OneSevenLiveConfigManager::removeLiveConfig(const std::string &streamUuid) {
    if (!initialized) {
        return false;
    }
    std::vector<OneSevenLiveStreamInfo> streamInfoList;
    loadAllLiveConfig(streamInfoList);
    for (auto it = streamInfoList.begin(); it != streamInfoList.end(); ++it) {
        if (it->streamUuid.toStdString() == streamUuid) {
            streamInfoList.erase(it);
            break;
        }
    }

    saveAllLiveConfig(streamInfoList);

    return true;
}

bool OneSevenLiveConfigManager::setConfig(const Json &configData) {
    try {
        if (!initialized) {
            return false;
        }

        // Write operation uses exclusive lock
        std::unique_lock<std::shared_mutex> lock(configMutex);

        const std::string configJson = configData.dump();

        const std::string configJsonPath = configPath + "/config_17live.json";
        const QString configJsonPathQt = QString::fromStdString(configJsonPath);

        QFile file(configJsonPathQt);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            obs_log(LOG_ERROR, "Failed to open config file for writing: %s",
                    configJsonPath.c_str());
            return false;
        }

        QByteArray data = QByteArray::fromStdString(configJson);
        qint64 written = file.write(data);
        if (written != data.size()) {
            obs_log(LOG_ERROR, "Failed to write config data to file: %s", configJsonPath.c_str());
            file.close();
            return false;
        }

        file.close();

        OneSevenLiveConfig parsedConfig;
        if (JsonToOneSevenLiveConfig(configData, parsedConfig)) {
            currentConfig = parsedConfig;
        }

        obs_log(LOG_INFO, "Config saved to %s", configJsonPath.c_str());
        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: setConfig exception: %s", e.what());
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: setConfig unknown exception");
        return false;
    }
}

bool OneSevenLiveConfigManager::getConfig(OneSevenLiveConfig &config) {
    if (!initialized) {
        return false;
    }

    // Read operation uses shared lock, allows multiple concurrent read operations
    std::shared_lock<std::shared_mutex> lock(configMutex);

    // Try to read configuration from file
    const std::string configJsonPath = configPath + "/config_17live.json";
    const QString configJsonPathQt = QString::fromStdString(configJsonPath);
    QFile file(configJsonPathQt);

    if (!file.exists()) {
        // If file doesn't exist, return current configuration in memory
        config = currentConfig;
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        obs_log(LOG_ERROR, "Failed to open config file for reading");
        return false;
    }

    const QByteArray jsonData = file.readAll();
    file.close();

    if (jsonData.isEmpty()) {
        // If file is empty, return current configuration in memory
        config = currentConfig;
        return true;
    }

    // Parse JSON data - convert once to std::string
    const std::string jsonDataStr = jsonData.toStdString();

    try {
        json jsonObj = json::parse(jsonDataStr);

        // Convert JSON to OneSevenLiveConfig structure
        if (!JsonToOneSevenLiveConfig(jsonObj, config)) {
            obs_log(LOG_ERROR, "Failed to convert JSON to config");
            return false;
        }

        // Update current configuration
        currentConfig = config;

        return true;
    } catch (const json::parse_error &e) {
        obs_log(LOG_ERROR, "Failed to parse config JSON: %s", e.what());
        return false;
    }
}

bool OneSevenLiveConfigManager::saveGifts(const Json &gifts) {
    try {
        if (!initialized) {
            return false;
        }

        QString giftsFile = QString::fromStdString(configPath) + "/" + "gifts.json";
        QFile file(giftsFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            obs_log(LOG_ERROR, "Failed to open gifts.json for writing");
            return false;
        }
        QTextStream out(&file);
        out << QString::fromStdString(gifts.dump());
        file.close();
        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: saveGifts exception: %s", e.what());
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: saveGifts unknown exception");
        return false;
    }
}

bool OneSevenLiveConfigManager::loadGifts(Json &gifts) {
    try {
        if (!initialized) {
            return false;
        }

        QString giftsFile = QString::fromStdString(configPath) + "/" + "gifts.json";
        QFile file(giftsFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            // File doesn't exist, return empty object
            gifts = json::object();
            return true;
        }
        QTextStream in(&file);
        QString jsonString = in.readAll();
        file.close();

        try {
            gifts = json::parse(jsonString.toStdString());
        } catch (const json::parse_error &e) {
            obs_log(LOG_ERROR, "Failed to parse gifts.json: %s", e.what());
            return false;
        }

        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: loadGifts exception: %s", e.what());
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: loadGifts unknown exception");
        return false;
    }
}

bool OneSevenLiveConfigManager::setTwitchTokens(const QString &accessToken,
                                                qint64 fetchedAtEpochSec) {
    if (!initialized) {
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    // Convert to std::string and maintain reference
    std::string accessTokenStr = accessToken.toStdString();
    std::string fetchedStr = std::to_string(static_cast<long long>(fetchedAtEpochSec));

    config_set_string(config, service, "TwitchAccessToken", accessTokenStr.c_str());
    config_set_string(config, service, "TwitchAccessTokenFetchedAt", fetchedStr.c_str());

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save Twitch access token");
        return false;
    }

    obs_log(LOG_INFO, "Twitch access token saved successfully");
    return true;
}

bool OneSevenLiveConfigManager::getTwitchTokens(QString &accessToken, qint64 &fetchedAtEpochSec) {
    if (!initialized) {
        return false;
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    const char *accessTokenChar = config_get_string(config, service, "TwitchAccessToken");
    const char *fetchedChar = config_get_string(config, service, "TwitchAccessTokenFetchedAt");

    if (!accessTokenChar) {
        return false;
    }

    accessToken = QString::fromUtf8(accessTokenChar);
    if (fetchedChar) {
        try {
            fetchedAtEpochSec = static_cast<qint64>(std::stoll(fetchedChar));
        } catch (...) {
            fetchedAtEpochSec = 0;
        }
    } else {
        fetchedAtEpochSec = 0;
    }

    return true;
}

bool OneSevenLiveConfigManager::clearTwitchTokens() {
    if (!initialized) {
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    config_set_string(config, service, "TwitchAccessToken", "");
    config_set_string(config, service, "TwitchAccessTokenFetchedAt", "");

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to clear Twitch tokens");
        return false;
    }

    obs_log(LOG_INFO, "Twitch access token cleared successfully");
    return true;
}

bool OneSevenLiveConfigManager::setYouTubeAccessToken(const QString &accessToken, int expiresInSec,
                                                      qint64 fetchedAtEpochSec) {
    if (!initialized) {
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    std::string accessTokenStr = accessToken.toStdString();
    std::string fetchedStr = std::to_string(static_cast<long long>(fetchedAtEpochSec));
    std::string expiresStr = std::to_string(static_cast<long long>(expiresInSec));

    config_set_string(config, service, "YouTubeAccessToken", accessTokenStr.c_str());
    config_set_string(config, service, "YouTubeAccessTokenFetchedAt", fetchedStr.c_str());
    config_set_string(config, service, "YouTubeAccessTokenExpiresIn", expiresStr.c_str());

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save YouTube access token");
        return false;
    }

    {
        QString tok = QString::fromUtf8(accessTokenStr.c_str());
        QString masked = tok.length() >= 12 ? tok.left(6) + "..." + tok.right(6) : tok;
        obs_log(LOG_INFO, "YouTube access token saved successfully at %s token(masked)=%s",
                configPath.c_str(), masked.toUtf8().constData());
    }
    return true;
}

bool OneSevenLiveConfigManager::getYouTubeAccessToken(QString &accessToken, int &expiresInSec,
                                                      qint64 &fetchedAtEpochSec) {
    if (!initialized) {
        return false;
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    const char *accessTokenChar = config_get_string(config, service, "YouTubeAccessToken");
    const char *fetchedChar = config_get_string(config, service, "YouTubeAccessTokenFetchedAt");
    const char *expiresChar = config_get_string(config, service, "YouTubeAccessTokenExpiresIn");

    if (!accessTokenChar) {
        return false;
    }

    accessToken = QString::fromUtf8(accessTokenChar);

    if (fetchedChar) {
        try {
            fetchedAtEpochSec = static_cast<qint64>(std::stoll(fetchedChar));
        } catch (...) {
            fetchedAtEpochSec = 0;
        }
    } else {
        fetchedAtEpochSec = 0;
    }

    if (expiresChar) {
        try {
            expiresInSec = static_cast<int>(std::stoi(expiresChar));
        } catch (...) {
            expiresInSec = 0;
        }
    } else {
        expiresInSec = 0;
    }

    return true;
}

bool OneSevenLiveConfigManager::clearYouTubeAccessToken() {
    if (!initialized) {
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    config_set_string(config, service, "YouTubeAccessToken", "");
    config_set_string(config, service, "YouTubeAccessTokenFetchedAt", "");
    config_set_string(config, service, "YouTubeAccessTokenExpiresIn", "");

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to clear YouTube access token");
        return false;
    }

    obs_log(LOG_INFO, "YouTube access token cleared successfully");
    return true;
}

bool OneSevenLiveConfigManager::getYouTubeRefreshToken(QString &refreshToken, int &expiresInSec,
                                                       qint64 &fetchedAtEpochSec) {
    if (!initialized) {
        return false;
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    const char *refreshTokenChar = config_get_string(config, service, "YouTubeRefreshToken");
    const char *fetchedChar = config_get_string(config, service, "YouTubeRefreshTokenFetchedAt");
    const char *expiresChar = config_get_string(config, service, "YouTubeRefreshTokenExpiresIn");
    if (!refreshTokenChar) {
        return false;
    }

    refreshToken = QString::fromUtf8(refreshTokenChar);

    if (fetchedChar) {
        try {
            fetchedAtEpochSec = static_cast<qint64>(std::stoll(fetchedChar));
        } catch (...) {
            fetchedAtEpochSec = 0;
        }
    } else {
        fetchedAtEpochSec = 0;
    }

    if (expiresChar) {
        try {
            expiresInSec = static_cast<int>(std::stoi(expiresChar));
        } catch (...) {
            expiresInSec = 0;
        }
    } else {
        expiresInSec = 0;
    }
    return true;
}

bool OneSevenLiveConfigManager::clearYouTubeRefreshToken() {
    if (!initialized) {
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    config_set_string(config, service, "YouTubeRefreshToken", "");
    config_set_string(config, service, "YouTubeRefreshTokenFetchedAt", "");
    config_set_string(config, service, "YouTubeRefreshTokenExpiresIn", "");

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to clear YouTube refresh token");
        return false;
    }

    obs_log(LOG_INFO, "YouTube refresh token cleared successfully");
    return true;
}

bool OneSevenLiveConfigManager::setTwitchUserInfo(const QString &userId, const QString &login,
                                                  const QString &displayName,
                                                  const QString &profileImageUrl,
                                                  const QString &email, int viewCount) {
    if (!initialized) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    std::string userIdStr = userId.toStdString();
    std::string loginStr = login.toStdString();
    std::string displayNameStr = displayName.toStdString();
    std::string profileImageUrlStr = profileImageUrl.toStdString();
    std::string emailStr = email.toStdString();

    config_set_string(config, service, "TwitchUserId", userIdStr.c_str());
    config_set_string(config, service, "TwitchLogin", loginStr.c_str());
    config_set_string(config, service, "TwitchDisplayName", displayNameStr.c_str());
    config_set_string(config, service, "TwitchProfileImageUrl", profileImageUrlStr.c_str());
    config_set_string(config, service, "TwitchEmail", emailStr.c_str());
    config_set_int(config, service, "TwitchViewCount", viewCount);

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save Twitch user info config");
        return false;
    }

    obs_log(LOG_INFO, "Twitch user info saved - User ID: %s, Login: %s", userIdStr.c_str(),
            loginStr.c_str());
    return true;
}

bool OneSevenLiveConfigManager::getTwitchUserInfo(QString &userId, QString &login,
                                                  QString &displayName, QString &profileImageUrl,
                                                  QString &email, int &viewCount) {
    if (!initialized) {
        return false;
    }

    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    const char *userIdChar = config_get_string(config, service, "TwitchUserId");
    const char *loginChar = config_get_string(config, service, "TwitchLogin");
    const char *displayNameChar = config_get_string(config, service, "TwitchDisplayName");
    const char *profileImageUrlChar = config_get_string(config, service, "TwitchProfileImageUrl");
    const char *emailChar = config_get_string(config, service, "TwitchEmail");

    if (!userIdChar || !loginChar) {
        return false;  // Required fields missing
    }

    userId = QString::fromUtf8(userIdChar);
    login = QString::fromUtf8(loginChar);
    displayName = displayNameChar ? QString::fromUtf8(displayNameChar) : "";
    profileImageUrl = profileImageUrlChar ? QString::fromUtf8(profileImageUrlChar) : "";
    email = emailChar ? QString::fromUtf8(emailChar) : "";
    viewCount = config_get_int(config, service, "TwitchViewCount");

    return true;
}

bool OneSevenLiveConfigManager::clearTwitchUserInfo() {
    if (!initialized) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    config_set_string(config, service, "TwitchUserId", "");
    config_set_string(config, service, "TwitchLogin", "");
    config_set_string(config, service, "TwitchDisplayName", "");
    config_set_string(config, service, "TwitchProfileImageUrl", "");
    config_set_string(config, service, "TwitchEmail", "");
    config_set_int(config, service, "TwitchViewCount", 0);

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save config");
        return false;
    }

    return true;
}

bool OneSevenLiveConfigManager::setYouTubeRefreshToken(const QString &refreshToken,
                                                       int expiresInSec, qint64 fetchedAtEpochSec) {
    if (!initialized) {
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    std::string refreshTokenStr = refreshToken.toStdString();
    std::string fetchedStr = std::to_string(static_cast<long long>(fetchedAtEpochSec));
    std::string expiresStr = std::to_string(static_cast<long long>(expiresInSec));

    config_set_string(config, service, "YouTubeRefreshToken", refreshTokenStr.c_str());
    config_set_string(config, service, "YouTubeRefreshTokenFetchedAt", fetchedStr.c_str());
    config_set_string(config, service, "YouTubeRefreshTokenExpiresIn", expiresStr.c_str());

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save YouTube refresh token");
        return false;
    }

    obs_log(LOG_INFO, "YouTube refresh token saved successfully");
    return true;
}

bool OneSevenLiveConfigManager::setYouTubeBroadcastInfo(const QString &broadcastId,
                                                        const QString &liveChatId) {
    if (!initialized) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    std::string bid = broadcastId.toStdString();
    std::string chatId = liveChatId.toStdString();

    config_set_string(config, service, "YouTubeBroadcastId", bid.c_str());
    config_set_string(config, service, "YouTubeLiveChatId", chatId.c_str());

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save YouTube broadcast info");
        return false;
    }

    obs_log(LOG_INFO, "YouTube broadcast info saved");
    return true;
}

bool OneSevenLiveConfigManager::getYouTubeBroadcastInfo(QString &broadcastId,
                                                        QString &liveChatId) {
    if (!initialized) {
        return false;
    }

    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    const char *bid = config_get_string(config, service, "YouTubeBroadcastId");
    const char *chatId = config_get_string(config, service, "YouTubeLiveChatId");

    if (!bid || !chatId) {
        return false;
    }

    broadcastId = QString::fromUtf8(bid);
    liveChatId = QString::fromUtf8(chatId);
    return true;
}

bool OneSevenLiveConfigManager::clearYouTubeBroadcastInfo() {
    if (!initialized) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        return false;
    }

    config_set_string(config, service, "YouTubeBroadcastId", "");
    config_set_string(config, service, "YouTubeLiveChatId", "");

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to clear YouTube broadcast info");
        return false;
    }

    obs_log(LOG_INFO, "YouTube broadcast info cleared");
    return true;
}

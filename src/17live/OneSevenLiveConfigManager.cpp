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

bool OneSevenLiveConfigManager::getConfigValue(const std::string &key, std::string &value) {
    if (!initialized) {
        return false;
    }

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

bool OneSevenLiveConfigManager::getLoginData(OneSevenLiveLoginData &loginData) {
    if (!initialized) {
        return false;
    }

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

    if (!config) {
        return QByteArray();
    }

    const char *dockStateChar = config_get_string(config, service, "DockState");
    if (!dockStateChar) {
        return QByteArray();
    }

    return QByteArray(dockStateChar);
}

bool OneSevenLiveConfigManager::setDockState(const QByteArray &state) {
    if (!initialized) {
        return false;
    }
    if (!config) {
        return false;
    }

    config_set_string(config, service, "DockState", state.toStdString().c_str());
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
    std::string error;
    Json json = Json::parse(jsonString.toStdString(), error);
    if (!error.empty()) {
        obs_log(LOG_ERROR, "Failed to parse live_list.json: %s", error.c_str());
        return false;
    }

    if (!json.is_array()) {
        obs_log(LOG_ERROR, "live_list.json is not an array");
        return false;
    }

    for (const auto &item : json.array_items()) {
        OneSevenLiveStreamInfo info;
        JsonToOneSevenLiveStreamInfo(item, info);
        streamInfo.push_back(info);
    }

    return true;
}

bool OneSevenLiveConfigManager::saveAllLiveConfig(
    const std::vector<OneSevenLiveStreamInfo> &streamInfoList) {
    if (!initialized) {
        return false;
    }

    std::vector<Json> json_array = Json::array();
    for (const auto &item : streamInfoList) {
        Json json_item;
        OneSevenLiveStreamInfoToJson(item, json_item);
        json_array.push_back(json_item);
    }
    Json json_data = Json(json_array);
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
    if (!initialized) {
        return false;
    }

    std::lock_guard<std::mutex> lock(configMutex);

    std::string configJson = configData.dump();

    // Save to configuration file
    std::string configJsonPath = configPath + "/config_17live.json";
    std::ofstream file(configJsonPath);
    if (!file.is_open()) {
        obs_log(LOG_ERROR, "Failed to open config file for writing");
        return false;
    }

    file << configJson;
    file.close();

    obs_log(LOG_INFO, "Config saved to %s", configJsonPath.c_str());
    return true;
}

bool OneSevenLiveConfigManager::getConfig(OneSevenLiveConfig &config) {
    if (!initialized) {
        return false;
    }

    std::lock_guard<std::mutex> lock(configMutex);

    // Try to read configuration from file
    std::string configJsonPath = configPath + "/config_17live.json";
    QFile file(QString::fromStdString(configJsonPath));

    if (!file.exists()) {
        // If file doesn't exist, return current configuration in memory
        config = currentConfig;
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        obs_log(LOG_ERROR, "Failed to open config file for reading");
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    if (jsonData.isEmpty()) {
        // If file is empty, return current configuration in memory
        config = currentConfig;
        return true;
    }

    // Parse JSON data
    std::string err;
    Json jsonObj = Json::parse(jsonData.toStdString(), err);

    if (!err.empty()) {
        obs_log(LOG_ERROR, "Failed to parse config JSON: %s", err.c_str());
        return false;
    }

    // Convert JSON to OneSevenLiveConfig structure
    if (!JsonToOneSevenLiveConfig(jsonObj, config)) {
        obs_log(LOG_ERROR, "Failed to convert JSON to config");
        return false;
    }

    // Update current configuration
    currentConfig = config;

    return true;
}

bool OneSevenLiveConfigManager::saveGifts(const Json &gifts) {
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
}

bool OneSevenLiveConfigManager::loadGifts(Json &gifts) {
    if (!initialized) {
        return false;
    }

    QString giftsFile = QString::fromStdString(configPath) + "/" + "gifts.json";
    QFile file(giftsFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // File doesn't exist, return empty object
        gifts = Json::object();
        return true;
    }
    QTextStream in(&file);
    QString jsonString = in.readAll();
    file.close();

    std::string error;
    gifts = Json::parse(jsonString.toStdString(), error);
    if (!error.empty()) {
        obs_log(LOG_ERROR, "Failed to parse gifts.json: %s", error.c_str());
        return false;
    }

    return true;
}

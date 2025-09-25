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

        // Save to configuration file
        const std::string configJsonPath = configPath + "/config_17live.json";
        std::ofstream file(configJsonPath);
        if (!file.is_open()) {
            obs_log(LOG_ERROR, "Failed to open config file for writing: %s",
                    configJsonPath.c_str());
            return false;
        }

        file << configJson;

        // Check if write operation was successful
        if (file.fail()) {
            obs_log(LOG_ERROR, "Failed to write config data to file: %s", configJsonPath.c_str());
            file.close();
            return false;
        }

        file.close();

        // Verify file was closed successfully
        if (file.fail()) {
            obs_log(LOG_ERROR, "Failed to close config file: %s", configJsonPath.c_str());
            return false;
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

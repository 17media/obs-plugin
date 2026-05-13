#include "OneSevenLiveConfigManager.hpp"

#include <obs-module.h>
#include <util/config-file.h>

#include <QDir>
#include <QFile>
#include <QString>
#include <fstream>
#include <unordered_set>

#include "api/OneSevenLiveApiWrappers.hpp"
#include "plugin-support.h"

const char *service = "OneSevenLive";

#define CONFIG_PATH ".17Live"
#define CONFIG_NAME "config.ini"

OneSevenLiveConfigManager::OneSevenLiveConfigManager() : initialized(false) {}

ResultError OneSevenLiveConfigManager::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void OneSevenLiveConfigManager::setLastError(ResultError error) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = std::move(error);
}

void OneSevenLiveConfigManager::clearLastError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = ResultError{};
}

bool OneSevenLiveConfigManager::initialize() {
    clearLastError();
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
            setLastError(ResultError{"IO.CreateDirFailed", "Failed to create config directory",
                                     false, configDir.toStdString()});
            return false;
        }
    }

    // Configuration file path
    QString configFilePath = configDir + "/" + CONFIG_NAME;

    configPath = configDir.toStdString();

    int ret = config_open(&config, configFilePath.toStdString().c_str(), CONFIG_OPEN_ALWAYS);
    if (ret != CONFIG_SUCCESS) {
        obs_log(LOG_ERROR, "Failed to open config file");
        setLastError(ResultError{"IO.OpenFailed", "Failed to open config file", false,
                                 configFilePath.toStdString()});
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
        setLastError(
            ResultError{"State.NotInitialized", "Config manager not initialized", false, key});
        return false;
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false, key});
        return false;
    }

    const char *valueChar = config_get_string(config, service, key.c_str());
    if (!valueChar) {
        setLastError(ResultError{"Config.KeyMissing", "Config key not found", false, key});
        return false;
    }
    value = valueChar;
    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::setConfigValue(const std::string &key, const std::string &value) {
    if (!initialized) {
        setLastError(
            ResultError{"State.NotInitialized", "Config manager not initialized", false, key});
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false, key});
        return false;
    }

    config_set_string(config, service, key.c_str(), value.c_str());
    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save config value: %s", key.c_str());
        setLastError(ResultError{"IO.SaveFailed", "Failed to save config value", false, key});
        return false;
    }

    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::getBoolValue(const std::string &key, bool defaultValue) {
    if (!initialized) {
        return defaultValue;
    }

    std::shared_lock<std::shared_mutex> lock(configMutex);
    if (!config) {
        return defaultValue;
    }

    const char *valueChar = config_get_string(config, service, key.c_str());
    if (!valueChar) {
        return defaultValue;
    }

    const std::string v = valueChar;
    return v == "true" || v == "1" || v == "TRUE" || v == "True";
}

bool OneSevenLiveConfigManager::setBoolValue(const std::string &key, bool value) {
    return setConfigValue(key, value ? "true" : "false");
}

std::vector<std::string> OneSevenLiveConfigManager::getCrashUploadHistory() {
    if (!initialized) {
        return {};
    }

    std::shared_lock<std::shared_mutex> lock(configMutex);
    if (!config) {
        return {};
    }

    const char *valueChar = config_get_string(config, service, "CrashUploadHistory");
    if (!valueChar) {
        return {};
    }

    try {
        json j = json::parse(valueChar);
        if (!j.is_array()) {
            return {};
        }
        std::vector<std::string> out;
        out.reserve(j.size());
        for (const auto &it : j) {
            if (it.is_string()) {
                out.push_back(it.get<std::string>());
            }
        }
        return out;
    } catch (...) {
        return {};
    }
}

bool OneSevenLiveConfigManager::addCrashUploadHistory(const std::vector<std::string> &keys) {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "addCrashUploadHistory"});
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(configMutex);
    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "addCrashUploadHistory"});
        return false;
    }

    std::unordered_set<std::string> uniq;
    json arr = json::array();

    const char *valueChar = config_get_string(config, service, "CrashUploadHistory");
    if (valueChar) {
        try {
            json existing = json::parse(valueChar);
            if (existing.is_array()) {
                for (const auto &it : existing) {
                    if (it.is_string()) {
                        const std::string s = it.get<std::string>();
                        if (uniq.insert(s).second) {
                            arr.push_back(s);
                        }
                    }
                }
            }
        } catch (...) {
        }
    }

    for (const auto &k : keys) {
        if (k.empty())
            continue;
        if (uniq.insert(k).second) {
            arr.push_back(k);
        }
    }

    const std::string jsonStr = arr.dump();
    config_set_string(config, service, "CrashUploadHistory", jsonStr.c_str());
    if (config_save(config) < 0) {
        setLastError(
            ResultError{"IO.SaveFailed", "Failed to save CrashUploadHistory", false, jsonStr});
        return false;
    }

    clearLastError();
    return true;
}

json OneSevenLiveConfigManager::getCustomizedCartoonsConfig() {
    if (!initialized) {
        return json::object();
    }

    std::shared_lock<std::shared_mutex> lock(configMutex);
    if (!config) {
        return json::object();
    }

    const char *valueChar = config_get_string(config, service, "CustomizedCartoonsConfigV1");
    if (!valueChar) {
        return json::object();
    }

    try {
        json j = json::parse(valueChar);
        if (!j.is_object()) {
            return json::object();
        }
        return j;
    } catch (...) {
        return json::object();
    }
}

bool OneSevenLiveConfigManager::setCustomizedCartoonsConfig(const json &cfg) {
    if (!initialized) {
        setLastError(
            ResultError{"State.NotInitialized", "Config manager not initialized", false,
                        "setCustomizedCartoonsConfig"});
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(configMutex);
    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "setCustomizedCartoonsConfig"});
        return false;
    }

    const std::string jsonStr = cfg.dump();
    config_set_string(config, service, "CustomizedCartoonsConfigV1", jsonStr.c_str());
    if (config_save(config) < 0) {
        setLastError(ResultError{"IO.SaveFailed", "Failed to save CustomizedCartoonsConfigV1", false,
                                 jsonStr});
        return false;
    }

    clearLastError();
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
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "saveLiveConfig"});
        return false;
    }

    std::vector<OneSevenLiveStreamInfo> streamInfoList;
    if (!loadAllLiveConfig(streamInfoList)) {
        return false;
    }

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
    return saveAllLiveConfig(streamInfoList);
}

bool OneSevenLiveConfigManager::loadAllLiveConfig(std::vector<OneSevenLiveStreamInfo> &streamInfo) {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "loadAllLiveConfig"});
        return false;
    }

    QString liveListFile = QString::fromStdString(configPath) + "/" + "live_list.json";
    QFile file(liveListFile);
    if (!file.exists()) {
        clearLastError();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setLastError(ResultError{"IO.OpenFailed", "Failed to open live list file for reading",
                                 false, liveListFile.toStdString()});
        return false;
    }
    QTextStream in(&file);
    QString jsonString = in.readAll();
    file.close();
    try {
        json jsonData = json::parse(jsonString.toStdString());

        if (!jsonData.is_array()) {
            obs_log(LOG_ERROR, "live_list.json is not an array");
            setLastError(ResultError{"Json.InvalidType", "live_list.json is not an array", false,
                                     liveListFile.toStdString()});
            return false;
        }

        for (const auto &item : jsonData) {
            OneSevenLiveStreamInfo info;
            JsonToOneSevenLiveStreamInfo(item, info);
            streamInfo.push_back(info);
        }

        clearLastError();
        return true;
    } catch (const json::parse_error &e) {
        obs_log(LOG_ERROR, "Failed to parse live_list.json: %s", e.what());
        setLastError(
            ResultError{"Json.ParseFailed", "Failed to parse live_list.json", false, e.what()});
        return false;
    }
}

bool OneSevenLiveConfigManager::saveAllLiveConfig(
    const std::vector<OneSevenLiveStreamInfo> &streamInfoList) {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "saveAllLiveConfig"});
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
        setLastError(ResultError{"IO.OpenFailed", "Failed to open live list file for writing",
                                 false, liveListFile.toStdString()});
        return false;
    }
    QTextStream out(&file);
    out << QString::fromStdString(json_data.dump());
    file.close();
    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::removeLiveConfig(const std::string &streamUuid) {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "removeLiveConfig"});
        return false;
    }
    std::vector<OneSevenLiveStreamInfo> streamInfoList;
    if (!loadAllLiveConfig(streamInfoList)) {
        return false;
    }
    for (auto it = streamInfoList.begin(); it != streamInfoList.end(); ++it) {
        if (it->streamUuid.toStdString() == streamUuid) {
            streamInfoList.erase(it);
            break;
        }
    }

    return saveAllLiveConfig(streamInfoList);
}

bool OneSevenLiveConfigManager::setConfig(const Json &configData) {
    try {
        if (!initialized) {
            setLastError(ResultError{"State.NotInitialized", "Config manager not initialized",
                                     false, "setConfig"});
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
            setLastError(ResultError{"IO.OpenFailed", "Failed to open config file for writing",
                                     false, configJsonPath});
            return false;
        }

        QByteArray data = QByteArray::fromStdString(configJson);
        qint64 written = file.write(data);
        if (written != data.size()) {
            obs_log(LOG_ERROR, "Failed to write config data to file: %s", configJsonPath.c_str());
            file.close();
            setLastError(ResultError{"IO.WriteFailed", "Failed to write config data to file", false,
                                     configJsonPath});
            return false;
        }

        file.close();

        OneSevenLiveConfig parsedConfig;
        if (JsonToOneSevenLiveConfig(configData, parsedConfig)) {
            currentConfig = parsedConfig;
        } else {
            setLastError(ResultError{"State.MappingFailed", "Failed to map config json to struct",
                                     false, configJsonPath});
        }

        obs_log(LOG_INFO, "Config saved to %s", configJsonPath.c_str());
        clearLastError();
        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: setConfig exception: %s", e.what());
        setLastError(ResultError{"State.Exception", "setConfig exception", false, e.what()});
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: setConfig unknown exception");
        setLastError(ResultError{"State.Exception", "setConfig unknown exception", false, ""});
        return false;
    }
}

bool OneSevenLiveConfigManager::getConfig(OneSevenLiveConfig &config) {
    if (!initialized) {
        setLastError(
            ResultError{"State.NotInitialized", "Config manager not initialized", false, "getConfig"});
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
        clearLastError();
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        obs_log(LOG_ERROR, "Failed to open config file for reading");
        setLastError(ResultError{"IO.OpenFailed", "Failed to open config file for reading", false,
                                 configJsonPath});
        return false;
    }

    const QByteArray jsonData = file.readAll();
    file.close();

    if (jsonData.isEmpty()) {
        // If file is empty, return current configuration in memory
        config = currentConfig;
        clearLastError();
        return true;
    }

    // Parse JSON data - convert once to std::string
    const std::string jsonDataStr = jsonData.toStdString();

    try {
        json jsonObj = json::parse(jsonDataStr);

        // Convert JSON to OneSevenLiveConfig structure
        if (!JsonToOneSevenLiveConfig(jsonObj, config)) {
            obs_log(LOG_ERROR, "Failed to convert JSON to config");
            setLastError(ResultError{"State.MappingFailed", "Failed to convert JSON to config",
                                     false, configJsonPath});
            return false;
        }

        // Update current configuration
        currentConfig = config;

        clearLastError();
        return true;
    } catch (const json::parse_error &e) {
        obs_log(LOG_ERROR, "Failed to parse config JSON: %s", e.what());
        setLastError(
            ResultError{"Json.ParseFailed", "Failed to parse config JSON", false, e.what()});
        return false;
    }
}

bool OneSevenLiveConfigManager::saveGifts(const Json &gifts) {
    try {
        if (!initialized) {
            setLastError(ResultError{"State.NotInitialized", "Config manager not initialized",
                                     false, "saveGifts"});
            return false;
        }

        QString giftsFile = QString::fromStdString(configPath) + "/" + "gifts.json";
        QFile file(giftsFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            obs_log(LOG_ERROR, "Failed to open gifts.json for writing");
            setLastError(ResultError{"IO.OpenFailed", "Failed to open gifts.json for writing",
                                     false, giftsFile.toStdString()});
            return false;
        }
        QTextStream out(&file);
        out << QString::fromStdString(gifts.dump());
        file.close();
        clearLastError();
        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: saveGifts exception: %s", e.what());
        setLastError(ResultError{"State.Exception", "saveGifts exception", false, e.what()});
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: saveGifts unknown exception");
        setLastError(ResultError{"State.Exception", "saveGifts unknown exception", false, ""});
        return false;
    }
}

bool OneSevenLiveConfigManager::loadGifts(Json &gifts) {
    try {
        if (!initialized) {
            setLastError(ResultError{"State.NotInitialized", "Config manager not initialized",
                                     false, "loadGifts"});
            return false;
        }

        QString giftsFile = QString::fromStdString(configPath) + "/" + "gifts.json";
        QFile file(giftsFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            // File doesn't exist, return empty object
            gifts = json::object();
            clearLastError();
            return true;
        }
        QTextStream in(&file);
        QString jsonString = in.readAll();
        file.close();

        try {
            gifts = json::parse(jsonString.toStdString());
        } catch (const json::parse_error &e) {
            obs_log(LOG_ERROR, "Failed to parse gifts.json: %s", e.what());
            setLastError(ResultError{"Json.ParseFailed", "Failed to parse gifts.json", false,
                                     e.what()});
            return false;
        }

        clearLastError();
        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: loadGifts exception: %s", e.what());
        setLastError(ResultError{"State.Exception", "loadGifts exception", false, e.what()});
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: loadGifts unknown exception");
        setLastError(ResultError{"State.Exception", "loadGifts unknown exception", false, ""});
        return false;
    }
}

bool OneSevenLiveConfigManager::saveEnterAnimationFiles(const Json &files) {
    try {
        if (!initialized) {
            setLastError(ResultError{"State.NotInitialized", "Config manager not initialized",
                                     false, "saveEnterAnimationFiles"});
            return false;
        }

        QString filePath = QString::fromStdString(configPath) + "/" + "enter_animation_files.json";
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            obs_log(LOG_ERROR, "Failed to open enter_animation_files.json for writing");
            setLastError(ResultError{"IO.OpenFailed",
                                     "Failed to open enter_animation_files.json for writing", false,
                                     filePath.toStdString()});
            return false;
        }
        QTextStream out(&file);
        out << QString::fromStdString(files.dump());
        file.close();
        clearLastError();
        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: saveEnterAnimationFiles exception: %s", e.what());
        setLastError(
            ResultError{"State.Exception", "saveEnterAnimationFiles exception", false, e.what()});
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: saveEnterAnimationFiles unknown exception");
        setLastError(ResultError{"State.Exception", "saveEnterAnimationFiles unknown exception",
                                 false, ""});
        return false;
    }
}

bool OneSevenLiveConfigManager::loadEnterAnimationFiles(Json &files) {
    try {
        if (!initialized) {
            setLastError(ResultError{"State.NotInitialized", "Config manager not initialized",
                                     false, "loadEnterAnimationFiles"});
            return false;
        }

        QString filePath = QString::fromStdString(configPath) + "/" + "enter_animation_files.json";
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            files = json::object();
            clearLastError();
            return true;
        }
        QTextStream in(&file);
        QString jsonString = in.readAll();
        file.close();

        try {
            files = json::parse(jsonString.toStdString());
        } catch (const json::parse_error &e) {
            obs_log(LOG_ERROR, "Failed to parse enter_animation_files.json: %s", e.what());
            setLastError(ResultError{"Json.ParseFailed",
                                     "Failed to parse enter_animation_files.json", false,
                                     e.what()});
            return false;
        }

        clearLastError();
        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: loadEnterAnimationFiles exception: %s", e.what());
        setLastError(
            ResultError{"State.Exception", "loadEnterAnimationFiles exception", false, e.what()});
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: loadEnterAnimationFiles unknown exception");
        setLastError(ResultError{"State.Exception", "loadEnterAnimationFiles unknown exception",
                                 false, ""});
        return false;
    }
}

bool OneSevenLiveConfigManager::saveI18nConfig(const Json &i18nConfig) {
    try {
        if (!initialized) {
            setLastError(ResultError{"State.NotInitialized", "Config manager not initialized",
                                     false, "saveI18nConfig"});
            return false;
        }

        QString filePath = QString::fromStdString(configPath) + "/" + "i18n_config.json";
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            obs_log(LOG_ERROR, "Failed to open i18n_config.json for writing");
            setLastError(ResultError{"IO.OpenFailed", "Failed to open i18n_config.json for writing",
                                     false, filePath.toStdString()});
            return false;
        }
        QTextStream out(&file);
        out << QString::fromStdString(i18nConfig.dump());
        file.close();
        clearLastError();
        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: saveI18nConfig exception: %s", e.what());
        setLastError(ResultError{"State.Exception", "saveI18nConfig exception", false, e.what()});
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: saveI18nConfig unknown exception");
        setLastError(ResultError{"State.Exception", "saveI18nConfig unknown exception", false, ""});
        return false;
    }
}

bool OneSevenLiveConfigManager::loadI18nConfig(Json &i18nConfig) {
    try {
        if (!initialized) {
            setLastError(ResultError{"State.NotInitialized", "Config manager not initialized",
                                     false, "loadI18nConfig"});
            return false;
        }

        QString filePath = QString::fromStdString(configPath) + "/" + "i18n_config.json";
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            i18nConfig = json::object();
            clearLastError();
            return true;
        }
        QTextStream in(&file);
        QString jsonString = in.readAll();
        file.close();

        try {
            i18nConfig = json::parse(jsonString.toStdString());
        } catch (const json::parse_error &e) {
            obs_log(LOG_ERROR, "Failed to parse i18n_config.json: %s", e.what());
            setLastError(ResultError{"Json.ParseFailed", "Failed to parse i18n_config.json", false,
                                     e.what()});
            return false;
        }

        clearLastError();
        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: loadI18nConfig exception: %s", e.what());
        setLastError(ResultError{"State.Exception", "loadI18nConfig exception", false, e.what()});
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: loadI18nConfig unknown exception");
        setLastError(ResultError{"State.Exception", "loadI18nConfig unknown exception", false, ""});
        return false;
    }
}

bool OneSevenLiveConfigManager::setTwitchTokens(const QString &accessToken,
                                                qint64 fetchedAtEpochSec) {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "setTwitchTokens"});
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(
            ResultError{"State.InvalidState", "Config handle not available", false, "setTwitchTokens"});
        return false;
    }

    // Convert to std::string and maintain reference
    std::string accessTokenStr = accessToken.toStdString();
    std::string fetchedStr = std::to_string(static_cast<long long>(fetchedAtEpochSec));

    config_set_string(config, service, "TwitchAccessToken", accessTokenStr.c_str());
    config_set_string(config, service, "TwitchAccessTokenFetchedAt", fetchedStr.c_str());

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save Twitch access token");
        setLastError(ResultError{"IO.WriteFailed", "Failed to save Twitch access token", false, ""});
        return false;
    }

    obs_log(LOG_INFO, "Twitch access token saved successfully");
    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::getTwitchTokens(QString &accessToken, qint64 &fetchedAtEpochSec) {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "getTwitchTokens"});
        return false;
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(
            ResultError{"State.InvalidState", "Config handle not available", false, "getTwitchTokens"});
        return false;
    }

    const char *accessTokenChar = config_get_string(config, service, "TwitchAccessToken");
    const char *fetchedChar = config_get_string(config, service, "TwitchAccessTokenFetchedAt");

    if (!accessTokenChar) {
        setLastError(
            ResultError{"Config.KeyMissing", "Twitch token not found", false, "TwitchAccessToken"});
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

    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::clearTwitchTokens() {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "clearTwitchTokens"});
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "clearTwitchTokens"});
        return false;
    }

    config_set_string(config, service, "TwitchAccessToken", "");
    config_set_string(config, service, "TwitchAccessTokenFetchedAt", "");

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to clear Twitch tokens");
        setLastError(ResultError{"IO.WriteFailed", "Failed to clear Twitch tokens", false, ""});
        return false;
    }

    obs_log(LOG_INFO, "Twitch access token cleared successfully");
    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::setYouTubeAccessToken(const QString &accessToken, int expiresInSec,
                                                      qint64 fetchedAtEpochSec) {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "setYouTubeAccessToken"});
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "setYouTubeAccessToken"});
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
        setLastError(ResultError{"IO.WriteFailed", "Failed to save YouTube access token", false, ""});
        return false;
    }

    {
        QString tok = QString::fromUtf8(accessTokenStr.c_str());
        QString masked = tok.length() >= 12 ? tok.left(6) + "..." + tok.right(6) : tok;
        obs_log(LOG_INFO, "YouTube access token saved successfully at %s token(masked)=%s",
                configPath.c_str(), masked.toUtf8().constData());
    }
    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::getYouTubeAccessToken(QString &accessToken, int &expiresInSec,
                                                      qint64 &fetchedAtEpochSec) {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "getYouTubeAccessToken"});
        return false;
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "getYouTubeAccessToken"});
        return false;
    }

    const char *accessTokenChar = config_get_string(config, service, "YouTubeAccessToken");
    const char *fetchedChar = config_get_string(config, service, "YouTubeAccessTokenFetchedAt");
    const char *expiresChar = config_get_string(config, service, "YouTubeAccessTokenExpiresIn");

    if (!accessTokenChar) {
        setLastError(ResultError{"Config.KeyMissing", "YouTube access token not found", false,
                                 "YouTubeAccessToken"});
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

    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::clearYouTubeAccessToken() {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "clearYouTubeAccessToken"});
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "clearYouTubeAccessToken"});
        return false;
    }

    config_set_string(config, service, "YouTubeAccessToken", "");
    config_set_string(config, service, "YouTubeAccessTokenFetchedAt", "");
    config_set_string(config, service, "YouTubeAccessTokenExpiresIn", "");

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to clear YouTube access token");
        setLastError(ResultError{"IO.WriteFailed", "Failed to clear YouTube access token", false, ""});
        return false;
    }

    obs_log(LOG_INFO, "YouTube access token cleared successfully");
    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::getYouTubeRefreshToken(QString &refreshToken, int &expiresInSec,
                                                       qint64 &fetchedAtEpochSec) {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "getYouTubeRefreshToken"});
        return false;
    }

    // Read operation uses shared lock
    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "getYouTubeRefreshToken"});
        return false;
    }

    const char *refreshTokenChar = config_get_string(config, service, "YouTubeRefreshToken");
    const char *fetchedChar = config_get_string(config, service, "YouTubeRefreshTokenFetchedAt");
    const char *expiresChar = config_get_string(config, service, "YouTubeRefreshTokenExpiresIn");
    if (!refreshTokenChar) {
        setLastError(ResultError{"Config.KeyMissing", "YouTube refresh token not found", false,
                                 "YouTubeRefreshToken"});
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
    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::clearYouTubeRefreshToken() {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "clearYouTubeRefreshToken"});
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "clearYouTubeRefreshToken"});
        return false;
    }

    config_set_string(config, service, "YouTubeRefreshToken", "");
    config_set_string(config, service, "YouTubeRefreshTokenFetchedAt", "");
    config_set_string(config, service, "YouTubeRefreshTokenExpiresIn", "");

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to clear YouTube refresh token");
        setLastError(ResultError{"IO.WriteFailed", "Failed to clear YouTube refresh token", false,
                                 ""});
        return false;
    }

    obs_log(LOG_INFO, "YouTube refresh token cleared successfully");
    clearLastError();
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
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "setYouTubeRefreshToken"});
        return false;
    }

    // Write operation uses exclusive lock
    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "setYouTubeRefreshToken"});
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
        setLastError(ResultError{"IO.WriteFailed", "Failed to save YouTube refresh token", false,
                                 ""});
        return false;
    }

    obs_log(LOG_INFO, "YouTube refresh token saved successfully");
    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::setYouTubeBroadcastInfo(const QString &broadcastId,
                                                        const QString &liveChatId) {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "setYouTubeBroadcastInfo"});
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "setYouTubeBroadcastInfo"});
        return false;
    }

    std::string bid = broadcastId.toStdString();
    std::string chatId = liveChatId.toStdString();

    config_set_string(config, service, "YouTubeBroadcastId", bid.c_str());
    config_set_string(config, service, "YouTubeLiveChatId", chatId.c_str());

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save YouTube broadcast info");
        setLastError(ResultError{"IO.WriteFailed", "Failed to save YouTube broadcast info", false,
                                 ""});
        return false;
    }

    obs_log(LOG_INFO, "YouTube broadcast info saved");
    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::getYouTubeBroadcastInfo(QString &broadcastId, QString &liveChatId) {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "getYouTubeBroadcastInfo"});
        return false;
    }

    std::shared_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "getYouTubeBroadcastInfo"});
        return false;
    }

    const char *bid = config_get_string(config, service, "YouTubeBroadcastId");
    const char *chatId = config_get_string(config, service, "YouTubeLiveChatId");

    if (!bid || !chatId) {
        setLastError(ResultError{"Config.KeyMissing", "YouTube broadcast info not found", false,
                                 "YouTubeBroadcastId/YouTubeLiveChatId"});
        return false;
    }

    broadcastId = QString::fromUtf8(bid);
    liveChatId = QString::fromUtf8(chatId);
    clearLastError();
    return true;
}

bool OneSevenLiveConfigManager::clearYouTubeBroadcastInfo() {
    if (!initialized) {
        setLastError(ResultError{"State.NotInitialized", "Config manager not initialized", false,
                                 "clearYouTubeBroadcastInfo"});
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(configMutex);

    if (!config) {
        setLastError(ResultError{"State.InvalidState", "Config handle not available", false,
                                 "clearYouTubeBroadcastInfo"});
        return false;
    }

    config_set_string(config, service, "YouTubeBroadcastId", "");
    config_set_string(config, service, "YouTubeLiveChatId", "");

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to clear YouTube broadcast info");
        setLastError(ResultError{"IO.WriteFailed", "Failed to clear YouTube broadcast info", false,
                                 ""});
        return false;
    }

    obs_log(LOG_INFO, "YouTube broadcast info cleared");
    clearLastError();
    return true;
}

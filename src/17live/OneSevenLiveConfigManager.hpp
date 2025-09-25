#pragma once

#include <util/config-file.h>

#include <QByteArray>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared_mutex>

#include "api/OneSevenLiveModels.hpp"

using json = nlohmann::json;

class OneSevenLiveConfigManager {
   public:
    OneSevenLiveConfigManager();
    bool initialize();

    bool getLoginData(OneSevenLiveLoginData &loginData);
    bool setLoginData(const OneSevenLiveLoginData &loginData);
    void clearLoginData();

    bool setStreamingInfo(const std::string &liveStreamID, const std::string &streamUrl,
                          const std::string &streamKey);
    bool getStreamingInfo(std::string &liveStreamID, std::string &streamUrl,
                          std::string &streamKey);
    bool clearStreamingInfo();

    // WHIP streaming configuration methods
    bool setWhipStreamingInfo(const std::string &liveStreamID, const std::string &whipServer,
                              const std::string &whipToken);
    bool getWhipStreamingInfo(std::string &liveStreamID, std::string &whipServer,
                              std::string &whipToken);
    bool clearWhipStreamingInfo();

    // Check if current streaming mode is WHIP
    bool isWhipMode();
    void setWhipMode(bool isWhip);

    void setStreamingPullUrl(const std::string &streamPullUrl);
    bool getStreamingPullUrl(std::string &streamPullUrl);
    void clearStreamingPullUrl();

    bool getConfigValue(const std::string &key, std::string &value);

    bool saveLiveConfig(const OneSevenLiveStreamInfo &streamInfo);
    bool loadAllLiveConfig(std::vector<OneSevenLiveStreamInfo> &streamInfo);
    bool saveAllLiveConfig(const std::vector<OneSevenLiveStreamInfo> &streamInfo);
    bool removeLiveConfig(const std::string &streamUuid);

    QByteArray getDockState();
    bool setDockState(const QByteArray &state);

    bool getDockVisibility(const std::string &dockName);
    bool setDockVisibility(const std::string &dockName, bool visible);

    // Set configuration data
    bool setConfig(const json &configData);
    // Get configuration data
    bool getConfig(OneSevenLiveConfig &config);

    bool saveGifts(const json &gifts);
    bool loadGifts(json &gifts);

   private:
    bool initialized = false;

    config_t *config = nullptr;

    std::string configPath;

    // Read-write lock to protect config file operations, allows multiple concurrent read operations
    mutable std::shared_mutex configMutex;
    // Current configuration
    OneSevenLiveConfig currentConfig;
};

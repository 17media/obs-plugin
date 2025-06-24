#pragma once

#include <util/config-file.h>

#include <QByteArray>
#include <mutex>

#include "api/OneSevenLiveModels.hpp"
#include "json11.hpp"

using namespace json11;

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

    // Set configuration data
    bool setConfig(const Json &configData);
    // Get configuration data
    bool getConfig(OneSevenLiveConfig &config);

    bool saveGifts(const Json &gifts);
    bool loadGifts(Json &gifts);

   private:
    bool initialized = false;

    config_t *config = nullptr;

    std::string configPath;

    // Mutex for saving configuration file
    std::mutex configMutex;
    // Current configuration
    OneSevenLiveConfig currentConfig;
};

#pragma once

#include <mutex>

#include <util/config-file.h>
#include "json11.hpp"

#include <QByteArray>

#include "api/OneSevenLiveModels.hpp"

using namespace json11;

class OneSevenLiveConfigManager {
public:
  OneSevenLiveConfigManager();
  bool initialize();

  bool getLoginData(OneSevenLiveLoginData &loginData);
  bool setLoginData(const OneSevenLiveLoginData &loginData);
  void clearLoginData();

  bool setStreamingInfo(const std::string &liveStreamID,
                        const std::string &streamUrl,
                        const std::string &streamKey);
  bool getStreamingInfo(std::string &liveStreamID,
                        std::string &streamUrl,
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

  // 设置配置数据
  bool setConfig(const Json &configData);
  // 获取配置数据
  bool getConfig(OneSevenLiveConfig &config);

  
private:
  bool initialized = false;

  config_t* config = nullptr;

  std::string configPath;

  // 用于保存配置文件的互斥锁
  std::mutex configMutex;
  // 当前配置
  OneSevenLiveConfig currentConfig;
};

#pragma once

#include <util/config-file.h>

#include <QByteArray>

struct SeventeenLiveLoginData;

struct SeventeenLiveConfigStreamerResponse;

struct SeventeenLiveStreamInfo;

class SeventeenLiveConfigManager {
public:
  SeventeenLiveConfigManager();
  bool initialize();

  bool getLoginData(SeventeenLiveLoginData &loginData);
  bool setLoginData(const SeventeenLiveLoginData &loginData);
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

  bool setConfigStreamer(const SeventeenLiveConfigStreamerResponse &response);
  bool getConfigStreamer(SeventeenLiveConfigStreamerResponse &response);

  bool saveLiveConfig(const SeventeenLiveStreamInfo &streamInfo);
  bool loadAllLiveConfig(std::vector<SeventeenLiveStreamInfo> &streamInfo);
  bool saveAllLiveConfig(const std::vector<SeventeenLiveStreamInfo> &streamInfo);
  bool removeLiveConfig(const std::string &streamUuid);

  QByteArray getDockState();
  bool setDockState(const QByteArray &state);

  
private:
  bool initialized = false;

  config_t* config = nullptr;

  std::string configPath;
};

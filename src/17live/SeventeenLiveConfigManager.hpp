#pragma once

#include <util/config-file.h>

#include <QByteArray>

namespace seventeenlive {

struct SeventeenLiveLoginData;

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

  QByteArray getDockState();
  bool setDockState(const QByteArray &state);

  
private:
  bool initialized = false;

  config_t* config = nullptr;
};

}

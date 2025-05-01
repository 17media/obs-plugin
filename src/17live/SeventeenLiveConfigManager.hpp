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

  QByteArray getDockState();
  bool setDockState(const QByteArray &state);

  
private:
  bool initialized = false;

  config_t* config = nullptr;
};

}

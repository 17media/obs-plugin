#pragma once

#include <util/config-file.h>

namespace seventeenlive {

struct SeventeenLiveLoginData;

class SeventeenLiveConfigManager {
public:
  SeventeenLiveConfigManager();
  bool initialize();

  bool getLoginData(SeventeenLiveLoginData &loginData);
  bool setLoginData(const SeventeenLiveLoginData &loginData);
  void clearLoginData();
  
private:
  bool initialized = false;

  config_t* config = nullptr;
};

}

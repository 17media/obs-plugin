#include "SeventeenLiveConfigManager.hpp"

#include <obs-module.h>
#include <util/config-file.h>
#include <QDir>
#include <QFile>
#include <QString>
#include "plugin-support.h"

#include "api/SeventeenLiveApiWrappers.hpp"

namespace seventeenlive {

const char* service = "SeventeenLive";

#define CONFIG_PATH ".17Live"
#define CONFIG_NAME "config.ini"

SeventeenLiveConfigManager::SeventeenLiveConfigManager(): initialized(false) {}
bool SeventeenLiveConfigManager::initialize()
{
  // 防止重复初始化
  if (initialized) {
    return true;
  }

  // 当前用户的home目录下的.17Live目录，采用Qt的方式获取
  QString homeDir = QDir::homePath();
  QString configDir = homeDir + "/" + CONFIG_PATH;
  QDir dir(configDir);
  // 如果目录不存在，创建目录
  if (!dir.exists()) {
    if (!dir.mkpath(configDir)) {
      obs_log(LOG_ERROR, "Failed to create config directory");
      return false;
    }
  }

  // 配置文件路径
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

bool SeventeenLiveConfigManager::getConfigValue(const std::string &key, std::string &value)
{
  if (!initialized) {
    return false;
  }

  if (!config) {
    return false;
  }

  const char* valueChar = config_get_string(config, service, key.c_str());
  if (!valueChar) {
    return false;
  }
  value = valueChar;
  return true;
}
bool SeventeenLiveConfigManager::getLoginData(SeventeenLiveLoginData &loginData)
{
  if (!initialized) {
    return false;
  }

  if (!config) {
    return false;
  }

  const char* jwtTokenChar = config_get_string(config, service, "JwtToken");
  const char* openIdChar = config_get_string(config, service, "OpenID");
  const char* displayNameChar = config_get_string(config, service, "DisplayName");
    
  std::string jwtToken = jwtTokenChar? jwtTokenChar : "";
  std::string openId = openIdChar? openIdChar : "";
  std::string displayName = displayNameChar? displayNameChar : "";

  
  loginData.jwtAccessToken = QString::fromStdString(jwtToken);
  loginData.userInfo.openID = QString::fromStdString(openId);
  loginData.userInfo.displayName = QString::fromStdString(displayName);
  loginData.userInfo.roomID = config_get_uint(config, service, "RoomID");
  
  return true;
}

bool SeventeenLiveConfigManager::setLoginData(const SeventeenLiveLoginData &loginData)
{
  if (!initialized) {
    return false;
  }
  if (!config) {
    return false;
  }
    
  // 转换为std::string并保持引用
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

void SeventeenLiveConfigManager::clearLoginData()
{
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

QByteArray SeventeenLiveConfigManager::getDockState()
{
  if (!initialized) {
    return QByteArray();
  }

  if (!config) {
    return QByteArray();
  }

  const char* dockStateChar = config_get_string(config, service, "DockState");
  if (!dockStateChar) {
      return QByteArray();
  }

  return QByteArray(dockStateChar);
}

bool SeventeenLiveConfigManager::setDockState(const QByteArray &state)
{
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

bool SeventeenLiveConfigManager::setStreamingInfo(const std::string &liveStreamID, const std::string &streamUrl,
  const std::string &streamKey)
{
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
bool SeventeenLiveConfigManager::getStreamingInfo(std::string &liveStreamID,
  std::string &streamUrl,
  std::string &streamKey)
{
  if (!initialized) {
    return false;
  }

  if (!config) {
    return false;
  }
  const char* liveStreamIDChar = config_get_string(config, service, "LiveStreamID");
  const char* streamUrlChar = config_get_string(config, service, "StreamUrl");
  const char* streamKeyChar = config_get_string(config, service, "StreamKey");
  if (!liveStreamIDChar || !streamUrlChar || !streamKeyChar) {
    return false;
  }
  liveStreamID = liveStreamIDChar;
  streamUrl = streamUrlChar;
  streamKey = streamKeyChar;
  return true;
}
bool SeventeenLiveConfigManager::clearStreamingInfo()
{
  return setStreamingInfo("", "", "");
}

void SeventeenLiveConfigManager::setStreamingPullUrl(const std::string &streamPullUrl)
{
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
  
bool SeventeenLiveConfigManager::getStreamingPullUrl(std::string &streamPullUrl)
{
  if (!initialized) {
    return false;
  }
  if (!config) {
    return false;
  }
  const char* streamPullUrlChar = config_get_string(config, service, "StreamPullUrl");
  if (!streamPullUrlChar) {
    return false;
  }
  streamPullUrl = streamPullUrlChar;
  return true;
}
  
void SeventeenLiveConfigManager::clearStreamingPullUrl()
{
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

bool SeventeenLiveConfigManager::setConfigStreamer(const SeventeenLiveConfigStreamerResponse &response)
{
  if (!initialized) {
    return false;
  }

  QString configStreamerFile = QString::fromStdString(configPath) + "/" + "config_streamer.json";
  QFile file(configStreamerFile);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  QTextStream out(&file);
  Json json_data;
  SeventeenLiveConfigStreamerResponseToJson(response, json_data);
  out << QString::fromStdString(json_data.dump());
  file.close();

  return true;
}

bool SeventeenLiveConfigManager::getConfigStreamer(SeventeenLiveConfigStreamerResponse &response)
{
  if (!initialized) {
    return false;
  }

  QString configStreamerFile = QString::fromStdString(configPath) + "/" + "config_streamer.json";
  QFile file(configStreamerFile);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  QTextStream in(&file);
  QString jsonString = in.readAll();
  file.close();
  JsonToSeventeenLiveConfigStreamerResponse(jsonString.toStdString(), response);
  
  return true;
}


} // namespace seventeenlive

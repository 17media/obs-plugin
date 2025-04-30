#include "SeventeenLiveCoreManager.hpp"
#include <QMainWindow>

#include "SeventeenLiveMenuManager.hpp"
#include "SeventeenLiveLoginDialog.hpp"
#include "api/SeventeenLiveApiWrappers.hpp"

#include <util/config-file.h>
#include "plugin-support.h"

#include <obs-frontend-api.h>

#include "json11.hpp"

using namespace json11;

namespace seventeenlive {

const char* service = "SeventeenLive";

// 初始化静态成员变量
SeventeenLiveCoreManager* SeventeenLiveCoreManager::instance = nullptr;
std::mutex SeventeenLiveCoreManager::instanceMutex;

SeventeenLiveCoreManager& SeventeenLiveCoreManager::getInstance(QMainWindow* mainWindow)
{
    // 使用双重检查锁定模式确保线程安全
    if (instance == nullptr) {
        std::lock_guard<std::mutex> lock(instanceMutex);
        if (instance == nullptr) {
            if (mainWindow == nullptr) {
                throw std::runtime_error("首次调用getInstance时必须提供mainWindow参数");
            }
            instance = new SeventeenLiveCoreManager(mainWindow);
        }
    }
    return *instance;
}

SeventeenLiveCoreManager::SeventeenLiveCoreManager(QMainWindow* mainWindow_)
    : mainWindow(mainWindow_), initialized(false)
{
}

SeventeenLiveCoreManager::~SeventeenLiveCoreManager()
{
    // 确保在析构前调用shutdown
    if (initialized) {
        shutdown();
    }
}

bool SeventeenLiveCoreManager::initialize()
{
    // 防止重复初始化
    if (initialized) {
        return true;
    }

    config_t *config = obs_frontend_get_global_config();
    if (!config) {
        obs_log(LOG_ERROR, "Failed to get global config");
        return false;
    }
  
    const char* jwtTokenChar = config_get_string(config, service, "JwtToken");
    
    std::string jwtToken = jwtTokenChar? jwtTokenChar : "";
    
    bool isLogin = false;

    if (!jwtToken.empty()) {
        apiWrapper = std::make_unique<SeventeenLiveApiWrappers>(jwtTokenChar);

        isLogin = checkLoginStatus();
    } 
    
    // if not login, reinitialize apiWrapper
    if (!isLogin) {
        apiWrapper = std::make_unique<SeventeenLiveApiWrappers>();
    }

    // 初始化菜单管理器
    menuManager = std::make_unique<SeventeenLiveMenuManager>(mainWindow);
    if (!menuManager) {
        obs_log(LOG_ERROR, "Failed to create menu manager");
        return false;
    }
    // connect menuManager's loginClicked signal to handleLoginClicked slot
    QObject::connect(menuManager.get(), &SeventeenLiveMenuManager::loginClicked, this, &SeventeenLiveCoreManager::handleLoginClicked);

    QObject::connect(menuManager.get(), &SeventeenLiveMenuManager::logoutClicked, this, &SeventeenLiveCoreManager::handleLogoutClicked);

    if (isLogin) {
        const char* openIdChar = config_get_string(config, service, "OpenID");
        const char* displayNameChar = config_get_string(config, service, "DisplayName");
        
        std::string openId = openIdChar ? openIdChar : "";
        std::string displayName = displayNameChar ? displayNameChar : "";

        std::string username = displayName;
        if (username.empty()) {
            username = openId;
        }
        menuManager->updateLoginStatus(true, QString::fromStdString(username));
    }

    initialized = true;
    return true;
}

void SeventeenLiveCoreManager::shutdown()
{
    if (!initialized) {
        return;
    }

    // 清理菜单管理器资源
    if (menuManager) {
        menuManager->cleanup();
    }

    initialized = false;
}

QMainWindow* SeventeenLiveCoreManager::getMainWindow() const
{
    return mainWindow;
}

void SeventeenLiveCoreManager::setConfig(const std::string& key, const std::string& value)
{
    configMap[key] = value;
}

std::string SeventeenLiveCoreManager::getConfig(const std::string& key, const std::string& defaultValue)
{
    auto it = configMap.find(key);
    if (it != configMap.end()) {
        return it->second;
    }
    return defaultValue;
}

SeventeenLiveMenuManager* SeventeenLiveCoreManager::getMenuManager() const
{
    return menuManager.get();
}

bool SeventeenLiveCoreManager::handleLoginClicked()
{
    SeventeenLiveLoginDialog dialog(mainWindow);

    // 连接登录成功信号到主窗口的槽函数
    QObject::connect(&dialog, &SeventeenLiveLoginDialog::loginSuccess, this, &SeventeenLiveCoreManager::handleLoginSuccess);

    return dialog.exec() == QDialog::Accepted;
}

void SeventeenLiveCoreManager::handleLoginSuccess(const SeventeenLiveLoginData& loginData)
{
    obs_log(LOG_INFO, "handleLoginSuccess");

    // 将登录信息存储到本地
    config_t *config = obs_frontend_get_global_config();
    if (!config) {
        obs_log(LOG_ERROR, "Failed to get global config");
        return;
    }
    
    // 转换为std::string并保持引用
    std::string userID = loginData.userInfo.userID.toStdString();
    std::string openID = loginData.userInfo.openID.toStdString();
    std::string displayName = loginData.userInfo.displayName.toStdString();
    std::string jwtToken = loginData.jwtAccessToken.toStdString();
    
    config_set_string(config, service, "UserID", userID.c_str());
    config_set_string(config, service, "OpenID", openID.c_str());
    config_set_string(config, service, "DisplayName", displayName.c_str());
    config_set_string(config, service, "JwtToken", jwtToken.c_str());
    config_set_uint(config, service, "RoomID", loginData.userInfo.roomID);

    if (config_save(config) < 0) {
        obs_log(LOG_ERROR, "Failed to save config");
        return;
    }

    obs_log(LOG_DEBUG, "Login data saved to config.");

    // 更新菜单
    QString username = loginData.userInfo.displayName;
    if (username.isEmpty()) {
        username = loginData.userInfo.openID;
    }
    menuManager->updateLoginStatus(true, username);
}

void SeventeenLiveCoreManager::handleLogoutClicked()
{
    // 重置登录状态
    menuManager->updateLoginStatus(false, "");
    // 重置配置
    config_t *config = obs_frontend_get_global_config();
    if (!config) {
        obs_log(LOG_ERROR, "Failed to get global config");
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

bool SeventeenLiveCoreManager::checkLoginStatus()
{
    // call apiWrapper->GetSelfInfo()
    SeventeenLiveLoginData loginData;
    if (apiWrapper->GetSelfInfo(loginData)) {
        return true;
    }
    
    // reset config
    config_t *config = obs_frontend_get_global_config();
    if (!config) {
        obs_log(LOG_ERROR, "Failed to get global config");
        return false;
    }
    config_set_string(config, service, "UserID", "");
    config_set_string(config, service, "OpenID", "");
    config_set_string(config, service, "DisplayName", "");
    config_set_string(config, service, "JwtToken", "");
    config_set_uint(config, service, "RoomID", 0);

    return false;
}

} // namespace seventeenlive

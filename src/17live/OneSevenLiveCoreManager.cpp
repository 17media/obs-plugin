#include "OneSevenLiveCoreManager.hpp"
#include <QMainWindow>
#include <QDesktopServices>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include "plugin-support.h"

#include "json11.hpp"

#include "OneSevenLiveMenuManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveLoginDialog.hpp"
#include "OneSevenLiveStreamingDock.hpp"
#include "OneSevenLiveStreamListDock.hpp"
#include "utility/Common.hpp"
#include "utility/Meta.hpp"
#include "OneSevenLiveHttpServer.hpp"

#include "cef-view.hpp"

extern QDockWidget *cef_window;

using namespace json11;
using namespace std;

// 初始化静态成员变量
OneSevenLiveCoreManager* OneSevenLiveCoreManager::instance = nullptr;
std::mutex OneSevenLiveCoreManager::instanceMutex;

OneSevenLiveCoreManager& OneSevenLiveCoreManager::getInstance(QMainWindow* mainWindow)
{
    // 使用双重检查锁定模式确保线程安全
    if (instance == nullptr) {
        std::lock_guard<std::mutex> lock(instanceMutex);
        if (instance == nullptr) {
            if (mainWindow == nullptr) {
                throw std::runtime_error("首次调用getInstance时必须提供mainWindow参数");
            }
            instance = new OneSevenLiveCoreManager(mainWindow);
        }
    }
    return *instance;
}

OneSevenLiveCoreManager::OneSevenLiveCoreManager(QMainWindow* mainWindow_)
    : mainWindow(mainWindow_), initialized(false)
{
}

OneSevenLiveCoreManager::~OneSevenLiveCoreManager()
{
    // 确保在析构前调用shutdown
    if (initialized) {
        shutdown();
    }
}

bool OneSevenLiveCoreManager::initialize()
{
    // 防止重复初始化
    if (initialized) {
        return true;
    }

    // 初始化并启动 HTTP 服务器
    // "html" 是相对于 obs_get_module_data_path() 的路径
    httpServer_ = std::make_unique<OneSevenLiveHttpServer>("localhost", 0, "html/chat");
    if (!httpServer_->start()) {
        blog(LOG_ERROR, "[17Live Core] Failed to start HTTP server.");
        // 根据需求决定是否因为 HTTP 服务器启动失败而中断整个初始化
        // return false; 
    } else {
        blog(LOG_INFO, "[17Live Core] HTTP server started successfully.");
    }

    // 初始化配置管理器
    configManager = std::make_unique<OneSevenLiveConfigManager>();

    if (!configManager->initialize()) {
        obs_log(LOG_ERROR, "Failed to initialize config manager");
        return false;
    }

    OneSevenLiveLoginData loginData;
    configManager->getLoginData(loginData);

    bool isLogin = false;

    if (!loginData.jwtAccessToken.isEmpty()) {
        apiWrapper = std::make_unique<OneSevenLiveApiWrappers>(loginData.jwtAccessToken.toStdString());

        isLogin = checkLoginStatus();
    } 
    
    // if not login, reinitialize apiWrapper
    if (!isLogin) {
        apiWrapper = std::make_unique<OneSevenLiveApiWrappers>();
    }

    // 初始化菜单管理器
    menuManager = std::make_unique<OneSevenLiveMenuManager>(mainWindow);
    if (!menuManager) {
        obs_log(LOG_ERROR, "Failed to create menu manager");
        return false;
    }
    // connect menuManager's loginClicked signal to handleLoginClicked slot
    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::loginClicked, this, &OneSevenLiveCoreManager::handleLoginClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::logoutClicked, this, &OneSevenLiveCoreManager::handleLogoutClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::streamingClicked, this, &OneSevenLiveCoreManager::handleStreamingClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::chatRoomClicked, this, &OneSevenLiveCoreManager::handleChatRoomClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::liveListClicked, this, &OneSevenLiveCoreManager::handleLiveListClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::checkUpdateClicked, this, [this] () {
        QUrl url = QUrl(obs_module_text("Menu.CheckUpdate.Url"), QUrl::TolerantMode);
	    QDesktopServices::openUrl(url);
    });

    if (isLogin) {
        QString openId = loginData.userInfo.openID;
        QString displayName = loginData.userInfo.displayName;

        QString username = displayName;
        if (username.isEmpty()) {
            username = openId;
        }
        menuManager->updateLoginStatus(true, username);

        // 加载meta data
        if (!LoadMetaData()) {
            obs_log(LOG_ERROR, "Failed to load meta data");
            return false;
        }
    }

    load17LiveConfig();

    initialized = true;
    return true;
}

void OneSevenLiveCoreManager::load17LiveConfig()
{
    // 在initialize方法中，在初始化configManager之后添加以下代码

    // 异步获取配置
    std::thread configThread([this]() {
        // 获取当前区域和语言
        OneSevenLiveLoginData loginData;
        configManager->getLoginData(loginData);
    
        std::string region = loginData.userInfo.region.toStdString();
        if (region.empty()) {
            region = "TW"; // 默认区域
        }
    
        std::string language = GetCurrentLanguage();
    
        // 调用API获取配置
        json11::Json configJson;
        if (apiWrapper->GetConfig(region, language, configJson)) {
            // 保存配置
            configManager->setConfig(configJson);
            obs_log(LOG_INFO, "Config loaded successfully");
        } else {
            obs_log(LOG_ERROR, "Failed to load config from API");
        }
    });
  
    // 分离线程，让它在后台运行
    configThread.detach(); 
}

void OneSevenLiveCoreManager::shutdown()
{
    if (!initialized) {
        return;
    }

    if (streamingDock) {
        streamingDock->disconnect(this);
    }

    if (liveListDock) {
        liveListDock->disconnect(this);
    }

    if (cef_window) {
        cef_window->disconnect(this);
    }

    saveDockState();

    // 清理菜单管理器资源
    if (menuManager) {
        menuManager->cleanup();
    }

    // os_event_destroy(cef_started_event);

    initialized = false;
}

QMainWindow* OneSevenLiveCoreManager::getMainWindow() const
{
    return mainWindow;
}

OneSevenLiveMenuManager* OneSevenLiveCoreManager::getMenuManager() const
{
    return menuManager.get();
}

OneSevenLiveApiWrappers* OneSevenLiveCoreManager::getApiWrapper() const
{
    return apiWrapper.get();
}

OneSevenLiveConfigManager* OneSevenLiveCoreManager::getConfigManager() const
{
    return configManager.get();
}

bool OneSevenLiveCoreManager::handleLoginClicked()
{
    OneSevenLiveLoginDialog dialog(mainWindow, getApiWrapper());

    // 连接登录成功信号到主窗口的槽函数
    QObject::connect(&dialog, &OneSevenLiveLoginDialog::loginSuccess, this, &OneSevenLiveCoreManager::handleLoginSuccess);

    return dialog.exec() == QDialog::Accepted;
}

void OneSevenLiveCoreManager::handleLoginSuccess(const OneSevenLiveLoginData& loginData)
{
    obs_log(LOG_INFO, "handleLoginSuccess");

    if (!configManager->setLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to save login data");
        return;
    }

    // 更新菜单
    QString username = loginData.userInfo.displayName;
    if (username.isEmpty()) {
        username = loginData.userInfo.openID;
    }
    menuManager->updateLoginStatus(true, username);
}

void OneSevenLiveCoreManager::handleLogoutClicked()
{
    // 关闭所有 dock 窗口，避免登出后出现错误操作
    if (streamingDock) {
        streamingDock->close();
        streamingDock = nullptr;
    }
    
    if (liveListDock) {
        liveListDock->close();
        liveListDock = nullptr;
    }
    
    // 关闭聊天室窗口（如果存在）
    if (cef_window && cef_window->isVisible()) {
        cef_window->close();
    }
    
    // 重置登录状态
    menuManager->updateLoginStatus(false, "");
    configManager->clearLoginData();
}

void OneSevenLiveCoreManager::handleStreamingClicked()
{
    obs_log(LOG_INFO, "handleStreamingClicked");

    if (!streamingDock) {
        OneSevenLiveLoginData loginData;
        if (!configManager->getLoginData(loginData)) {
            obs_log(LOG_ERROR, "Failed to get login data");
            return;
        }

        // 创建并显示流媒体窗口
        streamingDock = new OneSevenLiveStreamingDock(mainWindow, apiWrapper.get(), configManager.get());

        streamingDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        mainWindow->addDockWidget(Qt::RightDockWidgetArea, streamingDock);

        streamingDock->setFloating(true);
        streamingDock->setVisible(true);

        connect(streamingDock, &OneSevenLiveStreamingDock::streamInfoSaved, this, [this] () {
            if (liveListDock) {
                liveListDock->refreshStreamList();
            }
        });

        connect(streamingDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            menuManager->updateDockVisibility(cef_window && cef_window->isVisible(),
                                            visible,
                                            liveListDock && liveListDock->isVisible());
        });
        
        // 连接关闭信号到主窗口的槽函数
        connect(streamingDock, &QDockWidget::destroyed, this, [this]() {
            saveDockState();
        });

        streamingDock->loadRoomInfo(loginData.userInfo.roomID);
    } else {
        streamingDock->setVisible(!streamingDock->isVisible());

        QByteArray dockState = configManager->getDockState();
        if (mainWindow->isVisible())
            mainWindow->restoreState(dockState);
    }


    // 更新菜单项勾选状态
    if (menuManager) {
        menuManager->updateDockVisibility(cef_window && cef_window->isVisible(),
                                        streamingDock && streamingDock->isVisible(),
                                        liveListDock && liveListDock->isVisible());
    }

}

void OneSevenLiveCoreManager::handleLiveListClicked()
{
    obs_log(LOG_INFO, "handleLiveListClicked");

    if (!liveListDock) {

        liveListDock = new OneSevenLiveStreamListDock(mainWindow, configManager.get());
        liveListDock->setMinimumWidth(300);
        liveListDock->setMinimumHeight(400);

        liveListDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        mainWindow->addDockWidget(Qt::RightDockWidgetArea, liveListDock);

        liveListDock->setFloating(true);

        liveListDock->setVisible(true);

        connect(liveListDock, &OneSevenLiveStreamListDock::startLiveClicked, this, [this] (const OneSevenLiveRtmpRequest& request) {
            // if streamingDock is not visible, show it
            // in order to edit the live info item
            if (!streamingDock) {
                handleStreamingClicked();
            }
    
            streamingDock->createLiveWithRequest(request);
        });
    
        connect(liveListDock, &OneSevenLiveStreamListDock::editLiveClicked, this, [this] (const OneSevenLiveStreamInfo& info) {
            if (streamingDock) {
                streamingDock->editLiveWithInfo(info);
            }
        });

        // dock 关闭时，取消菜单项的勾选状态
        connect(liveListDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            menuManager->updateDockVisibility(cef_window && cef_window->isVisible(),
                                            streamingDock && streamingDock->isVisible(),
                                            visible);
        });
    
        // 连接关闭信号到主窗口的槽函数
        connect(liveListDock, &QDockWidget::destroyed, this, [this]() {
            saveDockState();
        });
    } else {
        liveListDock->setVisible(!liveListDock->isVisible());
        
        QByteArray dockState = configManager->getDockState();
        if (mainWindow->isVisible())
            mainWindow->restoreState(dockState);
    }

    // 更新菜单项勾选状态
    if (menuManager) {
        menuManager->updateDockVisibility(cef_window && cef_window->isVisible(),
                                        streamingDock && streamingDock->isVisible(),
                                        liveListDock && liveListDock->isVisible());
    }
}

bool OneSevenLiveCoreManager::checkLoginStatus()
{
    // call apiWrapper->GetSelfInfo()
    OneSevenLiveLoginData loginData;
    if (!apiWrapper->GetSelfInfo(loginData)) {
        configManager->clearLoginData();
        return false;
    }
    
    // TODO: update loginData: displayName

    return true;
}

void OneSevenLiveCoreManager::saveDockState()
{
    if (!initialized || !mainWindow || !configManager) {
        return;
    }
    
    QByteArray state = mainWindow->saveState();
    configManager->setDockState(state);
    
    obs_log(LOG_INFO, "Dock state saved successfully");
}

void OneSevenLiveCoreManager::handleChatRoomClicked()
{
    obs_log(LOG_INFO, "handleChatRoomClicked");

    if (!cef_window) { 
        OneSevenLiveLoginData loginData;
        if (!configManager->getLoginData(loginData)) {
            obs_log(LOG_ERROR, "Failed to get login data");
            return;
        }

        std::string locale = GetCurrentLocale();

        obs_log(LOG_INFO, "userID: %s", loginData.userInfo.userID.toStdString().c_str());

        QString chatUrl = QString("http://localhost:%1/%2.html?roomID=%3&userID=%4")
            .arg(QString::number(httpServer_->getPort()), QString::fromStdString(locale), QString::number(loginData.userInfo.roomID), loginData.userInfo.userID);
        obs_log(LOG_INFO, "chatUrl: %s", chatUrl.toStdString().c_str());
        cef_view_open_url(chatUrl.toStdString().c_str());

        connect(cef_window, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            menuManager->updateDockVisibility(visible,
                                            streamingDock && streamingDock->isVisible(),
                                            liveListDock && liveListDock->isVisible());
        });
    } else {
        cef_window->setVisible(!cef_window->isVisible());
    }
    
    // 更新聊天室可见状态（CEF 视图打开时视为可见）
    if (menuManager) {
        menuManager->updateDockVisibility(true, 
                                        streamingDock && streamingDock->isVisible(), 
                                        liveListDock && liveListDock->isVisible());
    }
}

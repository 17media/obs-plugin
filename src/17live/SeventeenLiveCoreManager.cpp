#include "SeventeenLiveCoreManager.hpp"
#include <QMainWindow>
#include <QDesktopServices>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include "plugin-support.h"

#include "json11.hpp"

#include "SeventeenLiveMenuManager.hpp"
#include "api/SeventeenLiveApiWrappers.hpp"
#include "SeventeenLiveConfigManager.hpp"
#include "SeventeenLiveLoginDialog.hpp"
#include "SeventeenLiveStreamingDock.hpp"
#include "SeventeenLiveStreamListDock.hpp"
#include "utility/Common.hpp"
#include "utility/Meta.hpp"
#include "SeventeenLiveHttpServer.hpp"

#include "cef-view.hpp"


using namespace json11;
using namespace std;

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

    // 初始化并启动 HTTP 服务器
    // "html" 是相对于 obs_get_module_data_path() 的路径
    httpServer_ = std::make_unique<SeventeenLiveHttpServer>("localhost", 0, "html");
    if (!httpServer_->start()) {
        blog(LOG_ERROR, "[17Live Core] Failed to start HTTP server.");
        // 根据需求决定是否因为 HTTP 服务器启动失败而中断整个初始化
        // return false; 
    } else {
        blog(LOG_INFO, "[17Live Core] HTTP server started successfully.");
    }

    // 初始化配置管理器
    configManager = std::make_unique<SeventeenLiveConfigManager>();

    if (!configManager->initialize()) {
        obs_log(LOG_ERROR, "Failed to initialize config manager");
        return false;
    }

    SeventeenLiveLoginData loginData;
    configManager->getLoginData(loginData);

    bool isLogin = false;

    if (!loginData.jwtAccessToken.isEmpty()) {
        apiWrapper = std::make_unique<SeventeenLiveApiWrappers>(loginData.jwtAccessToken.toStdString());

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

    QObject::connect(menuManager.get(), &SeventeenLiveMenuManager::streamingClicked, this, &SeventeenLiveCoreManager::handleStreamingClicked);

    QObject::connect(menuManager.get(), &SeventeenLiveMenuManager::chatRoomClicked, this, &SeventeenLiveCoreManager::handleChatRoomClicked);

    QObject::connect(menuManager.get(), &SeventeenLiveMenuManager::liveListClicked, this, &SeventeenLiveCoreManager::handleLiveListClicked);

    QObject::connect(menuManager.get(), &SeventeenLiveMenuManager::checkUpdateClicked, this, [this] () {
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

        // 登录成功后，加载配置
        loadConfigStreamer();

        // 加载meta data
        if (!LoadMetaData()) {
            obs_log(LOG_ERROR, "Failed to load meta data");
            return false;
        }
    }

    initialized = true;
    return true;
}

void SeventeenLiveCoreManager::shutdown()
{
    if (!initialized) {
        return;
    }

    saveDockState();

    // 清理菜单管理器资源
    if (menuManager) {
        menuManager->cleanup();
    }

    // os_event_destroy(cef_started_event);

    initialized = false;
}

QMainWindow* SeventeenLiveCoreManager::getMainWindow() const
{
    return mainWindow;
}

SeventeenLiveMenuManager* SeventeenLiveCoreManager::getMenuManager() const
{
    return menuManager.get();
}

SeventeenLiveApiWrappers* SeventeenLiveCoreManager::getApiWrapper() const
{
    return apiWrapper.get();
}

SeventeenLiveConfigManager* SeventeenLiveCoreManager::getConfigManager() const
{
    return configManager.get();
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

void SeventeenLiveCoreManager::handleLogoutClicked()
{
    // 重置登录状态
    menuManager->updateLoginStatus(false, "");
    configManager->clearLoginData();
}

void SeventeenLiveCoreManager::handleStreamingClicked()
{
    obs_log(LOG_INFO, "handleStreamingClicked");

    SeventeenLiveLoginData loginData;
    if (!configManager->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    SeventeenLiveRoomInfo roomInfo;
    if (!apiWrapper->GetRoomInfo(loginData.userInfo.roomID, roomInfo)) {
        obs_log(LOG_ERROR, "Failed to get self room info");
        return;
    }

    QSize size = mainWindow->size();
    QPoint pos = mainWindow->pos();

    // 创建并显示流媒体窗口
    streamingDock = new SeventeenLiveStreamingDock(mainWindow, roomInfo, apiWrapper.get(), configManager.get());

    streamingDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, streamingDock);

    streamingDock->setFloating(true);

    if (streamingDockFirstLoad) {
        streamingDock->setVisible(true);
        streamingDockFirstLoad = false;
    } else {
        streamingDock->setVisible(!streamingDock->isVisible());
        
        QByteArray dockState = configManager->getDockState();
        if (mainWindow->isVisible())
            mainWindow->restoreState(dockState);
    }

    connect(streamingDock, &SeventeenLiveStreamingDock::streamInfoSaved, this, [this] () {
        if (liveListDock) {
            liveListDock->refreshStreamList();
        }
    });
    
    // 连接关闭信号到主窗口的槽函数
    connect(streamingDock, &QDockWidget::destroyed, this, &SeventeenLiveCoreManager::saveDockState);

}

void SeventeenLiveCoreManager::handleLiveListClicked()
{
    obs_log(LOG_INFO, "handleLiveListClicked");

    if (!liveListDock) {
        QSize size = mainWindow->size();
        QPoint pos = mainWindow->pos();

        // 创建并显示流媒体窗口
        liveListDock = new SeventeenLiveStreamListDock(mainWindow, configManager.get());

        liveListDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        mainWindow->addDockWidget(Qt::RightDockWidgetArea, liveListDock);

        liveListDock->setFloating(true);

        liveListDock->setVisible(true);
    } else {
        liveListDock->setVisible(!liveListDock->isVisible());
        
        QByteArray dockState = configManager->getDockState();
        if (mainWindow->isVisible())
            mainWindow->restoreState(dockState);
    }

    connect(liveListDock, &SeventeenLiveStreamListDock::startLiveClicked, this, [this] (const SeventeenLiveRtmpRequest& request) {
        if (!streamingDock) {
            handleStreamingClicked();
        }

        streamingDock->createLiveWithRequest(request);
    });

    connect(liveListDock, &SeventeenLiveStreamListDock::editLiveClicked, this, [this] (const SeventeenLiveStreamInfo& info) {
        if (streamingDock) {
            streamingDock->editLiveWithInfo(info);
        }
    });

    // 连接关闭信号到主窗口的槽函数
    connect(liveListDock, &QDockWidget::destroyed, this, &SeventeenLiveCoreManager::saveDockState);
}

bool SeventeenLiveCoreManager::checkLoginStatus()
{
    // call apiWrapper->GetSelfInfo()
    SeventeenLiveLoginData loginData;
    if (!apiWrapper->GetSelfInfo(loginData)) {
        configManager->clearLoginData();
        return false;
    }
    
    // TODO: update loginData: displayName

    return true;
}

void SeventeenLiveCoreManager::saveDockState()
{
    // TODO: error here, to be fixed
    // if (mainWindow && streamingDock) {
    //     QByteArray state = mainWindow->saveState();
    //     configManager->setDockState(state);
    // }
}

void SeventeenLiveCoreManager::handleChatRoomClicked()
{
    obs_log(LOG_INFO, "handleChatRoomClicked");

    QString chatUrl = QString("http://localhost:%1/chat/").arg(QString::number(httpServer_->getPort()));
    obs_log(LOG_INFO, "chatUrl: %s", chatUrl.toStdString().c_str());
    cef_view_open_url(chatUrl.toStdString().c_str());
}

void SeventeenLiveCoreManager::loadConfigStreamer()
{
    std::string region;
    if (!configManager->getConfigValue("Region", region)) {
        obs_log(LOG_ERROR, "Failed to get region");
        return;
    }

    std::string language = GetCurrentLanguage();

    SeventeenLiveConfigStreamerResponse response;
    if (!apiWrapper->GetConfigStreamer(region, language, response)) {
        obs_log(LOG_ERROR, "Failed to get config streamer");
        return;
    }

    configManager->setConfigStreamer(response);
}

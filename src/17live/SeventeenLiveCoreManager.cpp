#include "SeventeenLiveCoreManager.hpp"
#include <QMainWindow>

#include "SeventeenLiveMenuManager.hpp"
#include "api/SeventeenLiveApiWrappers.hpp"
#include "SeventeenLiveConfigManager.hpp"

#include "SeventeenLiveLoginDialog.hpp"
#include "SeventeenLiveStreamingDock.hpp"

#include "plugin-support.h"

#include <obs-frontend-api.h>

#include "json11.hpp"

using namespace json11;

namespace seventeenlive {

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

    if (isLogin) {
        QString openId = loginData.userInfo.openID;
        QString displayName = loginData.userInfo.displayName;

        QString username = displayName;
        if (username.isEmpty()) {
            username = openId;
        }
        menuManager->updateLoginStatus(true, username);
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
    SeventeenLiveStreamingDock* streamingDock = new SeventeenLiveStreamingDock(mainWindow, roomInfo);

    streamingDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, streamingDock);

    streamingDock->setFloating(true);
    streamingDock->move(pos.x() + size.width() - streamingDock->width() - 50, pos.y() - 50);

    if (streamingDockFirstLoad) {
        streamingDock->setVisible(true);
        streamingDockFirstLoad = false;
    } else {
        streamingDock->setVisible(!streamingDock->isVisible());
        
        QByteArray dockState = configManager->getDockState();
        if (mainWindow->isVisible())
            mainWindow->restoreState(dockState);
    }

    this->streamingDock = streamingDock;

    connect(streamingDock, &SeventeenLiveStreamingDock::createStreamClicked, this, &SeventeenLiveCoreManager::handleCreateStreamClicked);
    connect(streamingDock, &SeventeenLiveStreamingDock::createAndStartStreamClicked, this, &SeventeenLiveCoreManager::handleCreateAndStartStreamClicked);
    connect(streamingDock, &SeventeenLiveStreamingDock::stopStreamingClicked, this, &SeventeenLiveCoreManager::handleStopStreamingClicked);
    connect(streamingDock, &SeventeenLiveStreamingDock::stopPushStreamingClicked, this, &SeventeenLiveCoreManager::handleStopPushStreamingClicked);
    // 连接关闭信号到主窗口的槽函数
    connect(streamingDock, &QDockWidget::destroyed, this, &SeventeenLiveCoreManager::saveDockState);

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
    if (mainWindow && streamingDock) {
        QByteArray state = mainWindow->saveState();
        configManager->setDockState(state);
    }
}

void SeventeenLiveCoreManager::handleCreateStreamClicked(const SeventeenLiveRtmpRequest &request)
{
    // 处理创建流的逻辑
    obs_log(LOG_INFO, "handleCreateStreamClicked");

    SeventeenLiveRtmpResponse response;
    if (!apiWrapper->CreateRtmp(request, response)) {
        obs_log(LOG_ERROR, "Failed to create stream");
        return;
    }

    QString streamUrl;
    QString streamKey;

    int questionIndex = response.rtmpURL.indexOf(request.userID + "?");
    if (questionIndex != -1) {
        streamUrl = response.rtmpURL.left(questionIndex - 1);
        streamKey = response.rtmpURL.mid(questionIndex);
    } else {
        obs_log(LOG_ERROR, "Failed to parse stream url");
        return;
    }
    
    obs_log(LOG_INFO, "streamUrl: %s", streamUrl.toStdString().c_str());
    obs_log(LOG_INFO, "streamKey: %s", streamKey.toStdString().c_str());

    configManager->setStreamingInfo(response.liveStreamID.toStdString(), streamUrl.toStdString(), streamKey.toStdString());

    currLiveStreamID = response.liveStreamID.toStdString();
    currUserID = request.userID.toStdString();

    streamingDock->updateStreamingStatus(SeventeenLiveStreamingStatus::Live);
}

void SeventeenLiveCoreManager::handleCreateAndStartStreamClicked(const SeventeenLiveRtmpRequest &request)
{
    // 处理创建并开始流的逻辑
    obs_log(LOG_INFO, "handleCreateAndStartStreamClicked");

    handleCreateStreamClicked(request);

    // 开始流
    std::string liveStreamID, streamUrl, streamKey;
    if (!configManager->getStreamingInfo(liveStreamID, streamUrl, streamKey)) {
        obs_log(LOG_ERROR, "Failed to get live stream id");
        return;
    }

    if (!apiWrapper->StartStream(liveStreamID, currUserID)) {
        obs_log(LOG_ERROR, "Failed to start stream");
        return;
    }
    startStreaming(liveStreamID, streamUrl, streamKey);

    streamingDock->updateStreamingStatus(SeventeenLiveStreamingStatus::Streaming);
}

void SeventeenLiveCoreManager::startStreaming(const std::string &liveStreamID, const std::string &streamUrl, const std::string &streamKey)
{
    // 处理开始流的逻辑
    obs_log(LOG_INFO, "startStreaming %s", liveStreamID.c_str());

    // 获取OBS输出
    obs_output_t *streamOutput = obs_frontend_get_streaming_output();
    if (!streamOutput) {
        obs_log(LOG_ERROR, "Failed to get streaming output");
        return;
    }

    // 获取OBS服务
    obs_service_t* service = obs_service_create("rtmp_custom", "default_service", NULL, NULL);
    
    // 设置流媒体URL和密钥
    obs_data_t *settings = obs_service_get_settings(service);
    obs_data_set_string(settings, "server", streamUrl.c_str());
    obs_data_set_string(settings, "key", streamKey.c_str());
    
    // 应用设置
    obs_service_update(service, settings);
    obs_data_release(settings);

    obs_frontend_set_streaming_service(service);

    // 将服务应用到输出
    obs_output_set_service(streamOutput, service);

    obs_frontend_save_streaming_service();

    // 开始推流
    if (!obs_output_start(streamOutput)) {
        obs_log(LOG_ERROR, "Failed to start streaming");
        const char* error = obs_output_get_last_error(streamOutput);
        if (error) {
            obs_log(LOG_ERROR, "Error: %s", error);
        }
        obs_output_release(streamOutput);
    } else {
        obs_log(LOG_INFO, "Streaming started successfully");
    }

    // 释放资源
    obs_service_release(service);

    obs_frontend_save();
}

void SeventeenLiveCoreManager::handleStopStreamingClicked()
{
    // 处理停止流的逻辑
    obs_log(LOG_INFO, "handleStopStreamingClicked");

    stopStreaming();

    SeventeenLiveCloseLiveRequest request;
    request.reason = "normalEnd";
    request.userID = QString::fromStdString(currUserID);

    if (!apiWrapper->StopStream(currLiveStreamID, request)) {
        obs_log(LOG_ERROR, "Failed to stop stream");
        return;
    }

    configManager->clearStreamingInfo();

    currLiveStreamID = "";

    streamingDock->updateStreamingStatus(SeventeenLiveStreamingStatus::NotStarted);
}

void SeventeenLiveCoreManager::handleStopPushStreamingClicked()
{
    // 处理停止推流的逻辑
    obs_log(LOG_INFO, "handleStopPushStreamingClicked");
    stopStreaming();

    streamingDock->updateStreamingStatus(SeventeenLiveStreamingStatus::Live);
}

void SeventeenLiveCoreManager::stopStreaming()
{
    // 处理停止流的逻辑
    obs_log(LOG_INFO, "stopStreaming");

    if (!obs_frontend_streaming_active()) {
        obs_log(LOG_ERROR, "Streaming is not active");
        return;
    }

    // 获取OBS输出
    obs_output_t *streamOutput = obs_frontend_get_streaming_output();
    if (!streamOutput) {
        obs_log(LOG_ERROR, "Failed to get streaming output");
        return;
    }
    // 停止推流
    obs_output_stop(streamOutput);
    // 释放资源
    obs_output_release(streamOutput);

}

} // namespace seventeenlive

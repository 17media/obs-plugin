#include "OneSevenLiveCoreManager.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QList>
#include <QMainWindow>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <nlohmann/json.hpp>
#include <thread>

#include "../diag/ui/DiagnosticsDialog.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveHttpServer.hpp"
#include "OneSevenLiveLoginDialog.hpp"
#include "OneSevenLiveMenuManager.hpp"
#include "OneSevenLiveUpdateManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "chat/OneSevenLiveChatMessageHandler.hpp"
#include "chat/OneSevenLiveChatWidget.hpp"
#include "multi-rtmp/OneSevenLiveMultiRtmpManager.hpp"
#include "multi-rtmp/ui/OneSevenLiveMultiRtmpDock.hpp"
#include "plugin-support.h"
#include "preview/OneSevenLivePreviewDock.hpp"
#include "rockzone/OneSevenLiveRockZoneDock.hpp"
#include "streaming/OneSevenLiveStreamManager.hpp"
#include "streaming/OneSevenLiveStreamingDock.hpp"
#include "streamlist/OneSevenLiveStreamListDock.hpp"
#include "twitch/OneSevenLiveTwitchAuth.hpp"
#include "utility/Common.hpp"
#include "utility/Meta.hpp"
#include "websocket/OneSevenLiveWebsocketServer.hpp"
#include "websocket/WsMessage.hpp"
#include "youtube/OneSevenLiveYouTubeAuth.hpp"
// Chat clients

#include "api/OneSevenLiveAblyChatClient.hpp"
#include "twitch/OneSevenLiveTwitchChatClient.hpp"
#include "websocket/WebsocketUtils.hpp"
#include "youtube/OneSevenLiveYouTubeChatClient.hpp"
#include "youtube/OneSevenLiveYouTubeClient.hpp"

using Json = nlohmann::json;
using namespace std;

static const int INITIAL_DOCK_WIDTH = 450;
static const int INITIAL_DOCK_HEIGHT = 550;

static bool isDockOpen(QDockWidget* dock) {
    return dock && dock->toggleViewAction() && dock->toggleViewAction()->isChecked();
}

// Initialize static member variables
OneSevenLiveCoreManager* OneSevenLiveCoreManager::instance = nullptr;
std::once_flag OneSevenLiveCoreManager::instanceOnceFlag;

OneSevenLiveCoreManager& OneSevenLiveCoreManager::getInstance(QMainWindow* mainWindow) {
    // Use std::call_once for thread-safe singleton creation
    std::call_once(instanceOnceFlag, [mainWindow]() {
        if (mainWindow == nullptr) {
            throw std::runtime_error(
                "mainWindow parameter must be provided on first call to getInstance");
        }
        instance = new OneSevenLiveCoreManager(mainWindow);
    });
    return *instance;
}

void OneSevenLiveCoreManager::destroyInstance() {
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

OneSevenLiveCoreManager::OneSevenLiveCoreManager(QMainWindow* mainWindow_)
    : mainWindow(mainWindow_), initialized(false), multiRtmpDockFirstLoad(true) {}

OneSevenLiveCoreManager::~OneSevenLiveCoreManager() {
    // Ensure shutdown is called before destruction
    if (initialized) {
        shutdown();
    }
}

bool OneSevenLiveCoreManager::initialize() {
    // Prevent duplicate initialization
    if (initialized) {
        return true;
    }

    obs_log(LOG_INFO, "[17Live Core] Initializing OneSevenLiveCoreManager...");

    m_cancelFlag.store(false);

    // Run network diagnostics to check API connectivity
    obs_log(LOG_INFO, "[17Live Core] Running startup network diagnostics...");
    NetworkDiagnostics::runStartupDiagnostics(ONESEVENLIVE_API_URL);

    // Initialize and start HTTP server
    // "html" is the path relative to obs_get_module_data_path()
    httpServer_ =
        std::make_unique<OneSevenLiveHttpServer>("localhost", 0, "html/chat", "17Live HTTP Server");
    if (!httpServer_) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to create HTTP server instance");
        return false;
    }

    if (!httpServer_->start()) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to start HTTP server");
        // Decide whether to interrupt the entire initialization due to HTTP server startup
        // failure based on requirements return false;
    } else {
        obs_log(LOG_INFO, "[17Live Core] HTTP server started successfully");
    }

    // Initialize and start WebSocket server
    websocketServer_ = std::make_shared<OneSevenLiveWebsocketServer>("localhost", 0);
    if (!websocketServer_) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to create WebSocket server instance");
        return false;
    }

    if (!websocketServer_->start()) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to start WebSocket server");
        // Continue initialization even if WebSocket server fails
    } else {
        obs_log(LOG_INFO, "[17Live Core] WebSocket server started successfully on port %d",
                websocketServer_->getPort());
    }

    // Set up WebSocket server callbacks
    websocketServer_->setMessageCallback(std::bind(&OneSevenLiveCoreManager::handleWebsocketMessage,
                                                   this, std::placeholders::_1,
                                                   std::placeholders::_2));

    websocketServer_->setConnectionCallback(
        std::bind(&OneSevenLiveCoreManager::handleWebsocketConnectionChanged, this,
                  std::placeholders::_1, std::placeholders::_2));

    // Initialize configuration manager
    configManager = std::make_unique<OneSevenLiveConfigManager>();
    if (!configManager) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to create config manager instance");
        return false;
    }

    if (!configManager->initialize()) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to initialize config manager");
        return false;
    }

    // Initialize API wrapper before creating stream manager
    OneSevenLiveLoginData loginData;
    configManager->getLoginData(loginData);

    bool isLogin = false;

    if (!loginData.jwtAccessToken.isEmpty()) {
        apiWrapper =
            std::make_unique<OneSevenLiveApiWrappers>(loginData.jwtAccessToken.toStdString());
        apiWrapper->setCancelFlag(&m_cancelFlag);

        isLogin = checkLoginStatus();
    }

    // if not login, initialize apiWrapper without token
    if (!isLogin) {
        apiWrapper = std::make_unique<OneSevenLiveApiWrappers>();
        apiWrapper->setCancelFlag(&m_cancelFlag);
    }

    // Instantiate auth handlers
    twitchAuth = std::make_unique<OneSevenLiveTwitchAuth>(this);
    youtubeAuth = std::make_unique<OneSevenLiveYouTubeAuth>(this);
    connect(youtubeAuth.get(), &OneSevenLiveYouTubeAuth::authorizationCompleted, this,
            [this](const QString& token) {
                if (!token.isEmpty()) {
                    if (youtubeApiClient) {
                        youtubeApiClient->setAccessToken(token);
                        youtubeApiClient->retryLastRequest();
                    }
                    if (youtubeChatClient) {
                        youtubeChatClient->setAccessToken(token);
                        youtubeChatClient->startDiscovery();
                    }
                }
            });

    // Load tokens from config and schedule checks/refreshes
    {
        // Twitch: load access token for status check
        QString twAccess;
        qint64 twFetched{0};
        if (configManager->getTwitchTokens(twAccess, twFetched)) {
            if (!twAccess.isEmpty()) {
                twitchAuth->setTokens(twAccess, QString());
                obs_log(LOG_INFO, "[17Live Core] Loaded Twitch access token from config");
            }
        }
    }

    {
        QString ytAccess;
        int ytExpiresIn{0};
        qint64 ytFetchedAt{0};
        const bool hasAccess =
            configManager->getYouTubeAccessToken(ytAccess, ytExpiresIn, ytFetchedAt) &&
            !ytAccess.isEmpty();

        QString ytRefresh;
        int ytRefreshExpiresIn{0};
        qint64 ytRefreshFetchedAt{0};
        const bool hasRefresh = configManager->getYouTubeRefreshToken(ytRefresh, ytRefreshExpiresIn,
                                                                      ytRefreshFetchedAt) &&
                                !ytRefresh.isEmpty();

        const qint64 nowEpoch = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();

        if (hasAccess) {
            youtubeAuth->setAccessToken(ytAccess);
            youtubeAuth->scheduleAutoRefresh(ytExpiresIn, ytFetchedAt, ytRefreshExpiresIn,
                                             ytRefreshFetchedAt);
        } else if (hasRefresh) {
            const bool hasExpiry = ytRefreshExpiresIn > 0;
            const bool notExpired =
                hasExpiry ? (nowEpoch < ytRefreshFetchedAt + ytRefreshExpiresIn) : true;
            if (notExpired) {
                QTimer::singleShot(0, youtubeAuth.get(),
                                   &OneSevenLiveYouTubeAuth::refreshAccessTokenAsync);
            } else {
                configManager->clearYouTubeAccessToken();
                configManager->clearYouTubeRefreshToken();
            }
        }
    }

    // Load gifts from saved config into memory map for fast lookup
    loadGiftsFromConfig();

    // Initialize menu manager
    menuManager = std::make_unique<OneSevenLiveMenuManager>(mainWindow);
    if (!menuManager) {
        obs_log(LOG_ERROR, "Failed to create menu manager");
        return false;
    }
    // connect menuManager's loginClicked signal to handleLoginClicked slot
    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::loginClicked, this,
                     &OneSevenLiveCoreManager::handleLoginClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::logoutClicked, this,
                     &OneSevenLiveCoreManager::handleLogoutClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::streamingClicked, this,
                     &OneSevenLiveCoreManager::handleStreamingClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::chatRoomClicked, this,
                     &OneSevenLiveCoreManager::handleChatRoomClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::liveListClicked, this,
                     &OneSevenLiveCoreManager::handleLiveListClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::rockZoneClicked, this,
                     &OneSevenLiveCoreManager::handleRockZoneClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::multiRtmpClicked, this,
                     &OneSevenLiveCoreManager::handleMultiRtmpClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::previewDockClicked, this,
                     &OneSevenLiveCoreManager::handlePreviewDockClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::checkUpdateClicked, this,
                     &OneSevenLiveCoreManager::handleCheckUpdateClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::diagnosticsClicked, this,
                     &OneSevenLiveCoreManager::handleDiagnosticsClicked);

    // Initialize update manager
    updateManager = new OneSevenLiveUpdateManager(this);

    // Connect update manager signals
    QPointer<OneSevenLiveCoreManager> self = this;
    QObject::connect(
        updateManager, &OneSevenLiveUpdateManager::updateAvailable, this,
        [self](const QString& latestVersion, const QJsonArray& assets) {
            if (!self)
                return;
            UNUSED_PARAMETER(assets);
            QMessageBox msgBox(self->mainWindow);
            msgBox.setWindowTitle(obs_module_text("Update.NewVersionFound"));
            msgBox.setText(
                QString(obs_module_text("Update.NewVersionFound.Message")).arg(latestVersion));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::Yes);

            if (msgBox.exec() == QMessageBox::Yes) {
                // open download page obs_module_text("Menu.CheckUpdate.Url")
                QDesktopServices::openUrl(QUrl(obs_module_text("Menu.CheckUpdate.Url")));
            }
        });

    QObject::connect(updateManager, &OneSevenLiveUpdateManager::updateNotAvailable, this, [self]() {
        if (self)
            obs_log(LOG_INFO, "Update check: no new version available.");
    });

    QObject::connect(updateManager, &OneSevenLiveUpdateManager::updateCheckFailed, this,
                     [self](const QString& error) {
                         if (self)
                             obs_log(LOG_WARNING, "Update check failed: %s",
                                     error.toUtf8().constData());
                     });

    // Load meta data
    if (!LoadMetaData()) {
        obs_log(LOG_ERROR, "Failed to load meta data");
        return false;
    }

    QTimer::singleShot(0, updateManager, &OneSevenLiveUpdateManager::checkForUpdates);

    isStartupRestore = true;

    // Handle login state during initialization
    if (isLogin) {
        configManager->getLoginData(loginData);

        // Use the new centralized login state handler for logged in users
        handleLoginStateChanged(true, loginData);
    }

    initialized = true;

    return true;
}

void OneSevenLiveCoreManager::handleCheckUpdateClicked() {
    if (updateManager) {
        updateManager->checkForUpdates();
    }
}

void OneSevenLiveCoreManager::handleDiagnosticsClicked() {
    // Create and show the diagnostics dialog
    seventeen::diag::ui::DiagnosticsDialog dialog(mainWindow);
    dialog.exec();
}

void OneSevenLiveCoreManager::handleWebsocketMessage(const std::string& clientId,
                                                     const std::string& message) {
    WsMessage m;
    if (!WsMessage::parse(message, m)) {
        obs_log(LOG_WARNING, "[17Live WebSocket Server] JSON parse error in message from %s",
                clientId.c_str());
        return;
    }
    // output m for debug
    // obs_log(LOG_INFO, "[17Live WebSocket Server] Message from %s: %s", clientId.c_str(),
    //         m.dump().c_str());
    const bool hasServer = (this->websocketServer_ && this->websocketServer_->is_running());
    if (m.type.empty() || !hasServer) {
        return;
    }
    if (m.is(ws::TypeAction)) {
        const std::string actionType = m.payloadString("type");
        if (actionType == ws::ActionRegisterChatDock) {
            {
                std::lock_guard<std::mutex> lock(chatQueueMutex);
                chatDockClientId = clientId;
            }
            obs_log(LOG_INFO, "[ChatQueue] ChatDock registered client=%s", clientId.c_str());
            flushChatEventQueue();
            return;
        }
    } else if (m.is(ws::EventAblyChatMessage)) {
        const std::string roomID = m.payloadString("roomID");
        const std::string data = m.payloadString("data");
        if (roomID.empty() || data.empty()) {
            obs_log(LOG_WARNING,
                    "[17Live WebSocket Server] Missing roomID or data in Ably message");
            return;
        }
        // Process Ably chat message via unified handler
        {
            nlohmann::json wrapper;
            wrapper["messages"] = nlohmann::json::array({nlohmann::json{{"data", data}}});
            OneSevenLiveChatMessageHandler handler;
            handler.handleRaw(wrapper.dump());
        }
    }
}

void OneSevenLiveCoreManager::handleWebsocketConnectionChanged(const std::string& clientId,
                                                               bool connected) {
    if (connected) {
        obs_log(LOG_INFO, "[17Live WebSocket] Client %s connected", clientId.c_str());
        flushChatEventQueue();
    } else {
        obs_log(LOG_INFO, "[17Live WebSocket] Client %s disconnected", clientId.c_str());
        std::lock_guard<std::mutex> lock(chatQueueMutex);
        if (!chatDockClientId.empty() && chatDockClientId == clientId) {
            chatDockClientId.clear();
        }
    }
}

void OneSevenLiveCoreManager::load17LiveConfig(const OneSevenLiveLoginData& loginData) {
    std::string region = loginData.userInfo.region.toStdString();
    if (region.empty()) {
        region = "TW";  // Default region
    }

    std::string language = GetCurrentLanguage();

    // Call API to get configuration
    Json configJson;
    if (apiWrapper->GetConfig(region, language, configJson)) {
        // Save configuration
        configManager->setConfig(configJson);
        obs_log(LOG_INFO, "Config loaded successfully");
    } else {
        obs_log(LOG_ERROR, "Failed to load config from API");
    }
}

void OneSevenLiveCoreManager::shutdown() {
    m_cancelFlag.store(true);
    if (!initialized) {
        return;
    }

    // Stop YouTube streams in MultiRTMP Manager first to ensure "complete" transition
    {
        auto* multiMgr = OneSevenLiveMultiRtmpManager::peekInstance();
        if (multiMgr && multiMgr->isInitialized()) {
            bool stoppedYouTube = false;
            auto activeStreams = multiMgr->getActiveStreamIds();
            for (const auto& streamId : activeStreams) {
                auto cfg = multiMgr->getStreamConfig(streamId);
                if (cfg.streamName == "YouTube") {
                    multiMgr->stopStream(streamId);
                    stoppedYouTube = true;
                }
            }

            if (stoppedYouTube) {
                // Give some time for the API request to be dispatched
                QElapsedTimer t;
                t.start();
                while (t.elapsed() < 1000) {
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                }
            }
        }
    }

    m_cancelFlag.store(true);

    if (obs_frontend_streaming_active()) {
        if (streamManager) {
            streamManager->stopOBSStreaming();
        } else {
            obs_frontend_streaming_stop();
            // Don't sleep on main thread
            // std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        QElapsedTimer t;
        t.start();
        while (obs_frontend_streaming_active() && t.elapsed() < 5000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
    }

    // Save dock state before closing any docks
    saveDockState();

    closeAllDocks();

    // After docks are closed and UI timers stopped, shutdown MultiRTMP manager
    {
        auto* multiMgr = OneSevenLiveMultiRtmpManager::peekInstance();
        if (multiMgr) {
            multiMgr->shutdown();
            OneSevenLiveMultiRtmpManager::destroyInstance();
            // Don't sleep on main thread
            // std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    // Stop WebSocket server
    if (websocketServer_) {
        websocketServer_->stop();
        obs_log(LOG_INFO, "[17Live Core] WebSocket server stopped");
    }

    // Stop HTTP server
    if (httpServer_) {
        // Stop synchronous to ensure clean shutdown before destroying other resources
        httpServer_->stop();
        obs_log(LOG_INFO, "[17Live Core] HTTP server stopped");
    }

    // Clean up menu manager resources
    if (menuManager) {
        menuManager->cleanup();
    }

    if (ytChatDiscoverTimer) {
        ytChatDiscoverTimer->stop();
        ytChatDiscoverTimer->deleteLater();
        ytChatDiscoverTimer = nullptr;
    }

    initialized = false;
}

QMainWindow* OneSevenLiveCoreManager::getMainWindow() const {
    return mainWindow;
}

OneSevenLiveMenuManager* OneSevenLiveCoreManager::getMenuManager() const {
    return menuManager.get();
}

OneSevenLiveApiWrappers* OneSevenLiveCoreManager::getApiWrapper() const {
    return apiWrapper.get();
}

OneSevenLiveConfigManager* OneSevenLiveCoreManager::getConfigManager() const {
    return configManager.get();
}

OneSevenLiveStreamManager* OneSevenLiveCoreManager::getStreamManager() const {
    return streamManager.get();
}

OneSevenLiveWebsocketServer* OneSevenLiveCoreManager::getWebsocketServer() const {
    return websocketServer_.get();
}

OneSevenLiveHttpServer* OneSevenLiveCoreManager::getHttpServer() const {
    return httpServer_.get();
}

OneSevenLiveTwitchAuth* OneSevenLiveCoreManager::getTwitchAuth() const {
    return twitchAuth.get();
}

OneSevenLiveYouTubeAuth* OneSevenLiveCoreManager::getYouTubeAuth() const {
    return youtubeAuth.get();
}

OneSevenLiveYouTubeChatClient* OneSevenLiveCoreManager::getYouTubeChatClient() const {
    return youtubeChatClient.get();
}

OneSevenLiveYouTubeClient* OneSevenLiveCoreManager::getYouTubeApiClient() const {
    return youtubeApiClient.get();
}

OneSevenLiveTwitchChatClient* OneSevenLiveCoreManager::getTwitchChatClient() const {
    return twitchChatClient.get();
}

OneSevenLiveAblyChatClient* OneSevenLiveCoreManager::getAblyChatClient() const {
    return ablyChatClient.get();
}

void OneSevenLiveCoreManager::createYouTubeChatClient() {
    if (youtubeApiClient && youtubeChatClient) {
        return;
    }
    if (!youtubeApiClient) {
        youtubeApiClient = std::make_unique<OneSevenLiveYouTubeClient>(this);
        if (youtubeAuth && youtubeAuth->hasValidToken()) {
            youtubeApiClient->setAccessToken(youtubeAuth->getAccessToken());
        }
        youtubeApiClient->setTimeout(12000);
    }
    if (!youtubeChatClient) {
        youtubeChatClient = std::make_unique<OneSevenLiveYouTubeChatClient>(this);
        youtubeChatClient->setTimeout(12000);
        youtubeChatClient->setMaxRetries(3);
        youtubeChatClient->setRetryDelay(1000);
        if (youtubeAuth && youtubeAuth->hasValidToken()) {
            youtubeChatClient->setAccessToken(youtubeAuth->getAccessToken());
        }
        youtubeChatClient->setApiClient(youtubeApiClient.get());
    }
}

void OneSevenLiveCoreManager::createTwitchChatClient() {
    if (twitchChatClient) {
        return;
    }

    twitchChatClient = std::make_unique<OneSevenLiveTwitchChatClient>(this);

    // If we have Twitch auth, try to connect automatically when needed
    if (twitchAuth) {
        const QString oauth = twitchAuth->getAccessToken();
        QString login;
        QString displayName;
        QString userId;
        QString profileImageUrl;
        QString email;
        int viewCount = 0;
        if (configManager && configManager->getTwitchUserInfo(userId, login, displayName,
                                                              profileImageUrl, email, viewCount)) {
            if (!login.isEmpty() && !oauth.isEmpty()) {
                twitchChatClient->connectToChat(login, oauth);
            }
        }
    }
}

void OneSevenLiveCoreManager::createAblyChatClient() {
    if (ablyChatClient)
        return;
    ablyChatClient = std::make_unique<OneSevenLiveAblyChatClient>(this);
}

void OneSevenLiveCoreManager::destroyAblyChatClient() {
    if (ablyChatClient) {
        ablyChatClient->disconnect();
        ablyChatClient.reset();
    }
}

void OneSevenLiveCoreManager::connectAblyChat(const QString& roomId, const QString& token) {
    createAblyChatClient();
    if (!ablyChatClient)
        return;
    ablyChatClient->setRoomId(roomId);
    if (!token.isEmpty())
        ablyChatClient->setAblyToken(token);

    QPointer<OneSevenLiveCoreManager> self = this;
    ablyChatClient->setAuthCallback([self](const QString& rid, nlohmann::json& out) {
        if (!self)
            return false;
        auto* api = self->getApiWrapper();
        if (!api)
            return false;
        return api->GetAblyToken(rid.toStdString(), out);
    });
    ablyChatClient->connect();
}

void OneSevenLiveCoreManager::disconnectAblyChat() {
    if (ablyChatClient)
        ablyChatClient->disconnect();
}

void OneSevenLiveCoreManager::refreshRockZoneUserList() {
    if (rockZoneDock) {
        rockZoneDock->refreshUserList();
    }
}

void OneSevenLiveCoreManager::enqueueOrBroadcastChatEvent(const QString& type,
                                                          const nlohmann::json& payload) {
    obs_log(LOG_DEBUG, "Enqueueing chat event: %s payload: %s", type.toStdString().c_str(),
            payload.dump().c_str());

    std::lock_guard<std::mutex> lock(chatQueueMutex);

    auto* ws = getWebsocketServer();
    if (ws && ws->is_running() && !chatDockClientId.empty()) {
        obs_log(LOG_DEBUG, "Sending chat event to chat dock client %s", chatDockClientId.c_str());
        auto ids = ws->getConnectedClientIds();
        if (std::find(ids.begin(), ids.end(), chatDockClientId) != ids.end()) {
            ws->sendMessageToClient(chatDockClientId,
                                    WsMessage{type.toStdString(), payload}.dump());
            return;
        }
    }
    chatEventQueue.push_back(WsMessage{type.toStdString(), payload});
    if (chatEventQueue.size() > chatQueueMaxSize) {
        chatEventQueue.pop_front();
    }
    obs_log(LOG_DEBUG, "[ChatQueue] Enqueued type=%s size=%zu", type.toUtf8().constData(),
            chatEventQueue.size());
}

void OneSevenLiveCoreManager::flushChatEventQueue() {
    auto* ws = getWebsocketServer();
    if (!ws || !ws->is_running())
        return;
    
    std::lock_guard<std::mutex> lock(chatQueueMutex);

    if (chatDockClientId.empty())
        return;
    auto ids = ws->getConnectedClientIds();
    if (std::find(ids.begin(), ids.end(), chatDockClientId) == ids.end())
        return;
    obs_log(LOG_DEBUG, "[ChatQueue] Flushing %zu events to ChatDock client %s",
            chatEventQueue.size(), chatDockClientId.c_str());
    while (!chatEventQueue.empty()) {
        const auto& m = chatEventQueue.front();
        // std::string payloadStr = m.payload.dump();
        // if (payloadStr.size() > 512) {
        //     payloadStr = payloadStr.substr(0, 512) + "...";
        // }
        // obs_log(LOG_INFO, "[ChatQueue] Flush item type=%s payload=%s", m.type.c_str(),
        //         payloadStr.c_str());
        ws->sendMessageToClient(chatDockClientId, m.dump());
        chatEventQueue.pop_front();
    }
    obs_log(LOG_DEBUG, "[ChatQueue] Flush complete");
}

void OneSevenLiveCoreManager::destroyYouTubeChatClient() {
    if (youtubeChatClient) {
        youtubeChatClient->stopChatPolling();
        youtubeChatClient.reset();
    }
    if (youtubeApiClient) {
        youtubeApiClient.reset();
    }
}

void OneSevenLiveCoreManager::destroyTwitchChatClient() {
    if (twitchChatClient) {
        twitchChatClient->leaveAllChannels();
        twitchChatClient->disconnectFromChat();
        twitchChatClient.reset();
    }
}

void OneSevenLiveCoreManager::startYouTubeChatPolling(const QString& liveChatId) {
    createYouTubeChatClient();
    if (!youtubeChatClient) {
        return;
    }
    if (youtubeAuth && youtubeAuth->hasValidToken()) {
        youtubeChatClient->setAccessToken(youtubeAuth->getAccessToken());
    }
    youtubeChatClient->startChatPolling(liveChatId);
}

void OneSevenLiveCoreManager::stopYouTubeChatPolling() {
    if (youtubeChatClient) {
        youtubeChatClient->stopChatPolling();
    }
}

void OneSevenLiveCoreManager::connectTwitchChatClient(const QString& channel) {
    if (!twitchChatClient) {
        createTwitchChatClient();
    }
    if (!twitchChatClient) {
        obs_log(LOG_ERROR, "Failed to create TwitchChatClient");
        return;
    }

    // If not connected, authenticate and optionally join a channel
    if (!twitchChatClient->isConnected()) {
        QString login;
        QString displayName;
        QString userId;
        QString profileImageUrl;
        QString email;
        int viewCount = 0;
        QString oauth = twitchAuth ? twitchAuth->getAccessToken() : QString();
        if (configManager && configManager->getTwitchUserInfo(userId, login, displayName,
                                                              profileImageUrl, email, viewCount)) {
            if (!login.isEmpty() && !oauth.isEmpty()) {
                twitchChatClient->connectToChat(login, oauth);
            }
        }
    }

    if (!channel.isEmpty()) {
        twitchChatClient->joinChannel(channel);
    }
}

void OneSevenLiveCoreManager::disconnectTwitchChatClient() {
    if (twitchChatClient) {
        twitchChatClient->leaveAllChannels();
        twitchChatClient->disconnectFromChat();
    }
}

bool OneSevenLiveCoreManager::handleLoginClicked() {
    m_cancelFlag.store(false);
    OneSevenLiveLoginDialog dialog(mainWindow, getApiWrapper());

    // Connect login success signal to main window slot function
    QObject::connect(&dialog, &OneSevenLiveLoginDialog::loginSuccess, this,
                     &OneSevenLiveCoreManager::handleLoginSuccess);

    return dialog.exec() == QDialog::Accepted;
}

void OneSevenLiveCoreManager::handleLoginSuccess(const OneSevenLiveLoginData& loginData) {
    obs_log(LOG_INFO, "handleLoginSuccess");

    if (!configManager->setLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to save login data");
        return;
    }

    QPointer<OneSevenLiveCoreManager> self = this;
    QMetaObject::invokeMethod(
        this,
        [self, loginData]() {
            if (self)
                self->handleLoginStateChanged(true, loginData);
        },
        Qt::QueuedConnection);
}

void OneSevenLiveCoreManager::handleLoginStateChanged(bool isLoggedIn,
                                                      const OneSevenLiveLoginData& loginData) {
    obs_log(LOG_INFO, "handleLoginStateChanged: %s", isLoggedIn ? "logged in" : "logged out");

    if (isLoggedIn) {
        performLoginOperations(loginData);
    } else {
        performLogoutOperations();
    }
}

void OneSevenLiveCoreManager::performLoginOperations(const OneSevenLiveLoginData& loginData) {
    obs_log(LOG_INFO, "performLoginOperations");
    loggingIn.store(true);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    // if apiWrappers token is empty or not equal to loginData.accessToken.toStdString(), update it
    if (apiWrapper->getToken().empty() ||
        apiWrapper->getToken() != loginData.jwtAccessToken.toStdString()) {
        apiWrapper->setToken(loginData.jwtAccessToken.toStdString());
    }

    // Initialize stream manager (after apiWrapper is ready)
    streamManager =
        std::make_unique<OneSevenLiveStreamManager>(apiWrapper.get(), configManager.get(), this);
    if (!streamManager) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to create stream manager instance");
        loggingIn.store(false);
        return;
    }

    QPointer<OneSevenLiveCoreManager> self = this;
    QTimer::singleShot(0, this, [self]() {
        if (self)
            self->loadGifts();
    });

    // Update menu with user info
    QString username = loginData.userInfo.displayName;
    if (username.isEmpty()) {
        username = loginData.userInfo.openID;
    }
    menuManager->updateLoginStatus(true, username);

    QTimer::singleShot(0, this, [self, loginData]() {
        if (self)
            self->load17LiveConfig(loginData);
    });

    // Restore dock states if this is during startup and there are saved states
    if (isStartupRestore) {
        QTimer::singleShot(0, this, [self]() {
            if (self) {
                self->restoreDockStatesOnLogin();
                self->isStartupRestore = false;
            }
        });
    }

    QTimer::singleShot(0, this, [self]() {
        if (!self)
            return;
        self->createYouTubeChatClient();
        self->createTwitchChatClient();
        self->setConnection();
    });

    QTimer::singleShot(0, this, [self]() {
        if (!self)
            return;
        if (self->streamManager && self->apiWrapper) {
            const qint64 rid = self->streamManager->getRoomID();
            if (rid > 0) {
                self->m_cancelFlag.store(false);
                self->connectAblyChat(QString::number(rid), QString());
            }
        }
    });

    // discovery is managed by YouTubeChatClient
    loggingIn.store(false);
}

void OneSevenLiveCoreManager::setConnection() {
    connect(
        streamManager.get(), &OneSevenLiveStreamManager::streamStatusChanged, this,
        [this](OneSevenLiveStreamingStatus status_) {
            status = status_;
            if (liveListDock) {
                liveListDock->setStatus(status_);
            }

            // Handle stream status change
            if (status_ == OneSevenLiveStreamingStatus::Streaming) {
                // Start timer to check stream status every 30 seconds
                if (!streamCheckTimer) {
                    streamCheckTimer = new QTimer(this);
                    connect(streamCheckTimer, &QTimer::timeout, this, [this]() {
                        if (streamCheckInFlight.load()) {
                            return;
                        }
                        std::string liveStreamID;
                        if (!configManager ||
                            !configManager->getConfigValue("LiveStreamID", liveStreamID)) {
                            return;
                        }
                        streamCheckInFlight.store(true);
                        QPointer<OneSevenLiveCoreManager> self = this;
                        ScheduleOBSTask([self, liveStreamID]() {
                            if (!self)
                                return;
                            bool ok = false;
                            try {
                                if (self->apiWrapper) {
                                    ok = self->apiWrapper->CheckStream(liveStreamID);
                                }
                            } catch (...) {
                                ok = false;
                            }
                            if (self) {
                                QMetaObject::invokeMethod(
                                    self,
                                    [self, ok]() {
                                        if (!self)
                                            return;
                                        if (!ok) {
                                            self->consecutiveFailureCount++;
                                            obs_log(
                                                LOG_WARNING,
                                                "Stream check failed. Consecutive failures: %d/%d",
                                                self->consecutiveFailureCount,
                                                MAX_CONSECUTIVE_FAILURES);
                                            if (self->consecutiveFailureCount >=
                                                MAX_CONSECUTIVE_FAILURES) {
                                                obs_log(
                                                    LOG_ERROR,
                                                    "Stream check failed %d times consecutively. "
                                                    "Showing auto-close confirmation.",
                                                    MAX_CONSECUTIVE_FAILURES);
                                                QString message =
                                                    QString(
                                                        obs_module_text(
                                                            "Live.Settings.CloseLive.Auto.Message"))
                                                        .arg(MAX_CONSECUTIVE_FAILURES);
                                                if (self->showAutoCloseConfirmation(message)) {
                                                    self->closeLive(true);
                                                    if (self->streamCheckTimer) {
                                                        self->streamCheckTimer->stop();
                                                        self->streamCheckTimer->deleteLater();
                                                        self->streamCheckTimer = nullptr;
                                                    }
                                                }
                                                self->consecutiveFailureCount = 0;
                                            }
                                        } else {
                                            if (self->consecutiveFailureCount > 0) {
                                                obs_log(LOG_INFO,
                                                        "Stream check succeeded. Resetting failure "
                                                        "count from %d to 0.",
                                                        self->consecutiveFailureCount);
                                                self->consecutiveFailureCount = 0;
                                            }
                                        }
                                        self->streamCheckInFlight.store(false);
                                    },
                                    Qt::QueuedConnection);
                            }
                        });
                    });
                }
                streamCheckTimer->start(30000);  // 30 seconds

                // trigger reload gifts when stream is live
                loadGifts();
            } else {
                if (streamCheckTimer) {
                    streamCheckTimer->stop();
                    streamCheckTimer->deleteLater();
                    streamCheckTimer = nullptr;
                    streamCheckInFlight.store(false);
                }
                if (pendingLogout.load()) {
                    pendingLogout.store(false);
                    QPointer<OneSevenLiveCoreManager> self = this;
                    QMetaObject::invokeMethod(
                        this,
                        [self]() {
                            if (self)
                                self->handleLoginStateChanged(false);
                        },
                        Qt::QueuedConnection);
                }
            }
        });
}

void OneSevenLiveCoreManager::performLogoutOperations() {
    obs_log(LOG_INFO, "performLogoutOperations");
    loggingOut.store(true);
    m_cancelFlag.store(true);

    {
        auto* ws = getWebsocketServer();
        if (ws && ws->is_running()) {
            ws->closeAllClients();
        }
    }

    // destroyYouTubeChatClient();
    destroyTwitchChatClient();
    destroyAblyChatClient();

    if (streamCheckTimer) {
        streamCheckTimer->stop();
        streamCheckTimer->deleteLater();
        streamCheckTimer = nullptr;
        streamCheckInFlight.store(false);
    }

    closeAllDocks();

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    streamManager.reset();

    // Reset login status in menu
    menuManager->updateLoginStatus(false, "");

    // Clear login data
    configManager->clearLoginData();

    // Clear third-party platform authorization data
    configManager->clearTwitchTokens();
    configManager->clearTwitchUserInfo();
    // configManager->clearYouTubeAccessToken();
    // configManager->clearYouTubeRefreshToken();

    // Clear streaming configuration
    configManager->clearStreamingInfo();
    configManager->clearWhipStreamingInfo();
    configManager->clearStreamingPullUrl();

    // Clear in-memory auth states
    if (twitchAuth) {
        twitchAuth->clearTokens();
    }
    // if (youtubeAuth) {
    //     youtubeAuth->clearToken();
    //     youtubeAuth->stopAutoRefresh();
    // }
    if (apiWrapper) {
        apiWrapper->shutdown();
    }

    if (ytChatDiscoverTimer) {
        ytChatDiscoverTimer->stop();
        ytChatDiscoverTimer->deleteLater();
        ytChatDiscoverTimer = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(chatQueueMutex);
        chatDockClientId.clear();
        chatEventQueue.clear();
    }
    
    if (apiWrapper) {
        apiWrapper->setToken(std::string());
    }

    // Destroy chat clients on logout
    loggingOut.store(false);
}

void OneSevenLiveCoreManager::restoreDockStatesOnLogin() {
    obs_log(LOG_INFO, "restoreDockStatesOnLogin");

    // Check if there are saved dock states and restore them
    QByteArray dockState = configManager->getDockState();
    if (!dockState.isEmpty() && mainWindow && mainWindow->isVisible()) {
        // Restore streaming dock if it was previously shown
        if (configManager->getDockVisibility("streaming")) {
            createStreamingDock();
        }

        // Restore live list dock if it was previously shown
        if (configManager->getDockVisibility("liveList")) {
            handleLiveListClicked();
        }

        // Restore chat room dock if it was previously shown
        if (configManager->getDockVisibility("chatRoom")) {
            handleChatRoomClicked();
        }

        // Restore rock zone dock if it was previously shown
        if (configManager->getDockVisibility("rockZone")) {
            handleRockZoneClicked();
        }

        // Restore multi-RTMP dock if it was previously shown
        if (configManager->getDockVisibility("multiRtmp")) {
            handleMultiRtmpClicked();
        }

        // Restore preview dock if it was previously shown
        if (configManager->getDockVisibility("previewDock")) {
            handlePreviewDockClicked();
        }

        // Apply the saved dock layout
        mainWindow->restoreState(dockState);

        QTimer::singleShot(0, this, [this]() {
            if (!mainWindow)
                return;

            QList<QDockWidget*> docks;
            QList<int> sizes;
            if (streamingDock) {
                docks << streamingDock;
                sizes << INITIAL_DOCK_HEIGHT;
            }
            if (liveListDock) {
                docks << liveListDock;
                sizes << INITIAL_DOCK_HEIGHT;
            }
            if (rockZoneDock) {
                docks << rockZoneDock;
                sizes << INITIAL_DOCK_HEIGHT;
            }
            if (multiRtmpDock) {
                docks << multiRtmpDock;
                sizes << INITIAL_DOCK_HEIGHT;
            }
            if (previewDock) {
                docks << previewDock;
                sizes << INITIAL_DOCK_HEIGHT;
            }
            if (chatDock) {
                docks << chatDock;
                sizes << INITIAL_DOCK_HEIGHT;
            }
            if (!docks.isEmpty())
                mainWindow->resizeDocks(docks, sizes, Qt::Vertical);
        });

        // Update menu visibility status after restoration
        if (menuManager) {
            menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                              isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                              isDockOpen(multiRtmpDock), isDockOpen(previewDock));
        }
    }
}

void OneSevenLiveCoreManager::closeAllDocks() {
    obs_log(LOG_INFO, "closeAllDocks");

    bool streamingVisible = false;
    if (streamingDock) {
        streamingVisible = isDockOpen(streamingDock);
        streamingDock->disconnect(this);
        streamingDock->close();
        delete streamingDock;
        streamingDock = nullptr;
    }
    configManager->setDockVisibility("streaming", streamingVisible);

    bool liveListVisible = false;
    if (liveListDock) {
        liveListVisible = isDockOpen(liveListDock);
        liveListDock->disconnect(this);
        liveListDock->close();
        delete liveListDock;
        liveListDock = nullptr;
    }
    configManager->setDockVisibility("liveList", liveListVisible);

    bool rockZoneVisible = false;
    if (rockZoneDock) {
        rockZoneVisible = isDockOpen(rockZoneDock);
        rockZoneDock->disconnect(this);
        rockZoneDock->close();
        delete rockZoneDock;
        rockZoneDock = nullptr;
    }
    configManager->setDockVisibility("rockZone", rockZoneVisible);

    bool chatRoomVisible = false;
    if (chatDock) {
        chatRoomVisible = isDockOpen(chatDock);
        chatDock->disconnect(this);
        OneSevenLiveChatWidget* widget = qobject_cast<OneSevenLiveChatWidget*>(chatDock->widget());
        if (widget) {
            obs_log(LOG_INFO, "Shutting down chat widget in closeAllDocks");
            widget->shutdown();
        }
        chatDock->close();
        delete chatDock;
        chatDock = nullptr;
    }
    configManager->setDockVisibility("chatRoom", chatRoomVisible);

    bool multiRtmpVisible = false;
    if (multiRtmpDock) {
        multiRtmpVisible = isDockOpen(multiRtmpDock);
        multiRtmpDock->disconnect(this);
        multiRtmpDock->close();
        delete multiRtmpDock;
        multiRtmpDock = nullptr;
    }
    configManager->setDockVisibility("multiRtmp", multiRtmpVisible);

    bool previewDockVisible = false;
    if (previewDock) {
        previewDockVisible = isDockOpen(previewDock);
        previewDock->disconnect(this);
        previewDock->close();
        delete previewDock;
        previewDock = nullptr;
    }
    configManager->setDockVisibility("previewDock", previewDockVisible);

    // Update menu visibility status after closing all docks
    if (menuManager) {
        menuManager->updateDockVisibility(false, false, false, false, false, false);
    }
}

void OneSevenLiveCoreManager::handleLogoutClicked() {
    obs_log(LOG_INFO, "handleLogoutClicked");

    if (status == OneSevenLiveStreamingStatus::Streaming) {
        auto* msgBox = new QMessageBox(mainWindow);
        msgBox->setWindowTitle(obs_module_text("Logout.Warning.Title"));
        msgBox->setText(obs_module_text("Logout.Warning.Message"));
        QPushButton* confirmButton =
            msgBox->addButton(obs_module_text("Logout.Warning.Button.Yes"), QMessageBox::YesRole);
        QPushButton* cancelButton =
            msgBox->addButton(obs_module_text("Logout.Warning.Button.No"), QMessageBox::NoRole);
        msgBox->setDefaultButton(cancelButton);

        QPointer<OneSevenLiveCoreManager> self = this;
        connect(msgBox, &QMessageBox::finished, this, [self, msgBox, confirmButton](int) {
            if (msgBox->clickedButton() != confirmButton) {
                msgBox->deleteLater();
                return;
            }
            if (self) {
                QMetaObject::invokeMethod(
                    self,
                    [self]() {
                        if (self) {
                            self->pendingLogout.store(true);
                            self->closeLive(false);
                        }
                    },
                    Qt::QueuedConnection);
            }
            msgBox->deleteLater();
        });
        msgBox->open();
        return;
    }

    QPointer<OneSevenLiveCoreManager> self = this;
    QMetaObject::invokeMethod(
        this,
        [self]() {
            if (self)
                self->handleLoginStateChanged(false);
        },
        Qt::QueuedConnection);
}

void OneSevenLiveCoreManager::closeLive(bool isAutoClose) {
    if (streamManager) {
        std::string currUserID;
        std::string currLiveStreamID;
        configManager->getConfigValue("UserID", currUserID);
        configManager->getConfigValue("LiveStreamID", currLiveStreamID);

        // Log detailed information about stream closure
        if (isAutoClose) {
            obs_log(LOG_INFO,
                    "Auto-closing live stream - UserID: %s, LiveStreamID: %s, Reason: Stream check "
                    "failures",
                    currUserID.c_str(), currLiveStreamID.c_str());
        } else {
            obs_log(LOG_INFO, "Manually closing live stream - UserID: %s, LiveStreamID: %s",
                    currUserID.c_str(), currLiveStreamID.c_str());
        }

        streamManager->stopStream(isAutoClose);
    }
}

void OneSevenLiveCoreManager::handleStreamingClicked() {
    obs_log(LOG_INFO, "handleStreamingClicked");

    if (!streamingDock) {
        createStreamingDock();
    } else {
        streamingDock->toggleViewAction()->trigger();
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                          isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                          isDockOpen(multiRtmpDock), isDockOpen(previewDock));
    }
}

void OneSevenLiveCoreManager::createStreamingDock() {
    if (streamingDock) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!configManager->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    // Create and show streaming window
    streamingDock = new OneSevenLiveStreamingDock(mainWindow, streamManager.get(), apiWrapper.get(),
                                                  configManager.get());
    streamingDock->setObjectName("OneSevenLiveStreamingDock");

    streamingDock->setMaximumWidth(600);
    streamingDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    streamingDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, streamingDock);

    // Only restore state during startup, otherwise set floating and center
    if (isStartupRestore) {
    } else {
        // First time creation or manual creation - set floating and center
        streamingDock->setFloating(true);
        streamingDock->setVisible(true);

        // Center the dock on the main window
        QRect mainWindowGeometry = mainWindow->geometry();
        int x = mainWindowGeometry.x() + (mainWindowGeometry.width() - streamingDock->width()) / 2;
        int y =
            mainWindowGeometry.y() + (mainWindowGeometry.height() - streamingDock->height()) / 2;
        streamingDock->move(x, y);
    }

    if (streamingDockFirstLoad) {
        connect(streamingDock, &OneSevenLiveStreamingDock::streamInfoSaved, this, [this]() {
            if (liveListDock) {
                liveListDock->refreshStreamList();
            }
        });

        connect(streamingDock, &QDockWidget::visibilityChanged, this, [this]() {
            menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                              isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                              isDockOpen(multiRtmpDock), isDockOpen(previewDock));
        });

        streamingDockFirstLoad = false;
    }
}

void OneSevenLiveCoreManager::handleRockZoneClicked() {
    obs_log(LOG_INFO, "handleRockZoneClicked");

    if (!rockZoneDock) {
        createRockZoneDock();
    } else {
        rockZoneDock->toggleViewAction()->trigger();
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                          isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                          isDockOpen(multiRtmpDock), isDockOpen(previewDock));
    }
}

void OneSevenLiveCoreManager::createRockZoneDock() {
    if (rockZoneDock) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!configManager->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    // Create and show rock zone window
    rockZoneDock = new OneSevenLiveRockZoneDock(mainWindow, apiWrapper.get(), configManager.get());
    rockZoneDock->setObjectName("OneSevenLiveRockZoneDock");

    rockZoneDock->setMinimumWidth(300);

    rockZoneDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    rockZoneDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, rockZoneDock);

    // Only restore state during startup, otherwise set floating and center
    if (isStartupRestore) {
        // During startup restoration, the state will be restored by initialize() method
        rockZoneDock->setVisible(true);
    } else {
        // First time creation or manual creation - set floating and center
        rockZoneDock->setFloating(true);
        rockZoneDock->setVisible(true);

        // Center the dock on the main window
        QRect mainWindowGeometry = mainWindow->geometry();
        int x = mainWindowGeometry.x() + (mainWindowGeometry.width() - rockZoneDock->width()) / 2;
        int y = mainWindowGeometry.y() + (mainWindowGeometry.height() - rockZoneDock->height()) / 2;
        rockZoneDock->move(x, y);
    }

    if (streamManager) {
        connect(streamManager.get(), &OneSevenLiveStreamManager::streamStatusChanged, this,
                [this](OneSevenLiveStreamingStatus status) {
                    if (status == OneSevenLiveStreamingStatus::NotStarted && rockZoneDock) {
                        rockZoneDock->clearUserList();
                    }
                });
        connect(streamManager.get(), &OneSevenLiveStreamManager::obsStreamStopped, this,
                [this](int, const QString&) {
                    if (rockZoneDock) {
                        rockZoneDock->clearUserList();
                    }
                });
    }

    if (rockZoneDockFirstLoad) {
        // When dock is closed, uncheck menu item status
        connect(rockZoneDock, &QDockWidget::visibilityChanged, this, [this]() {
            menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                              isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                              isDockOpen(multiRtmpDock), isDockOpen(previewDock));
        });

        rockZoneDockFirstLoad = false;
    }
}

void OneSevenLiveCoreManager::handleLiveListClicked() {
    obs_log(LOG_INFO, "handleLiveListClicked");

    if (!liveListDock) {
        liveListDock = new OneSevenLiveStreamListDock(mainWindow, configManager.get(), status);
        liveListDock->setObjectName("OneSevenLiveStreamListDock");
        liveListDock->setMinimumWidth(300);
        liveListDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

        liveListDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        mainWindow->addDockWidget(Qt::RightDockWidgetArea, liveListDock);

        // Only restore state during startup, otherwise set floating and center
        if (isStartupRestore) {
        } else {
            // First time creation or manual creation - set floating and center
            liveListDock->setFloating(true);
            liveListDock->setVisible(true);

            // Center the dock on the main window
            QRect mainWindowGeometry = mainWindow->geometry();
            int x =
                mainWindowGeometry.x() + (mainWindowGeometry.width() - liveListDock->width()) / 2;
            int y =
                mainWindowGeometry.y() + (mainWindowGeometry.height() - liveListDock->height()) / 2;
            liveListDock->move(x, y);
        }

        connect(liveListDock, &OneSevenLiveStreamListDock::startLiveClicked, this,
                [this](const OneSevenLiveRtmpRequest& request) {
                    // if streamingDock is not visible, show it
                    // in order to edit the live info item
                    if (!streamingDock) {
                        createStreamingDock();
                    }

                    // Show streamingDock in center of desktop
                    streamingDock->setFloating(true);
                    streamingDock->setVisible(true);
                    streamingDock->raise();
                    streamingDock->activateWindow();

                    // Move to center of main window
                    QRect mainWindowGeometry = mainWindow->geometry();
                    int x = mainWindowGeometry.x() +
                            (mainWindowGeometry.width() - streamingDock->width()) / 2;
                    int y = mainWindowGeometry.y() +
                            (mainWindowGeometry.height() - streamingDock->height()) / 2;
                    streamingDock->move(x, y);

                    streamingDock->createLiveWithRequest(request);
                });

        connect(liveListDock, &OneSevenLiveStreamListDock::editLiveClicked, this,
                [this](const OneSevenLiveStreamInfo& info) {
                    // Create streamingDock if it doesn't exist
                    if (!streamingDock) {
                        createStreamingDock();
                    }

                    // Edit live with info
                    streamingDock->editLiveWithInfo(info);

                    // Show streamingDock in center of desktop
                    streamingDock->setFloating(true);
                    streamingDock->setVisible(true);
                    streamingDock->raise();
                    streamingDock->activateWindow();

                    // Move to center of main window
                    QRect mainWindowGeometry = mainWindow->geometry();
                    int x = mainWindowGeometry.x() +
                            (mainWindowGeometry.width() - streamingDock->width()) / 2;
                    int y = mainWindowGeometry.y() +
                            (mainWindowGeometry.height() - streamingDock->height()) / 2;
                    streamingDock->move(x, y);

                    // Scroll to title edit box and focus on it
                    QTimer::singleShot(100, [this]() {
                        if (streamingDock) {
                            QScrollArea* scrollArea = streamingDock->findChild<QScrollArea*>();
                            QLineEdit* titleEdit =
                                streamingDock->findChild<QLineEdit*>("titleEdit");
                            if (scrollArea && titleEdit) {
                                scrollArea->ensureWidgetVisible(titleEdit);
                                titleEdit->setFocus();
                                titleEdit->selectAll();
                            }
                        }
                    });
                });

        // When dock is closed, uncheck menu item status
        connect(liveListDock, &QDockWidget::visibilityChanged, this, [this]() {
            menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                              isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                              isDockOpen(multiRtmpDock), isDockOpen(previewDock));
        });
    } else {
        liveListDock->toggleViewAction()->trigger();
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                          isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                          isDockOpen(multiRtmpDock), isDockOpen(previewDock));
    }
}

bool OneSevenLiveCoreManager::checkLoginStatus() {
    // call apiWrapper->GetSelfInfo()
    OneSevenLiveLoginData loginData;
    if (!apiWrapper->GetSelfInfo(loginData)) {
        configManager->clearLoginData();
        return false;
    }

    // TODO: update loginData: displayName

    return true;
}

void OneSevenLiveCoreManager::saveDockState() {
    if (!initialized || !mainWindow || !configManager) {
        return;
    }

    QByteArray state = mainWindow->saveState();
    configManager->setDockState(state);

    obs_log(LOG_INFO, "Dock state saved successfully");
}

void OneSevenLiveCoreManager::handleChatRoomClicked() {
    obs_log(LOG_INFO, "handleChatRoomClicked");

    OneSevenLiveLoginData loginData;
    if (!configManager->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    std::string locale = GetCurrentLocale();
    QString wsUrl = QString::fromStdString("ws://127.0.0.1:%1").arg(websocketServer_->getPort());
    QString chatUrl =
        QString("http://localhost:%1/%2.html?roomID=%3&userID=%4&ws=%5")
            .arg(QString::number(httpServer_->getPort()), QString::fromStdString(locale),
                 QString::number(loginData.userInfo.roomID), loginData.userInfo.userID, wsUrl);

    obs_log(LOG_INFO, "Chat URL: %s", chatUrl.toStdString().c_str());

    if (!chatDock) {
        obs_log(LOG_INFO, "Creating new chatDock instance");
        chatDock = new QDockWidget(obs_module_text("ChatRoom.Title"), mainWindow);
        chatDock->setObjectName("OneSevenLiveChatDock");
        chatDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        chatDock->setAttribute(Qt::WA_DeleteOnClose, false);
        chatDock->installEventFilter(this);
        chatDock->setMinimumWidth(300);

        // Create the chat widget and set it as the dock's widget
        OneSevenLiveChatWidget* chatWidget = new OneSevenLiveChatWidget(chatDock, chatUrl);
        chatDock->setWidget(chatWidget);

        mainWindow->addDockWidget(Qt::RightDockWidgetArea, chatDock);

        if (isStartupRestore) {
            chatDock->setVisible(true);
        } else {
            obs_log(LOG_INFO, "Setting chatDock to floating mode");
            chatDock->setFloating(true);
            bool hadChatStored =
                configManager ? configManager->getDockVisibility("chatRoom") : false;
            if (!hadChatStored) {
                chatDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);
            }
            chatDock->setVisible(true);

            // Center the dock
            QRect mainWindowGeometry = mainWindow->geometry();
            int x = mainWindowGeometry.x() + (mainWindowGeometry.width() - chatDock->width()) / 2;
            int y = mainWindowGeometry.y() + (mainWindowGeometry.height() - chatDock->height()) / 2;
            chatDock->move(x, y);
        }

        connect(chatDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            // obs_log(LOG_INFO, "chatDock visibility changed: %s, isFloating: %s",
            //         visible ? "true" : "false",
            //         (chatDock && chatDock->isFloating()) ? "true" : "false");

            if (menuManager) {
                menuManager->updateDockVisibility(
                    isDockOpen(chatDock), isDockOpen(streamingDock), isDockOpen(liveListDock),
                    isDockOpen(rockZoneDock), isDockOpen(multiRtmpDock), isDockOpen(previewDock));
            }
            chatDockVisible = visible;
            if (visible)
                flushChatEventQueue();
        });
    } else {
        obs_log(LOG_INFO, "Toggling existing chatDock visibility. Current: %s",
                chatDock->isVisible() ? "visible" : "hidden");
        chatDock->toggleViewAction()->trigger();
    }

    // Update visibility status for menu
    if (menuManager) {
        menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                          isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                          isDockOpen(multiRtmpDock), isDockOpen(previewDock));
    }
}

bool OneSevenLiveCoreManager::eventFilter(QObject* obj, QEvent* event) {
    if (obj == chatDock) {
        if (event->type() == QEvent::Close) {
            event->ignore();
            chatDock->hide();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void OneSevenLiveCoreManager::loadGifts() {
    if (giftsLoading_.load()) {
        obs_log(LOG_INFO, "Gifts are already loading, skipping request");
        return;
    }
    giftsLoading_.store(true);

    obs_log(LOG_INFO, "Starting to load gifts asynchronously");

    QPointer<OneSevenLiveCoreManager> self = this;
    ScheduleOBSTask([self]() {
        if (!self)
            return;
        Json apiResult;
        bool ok = false;
        try {
            std::string language = GetCurrentLanguage();
            if (self->apiWrapper) {
                ok = self->apiWrapper->GetGifts(language, apiResult);
            }
        } catch (...) {
            ok = false;
        }

        if (self) {
            QMetaObject::invokeMethod(
                self,
                [self, ok, apiResult]() {
                    if (!self)
                        return;
                    self->giftsLoading_.store(false);
                    if (!ok) {
                        obs_log(LOG_WARNING, "Failed to load gifts from API");
                        return;
                    }
                    if (self->configManager) {
                        self->configManager->saveGifts(apiResult);
                    }
                    self->buildGiftsMapFromJson(apiResult);
                },
                Qt::QueuedConnection);
        }
    });
}

void OneSevenLiveCoreManager::loadGiftsFromConfig() {
    if (!configManager)
        return;
    nlohmann::json gifts;
    if (configManager->loadGifts(gifts)) {
        buildGiftsMapFromJson(gifts);
        obs_log(LOG_INFO, "Loaded gifts into memory map from config");
    }
}

void OneSevenLiveCoreManager::buildGiftsMapFromJson(const nlohmann::json& giftsJson) {
    obs_log(LOG_INFO, "Building gifts map from json");

    giftsMap.clear();
    try {
        if (giftsJson.contains("gifts") && giftsJson["gifts"].is_array()) {
            for (const auto& gift : giftsJson["gifts"]) {
                if (gift.contains("giftID") && gift["giftID"].is_string()) {
                    const std::string gid = gift["giftID"].get<std::string>();
                    giftsMap[gid] = gift;
                }
            }
        }
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "Failed to build gifts map: %s", e.what());
    }

    obs_log(LOG_INFO, "Gifts map built with %d entries", giftsMap.size());
    emit giftsLoaded();
}

bool OneSevenLiveCoreManager::isGiftsLoaded() const {
    return !giftsMap.empty();
}

bool OneSevenLiveCoreManager::isGiftsLoading() const {
    return giftsLoading_.load();
}

std::optional<nlohmann::json> OneSevenLiveCoreManager::getGiftByID(
    const std::string& giftID) const {
    auto it = giftsMap.find(giftID);
    if (it != giftsMap.end())
        return it->second;
    return std::nullopt;
}

bool OneSevenLiveCoreManager::showAutoCloseConfirmation(const QString& message) {
    QMessageBox msgBox(mainWindow);
    msgBox.setWindowTitle(obs_module_text("Live.Settings.CloseLive.Auto.Title"));
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Warning);

    // Add custom buttons
    QPushButton* confirmButton = msgBox.addButton(
        obs_module_text("Live.Settings.CloseLive.Auto.Confirm"), QMessageBox::AcceptRole);
    QPushButton* cancelButton = msgBox.addButton(
        obs_module_text("Live.Settings.CloseLive.Auto.Cancel"), QMessageBox::RejectRole);

    // Set default focus to cancel button for safety
    msgBox.setDefaultButton(cancelButton);

    // Apply styling
    msgBox.setStyleSheet(
        "QMessageBox {"
        "    background-color: #2b2b2b;"
        "    color: #ffffff;"
        "    border: 1px solid #555555;"
        "}"
        "QMessageBox QPushButton {"
        "    background-color: #404040;"
        "    color: #ffffff;"
        "    border: 1px solid #666666;"
        "    padding: 8px 16px;"
        "    border-radius: 4px;"
        "    min-width: 80px;"
        "}"
        "QMessageBox QPushButton:hover {"
        "    background-color: #505050;"
        "}"
        "QMessageBox QPushButton:pressed {"
        "    background-color: #353535;"
        "}");

    msgBox.exec();
    return msgBox.clickedButton() == confirmButton;
}

void OneSevenLiveCoreManager::handleMultiRtmpClicked() {
    obs_log(LOG_INFO, "handleMultiRtmpClicked");

    if (!multiRtmpDock) {
        createMultiRtmpDock();
    } else {
        multiRtmpDock->toggleViewAction()->trigger();
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                          isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                          isDockOpen(multiRtmpDock), isDockOpen(previewDock));
    }
}

void OneSevenLiveCoreManager::createMultiRtmpDock() {
    if (multiRtmpDock) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!configManager->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    // Create multi-RTMP dock
    multiRtmpDock = new OneSevenLiveMultiRtmpDock(mainWindow);
    multiRtmpDock->setObjectName("OneSevenLiveMultiRtmpDock");

    multiRtmpDock->setMaximumWidth(600);
    multiRtmpDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    multiRtmpDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, multiRtmpDock);

    // Only restore state during startup, otherwise set floating and center
    if (isStartupRestore) {
    } else {
        // First time creation or manual creation - set floating and center
        multiRtmpDock->setFloating(true);
        multiRtmpDock->setVisible(true);

        // Center the dock on the main window
        QRect mainWindowGeometry = mainWindow->geometry();
        int x = mainWindowGeometry.x() + (mainWindowGeometry.width() - multiRtmpDock->width()) / 2;
        int y =
            mainWindowGeometry.y() + (mainWindowGeometry.height() - multiRtmpDock->height()) / 2;
        multiRtmpDock->move(x, y);
    }

    if (multiRtmpDockFirstLoad) {
        // Connect visibility change signal to update menu status
        connect(multiRtmpDock, &QDockWidget::visibilityChanged, this, [this]() {
            if (menuManager) {
                menuManager->updateDockVisibility(
                    isDockOpen(chatDock), isDockOpen(streamingDock), isDockOpen(liveListDock),
                    isDockOpen(rockZoneDock), isDockOpen(multiRtmpDock), isDockOpen(previewDock));
            }
        });

        multiRtmpDockFirstLoad = false;
    }
}

void OneSevenLiveCoreManager::handlePreviewDockClicked() {
    obs_log(LOG_INFO, "handlePreviewDockClicked");

    if (!previewDock) {
        createPreviewDock();
    } else {
        previewDock->toggleViewAction()->trigger();
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                          isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                          isDockOpen(multiRtmpDock), isDockOpen(previewDock));
    }
}

void OneSevenLiveCoreManager::createPreviewDock() {
    if (previewDock) {
        return;
    }

    QString wsUrl = QString::fromStdString("ws://127.0.0.1:%1").arg(websocketServer_->getPort());

    QString cartoonUrl = QString("http://localhost:%1/vff/?ws=%2")
                             .arg(QString::number(httpServer_->getPort()), wsUrl);
    obs_log(LOG_INFO, "cartoonUrl: %s", cartoonUrl.toStdString().c_str());
    // Create preview dock
    previewDock = new OneSevenLivePreviewDock(mainWindow, cartoonUrl);
    previewDock->setObjectName("OneSevenLivePreviewDock");

    previewDock->setMaximumWidth(800);
    previewDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    previewDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, previewDock);

    // Only restore state during startup, otherwise set floating and center
    if (isStartupRestore) {
    } else {
        // First time creation or manual creation - set floating and center
        previewDock->setFloating(true);
        previewDock->setVisible(true);

        // Center the dock on the main window
        QRect mainWindowGeometry = mainWindow->geometry();
        int x = mainWindowGeometry.x() + (mainWindowGeometry.width() - previewDock->width()) / 2;
        int y = mainWindowGeometry.y() + (mainWindowGeometry.height() - previewDock->height()) / 2;
        previewDock->move(x, y);
    }

    if (previewDockFirstLoad) {
        // Connect visibility change signal to update menu status
        connect(previewDock, &QDockWidget::visibilityChanged, this, [this]() {
            if (menuManager) {
                menuManager->updateDockVisibility(
                    isDockOpen(chatDock), isDockOpen(streamingDock), isDockOpen(liveListDock),
                    isDockOpen(rockZoneDock), isDockOpen(multiRtmpDock), isDockOpen(previewDock));
            }
        });

        previewDockFirstLoad = false;
    }
}

void OneSevenLiveCoreManager::setShuttingDown(bool v) {
    shuttingDown = v;
}

bool OneSevenLiveCoreManager::isShuttingDown() const {
    return shuttingDown;
}

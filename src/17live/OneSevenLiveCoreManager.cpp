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
#include <functional>
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
#include "core/DockOrchestrator.hpp"
#include "core/CoreRuntime.hpp"
#include "core/AuthSessionService.hpp"
#include "core/ChatBridgeService.hpp"
#include "core/LocalGatewayService.hpp"
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
    : mainWindow(mainWindow_), initialized(false), multiRtmpDockFirstLoad(true) {
    dockOrchestrator_ = std::make_unique<DockOrchestrator>(this);
    authSessionService_ = std::make_unique<AuthSessionService>(this);
    localGatewayService_ = std::make_unique<LocalGatewayService>(this);
    chatBridgeService_ = std::make_unique<ChatBridgeService>(this);

    CoreRuntime::State state{&initialized, &shuttingDown,
                             [this](bool v) { this->setShutdownCancel(v); }};
    CoreRuntime::Hooks hooks;
    hooks.initLocalServers = [this]() { return this->initLocalServers(); };
    hooks.initConfigAndApi = [this]() { return this->initConfigAndApi(); };
    hooks.initAuthHandlers = [this]() { this->initAuthHandlers(); };
    hooks.initMenuAndBaseUI = [this]() { return this->initMenuAndBaseUI(); };
    hooks.restoreRuntimeStateIfNeeded = [this]() { this->restoreRuntimeStateIfNeeded(); };
    hooks.stopStreamingSafely = [this]() { this->stopStreamingSafely(); };
    hooks.saveAndCloseUI = [this]() { this->saveAndCloseUI(); };
    hooks.shutdownRtmpAndChat = [this]() { this->shutdownRtmpAndChat(); };
    hooks.shutdownLocalServers = [this]() { this->shutdownLocalServers(); };
    hooks.cleanupTimersAndFlags = [this]() { this->cleanupTimersAndFlags(); };
    runtime_ = std::make_unique<CoreRuntime>(state, std::move(hooks));
}

OneSevenLiveCoreManager::~OneSevenLiveCoreManager() {
    // Ensure shutdown is called before destruction
    if (initialized) {
        shutdown();
    }
}

bool OneSevenLiveCoreManager::initLocalServers() {
    if (!localGatewayService_->initLocalServers()) {
        return false;
    }

    auto ws = localGatewayService_->getWebsocketServer();
    if (ws) {
        ws->setMessageCallback(
            [this](const std::string& clientId, const std::string& message) {
                if (chatBridgeService_) {
                    chatBridgeService_->onWebsocketMessage(clientId, message);
                }
            });

        ws->setConnectionCallback([this](const std::string& clientId, bool connected) {
            if (chatBridgeService_) {
                chatBridgeService_->onWebsocketConnectionChanged(clientId, connected);
            }
        });
    }
    return true;
}

bool OneSevenLiveCoreManager::initConfigAndApi() {
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
    configManager->getLoginData(initLoginData_);
    initIsLogin_ = false;

    if (!initLoginData_.jwtAccessToken.isEmpty()) {
        apiWrapper =
            std::make_unique<OneSevenLiveApiWrappers>(initLoginData_.jwtAccessToken.toStdString());
        apiWrapper->setCancelFlag(getCancelFlag());

        initIsLogin_ = authSessionService_->checkLoginStatus();
    }

    // if not login, initialize apiWrapper without token
    if (!initIsLogin_) {
        apiWrapper = std::make_unique<OneSevenLiveApiWrappers>();
        apiWrapper->setCancelFlag(getCancelFlag());
    }
    return true;
}

void OneSevenLiveCoreManager::initAuthHandlers() {
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
}

bool OneSevenLiveCoreManager::initMenuAndBaseUI() {
    // Load gifts from saved config into memory map for fast lookup
    loadGiftsFromConfig();

    // Initialize menu manager
    menuManager = std::make_unique<OneSevenLiveMenuManager>(mainWindow);
    if (!menuManager) {
        obs_log(LOG_ERROR, "Failed to create menu manager");
        return false;
    }
    // connect menuManager's loginClicked signal to handleLoginClicked slot
    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::loginClicked, authSessionService_.get(),
                     &AuthSessionService::handleLoginClicked);

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::logoutClicked, authSessionService_.get(),
                     &AuthSessionService::handleLogoutClicked);

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
    return true;
}

void OneSevenLiveCoreManager::restoreRuntimeStateIfNeeded() {
    isStartupRestore = true;

    // Handle login state during initialization
    if (initIsLogin_) {
        configManager->getLoginData(initLoginData_);

        // Use the new centralized login state handler for logged in users
        authSessionService_->handleLoginStateChanged(true, initLoginData_);
    } else {
        authSessionService_->handleLoginStateChanged(false);
    }
}

bool OneSevenLiveCoreManager::initialize() {
    obs_log(LOG_INFO, "[17Live Core] Initializing OneSevenLiveCoreManager...");
    return runtime_ ? runtime_->initialize() : false;
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

void OneSevenLiveCoreManager::stopStreamingSafely() {
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
                QEventLoop loop;
                QTimer::singleShot(1000, &loop, &QEventLoop::quit);
                loop.exec();
            }
        }
    }

    if (obs_frontend_streaming_active()) {
        if (streamManager) {
            streamManager->stopOBSStreaming();
        } else {
            obs_frontend_streaming_stop();
            // Don't sleep on main thread
            // std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        QEventLoop loop;
        QTimer poll;
        poll.setInterval(50);
        QObject::connect(&poll, &QTimer::timeout, &loop, [&loop]() {
            if (!obs_frontend_streaming_active()) {
                loop.quit();
            }
        });
        poll.start();

        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
    }
}

void OneSevenLiveCoreManager::saveAndCloseUI() {
    // Save dock state before closing any docks
    saveDockState();

    closeAllDocks();
}

void OneSevenLiveCoreManager::shutdownRtmpAndChat() {
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
}

void OneSevenLiveCoreManager::shutdownLocalServers() {
    if (localGatewayService_) {
        localGatewayService_->shutdownLocalServers();
    }
}

void OneSevenLiveCoreManager::cleanupTimersAndFlags() {
    // Clean up menu manager resources
    if (menuManager) {
        menuManager->cleanup();
    }

    if (ytChatDiscoverTimer) {
        ytChatDiscoverTimer->stop();
        ytChatDiscoverTimer->deleteLater();
        ytChatDiscoverTimer = nullptr;
    }
}

void OneSevenLiveCoreManager::shutdown() {
    if (runtime_) {
        runtime_->shutdown();
    }
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
    return localGatewayService_ ? localGatewayService_->getWebsocketServer() : nullptr;
}

OneSevenLiveHttpServer* OneSevenLiveCoreManager::getHttpServer() const {
    return localGatewayService_ ? localGatewayService_->getHttpServer() : nullptr;
}

AuthSessionService* OneSevenLiveCoreManager::getAuthSessionService() const {
    return authSessionService_.get();
}

LocalGatewayService* OneSevenLiveCoreManager::getLocalGatewayService() const {
    return localGatewayService_.get();
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
    if (chatBridgeService_) {
        chatBridgeService_->enqueueOrBroadcastChatEvent(type, payload);
    }
}

void OneSevenLiveCoreManager::flushChatEventQueue() {
    if (chatBridgeService_) {
        chatBridgeService_->flushChatEventQueue();
    }
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
                if (authSessionService_->isPendingLogout()) {
                    authSessionService_->setPendingLogout(false);
                    QPointer<AuthSessionService> auth = authSessionService_.get();
                    QMetaObject::invokeMethod(
                        this,
                        [auth]() {
                            if (auth)
                                auth->handleLoginStateChanged(false);
                        },
                        Qt::QueuedConnection);
                }
            }
        });
}

void OneSevenLiveCoreManager::closeAllDocks() {
    if (dockOrchestrator_) {
        dockOrchestrator_->closeAllDocks();
    }
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
    if (dockOrchestrator_) {
        dockOrchestrator_->handleStreamingClicked();
    }
}

void OneSevenLiveCoreManager::createStreamingDock() {
    if (dockOrchestrator_) {
        dockOrchestrator_->createStreamingDock();
    }
}

void OneSevenLiveCoreManager::handleRockZoneClicked() {
    if (dockOrchestrator_) {
        dockOrchestrator_->handleRockZoneClicked();
    }
}

void OneSevenLiveCoreManager::createRockZoneDock() {
    if (dockOrchestrator_) {
        dockOrchestrator_->createRockZoneDock();
    }
}

void OneSevenLiveCoreManager::handleLiveListClicked() {
    if (dockOrchestrator_) {
        dockOrchestrator_->handleLiveListClicked();
    }
}

void OneSevenLiveCoreManager::saveDockState() {
    if (dockOrchestrator_) {
        dockOrchestrator_->saveDockState();
    }
}

void OneSevenLiveCoreManager::handleChatRoomClicked() {
    if (dockOrchestrator_) {
        dockOrchestrator_->handleChatRoomClicked();
    }
}

bool OneSevenLiveCoreManager::eventFilter(QObject* obj, QEvent* event) {
    if (obj == chatDock) {
        if (event->type() == QEvent::Close) {
            if (auto* chatWidget = qobject_cast<OneSevenLiveChatWidget*>(chatDock->widget())) {
                chatWidget->shutdown();
                chatDock->setWidget(nullptr);
                delete chatWidget;
            }
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
    if (dockOrchestrator_) {
        dockOrchestrator_->handleMultiRtmpClicked();
    }
}

void OneSevenLiveCoreManager::createMultiRtmpDock() {
    if (dockOrchestrator_) {
        dockOrchestrator_->createMultiRtmpDock();
    }
}

void OneSevenLiveCoreManager::handlePreviewDockClicked() {
    if (dockOrchestrator_) {
        dockOrchestrator_->handlePreviewDockClicked();
    }
}

void OneSevenLiveCoreManager::createPreviewDock() {
    if (dockOrchestrator_) {
        dockOrchestrator_->createPreviewDock();
    }
}

void OneSevenLiveCoreManager::setShuttingDown(bool v) {
    shuttingDown = v;
}

bool OneSevenLiveCoreManager::isShuttingDown() const {
    return shuttingDown;
}

void OneSevenLiveCoreManager::syncMenuDockVisibility() {
    if (dockOrchestrator_) {
        dockOrchestrator_->syncMenuDockVisibility();
    }
}

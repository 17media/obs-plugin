#include "OneSevenLiveCoreManager.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QApplication>
#include <QDesktopServices>
#include <QDockWidget>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
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
#include "OneSevenLiveChatDock.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveHttpServer.hpp"
#include "OneSevenLiveLoginDialog.hpp"
#include "OneSevenLiveMenuManager.hpp"
#include "OneSevenLiveUpdateManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "chat/OneSevenLiveChatMessageHandler.hpp"
#include "chat/OneSevenLiveChatRelayWidget.hpp"
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

    // Run network diagnostics to check API connectivity
    obs_log(LOG_INFO, "[17Live Core] Running startup network diagnostics...");
    NetworkDiagnostics::runStartupDiagnostics(ONESEVENLIVE_API_URL);

    // Initialize and start HTTP server
    // "html" is the path relative to obs_get_module_data_path()
    httpServer_ = std::make_unique<OneSevenLiveHttpServer>("localhost", 0, "html/chat");
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

    ablyHttpServer_ = std::make_unique<OneSevenLiveHttpServer>("localhost", 0, "html/ably");
    if (!ablyHttpServer_) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to create Ably HTTP server instance");
    } else {
        if (!ablyHttpServer_->start()) {
            obs_log(LOG_ERROR, "[17Live Core] Failed to start Ably HTTP server");
        } else {
            obs_log(LOG_INFO, "[17Live Core] Ably HTTP server started successfully");
        }
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

        isLogin = checkLoginStatus();
    }

    // if not login, initialize apiWrapper without token
    if (!isLogin) {
        apiWrapper = std::make_unique<OneSevenLiveApiWrappers>();
    }

    // Instantiate auth handlers
    twitchAuth = std::make_unique<OneSevenLiveTwitchAuth>(this);
    youtubeAuth = std::make_unique<OneSevenLiveYouTubeAuth>(this);

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
        // YouTube: load access and refresh tokens
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
            // Schedule auto refresh; if access already expired, this will attempt immediate refresh
            youtubeAuth->scheduleAutoRefresh(ytExpiresIn, ytFetchedAt, ytRefreshExpiresIn,
                                             ytRefreshFetchedAt);
        } else if (hasRefresh) {
            const bool hasExpiry = ytRefreshExpiresIn > 0;
            const bool notExpired =
                hasExpiry ? (nowEpoch < ytRefreshFetchedAt + ytRefreshExpiresIn) : true;
            if (notExpired) {
                obs_log(LOG_INFO,
                        "[17Live Core] No YouTube access token; refreshing using refresh token");
                QTimer::singleShot(0, youtubeAuth.get(),
                                   &OneSevenLiveYouTubeAuth::refreshAccessTokenAsync);
            } else {
                obs_log(LOG_INFO,
                        "[17Live Core] YouTube refresh token expired; clearing stored tokens");
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
    QObject::connect(
        updateManager, &OneSevenLiveUpdateManager::updateAvailable, this,
        [this](const QString& latestVersion, const QJsonArray& assets) {
            UNUSED_PARAMETER(assets);
            QMessageBox msgBox(mainWindow);
            msgBox.setWindowTitle(obs_module_text("Update.NewVersionFound"));
            msgBox.setText(
                QString(obs_module_text("Update.NewVersionFound.Message")).arg(latestVersion));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::Yes);

            if (msgBox.exec() == QMessageBox::Yes) {
                // open download page obs_module_text("Menu.CheckUpdate.Url")
                QDesktopServices::openUrl(QUrl(obs_module_text("Menu.CheckUpdate.Url")));

                // QString systemInfo = updateManager->getSystemInfo();
                // QString downloadUrl;
                // QString fileName;

                // for (QJsonValue assetValue : assets) {
                //     QJsonObject asset = assetValue.toObject();
                //     QString assetName = asset["name"].toString();

                //     obs_log(LOG_INFO, "Asset name: %s", assetName.toStdString().c_str());
                //     obs_log(LOG_INFO, "systemInfo: %s", systemInfo.toStdString().c_str());

                //     if (systemInfo.contains("macOS")) {
                //         if (systemInfo.contains("arm64") &&
                //         assetName.contains("macAppleSilicon")) {
                //             downloadUrl = asset["browser_download_url"].toString();
                //             fileName = assetName;
                //             break;
                //         } else if (systemInfo.contains("x86_64") &&
                //         assetName.contains("macIntel")) {
                //             downloadUrl = asset["browser_download_url"].toString();
                //             fileName = assetName;
                //             break;
                //         }
                //     } else if (systemInfo.contains("Windows") && assetName.contains("windows")) {
                //         downloadUrl = asset["browser_download_url"].toString();
                //         fileName = assetName;
                //         break;
                //     }
                // }

                // if (downloadUrl.isEmpty()) {
                //     QMessageBox::warning(mainWindow, obs_module_text("Update.DownloadFailed"),
                //                          obs_module_text("Update.DownloadFailed.NoPackage"));
                //     return;
                // }

                // updateManager->downloadUpdate(downloadUrl, fileName);
            }
        });

    QObject::connect(updateManager, &OneSevenLiveUpdateManager::updateNotAvailable, this,
                     [this]() { obs_log(LOG_INFO, "Update check: no new version available."); });

    QObject::connect(updateManager, &OneSevenLiveUpdateManager::updateCheckFailed, this,
                     [this](const QString& error) {
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
            chatDockClientId = clientId;
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
    if (!initialized) {
        return;
    }


    if (obs_frontend_streaming_active()) {
        if (streamManager) {
            streamManager->stopOBSStreaming();
        } else {
            obs_frontend_streaming_stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
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
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    if (chatRelayWidget) {
        chatRelayWidget->deleteLater();
        chatRelayWidget = nullptr;
    }

    // Stop WebSocket server
    if (websocketServer_) {
        websocketServer_->stop();
        obs_log(LOG_INFO, "[17Live Core] WebSocket server stopped");
    }

    // Stop HTTP server
    if (httpServer_) {
        httpServer_->stopAsync();
        obs_log(LOG_INFO, "[17Live Core] HTTP server stopped");
    }

    if (ablyHttpServer_) {
        ablyHttpServer_->stopAsync();
        obs_log(LOG_INFO, "[17Live Core] Ably HTTP server stopped");
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

OneSevenLiveTwitchChatClient* OneSevenLiveCoreManager::getTwitchChatClient() const {
    return twitchChatClient.get();
}

OneSevenLiveAblyChatClient* OneSevenLiveCoreManager::getAblyChatClient() const {
    return ablyChatClient.get();
}

void OneSevenLiveCoreManager::createYouTubeChatClient() {
    if (youtubeChatClient) {
        return;
    }

    youtubeChatClient = std::make_unique<OneSevenLiveYouTubeChatClient>(this);

    // Configure token and API key from auth/config
    if (youtubeAuth) {
        const QString accessToken = youtubeAuth->getAccessToken();
        if (!accessToken.isEmpty()) {
            youtubeChatClient->setAccessToken(accessToken);
        }
    }

    if (!youtubeApiClient) {
        youtubeApiClient = std::make_unique<OneSevenLiveYouTubeClient>(this);
        if (youtubeAuth) {
            const QString accessToken = youtubeAuth->getAccessToken();
            if (!accessToken.isEmpty()) {
                youtubeApiClient->setAccessToken(accessToken);
            }
        }
        if (youtubeChatClient)
            youtubeChatClient->setApiClient(youtubeApiClient.get());
        connect(youtubeApiClient.get(), &OneSevenLiveYouTubeClient::myLiveStreamsReceived, this,
                [this](const YouTubeLiveStreamListResponse& resp) {
                    for (const auto& s : resp.items) {
                        obs_log(LOG_INFO, "YouTube stream: id=%s status=%s",
                                s.id.toUtf8().constData(),
                                s.status.streamStatus.toUtf8().constData());
                    }
                });
        connect(youtubeApiClient.get(), &OneSevenLiveYouTubeClient::errorOccurred, this,
                [this](const QString& err, const QString& op) {
                    int status = -1;
                    QRegularExpression r1(R"(HTTP\s+(\d{3}))");
                    QRegularExpressionMatch m1 = r1.match(err);
                    if (m1.hasMatch())
                        status = m1.captured(1).toInt();
                    if (status == -1) {
                        QRegularExpression r2(R"(returned error:\s*(\d{3}))");
                        QRegularExpressionMatch m2 = r2.match(err);
                        if (m2.hasMatch())
                            status = m2.captured(1).toInt();
                    }
                    if (!(op == "getMyLiveBroadcasts" && err.contains("No valid authentication token"))) {
                        obs_log(LOG_WARNING, "YouTube API op=%s error=%s status=%d",
                                op.toUtf8().constData(), err.toUtf8().constData(), status);
                    }
                    if (status == 401 && youtubeAuth) {
                        obs_log(LOG_INFO, "Attempting YouTube token refresh due to 401");
                        if (youtubeAuth->refreshAccessToken()) {
                            const QString accessToken = youtubeAuth->getAccessToken();
                            if (!accessToken.isEmpty()) {
                                youtubeApiClient->setAccessToken(accessToken);
                                if (op == "getMyLiveBroadcasts" && youtubeApiClient->hasValidAuth()) {
                                    youtubeApiClient->getMyLiveBroadcasts();
                                }
                            }
                        }
                    }
                });
        if (youtubeAuth) {
            connect(youtubeAuth.get(), &OneSevenLiveYouTubeAuth::authorizationCompleted, this,
                    [this](const QString& accessToken) {
                        obs_log(LOG_INFO, "YouTube authorizationCompleted: token refreshed");
                        if (youtubeApiClient && !accessToken.isEmpty()) {
                            youtubeApiClient->setAccessToken(accessToken);
                            {
                                QString tok = accessToken;
                                QString masked =
                                    tok.length() >= 12 ? tok.left(6) + "..." + tok.right(6) : tok;
                                obs_log(LOG_INFO, "YouTube API client token set token(masked)=%s",
                                        masked.toUtf8().constData());
                            }
                            if (youtubeApiClient->hasValidAuth())
                                youtubeApiClient->getMyLiveBroadcasts();
                        }
                        if (youtubeChatClient && !accessToken.isEmpty()) {
                            youtubeChatClient->setAccessToken(accessToken);
                            {
                                QString tok = accessToken;
                                QString masked =
                                    tok.length() >= 12 ? tok.left(6) + "..." + tok.right(6) : tok;
                                obs_log(LOG_INFO, "YouTube chat client token set token(masked)=%s",
                                        masked.toUtf8().constData());
                            }
                        }
                    });
        }

        if (youtubeChatClient && youtubeAuth) {
            connect(
                youtubeChatClient.get(), &OneSevenLiveYouTubeChatClient::errorOccurred, this,
                [this](const QString& err, const QString& op) {
                    int status = -1;
                    QRegularExpression r1(R"(HTTP\s+(\d{3}))");
                    QRegularExpressionMatch m1 = r1.match(err);
                    if (m1.hasMatch())
                        status = m1.captured(1).toInt();
                    obs_log(LOG_WARNING, "YouTube Chat error op=%s status=%d err=%s",
                            op.toUtf8().constData(), status, err.toUtf8().constData());
                    if (status == 401 && youtubeAuth) {
                        obs_log(LOG_INFO, "Refreshing YouTube token due to chat 401");
                        if (youtubeAuth->refreshAccessToken()) {
                            const QString accessToken = youtubeAuth->getAccessToken();
                            if (!accessToken.isEmpty()) {
                                if (youtubeApiClient)
                                    youtubeApiClient->setAccessToken(accessToken);
                                if (youtubeChatClient)
                                    youtubeChatClient->setAccessToken(accessToken);
                                {
                                    QString tok = accessToken;
                                    QString masked = tok.length() >= 12
                                                         ? tok.left(6) + "..." + tok.right(6)
                                                         : tok;
                                    obs_log(
                                        LOG_INFO,
                                        "YouTube tokens synchronized to clients token(masked)=%s",
                                        masked.toUtf8().constData());
                                }
                                if (youtubeApiClient && youtubeApiClient->hasValidAuth())
                                    youtubeApiClient->getMyLiveBroadcasts();
                            }
                        }
                    }
                    if (err.contains("liveChatEnded", Qt::CaseInsensitive)) {
                        obs_log(LOG_INFO, "Chat reported liveChatEnded; rediscovering liveChatId");
                        if (youtubeApiClient && youtubeApiClient->hasValidAuth())
                            youtubeApiClient->getMyLiveBroadcasts();
                    }
                });
        }
    }
    if (youtubeChatClient)
        youtubeChatClient->startDiscovery();
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
    ablyChatClient->setAuthCallback([this](const QString& rid, nlohmann::json& out) {
        auto* api = this->getApiWrapper();
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
}

void OneSevenLiveCoreManager::destroyTwitchChatClient() {
    if (twitchChatClient) {
        twitchChatClient->disconnectFromChat();
        twitchChatClient.reset();
    }
}

void OneSevenLiveCoreManager::startYouTubeChatPolling(const QString& liveChatId) {
    if (!youtubeChatClient) {
        createYouTubeChatClient();
    }
    if (!youtubeChatClient) {
        obs_log(LOG_ERROR, "Failed to create YouTubeChatClient");
        return;
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
    QMetaObject::invokeMethod(
        this, [this, loginData]() { handleLoginStateChanged(true, loginData); },
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
        return;
    }

    QTimer::singleShot(0, this, [this]() { loadGifts(); });

    // Update menu with user info
    QString username = loginData.userInfo.displayName;
    if (username.isEmpty()) {
        username = loginData.userInfo.openID;
    }
    menuManager->updateLoginStatus(true, username);

    QTimer::singleShot(0, this, [this, loginData]() { load17LiveConfig(loginData); });

    // Restore dock states if this is during startup and there are saved states
    if (isStartupRestore) {
        QTimer::singleShot(0, this, [this]() {
            restoreDockStatesOnLogin();
            isStartupRestore = false;
        });
    }

    // Start chat relay widget (hidden) to connect Ably via web relay
    QTimer::singleShot(0, this, [this]() {
        if (!chatRelayWidget)
            chatRelayWidget = new OneSevenLiveChatRelayWidget(mainWindow);
        qint64 rid = 0;
        if (streamManager)
            rid = streamManager->getRoomID();
        int httpPort = 0;
        if (ablyHttpServer_)
            httpPort = ablyHttpServer_->getPort();
        int wsPort = 0;
        if (websocketServer_)
            wsPort = websocketServer_->getPort();
        if (rid > 0 && httpPort > 0 && wsPort > 0)
            chatRelayWidget->startRelay(QString::number(rid), httpPort, wsPort);
    });

    // Create chat clients on login
    createYouTubeChatClient();
    createTwitchChatClient();

    // Connect Ably chat based on current room ID and fetched token
    // if (streamManager && apiWrapper) {
    //     const qint64 rid = streamManager->getRoomID();
    //     if (rid > 0) {
    //         nlohmann::json ablyResp;
    //         QString token;
    //         if (apiWrapper->GetAblyToken(std::to_string(rid), ablyResp)) {
    //             if (ablyResp.contains("token") && ablyResp["token"].is_string()) {
    //                 token = QString::fromStdString(ablyResp["token"].get<std::string>());
    //                 QString masked =
    //                     token.length() >= 12 ? token.left(6) + "..." + token.right(6) : token;
    //                 obs_log(LOG_INFO,
    //                         "[17Live Core] Fetched Ably token for room %lld token(masked)=%s",
    //                         (long long) rid, masked.toUtf8().constData());
    //             } else {
    //                 obs_log(LOG_WARNING,
    //                         "[17Live Core] Ably token response missing 'token' field for room
    //                         %lld", (long long) rid);
    //             }
    //         } else {
    //             obs_log(LOG_WARNING, "[17Live Core] Failed to fetch Ably token for room %lld",
    //                     (long long) rid);
    //         }
    //         connectAblyChat(QString::number(rid), token);
    //     }
    // }

    // discovery is managed by YouTubeChatClient
}

void OneSevenLiveCoreManager::performLogoutOperations() {
    obs_log(LOG_INFO, "performLogoutOperations");

    // Close all dock windows
    closeAllDocks();

    // Reset login status in menu
    menuManager->updateLoginStatus(false, "");

    // Cleanup chat relay widget
    if (chatRelayWidget) {
        chatRelayWidget->deleteLater();
        chatRelayWidget = nullptr;
    }

    // Clear login data
    configManager->clearLoginData();

    // Destroy chat clients on logout
    destroyYouTubeChatClient();
    destroyTwitchChatClient();
    disconnectAblyChat();
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

        // Update menu visibility status after restoration
        if (menuManager) {
            menuManager->updateDockVisibility(chatDock && chatDock->isVisible(),
                                              streamingDock && streamingDock->isVisible(),
                                              liveListDock && liveListDock->isVisible(),
                                              rockZoneDock && rockZoneDock->isVisible(),
                                              multiRtmpDock && multiRtmpDock->isVisible(),
                                              previewDock && previewDock->isVisible());
        }
    }
}

void OneSevenLiveCoreManager::closeAllDocks() {
    obs_log(LOG_INFO, "closeAllDocks");

    bool streamingVisible = false;
    if (streamingDock) {
        streamingVisible = streamingDock->isVisible();
        streamingDock->disconnect(this);
        streamingDock->close();
        streamingDock->deleteLater();
        streamingDock = nullptr;
    }
    configManager->setDockVisibility("streaming", streamingVisible);

    bool liveListVisible = false;
    if (liveListDock) {
        liveListVisible = liveListDock->isVisible();
        liveListDock->disconnect(this);
        liveListDock->close();
        liveListDock->deleteLater();
        liveListDock = nullptr;
    }
    configManager->setDockVisibility("liveList", liveListVisible);

    bool rockZoneVisible = false;
    if (rockZoneDock) {
        rockZoneVisible = rockZoneDock->isVisible();
        rockZoneDock->disconnect(this);
        rockZoneDock->close();
        rockZoneDock->deleteLater();
        rockZoneDock = nullptr;
    }
    configManager->setDockVisibility("rockZone", rockZoneVisible);

    bool chatRoomVisible = false;
    if (chatDock) {
        chatRoomVisible = chatDock->isVisible();
        chatDock->disconnect(this);
        chatDock->close();
        chatDock->deleteLater();
        chatDock = nullptr;
    }
    configManager->setDockVisibility("chatRoom", chatRoomVisible);

    bool multiRtmpVisible = false;
    if (multiRtmpDock) {
        multiRtmpVisible = multiRtmpDock->isVisible();
        multiRtmpDock->disconnect(this);
        multiRtmpDock->close();
        multiRtmpDock->deleteLater();
        multiRtmpDock = nullptr;
    }
    configManager->setDockVisibility("multiRtmp", multiRtmpVisible);

    bool previewDockVisible = false;
    if (previewDock) {
        previewDockVisible = previewDock->isVisible();
        previewDock->disconnect(this);
        previewDock->close();
        previewDock->deleteLater();
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
        connect(msgBox, &QMessageBox::finished, this, [this, msgBox, confirmButton](int) {
            if (msgBox->clickedButton() != confirmButton) {
                msgBox->deleteLater();
                return;
            }
            QMetaObject::invokeMethod(
                this,
                [this]() {
                    closeLive(false);
                    handleLoginStateChanged(false);
                },
                Qt::QueuedConnection);
            msgBox->deleteLater();
        });
        msgBox->open();
        return;
    }
    QMetaObject::invokeMethod(
        this, [this]() { handleLoginStateChanged(false); }, Qt::QueuedConnection);
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
        streamingDock->setVisible(!streamingDock->isVisible());
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(
            chatDock && chatDock->isVisible(), streamingDock && streamingDock->isVisible(),
            liveListDock && liveListDock->isVisible(), rockZoneDock && rockZoneDock->isVisible(),
            multiRtmpDock && multiRtmpDock->isVisible(), previewDock && previewDock->isVisible());
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
    streamingDock = new OneSevenLiveStreamingDock(mainWindow, streamManager.get(), apiWrapper.get());  
    streamingDock->setObjectName("OneSevenLiveStreamingDock");

    streamingDock->setMaximumWidth(600);
    streamingDock->resize(450, 600);

    streamingDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, streamingDock);

    // Only restore state during startup, otherwise set floating and center
    if (isStartupRestore) {
        // During startup restoration, the state will be restored by initialize() method
        streamingDock->setVisible(true);
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

        connect(
            streamingDock, &OneSevenLiveStreamingDock::streamStatusUpdated, this,
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
                            if (streamCheckInFlight.load())
                                return;
                            std::string liveStreamID;
                            if (!configManager->getConfigValue("LiveStreamID", liveStreamID))
                                return;
                            streamCheckInFlight.store(true);
                            std::thread([this, liveStreamID]() {
                                bool ok = false;
                                try {
                                    ok = apiWrapper->CheckStream(liveStreamID);
                                } catch (...) {
                                    ok = false;
                                }
                                QMetaObject::invokeMethod(
                                    this,
                                    [this, ok]() {
                                        if (!ok) {
                                            consecutiveFailureCount++;
                                            obs_log(
                                                LOG_WARNING,
                                                "Stream check failed. Consecutive failures: %d/%d",
                                                consecutiveFailureCount, MAX_CONSECUTIVE_FAILURES);
                                            if (consecutiveFailureCount >=
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
                                                if (showAutoCloseConfirmation(message)) {
                                                    closeLive(true);
                                                    if (streamCheckTimer) {
                                                        streamCheckTimer->stop();
                                                        streamCheckTimer->deleteLater();
                                                        streamCheckTimer = nullptr;
                                                    }
                                                }
                                                consecutiveFailureCount = 0;
                                            }
                                        } else {
                                            if (consecutiveFailureCount > 0) {
                                                obs_log(LOG_INFO,
                                                        "Stream check succeeded. Resetting failure "
                                                        "count from %d to 0.",
                                                        consecutiveFailureCount);
                                                consecutiveFailureCount = 0;
                                            }
                                        }
                                        streamCheckInFlight.store(false);
                                    },
                                    Qt::QueuedConnection);
                            }).detach();
                        });
                    }
                    streamCheckTimer->start(30000);  // 30 seconds
                } else {
                    // Stop timer when not streaming
                    if (streamCheckTimer) {
                        streamCheckTimer->stop();
                        streamCheckTimer->deleteLater();
                        streamCheckTimer = nullptr;
                        streamCheckInFlight.store(false);
                    }
                }
            });

        connect(streamingDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            menuManager->updateDockVisibility(chatDock && chatDock->isVisible(), visible,
                                              liveListDock && liveListDock->isVisible(),
                                              rockZoneDock && rockZoneDock->isVisible(),
                                              multiRtmpDock && multiRtmpDock->isVisible(),
                                              previewDock && previewDock->isVisible());
        });

        streamingDockFirstLoad = false;
    }
}

void OneSevenLiveCoreManager::handleRockZoneClicked() {
    obs_log(LOG_INFO, "handleRockZoneClicked");

    if (!rockZoneDock) {
        createRockZoneDock();
    } else {
        rockZoneDock->setVisible(!rockZoneDock->isVisible());
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(
            chatDock && chatDock->isVisible(), streamingDock && streamingDock->isVisible(),
            liveListDock && liveListDock->isVisible(), rockZoneDock && rockZoneDock->isVisible(),
            multiRtmpDock && multiRtmpDock->isVisible(), previewDock && previewDock->isVisible());
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
    rockZoneDock->setMinimumHeight(400);

    rockZoneDock->resize(370, 500);

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

    if (rockZoneDockFirstLoad) {
        // When dock is closed, uncheck menu item status
        connect(rockZoneDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            menuManager->updateDockVisibility(chatDock && chatDock->isVisible(),
                                              streamingDock && streamingDock->isVisible(),
                                              liveListDock && liveListDock->isVisible(), visible,
                                              multiRtmpDock && multiRtmpDock->isVisible(),
                                              previewDock && previewDock->isVisible());
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
        liveListDock->setMinimumHeight(400);

        liveListDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        mainWindow->addDockWidget(Qt::RightDockWidgetArea, liveListDock);

        // Only restore state during startup, otherwise set floating and center
        if (isStartupRestore) {
            // During startup restoration, the state will be restored by initialize() method
            liveListDock->setVisible(true);
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
        connect(liveListDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            menuManager->updateDockVisibility(chatDock && chatDock->isVisible(),
                                              streamingDock && streamingDock->isVisible(), visible,
                                              rockZoneDock && rockZoneDock->isVisible(),
                                              multiRtmpDock && multiRtmpDock->isVisible(),
                                              previewDock && previewDock->isVisible());
        });
    } else {
        liveListDock->setVisible(!liveListDock->isVisible());
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(
            chatDock && chatDock->isVisible(), streamingDock && streamingDock->isVisible(),
            liveListDock && liveListDock->isVisible(), rockZoneDock && rockZoneDock->isVisible(),
            multiRtmpDock && multiRtmpDock->isVisible(), previewDock && previewDock->isVisible());
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
        chatDock = new OneSevenLiveChatDock(mainWindow, chatUrl);
        chatDock->setObjectName("OneSevenLiveChatDock");
        chatDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        mainWindow->addDockWidget(Qt::RightDockWidgetArea, chatDock);

        connect(chatDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            menuManager->updateDockVisibility(visible, streamingDock && streamingDock->isVisible(),
                                              liveListDock && liveListDock->isVisible(),
                                              rockZoneDock && rockZoneDock->isVisible(),
                                              multiRtmpDock && multiRtmpDock->isVisible(),
                                              previewDock && previewDock->isVisible());
            chatDockVisible = visible;
            if (visible)
                flushChatEventQueue();
        });
    } else {
        if (chatDock->isVisible()) {
            chatDock->close();
            return;
        }
        chatDock->setUrl(chatUrl);
    }

    chatDock->resize(378, 600);

    if (isStartupRestore) {
        chatDock->setVisible(true);
    } else {
        chatDock->setFloating(true);
        chatDock->setVisible(true);

        QRect mainWindowGeometry = mainWindow->geometry();
        int x = mainWindowGeometry.x() + (mainWindowGeometry.width() - chatDock->width()) / 2;
        int y = mainWindowGeometry.y() + (mainWindowGeometry.height() - chatDock->height()) / 2;
        chatDock->move(x, y);
    }

    if (menuManager) {
        menuManager->updateDockVisibility(
            true, streamingDock && streamingDock->isVisible(),
            liveListDock && liveListDock->isVisible(), rockZoneDock && rockZoneDock->isVisible(),
            multiRtmpDock && multiRtmpDock->isVisible(), previewDock && previewDock->isVisible());
    }
    chatDockVisible = true;
    flushChatEventQueue();
}

void OneSevenLiveCoreManager::loadGifts() {
    obs_log(LOG_INFO, "Starting to load gifts asynchronously");
    std::thread giftLoadThread([this]() {
        Json apiResult;
        bool ok = false;
        try {
            std::string language = GetCurrentLanguage();
            ok = apiWrapper->GetGifts(language, apiResult);
        } catch (...) {
            ok = false;
        }
        QMetaObject::invokeMethod(
            this,
            [this, ok, apiResult]() {
                if (!ok) {
                    obs_log(LOG_WARNING, "Failed to load gifts from API");
                    return;
                }
                configManager->saveGifts(apiResult);
                buildGiftsMapFromJson(apiResult);
                if (chatDock && chatDock->isVisible()) {
                    chatDock->reload();
                }
            },
            Qt::QueuedConnection);
    });
    giftLoadThread.detach();
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
        multiRtmpDock->setVisible(!multiRtmpDock->isVisible());
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(
            chatDock && chatDock->isVisible(), streamingDock && streamingDock->isVisible(),
            liveListDock && liveListDock->isVisible(), rockZoneDock && rockZoneDock->isVisible(),
            multiRtmpDock && multiRtmpDock->isVisible(), previewDock && previewDock->isVisible());
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
    multiRtmpDock->resize(450, 600);

    multiRtmpDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, multiRtmpDock);

    // Only restore state during startup, otherwise set floating and center
    if (isStartupRestore) {
        // During startup restoration, the state will be restored by initialize() method
        multiRtmpDock->setVisible(true);
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
        connect(multiRtmpDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            if (menuManager) {
                menuManager->updateDockVisibility(chatDock && chatDock->isVisible(),
                                                  streamingDock && streamingDock->isVisible(),
                                                  liveListDock && liveListDock->isVisible(),
                                                  rockZoneDock && rockZoneDock->isVisible(),
                                                  visible, previewDock && previewDock->isVisible());
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
        previewDock->setVisible(!previewDock->isVisible());
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(
            chatDock && chatDock->isVisible(), streamingDock && streamingDock->isVisible(),
            liveListDock && liveListDock->isVisible(), rockZoneDock && rockZoneDock->isVisible(),
            multiRtmpDock && multiRtmpDock->isVisible(), previewDock && previewDock->isVisible());
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
    previewDock->resize(640, 480);

    previewDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, previewDock);

    // Only restore state during startup, otherwise set floating and center
    if (isStartupRestore) {
        // During startup restoration, the state will be restored by initialize() method
        previewDock->setVisible(true);
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
        connect(previewDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            if (menuManager) {
                menuManager->updateDockVisibility(
                    chatDock && chatDock->isVisible(), streamingDock && streamingDock->isVisible(),
                    liveListDock && liveListDock->isVisible(),
                    rockZoneDock && rockZoneDock->isVisible(),
                    multiRtmpDock && multiRtmpDock->isVisible(), visible);
            }
        });

        previewDockFirstLoad = false;
    }
}

void OneSevenLiveCoreManager::reloadChatUrls() {
    if (isShuttingDown()) {
        return;
    }

    obs_log(LOG_INFO, "[17Live Core] Reloading chat URLs...");

    qint64 rid = 0;
    if (streamManager) {
        rid = streamManager->getRoomID();
    }

    int httpPort = 0;
    if (ablyHttpServer_) {
        httpPort = ablyHttpServer_->getPort();
    }

    int wsPort = 0;
    if (websocketServer_) {
        wsPort = websocketServer_->getPort();
    }

    // Reload ChatRelayWidget
    if (chatRelayWidget && rid > 0 && httpPort > 0 && wsPort > 0) {
        obs_log(LOG_INFO, "[17Live Core] Reloading ChatRelayWidget with RoomID: %lld", rid);
        chatRelayWidget->startRelay(QString::number(rid), httpPort, wsPort);
    }

    // Reload ChatDock
    if (chatDock) {
        obs_log(LOG_INFO, "[17Live Core] Reloading ChatDock");
        chatDock->reload();
    }

    // Reconnect Ably chat client
    // if (ablyChatClient && rid > 0) {
    //     obs_log(LOG_INFO, "[17Live Core] Reconnecting Ably chat client for RoomID: %lld", rid);
    //     connectAblyChat(QString::number(rid));
    // }
}

void OneSevenLiveCoreManager::setShuttingDown(bool v) {
    shuttingDown = v;
}

bool OneSevenLiveCoreManager::isShuttingDown() const {
    return shuttingDown;
}

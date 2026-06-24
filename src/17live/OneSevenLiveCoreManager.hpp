#pragma once

#include <QObject>
#include <QPointer>
#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>

#include "api/OneSevenLiveModels.hpp"
#include "core/OneSevenLiveCoreContext.hpp"
#include "utility/NetworkDiagnostics.hpp"
#include "websocket/WsMessage.hpp"

// Forward declarations for auth handlers
class OneSevenLiveTwitchAuth;
class OneSevenLiveYouTubeAuth;

// Forward declarations
class QMainWindow;
class QTimer;
class QDockWidget;
class QProgressDialog;

class BrowserApp;

class LocalGatewayService;
class ChatBridgeService;

// Forward declaration of OneSevenLiveMenuManager class
class OneSevenLiveMenuManager;

class OneSevenLiveApiWrappers;

class OneSevenLiveConfigManager;

#include <QDockWidget>

class OneSevenLiveStreamingDock;

class OneSevenLiveStreamListDock;

class OneSevenLiveRockZoneDock;

class OneSevenLiveChatWidget;

class OneSevenLiveMultiRtmpDock;

class OneSevenLivePreviewDock;
class CustomizedCartoonDock;

class OneSevenLiveHttpServer;

class OneSevenLiveWebsocketServer;

class OneSevenLiveStreamManager;

// Forward declarations for chat clients
class OneSevenLiveYouTubeChatClient;
class OneSevenLiveTwitchChatClient;
class OneSevenLiveYouTubeClient;
class OneSevenLiveAblyChatClient;
class CoreRuntime;
class DockOrchestrator;

class AuthSessionService;
class CrashUploadService;
class CustomizedCartoonService;

/**
 * @brief OneSevenLiveCoreManager class is the core management class for the 17live plugin
 *
 * This class uses singleton pattern design as the control center for managing all 17live plugins.
 * Responsible for plugin initialization, configuration management, resource allocation and other
 * core functions.
 */
class OneSevenLiveCoreManager : public QObject, public OneSevenLiveCoreContext {
    Q_OBJECT

   public:
    /**
     * @brief Get the singleton instance of OneSevenLiveCoreManager
     *
     * @param mainWindow OBS main window, only needs to be provided on first call
     * @return OneSevenLiveCoreManager& Reference to the singleton instance
     */
    static OneSevenLiveCoreManager& getInstance(QMainWindow* mainWindow = nullptr);

    /**
     * @brief Destroy the singleton instance
     */
    static void destroyInstance();

    /**
     * @brief Initialize the core manager
     *
     * @return bool Whether initialization was successful
     */
    bool initialize();

    /**
     * @brief Shutdown and cleanup resources
     */
    void shutdown();

    void syncPreviewDockLayoutToObsCanvas();

    /**
     * @brief Get OBS main window
     *
     * @return QMainWindow* Pointer to OBS main window
     */
    QMainWindow* getMainWindow() const override;

    /**
     * @brief Get menu manager
     *
     * @return OneSevenLiveMenuManager* Pointer to menu manager
     */
    OneSevenLiveMenuManager* getMenuManager() const override;

    /**
     * @brief Get API wrapper
     *
     * @return OneSevenLiveApiWrappers* Pointer to API wrapper
     */
    OneSevenLiveApiWrappers* getApiWrapper() const override;

    OneSevenLiveConfigManager* getConfigManager() const override;

    /**
     * @brief Get stream manager
     *
     * @return OneSevenLiveStreamManager* Pointer to stream manager
     */
    OneSevenLiveStreamManager* getStreamManager() const override;

    /**
     * @brief Get WebSocket server
     *
     * @return OneSevenLiveWebsocketServer* Pointer to WebSocket server
     */
    OneSevenLiveWebsocketServer* getWebsocketServer() const override;

    /**
     * @brief Get HTTP server
     *
     * @return OneSevenLiveHttpServer* Pointer to HTTP server
     */
    OneSevenLiveHttpServer* getHttpServer() const override;

    // Auth handlers accessors
    OneSevenLiveTwitchAuth* getTwitchAuth() const;
    OneSevenLiveYouTubeAuth* getYouTubeAuth() const;

    // Chat clients accessors
    OneSevenLiveYouTubeChatClient* getYouTubeChatClient() const;
    OneSevenLiveTwitchChatClient* getTwitchChatClient() const;
    OneSevenLiveAblyChatClient* getAblyChatClient() const;
    OneSevenLiveYouTubeClient* getYouTubeApiClient() const;

    AuthSessionService* getAuthSessionService() const;
    LocalGatewayService* getLocalGatewayService() const;

    // Chat clients lifecycle
    void createYouTubeChatClient();
    void createTwitchChatClient();
    void destroyYouTubeChatClient();
    void destroyTwitchChatClient();
    void createAblyChatClient();
    void destroyAblyChatClient();
    void connectAblyChat(const QString& roomId, const QString& token = QString());
    void disconnectAblyChat();
    void refreshRockZoneUserList();
    std::optional<nlohmann::json> getGiftByID(const std::string& giftID) const;
    void enqueueOrBroadcastChatEvent(const QString& type, const nlohmann::json& payload);

    void setConnection();

    // Chat tracking external calls
    void startYouTubeChatPolling(const QString& liveChatId);
    void stopYouTubeChatPolling();
    void connectTwitchChatClient(const QString& channel = QString());
    void disconnectTwitchChatClient();

    void setShuttingDown(bool v);
    bool isShuttingDown() const;

    bool isGiftsLoaded() const;
    bool isGiftsLoading() const;

   signals:
    void giftsLoaded();

   public:
    // Disable copy constructor and assignment operator
    OneSevenLiveCoreManager(const OneSevenLiveCoreManager&) = delete;
    OneSevenLiveCoreManager& operator=(const OneSevenLiveCoreManager&) = delete;

    friend class AuthSessionService;

    // Accessor for cancellation flag
    std::atomic<bool>* getCancelFlag() {
        return &cancelFlag_;
    }

    std::atomic<bool>* getShutdownCancelFlag() {
        return &shutdownCancelFlag_;
    }

    std::atomic<bool>* getSessionCancelFlag() {
        return &sessionCancelFlag_;
    }

    void setShutdownCancel(bool v) {
        shutdownCancelFlag_.store(v);
        cancelFlag_.store(v || sessionCancelFlag_.load());
    }

    void setSessionCancel(bool v) {
        sessionCancelFlag_.store(v);
        cancelFlag_.store(v || shutdownCancelFlag_.load());
    }

   private:
    std::atomic<bool> giftsLoading_{false};
    std::atomic<bool> cancelFlag_{false};
    std::atomic<bool> shutdownCancelFlag_{false};
    std::atomic<bool> sessionCancelFlag_{false};

   protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

   private:
    QObject* getUiOwner() override;
    OneSevenLiveStreamingStatus getStreamingStatus() const override;
    bool getStartupRestore() const override;
    void setStartupRestore(bool v) override;
    void requestFlushChatEventQueue() override;
    OneSevenLiveStreamingDock* getStreamingDock() const override;
    void setStreamingDock(OneSevenLiveStreamingDock* dock) override;
    QDockWidget* getChatDock() const override;
    void setChatDock(QDockWidget* dock) override;
    OneSevenLiveStreamListDock* getLiveListDock() const override;
    void setLiveListDock(OneSevenLiveStreamListDock* dock) override;
    OneSevenLiveRockZoneDock* getRockZoneDock() const override;
    void setRockZoneDock(OneSevenLiveRockZoneDock* dock) override;
    OneSevenLiveMultiRtmpDock* getMultiRtmpDock() const override;
    void setMultiRtmpDock(OneSevenLiveMultiRtmpDock* dock) override;
    OneSevenLivePreviewDock* getPreviewDock() const override;
    void setPreviewDock(OneSevenLivePreviewDock* dock) override;
    CustomizedCartoonDock* getCustomizedCartoonDock() const override;
    void setCustomizedCartoonDock(CustomizedCartoonDock* dock) override;
    CustomizedCartoonService* getCustomizedCartoonService() const override;

    // Private constructor, ensure instance can only be obtained through getInstance method
    explicit OneSevenLiveCoreManager(QMainWindow* mainWindow);

    // Private destructor
    ~OneSevenLiveCoreManager();

    // Singleton instance
    static OneSevenLiveCoreManager* instance;

    // OBS main window
    QMainWindow* mainWindow = nullptr;

    // Configuration storage

    // Initialization flag
    bool initialized = false;
    bool shuttingDown = false;

    // Flag to track if we are in startup dock restoration phase
    bool isStartupRestore = false;

    std::unique_ptr<OneSevenLiveConfigManager> configManager;

    std::unique_ptr<OneSevenLiveApiWrappers> apiWrapper;

    // Stream manager
    std::unique_ptr<OneSevenLiveStreamManager> streamManager;

    // Menu manager
    std::unique_ptr<OneSevenLiveMenuManager> menuManager;

    // std::unique_ptr<OneSevenLiveHttpServer> httpServer_;

    // std::shared_ptr<OneSevenLiveWebsocketServer> websocketServer_;

    void closeAllDocks();

    // Streaming Dock load status
    bool streamingDockFirstLoad = true;
    QPointer<OneSevenLiveStreamingDock> streamingDock;
    void handleStreamingClicked();
    void createStreamingDock();

    QPointer<QDockWidget> chatDock;
    void handleChatRoomClicked();

    bool liveListDockFirstLoad = true;
    QPointer<OneSevenLiveStreamListDock> liveListDock;
    void handleLiveListClicked();

    bool rockZoneDockFirstLoad = true;
    QPointer<OneSevenLiveRockZoneDock> rockZoneDock;
    void handleRockZoneClicked();
    void createRockZoneDock();

    bool multiRtmpDockFirstLoad = true;
    QPointer<OneSevenLiveMultiRtmpDock> multiRtmpDock;
    void handleMultiRtmpClicked();
    void createMultiRtmpDock();

    bool previewDockFirstLoad = true;
    QPointer<OneSevenLivePreviewDock> previewDock;
    void handlePreviewDockClicked();
    void createPreviewDock();

    bool customizedCartoonDockFirstLoad = true;
    QPointer<CustomizedCartoonDock> customizedCartoonDock;
    void handleCustomizedCartoonClicked();
    void createCustomizedCartoonDock();

    void saveDockState();

    void load17LiveConfig(const OneSevenLiveLoginData& loginData);

    void closeLive(bool isAutoClose = false);

    bool showAutoCloseConfirmation(const QString& message);

    OneSevenLiveStreamingStatus status = OneSevenLiveStreamingStatus::NotStarted;

    // Timer for checking stream status
    QPointer<QTimer> streamCheckTimer;
    std::atomic<bool> streamCheckInFlight{false};
    // Timer for periodic YouTube chat discovery
    QPointer<QTimer> ytChatDiscoverTimer;

    // Consecutive failure detection related variables
    int consecutiveFailureCount{0};  // Consecutive failure counter

    // Version update related methods
    void handleCheckUpdateClicked();
    void checkForUpdates();

    // Diagnostics related methods
    void handleDiagnosticsClicked();
    void handleCrashRecordsClicked();
    void handleSettingsClicked();

    void loadGifts();
    void loadGiftsFromConfig();
    void buildGiftsMapFromJson(const nlohmann::json& giftsJson);

    class OneSevenLiveUpdateManager* updateManager = nullptr;

    // Auth handlers
    std::unique_ptr<OneSevenLiveTwitchAuth> twitchAuth;
    std::unique_ptr<OneSevenLiveYouTubeAuth> youtubeAuth;

    // Chat clients
    std::unique_ptr<OneSevenLiveYouTubeChatClient> youtubeChatClient;
    std::unique_ptr<OneSevenLiveTwitchChatClient> twitchChatClient;
    std::unique_ptr<OneSevenLiveYouTubeClient> youtubeApiClient;
    std::unique_ptr<OneSevenLiveAblyChatClient> ablyChatClient;

    // Gifts lookup map: giftID (string) -> gift json
    std::unordered_map<std::string, nlohmann::json> giftsMap;

    bool initLocalServers();
    bool initConfigAndApi();
    void initAuthHandlers();
    bool initMenuAndBaseUI();
    void restoreRuntimeStateIfNeeded();
    void stopStreamingSafely();
    void saveAndCloseUI();
    void shutdownRtmpAndChat();
    void shutdownLocalServers();
    void cleanupTimersAndFlags();

    std::unique_ptr<CoreRuntime> runtime_;
    std::unique_ptr<DockOrchestrator> dockOrchestrator_;
    std::unique_ptr<AuthSessionService> authSessionService_;
    std::unique_ptr<CrashUploadService> crashUploadService_;
    std::unique_ptr<CustomizedCartoonService> customizedCartoonService_;
    std::unique_ptr<LocalGatewayService> localGatewayService_;
    std::unique_ptr<ChatBridgeService> chatBridgeService_;
    bool initIsLogin_{false};
    OneSevenLiveLoginData initLoginData_;
    bool previousRunClean_{true};

    void syncMenuDockVisibility();
    void flushChatEventQueue();
};

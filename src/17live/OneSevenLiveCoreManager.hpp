#pragma once

#include <QObject>
#include <QPointer>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "api/OneSevenLiveModels.hpp"
#include "utility/NetworkDiagnostics.hpp"

// Forward declarations
class QMainWindow;
class QTimer;
class QDockWidget;
class QProgressDialog;

class BrowserApp;

// Forward declaration of OneSevenLiveMenuManager class
class OneSevenLiveMenuManager;

class OneSevenLiveApiWrappers;

class OneSevenLiveConfigManager;

class OneSevenLiveStreamingDock;

class OneSevenLiveStreamListDock;

class OneSevenLiveRockZoneDock;

class OneSevenLiveHttpServer;

class QCefView;

/**
 * @brief OneSevenLiveCoreManager class is the core management class for the 17live plugin
 *
 * This class uses singleton pattern design as the control center for managing all 17live plugins.
 * Responsible for plugin initialization, configuration management, resource allocation and other
 * core functions.
 */
class OneSevenLiveCoreManager : public QObject {
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
     * @brief Initialize the core manager
     *
     * @return bool Whether initialization was successful
     */
    bool initialize();

    /**
     * @brief Shutdown and cleanup resources
     */
    void shutdown();

    /**
     * @brief Get OBS main window
     *
     * @return QMainWindow* Pointer to OBS main window
     */
    QMainWindow* getMainWindow() const;

    /**
     * @brief Get menu manager
     *
     * @return OneSevenLiveMenuManager* Pointer to menu manager
     */
    OneSevenLiveMenuManager* getMenuManager() const;

    /**
     * @brief Get API wrapper
     *
     * @return OneSevenLiveApiWrappers* Pointer to API wrapper
     */
    OneSevenLiveApiWrappers* getApiWrapper() const;

    OneSevenLiveConfigManager* getConfigManager() const;

    bool handleLoginClicked();

    // Disable copy constructor and assignment operator
    OneSevenLiveCoreManager(const OneSevenLiveCoreManager&) = delete;
    OneSevenLiveCoreManager& operator=(const OneSevenLiveCoreManager&) = delete;

   private:
    // Private constructor, ensure instance can only be obtained through getInstance method
    explicit OneSevenLiveCoreManager(QMainWindow* mainWindow);

    // Private destructor
    ~OneSevenLiveCoreManager();

    // Singleton instance
    static OneSevenLiveCoreManager* instance;

    // Once flag for thread-safe singleton creation using std::call_once
    static std::once_flag instanceOnceFlag;

    // OBS main window
    QMainWindow* mainWindow;

    // Configuration storage
    std::map<std::string, std::string> configMap;

    // Initialization flag
    bool initialized;

    // Flag to track if we are in startup dock restoration phase
    bool isStartupRestore = false;

    std::unique_ptr<OneSevenLiveConfigManager> configManager;

    // Menu manager
    std::unique_ptr<OneSevenLiveMenuManager> menuManager;

    std::unique_ptr<OneSevenLiveApiWrappers> apiWrapper;

    std::unique_ptr<OneSevenLiveHttpServer> httpServer_;

    /**
     * @brief Slot function to handle successful login
     *
     * @param userData User data returned after successful login
     */
    void handleLoginSuccess(const OneSevenLiveLoginData& userData);

    void handleLogoutClicked();

    // New login state management methods
    void handleLoginStateChanged(bool isLoggedIn,
                                 const OneSevenLiveLoginData& loginData = OneSevenLiveLoginData());
    void performLoginOperations(const OneSevenLiveLoginData& loginData);
    void performLogoutOperations();
    void restoreDockStatesOnLogin();
    void closeAllDocks();

    // Function to check if login status is valid
    bool checkLoginStatus();

    // Streaming Dock load status
    bool streamingDockFirstLoad = true;
    QPointer<OneSevenLiveStreamingDock> streamingDock;
    void handleStreamingClicked();
    void createStreamingDock();

    bool chatRoomDockFirstLoad = true;
    QPointer<QDockWidget> chatRoomDock;
    QPointer<QCefView> cefView;
    void handleChatRoomClicked();

    bool liveListDockFirstLoad = true;
    QPointer<OneSevenLiveStreamListDock> liveListDock;
    void handleLiveListClicked();

    bool rockZoneDockFirstLoad = true;
    QPointer<OneSevenLiveRockZoneDock> rockZoneDock;
    void handleRockZoneClicked();
    void createRockZoneDock();

    void saveDockState();

    void load17LiveConfig(const OneSevenLiveLoginData& loginData);

    void closeLive(bool isAutoClose = false);

    bool showAutoCloseConfirmation(const QString& message);

    OneSevenLiveStreamingStatus status = OneSevenLiveStreamingStatus::NotStarted;

    // Timer for checking stream status
    QPointer<QTimer> streamCheckTimer;

    // Consecutive failure detection related variables
    int consecutiveFailureCount{0};  // Consecutive failure counter

    // Version update related methods
    void handleCheckUpdateClicked();
    void checkForUpdates();

    void loadGifts();

    class OneSevenLiveUpdateManager* updateManager = nullptr;
};

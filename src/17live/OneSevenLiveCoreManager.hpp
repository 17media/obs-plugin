#pragma once

#include <QObject>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "api/OneSevenLiveModels.hpp"

// Forward declarations
class QMainWindow;
class QTimer;
class QDockWidget;

class BrowserApp;

// Forward declaration of OneSevenLiveMenuManager class
class OneSevenLiveMenuManager;

class OneSevenLiveApiWrappers;

class OneSevenLiveConfigManager;

class OneSevenLiveStreamingDock;

class OneSevenLiveStreamListDock;

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

    // Mutex for thread-safe singleton access
    static std::mutex instanceMutex;

    // OBS main window
    QMainWindow* mainWindow;

    // Configuration storage
    std::map<std::string, std::string> configMap;

    // Initialization flag
    bool initialized;

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

    // Function to check if login status is valid
    bool checkLoginStatus();

    // Streaming Dock load status
    bool streamingDockFirstLoad = true;
    OneSevenLiveStreamingDock* streamingDock{nullptr};
    void handleStreamingClicked();
    void createStreamingDock();

    bool chatRoomDockFirstLoad = true;
    QDockWidget* chatRoomDock{nullptr};
    QCefView* cefView{nullptr};
    void handleChatRoomClicked();

    bool liveListDockFirstLoad = true;
    OneSevenLiveStreamListDock* liveListDock{nullptr};
    void handleLiveListClicked();

    void saveDockState();

    void load17LiveConfig();

    void closeLive();

    OneSevenLiveStreamingStatus status = OneSevenLiveStreamingStatus::NotStarted;

    // Timer for checking stream status
    QTimer* streamCheckTimer{nullptr};
};

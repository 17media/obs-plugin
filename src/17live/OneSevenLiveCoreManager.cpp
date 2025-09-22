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
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <nlohmann/json.hpp>
#include <thread>

#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveHttpServer.hpp"
#include "OneSevenLiveLoginDialog.hpp"
#include "OneSevenLiveMenuManager.hpp"
#include "OneSevenLiveRockZoneDock.hpp"
#include "OneSevenLiveStreamListDock.hpp"
#include "OneSevenLiveStreamingDock.hpp"
#include "OneSevenLiveUpdateManager.hpp"
#include "QCefView.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "plugin-support.h"
#include "utility/Common.hpp"
#include "utility/Meta.hpp"

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
    : mainWindow(mainWindow_), initialized(false) {}

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

    try {
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
    } catch (const std::bad_alloc& e) {
        obs_log(LOG_ERROR, "[17Live Core] Memory allocation failed during initialization: %s",
                e.what());
        return false;
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[17Live Core] Exception during initialization: %s", e.what());
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[17Live Core] Unknown exception during initialization");
        return false;
    }

    OneSevenLiveLoginData loginData;
    configManager->getLoginData(loginData);

    bool isLogin = false;

    if (!loginData.jwtAccessToken.isEmpty()) {
        apiWrapper =
            std::make_unique<OneSevenLiveApiWrappers>(loginData.jwtAccessToken.toStdString());

        isLogin = checkLoginStatus();
    }

    // if not login, reinitialize apiWrapper
    if (!isLogin) {
        apiWrapper = std::make_unique<OneSevenLiveApiWrappers>();
    }

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

    QObject::connect(menuManager.get(), &OneSevenLiveMenuManager::checkUpdateClicked, this,
                     &OneSevenLiveCoreManager::handleCheckUpdateClicked);

    // Initialize update manager
    updateManager = new OneSevenLiveUpdateManager(this);

    // Connect update manager signals
    QObject::connect(
        updateManager, &OneSevenLiveUpdateManager::updateAvailable, this,
        [this](const QString& latestVersion, const QJsonArray& assets) {
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

    // Load gifts for logged in user
    loadGifts();

    // Check for updates
    std::thread updateThread([this]() { updateManager->checkForUpdates(); });
    updateThread.detach();

    isStartupRestore = true;

    // Handle login state during initialization
    if (isLogin) {
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

    // Save dock state before closing any docks
    saveDockState();

    closeAllDocks();

    // Clean up menu manager resources
    if (menuManager) {
        menuManager->cleanup();
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

    // Use the new centralized login state handler
    handleLoginStateChanged(true, loginData);
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

    // Update menu with user info
    QString username = loginData.userInfo.displayName;
    if (username.isEmpty()) {
        username = loginData.userInfo.openID;
    }
    menuManager->updateLoginStatus(true, username);

    // Load configuration
    load17LiveConfig(loginData);

    // Restore dock states if this is during startup and there are saved states
    if (isStartupRestore) {
        restoreDockStatesOnLogin();
        isStartupRestore = false;
    }
}

void OneSevenLiveCoreManager::performLogoutOperations() {
    obs_log(LOG_INFO, "performLogoutOperations");

    // Close all dock windows
    closeAllDocks();

    // Reset login status in menu
    menuManager->updateLoginStatus(false, "");

    // Clear login data
    configManager->clearLoginData();
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

        // Apply the saved dock layout
        mainWindow->restoreState(dockState);

        // Update menu visibility status after restoration
        if (menuManager) {
            menuManager->updateDockVisibility(chatRoomDock && chatRoomDock->isVisible(),
                                              streamingDock && streamingDock->isVisible(),
                                              liveListDock && liveListDock->isVisible(),
                                              rockZoneDock && rockZoneDock->isVisible());
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
    if (chatRoomDock) {
        chatRoomVisible = chatRoomDock->isVisible();
        chatRoomDock->disconnect(this);

        obs_log(LOG_INFO, "Closing chat room dock");

        if (cefView) {
            delete cefView;
            cefView = nullptr;
        }

        chatRoomDock->close();
        chatRoomDock->deleteLater();
        chatRoomDock = nullptr;
    }
    configManager->setDockVisibility("chatRoom", chatRoomVisible);

    // Update menu visibility status after closing all docks
    if (menuManager) {
        menuManager->updateDockVisibility(false, false, false, false);
    }
}

void OneSevenLiveCoreManager::handleLogoutClicked() {
    obs_log(LOG_INFO, "handleLogoutClicked");

    // Check if currently streaming
    if (status == OneSevenLiveStreamingStatus::Streaming) {
        // Show warning message to user about interrupting live stream
        QMessageBox msgBox;
        msgBox.setWindowTitle(obs_module_text("Logout.Warning.Title"));
        msgBox.setText(obs_module_text("Logout.Warning.Message"));
        QPushButton* confirmButton =
            msgBox.addButton(obs_module_text("Logout.Warning.Button.Yes"), QMessageBox::YesRole);
        QPushButton* cancelButton =
            msgBox.addButton(obs_module_text("Logout.Warning.Button.No"), QMessageBox::NoRole);
        msgBox.setDefaultButton(cancelButton);

        msgBox.exec();
        if (msgBox.clickedButton() != confirmButton) {
            // User cancelled the operation
            return;
        }

        // User confirmed, stop streaming using the streaming dock's method
        closeLive(false);  // Pass false to indicate manual stream closure
    }

    // Use the new centralized logout state handler
    handleLoginStateChanged(false);
}

void OneSevenLiveCoreManager::closeLive(bool isAutoClose) {
    if (streamingDock) {
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

        streamingDock->closeLive(currUserID, currLiveStreamID, isAutoClose);
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
            chatRoomDock && chatRoomDock->isVisible(), streamingDock && streamingDock->isVisible(),
            liveListDock && liveListDock->isVisible(), rockZoneDock && rockZoneDock->isVisible());
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
    streamingDock =
        new OneSevenLiveStreamingDock(mainWindow, apiWrapper.get(), configManager.get());
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

    streamingDock->loadRoomInfo(loginData.userInfo.roomID);

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
                            std::string liveStreamID;
                            if (configManager->getConfigValue("LiveStreamID", liveStreamID)) {
                                if (!apiWrapper->CheckStream(liveStreamID)) {
                                    // Stream check failed, increment consecutive failure count
                                    consecutiveFailureCount++;
                                    obs_log(LOG_WARNING,
                                            "Stream check failed. Consecutive failures: %d/%d",
                                            consecutiveFailureCount, MAX_CONSECUTIVE_FAILURES);

                                    // Only trigger auto-close when consecutive failures reach
                                    // threshold
                                    if (consecutiveFailureCount >= MAX_CONSECUTIVE_FAILURES) {
                                        obs_log(LOG_ERROR,
                                                "Stream check failed %d times consecutively. "
                                                "Showing auto-close confirmation.",
                                                MAX_CONSECUTIVE_FAILURES);

                                        // Show confirmation dialog before auto-closing
                                        QString message =
                                            QString(obs_module_text(
                                                        "Live.Settings.CloseLive.Auto.Message"))
                                                .arg(MAX_CONSECUTIVE_FAILURES);

                                        if (showAutoCloseConfirmation(message)) {
                                            obs_log(LOG_INFO,
                                                    "User confirmed auto-close live stream due to "
                                                    "stream check failures");
                                            closeLive(
                                                true);  // Pass true to indicate this is auto-close
                                            if (streamCheckTimer) {
                                                streamCheckTimer->stop();
                                                streamCheckTimer->deleteLater();
                                                streamCheckTimer = nullptr;
                                            }
                                        } else {
                                            obs_log(LOG_INFO,
                                                    "User cancelled auto-close live stream");
                                        }
                                        // Reset failure counter regardless of user choice
                                        consecutiveFailureCount = 0;
                                    }
                                } else {
                                    // Stream check succeeded, reset consecutive failure counter
                                    if (consecutiveFailureCount > 0) {
                                        obs_log(LOG_INFO,
                                                "Stream check succeeded. Resetting failure count "
                                                "from %d to 0.",
                                                consecutiveFailureCount);
                                        consecutiveFailureCount = 0;
                                    }
                                }
                            }
                        });
                    }
                    streamCheckTimer->start(30000);  // 30 seconds
                } else {
                    // Stop timer when not streaming
                    if (streamCheckTimer) {
                        streamCheckTimer->stop();
                        streamCheckTimer->deleteLater();
                        streamCheckTimer = nullptr;
                    }
                }
            });

        connect(streamingDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            menuManager->updateDockVisibility(chatRoomDock && chatRoomDock->isVisible(), visible,
                                              liveListDock && liveListDock->isVisible());
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
            chatRoomDock && chatRoomDock->isVisible(), streamingDock && streamingDock->isVisible(),
            liveListDock && liveListDock->isVisible(), rockZoneDock && rockZoneDock->isVisible());
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
            menuManager->updateDockVisibility(chatRoomDock && chatRoomDock->isVisible(),
                                              streamingDock && streamingDock->isVisible(),
                                              liveListDock && liveListDock->isVisible(), visible);
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
            menuManager->updateDockVisibility(chatRoomDock && chatRoomDock->isVisible(),
                                              streamingDock && streamingDock->isVisible(), visible);
        });
    } else {
        liveListDock->setVisible(!liveListDock->isVisible());
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(chatRoomDock && chatRoomDock->isVisible(),
                                          streamingDock && streamingDock->isVisible(),
                                          liveListDock && liveListDock->isVisible());
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

    if (!chatRoomDock) {
        chatRoomDock = new QDockWidget(obs_module_text("ChatRoom.Title"), mainWindow);
        chatRoomDock->setObjectName("OneSevenLiveChatRoomDock");
        chatRoomDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        chatRoomDock->setFeatures(QDockWidget::DockWidgetMovable |
                                  QDockWidget::DockWidgetFloatable |
                                  QDockWidget::DockWidgetClosable);

        mainWindow->addDockWidget(Qt::RightDockWidgetArea, chatRoomDock);

        connect(chatRoomDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            menuManager->updateDockVisibility(chatRoomDock && chatRoomDock->isVisible(),
                                              streamingDock && streamingDock->isVisible(),
                                              liveListDock && liveListDock->isVisible());
        });
    } else if (chatRoomDock->isVisible()) {
        // cefView will be destroyed when dock is hidden
        if (cefView) {
            cefView->deleteLater();
            cefView = nullptr;
        }
        chatRoomDock->hide();
        return;
    }

    if (cefView) {
        cefView->deleteLater();
        cefView = nullptr;
    }

    cefView = new QCefView(chatRoomDock);
    chatRoomDock->setWidget(cefView);

    OneSevenLiveLoginData loginData;
    if (!configManager->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    std::string locale = GetCurrentLocale();

    QString chatUrl =
        QString("http://localhost:%1/%2.html?roomID=%3&userID=%4")
            .arg(QString::number(httpServer_->getPort()), QString::fromStdString(locale),
                 QString::number(loginData.userInfo.roomID), loginData.userInfo.userID);
    obs_log(LOG_INFO, "chatUrl: %s", chatUrl.toStdString().c_str());

    chatRoomDock->resize(378, 600);
    cefView->loadUrl(chatUrl);

    // Only restore state during startup, otherwise set floating and center
    if (isStartupRestore) {
        // During startup restoration, the state will be restored by initialize() method
        chatRoomDock->setVisible(true);
    } else {
        // First time creation or manual creation - set floating and center
        chatRoomDock->setFloating(true);
        chatRoomDock->setVisible(true);

        // Center the dock on the main window
        QRect mainWindowGeometry = mainWindow->geometry();
        int x = mainWindowGeometry.x() + (mainWindowGeometry.width() - chatRoomDock->width()) / 2;
        int y = mainWindowGeometry.y() + (mainWindowGeometry.height() - chatRoomDock->height()) / 2;
        chatRoomDock->move(x, y);
    }

    // Update chat room visibility status (considered visible when CEF view is open)
    if (menuManager) {
        menuManager->updateDockVisibility(true, streamingDock && streamingDock->isVisible(),
                                          liveListDock && liveListDock->isVisible());
    }
}

void OneSevenLiveCoreManager::loadGifts() {
    obs_log(LOG_INFO, "Starting to load gifts asynchronously");

    // Run gift loading in a separate thread to avoid blocking main thread
    std::thread giftLoadThread([this]() {
        try {
            std::string language = GetCurrentLanguage();

            Json apiResult;
            bool success = apiWrapper->GetGifts(language, apiResult);

            if (success) {
                configManager->saveGifts(apiResult);
                obs_log(LOG_INFO, "Gifts loaded and saved successfully");

                // Reload chat room dock to support new gifts
                if (chatRoomDock && chatRoomDock->isVisible() && cefView) {
                    obs_log(LOG_INFO, "Reloading chat room to support new gifts");
                    cefView->reload();
                    obs_log(LOG_INFO, "Chat room reloaded with new gifts support");
                }
            } else {
                obs_log(LOG_WARNING, "Failed to load gifts from API");
            }
        } catch (const std::exception& e) {
            obs_log(LOG_ERROR, "Exception while loading gifts: %s", e.what());
        } catch (...) {
            obs_log(LOG_ERROR, "Unknown exception while loading gifts");
        }
    });

    giftLoadThread.detach();
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

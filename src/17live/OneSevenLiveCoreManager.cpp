#include "OneSevenLiveCoreManager.hpp"
#include <QMainWindow>
#include <QDesktopServices>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QScrollArea>
#include <QLineEdit>

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

// Initialize static member variables
OneSevenLiveCoreManager* OneSevenLiveCoreManager::instance = nullptr;
std::mutex OneSevenLiveCoreManager::instanceMutex;

OneSevenLiveCoreManager& OneSevenLiveCoreManager::getInstance(QMainWindow* mainWindow)
{
    // Use double-checked locking pattern to ensure thread safety
    if (instance == nullptr) {
        std::lock_guard<std::mutex> lock(instanceMutex);
        if (instance == nullptr) {
            if (mainWindow == nullptr) {
                throw std::runtime_error("mainWindow parameter must be provided on first call to getInstance");
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
    // Ensure shutdown is called before destruction
    if (initialized) {
        shutdown();
    }
}

bool OneSevenLiveCoreManager::initialize()
{
    // Prevent duplicate initialization
    if (initialized) {
        return true;
    }

    // Initialize and start HTTP server
    // "html" is the path relative to obs_get_module_data_path()
    httpServer_ = std::make_unique<OneSevenLiveHttpServer>("localhost", 0, "html/chat");
    if (!httpServer_->start()) {
        blog(LOG_ERROR, "[17Live Core] Failed to start HTTP server.");
        // Decide whether to interrupt the entire initialization due to HTTP server startup failure based on requirements
        // return false; 
    } else {
        blog(LOG_INFO, "[17Live Core] HTTP server started successfully.");
    }

    // Initialize configuration manager
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

    // Initialize menu manager
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

        // Load meta data
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
    // In the initialize method, add the following code after initializing configManager

    // Asynchronously get configuration
    std::thread configThread([this]() {
        // Get current region and language
        OneSevenLiveLoginData loginData;
        configManager->getLoginData(loginData);
    
        std::string region = loginData.userInfo.region.toStdString();
        if (region.empty()) {
            region = "TW"; // Default region
        }
    
        std::string language = GetCurrentLanguage();
    
        // Call API to get configuration
        json11::Json configJson;
        if (apiWrapper->GetConfig(region, language, configJson)) {
            // Save configuration
            configManager->setConfig(configJson);
            obs_log(LOG_INFO, "Config loaded successfully");
        } else {
            obs_log(LOG_ERROR, "Failed to load config from API");
        }
    });
  
    // Detach thread to let it run in background
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

    // Clean up menu manager resources
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

    // Connect login success signal to main window slot function
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

    // Update menu
    QString username = loginData.userInfo.displayName;
    if (username.isEmpty()) {
        username = loginData.userInfo.openID;
    }
    menuManager->updateLoginStatus(true, username);
}

void OneSevenLiveCoreManager::handleLogoutClicked()
{
    obs_log(LOG_INFO, "handleLogoutClicked");

    // Check if currently streaming
    if (status == OneSevenLiveStreamingStatus::Streaming) {
        // Show warning message to user about interrupting live stream
        QMessageBox msgBox;
        msgBox.setWindowTitle(obs_module_text("Logout.Warning.Title"));
        msgBox.setText(obs_module_text("Logout.Warning.Message"));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        
        int ret = msgBox.exec();
        if (ret == QMessageBox::Yes) {
            // User confirmed, stop streaming using the streaming dock's method
            if (streamingDock) {
                streamingDock->closeLive();
            }
        } else {
            // User cancelled, do nothing
            return;
        }
    }

    // Close all dock windows to avoid incorrect operations after logout
    if (streamingDock) {
        streamingDock->close();
        streamingDock = nullptr;
    }
    
    if (liveListDock) {
        liveListDock->close();
        liveListDock = nullptr;
    }
    
    // Close chat room window (if exists)
    if (cef_window && cef_window->isVisible()) {
        cef_window->close();
    }
    
    // Reset login status
    menuManager->updateLoginStatus(false, "");
    configManager->clearLoginData();
}

void OneSevenLiveCoreManager::handleStreamingClicked()
{
    obs_log(LOG_INFO, "handleStreamingClicked");

    if (!streamingDock) {
        createStreamingDock();
    } else {
        streamingDock->setVisible(!streamingDock->isVisible());

        QByteArray dockState = configManager->getDockState();
        if (mainWindow->isVisible())
            mainWindow->restoreState(dockState);
    }

    // Update menu item checked status
    if (menuManager) {
        menuManager->updateDockVisibility(cef_window && cef_window->isVisible(),
                                        streamingDock && streamingDock->isVisible(),
                                        liveListDock && liveListDock->isVisible());
    }
}

void OneSevenLiveCoreManager::createStreamingDock()
{ 
    if (streamingDock) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!configManager->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    // Create and show streaming window
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

    connect(streamingDock, &OneSevenLiveStreamingDock::streamStatusUpdated, this, [this] (OneSevenLiveStreamingStatus status_) {
        status = status_;
        if (liveListDock) {
            liveListDock->setStatus(status_);
        }
    });

    connect(streamingDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        menuManager->updateDockVisibility(cef_window && cef_window->isVisible(),
                                        visible,
                                        liveListDock && liveListDock->isVisible());
    });
    
    // Connect close signal to main window slot function
    connect(streamingDock, &QDockWidget::destroyed, this, [this]() {
        saveDockState();
    });

    streamingDock->loadRoomInfo(loginData.userInfo.roomID);
}

void OneSevenLiveCoreManager::handleLiveListClicked()
{
    obs_log(LOG_INFO, "handleLiveListClicked");

    if (!liveListDock) {

        liveListDock = new OneSevenLiveStreamListDock(mainWindow, configManager.get(), status);
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
                createStreamingDock();
            }

            // Show streamingDock in center of desktop
            streamingDock->setVisible(true);
            streamingDock->raise();
            streamingDock->activateWindow();
            
            // Move to center of main window
            QRect mainWindowGeometry = mainWindow->geometry();
            int x = mainWindowGeometry.x() + (mainWindowGeometry.width() - streamingDock->width()) / 2;
            int y = mainWindowGeometry.y() + (mainWindowGeometry.height() - streamingDock->height()) / 2;
            streamingDock->move(x, y);
    
            streamingDock->createLiveWithRequest(request);
        });
    
        connect(liveListDock, &OneSevenLiveStreamListDock::editLiveClicked, this, [this] (const OneSevenLiveStreamInfo& info) {
            // Create streamingDock if it doesn't exist
            if (!streamingDock) {
                createStreamingDock();
            }
            
            // Edit live with info
            streamingDock->editLiveWithInfo(info);
            
            // Show streamingDock in center of desktop
            streamingDock->setVisible(true);
            streamingDock->raise();
            streamingDock->activateWindow();
            
            // Move to center of main window
            QRect mainWindowGeometry = mainWindow->geometry();
            int x = mainWindowGeometry.x() + (mainWindowGeometry.width() - streamingDock->width()) / 2;
            int y = mainWindowGeometry.y() + (mainWindowGeometry.height() - streamingDock->height()) / 2;
            streamingDock->move(x, y);
            
            // Scroll to title edit box and focus on it
            QTimer::singleShot(100, [this]() {
                if (streamingDock) {
                    QScrollArea *scrollArea = streamingDock->findChild<QScrollArea*>();
                    QLineEdit *titleEdit = streamingDock->findChild<QLineEdit*>("titleEdit");
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
            menuManager->updateDockVisibility(cef_window && cef_window->isVisible(),
                                            streamingDock && streamingDock->isVisible(),
                                            visible);
        });
    
        // Connect close signal to main window slot function
        connect(liveListDock, &QDockWidget::destroyed, this, [this]() {
            saveDockState();
        });
    } else {
        liveListDock->setVisible(!liveListDock->isVisible());
        
        QByteArray dockState = configManager->getDockState();
        if (mainWindow->isVisible())
            mainWindow->restoreState(dockState);
    }

    // Update menu item checked status
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
    
    // Update chat room visibility status (considered visible when CEF view is open)
    if (menuManager) {
        menuManager->updateDockVisibility(true, 
                                        streamingDock && streamingDock->isVisible(), 
                                        liveListDock && liveListDock->isVisible());
    }
}

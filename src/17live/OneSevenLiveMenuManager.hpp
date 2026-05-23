#pragma once

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QString>
#include <memory>

class OneSevenLiveMenuManager : public QObject {
    Q_OBJECT

   public:
    OneSevenLiveMenuManager(QMainWindow* mainWindow);
    ~OneSevenLiveMenuManager();

    /**
     * @brief Initialize menu manager
     *
     * @return bool Whether initialization was successful
     */
    bool initialize();

    void updateLoginStatus(bool logged, QString username = "");
    void checkUpdate();
    void handleLogin();
    void handleLogout();
    void cleanup();

    // Update dock window visibility status
    void updateDockVisibility(bool chatRoomVisible, bool broadcastVisible, bool liveListVisible,
                              bool rockZoneVisible = false, bool multiRtmpVisible = false,
                              bool previewDockVisible = false, bool customizedCartoonVisible = false);

    // Update menu item enable status
    void updateMenuItemsEnabled();

   signals:
    void chatRoomClicked();
    void settingsClicked();
    void streamingClicked();
    void liveListClicked();
    void rockZoneClicked();
    void multiRtmpClicked();
    void previewDockClicked();
    void customizedCartoonClicked();
    void helpClicked();
    void loginClicked();
    void logoutClicked();
    void checkUpdateClicked();
    void diagnosticsClicked();
    void crashRecordsClicked();

   private:
    QMainWindow* mainWindow = nullptr;
    QMenu* menu = nullptr;
    QMenu* dockSubMenu = nullptr;
    QAction* chatRoomAction = nullptr;
    QAction* settingsAction = nullptr;
    QAction* broadcastAction = nullptr;
    QAction* liveListAction = nullptr;
    QAction* rockZoneAction = nullptr;
    QAction* multiRtmpAction = nullptr;
    QAction* previewDockAction = nullptr;
    QAction* customizedCartoonAction = nullptr;
    QAction* helpAction = nullptr;
    QAction* checkUpdateAction = nullptr;
    QAction* diagnosticsAction = nullptr;
    QAction* crashRecordsAction = nullptr;
    QAction* loginAction = nullptr;
    bool isLoggedIn = false;

    // Dock window visibility status
    bool isChatRoomVisible = false;
    bool isBroadcastVisible = false;
    bool isLiveListVisible = false;
    bool isRockZoneVisible = false;
    bool isMultiRtmpVisible = false;
    bool isPreviewDockVisible = false;
    bool isCustomizedCartoonVisible = false;
};

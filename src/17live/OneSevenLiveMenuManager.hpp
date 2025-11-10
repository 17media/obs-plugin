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
                              bool previewDockVisible = false);

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
    void helpClicked();
    void loginClicked();
    void logoutClicked();
    void checkUpdateClicked();
    void diagnosticsClicked();

   private:
    QMainWindow* mainWindow;
    QMenu* menu;
    QMenu* dockSubMenu;
    QAction* chatRoomAction;
    QAction* settingsAction;
    QAction* broadcastAction;
    QAction* liveListAction;
    QAction* rockZoneAction;
    QAction* multiRtmpAction;
    QAction* previewDockAction;
    QAction* helpAction;
    QAction* checkUpdateAction;
    QAction* diagnosticsAction;
    QAction* loginAction;
    bool isLoggedIn;

    // Dock window visibility status
    bool isChatRoomVisible;
    bool isBroadcastVisible;
    bool isLiveListVisible;
    bool isRockZoneVisible;
    bool isMultiRtmpVisible;
    bool isPreviewDockVisible;
};

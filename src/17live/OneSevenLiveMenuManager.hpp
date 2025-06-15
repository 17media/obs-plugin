#pragma once

#include <QMenu>
#include <QAction>
#include <memory>
#include <QString>
#include <QMainWindow>

class OneSevenLiveMenuManager : public QObject {
    Q_OBJECT

public:
    OneSevenLiveMenuManager(QMainWindow* mainWindow);
    ~OneSevenLiveMenuManager();

    /**
     * @brief 初始化菜单管理器
     * 
     * @return bool 初始化是否成功
     */
    bool initialize();

    void updateLoginStatus(bool logged, QString username = "");
    void checkUpdate();
    void handleLogin();
    void handleLogout();
    void cleanup();
    
    // 更新 dock 窗口可见状态
    void updateDockVisibility(bool chatRoomVisible, bool broadcastVisible, bool liveListVisible);
    
    // 更新菜单项启用状态
    void updateMenuItemsEnabled();

signals:
    void chatRoomClicked();
    void settingsClicked();
    void streamingClicked();
    void liveListClicked();
    void helpClicked();
    void loginClicked();
    void logoutClicked();
    void checkUpdateClicked();
    
private:
    QMainWindow* mainWindow;
    QMenu* menu;
    QMenu* dockSubMenu;
    QAction* chatRoomAction;
    QAction* settingsAction;
    QAction* broadcastAction;
    QAction* liveListAction;
    QAction* helpAction;
    QAction* checkUpdateAction;
    QAction* loginAction;
    bool isLoggedIn;
    
    // Dock 窗口可见状态
    bool isChatRoomVisible;
    bool isBroadcastVisible;
    bool isLiveListVisible;

};

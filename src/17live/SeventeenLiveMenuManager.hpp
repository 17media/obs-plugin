#pragma once

#include <QMenu>
#include <QAction>
#include <memory>

// 前向声明
class QMainWindow;

namespace seventeenlive {

class SeventeenLiveMenuManager : public QObject {
    Q_OBJECT

public:
    SeventeenLiveMenuManager(QMainWindow* mainWindow);
    ~SeventeenLiveMenuManager();

    /**
     * @brief 初始化菜单管理器
     * 
     * @return bool 初始化是否成功
     */
    bool initialize();

    void updateLoginStatus(bool logged);
    void checkUpdate();
    void handleLogin();
    void handleLogout();
    void cleanup();

signals:
    void chatRoomClicked();
    void settingsClicked();
    void broadcastClicked();
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
    QAction* helpAction;
    QAction* checkUpdateAction;
    QAction* loginAction;
    bool isLoggedIn;

};

};

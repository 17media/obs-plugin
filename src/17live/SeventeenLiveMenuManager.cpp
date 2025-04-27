#include <obs-module.h>

#include "SeventeenLiveMenuManager.hpp"
#include <QMainWindow>
#include <QMenuBar>

#include "moc_SeventeenLiveMenuManager.cpp"

namespace seventeenlive {
SeventeenLiveMenuManager::SeventeenLiveMenuManager(QMainWindow* parent)
    : mainWindow(parent), isLoggedIn(false)
{
    // 创建17Live菜单
    menu = mainWindow->menuBar()->addMenu(obs_module_text("17Live"));

    // add submenu fro dock menu
    dockSubMenu = new QMenu(obs_module_text("Menu.Dock"));
    menu->addMenu(dockSubMenu);

    // add submenu item
    chatRoomAction = dockSubMenu->addAction(obs_module_text("Menu.ChatRoom"));
    connect(chatRoomAction, &QAction::triggered, this, [this](){
        emit chatRoomClicked();
    });

    settingsAction = dockSubMenu->addAction(obs_module_text("Menu.Settings"));
    connect(settingsAction, &QAction::triggered, this, [this](){
        emit settingsClicked();
    });

    broadcastAction = dockSubMenu->addAction(obs_module_text("Menu.Broadcast"));
    connect(broadcastAction, &QAction::triggered, this, [this](){
        emit broadcastClicked();
    });
    
    menu->addSeparator();

    // common menu
    helpAction = menu->addAction(obs_module_text("Menu.Help"));

    // 创建检查更新菜单项
    checkUpdateAction = menu->addAction(obs_module_text("Menu.CheckUpdate"));
    connect(checkUpdateAction, &QAction::triggered, this, &SeventeenLiveMenuManager::checkUpdate);

    menu->addSeparator();

    // 创建登录菜单项
    loginAction = menu->addAction(obs_module_text("Menu.SignIn"));
    connect(loginAction, &QAction::triggered, this, &SeventeenLiveMenuManager::handleLogin);
}

SeventeenLiveMenuManager::~SeventeenLiveMenuManager()
{
}

void SeventeenLiveMenuManager::updateLoginStatus(bool logged, QString username)
{
    isLoggedIn = logged;
    QString text = QString::fromStdString(obs_module_text("Menu.SignIn"));
    if (isLoggedIn) {
        if (username.isEmpty()) {
            text = QString::fromStdString(obs_module_text("Menu.SignOut"));
        } else {
            text = username + ": " + QString::fromStdString(obs_module_text("Menu.SignOut"));
        }
    }
    loginAction->setText(text);
    
    if (isLoggedIn) {
        disconnect(loginAction, &QAction::triggered, this, &SeventeenLiveMenuManager::handleLogin);
        connect(loginAction, &QAction::triggered, this, &SeventeenLiveMenuManager::handleLogout);
    } else {
        disconnect(loginAction, &QAction::triggered, this, &SeventeenLiveMenuManager::handleLogout);
        connect(loginAction, &QAction::triggered, this, &SeventeenLiveMenuManager::handleLogin);
    }
}

void SeventeenLiveMenuManager::handleLogin()
{
    emit loginClicked();
}

void SeventeenLiveMenuManager::handleLogout()
{
    emit logoutClicked();
}

void SeventeenLiveMenuManager::checkUpdate()
{
    emit checkUpdateClicked();
}

void SeventeenLiveMenuManager::cleanup()
{
    if (dockSubMenu) {
        delete dockSubMenu;
        dockSubMenu = nullptr;
    }

    if (menu) {
        delete menu;
        menu = nullptr;
    }

    chatRoomAction = nullptr;
    settingsAction = nullptr;
    broadcastAction = nullptr;
    helpAction = nullptr;
    loginAction = nullptr;
    checkUpdateAction = nullptr;
}

} // namespace seventeenlive

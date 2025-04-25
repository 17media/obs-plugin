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

    // 创建登录菜单项
    loginAction = menu->addAction(obs_module_text("Menu.SignIn"));
    connect(loginAction, &QAction::triggered, this, &SeventeenLiveMenuManager::handleLogin);

    // 创建检查更新菜单项
    checkUpdateAction = menu->addAction(obs_module_text("Menu.CheckUpdate"));
    connect(checkUpdateAction, &QAction::triggered, this, &SeventeenLiveMenuManager::checkUpdate);
}

SeventeenLiveMenuManager::~SeventeenLiveMenuManager()
{
}

void SeventeenLiveMenuManager::updateLoginStatus(bool logged)
{
    isLoggedIn = logged;
    loginAction->setText(isLoggedIn ? obs_module_text("Menu.SignOut") : obs_module_text("Menu.SignIn"));
    
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
    menu = nullptr;
    loginAction = nullptr;
    checkUpdateAction = nullptr;
}

} // namespace seventeenlive

#include <obs-module.h>

#include "OneSevenLiveMenuManager.hpp"
#include <QMenuBar>
#include <QUrl>
#include <QDesktopServices>

#include "moc_OneSevenLiveMenuManager.cpp"

OneSevenLiveMenuManager::OneSevenLiveMenuManager(QMainWindow* parent)
    : mainWindow(parent), isLoggedIn(false), isChatRoomVisible(false), isBroadcastVisible(false), isLiveListVisible(false)
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

    broadcastAction = dockSubMenu->addAction(obs_module_text("Menu.Broadcast"));
    connect(broadcastAction, &QAction::triggered, this, [this](){
        emit streamingClicked();
    });

    liveListAction = dockSubMenu->addAction(obs_module_text("Menu.LiveList"));
    connect(liveListAction, &QAction::triggered, this, [this](){
        emit liveListClicked();
    });
    
    menu->addSeparator();

    // common menu
    helpAction = menu->addAction(obs_module_text("Menu.Help"));

    connect(helpAction, &QAction::triggered, this, [this](){
        // open url obs_module_text("Menu.Help.Url");
	    QUrl url = QUrl(obs_module_text("Menu.Help.Url"), QUrl::TolerantMode);
	    QDesktopServices::openUrl(url);
    });

    // 创建检查更新菜单项
    checkUpdateAction = menu->addAction(obs_module_text("Menu.CheckUpdate"));
    connect(checkUpdateAction, &QAction::triggered, this, &OneSevenLiveMenuManager::checkUpdate);

    menu->addSeparator();

    // 创建登录菜单项
    loginAction = menu->addAction(obs_module_text("Menu.SignIn"));
    connect(loginAction, &QAction::triggered, this, &OneSevenLiveMenuManager::handleLogin);
    
    // 初始化菜单项启用状态
    updateMenuItemsEnabled();
    
    // 初始化菜单项勾选状态
    chatRoomAction->setCheckable(true);
    broadcastAction->setCheckable(true);
    liveListAction->setCheckable(true);
    chatRoomAction->setChecked(false);
    broadcastAction->setChecked(false);
    liveListAction->setChecked(false);
}

OneSevenLiveMenuManager::~OneSevenLiveMenuManager()
{
}

void OneSevenLiveMenuManager::updateLoginStatus(bool logged, QString username)
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
        disconnect(loginAction, &QAction::triggered, this, &OneSevenLiveMenuManager::handleLogin);
        connect(loginAction, &QAction::triggered, this, &OneSevenLiveMenuManager::handleLogout);
    } else {
        disconnect(loginAction, &QAction::triggered, this, &OneSevenLiveMenuManager::handleLogout);
        connect(loginAction, &QAction::triggered, this, &OneSevenLiveMenuManager::handleLogin);
    }
    
    // 更新菜单项启用状态
    updateMenuItemsEnabled();
}

void OneSevenLiveMenuManager::handleLogin()
{
    emit loginClicked();
}

void OneSevenLiveMenuManager::handleLogout()
{
    emit logoutClicked();
}

void OneSevenLiveMenuManager::checkUpdate()
{
    emit checkUpdateClicked();
}

void OneSevenLiveMenuManager::updateDockVisibility(bool chatRoomVisible, bool broadcastVisible, bool liveListVisible)
{
    // 更新可见状态变量
    isChatRoomVisible = chatRoomVisible;
    isBroadcastVisible = broadcastVisible;
    isLiveListVisible = liveListVisible;
    
    // 更新菜单项勾选状态
    if (chatRoomAction) {
        chatRoomAction->setCheckable(true);
        chatRoomAction->setChecked(isChatRoomVisible);
    }

    if (broadcastAction) {
        broadcastAction->setCheckable(true);    
        broadcastAction->setChecked(isBroadcastVisible);
    }

    if (liveListAction) {
        liveListAction->setCheckable(true);    
        liveListAction->setChecked(isLiveListVisible);
    }
}

void OneSevenLiveMenuManager::updateMenuItemsEnabled()
{
    // 根据登录状态更新菜单项启用状态
    chatRoomAction->setEnabled(isLoggedIn);
    broadcastAction->setEnabled(isLoggedIn);
    liveListAction->setEnabled(isLoggedIn);
}

void OneSevenLiveMenuManager::cleanup()
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

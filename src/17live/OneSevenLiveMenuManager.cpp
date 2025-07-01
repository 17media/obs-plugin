#include "OneSevenLiveMenuManager.hpp"

#include <obs-module.h>

#include <QDesktopServices>
#include <QMenuBar>
#include <QUrl>

#include "moc_OneSevenLiveMenuManager.cpp"

OneSevenLiveMenuManager::OneSevenLiveMenuManager(QMainWindow* parent)
    : mainWindow(parent),
      isLoggedIn(false),
      isChatRoomVisible(false),
      isBroadcastVisible(false),
      isLiveListVisible(false) {
    // Create 17Live menu
    menu = mainWindow->menuBar()->addMenu(obs_module_text("17Live"));

    // Add submenu for dock menu
    dockSubMenu = new QMenu(obs_module_text("Menu.Dock"));
    menu->addMenu(dockSubMenu);

    // Add submenu items
    chatRoomAction = dockSubMenu->addAction(obs_module_text("Menu.ChatRoom"));
    connect(chatRoomAction, &QAction::triggered, this, [this]() { emit chatRoomClicked(); });

    broadcastAction = dockSubMenu->addAction(obs_module_text("Menu.Broadcast"));
    connect(broadcastAction, &QAction::triggered, this, [this]() { emit streamingClicked(); });

    liveListAction = dockSubMenu->addAction(obs_module_text("Menu.LiveList"));
    connect(liveListAction, &QAction::triggered, this, [this]() { emit liveListClicked(); });

    menu->addSeparator();

    // Common menu
    helpAction = menu->addAction(obs_module_text("Menu.Help"));

    connect(helpAction, &QAction::triggered, this, [this]() {
        // open url obs_module_text("Menu.Help.Url");
        QUrl url = QUrl(obs_module_text("Menu.Help.Url"), QUrl::TolerantMode);
        QDesktopServices::openUrl(url);
    });

    // Create check update menu item
    checkUpdateAction = menu->addAction(obs_module_text("Menu.CheckUpdate"));
    connect(checkUpdateAction, &QAction::triggered, this, &OneSevenLiveMenuManager::checkUpdate);

    menu->addSeparator();

    // Create login menu item
    loginAction = menu->addAction(obs_module_text("Menu.SignIn"));
    connect(loginAction, &QAction::triggered, this, &OneSevenLiveMenuManager::handleLogin);

    // Initialize menu item enabled status
    updateMenuItemsEnabled();

    // Initialize menu item checked status
    chatRoomAction->setCheckable(true);
    broadcastAction->setCheckable(true);
    liveListAction->setCheckable(true);
    chatRoomAction->setChecked(false);
    broadcastAction->setChecked(false);
    liveListAction->setChecked(false);
}

OneSevenLiveMenuManager::~OneSevenLiveMenuManager() {}

void OneSevenLiveMenuManager::updateLoginStatus(bool logged, QString username) {
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

    // Update menu item enabled status
    updateMenuItemsEnabled();
}

void OneSevenLiveMenuManager::handleLogin() {
    emit loginClicked();
}

void OneSevenLiveMenuManager::handleLogout() {
    emit logoutClicked();
}

void OneSevenLiveMenuManager::checkUpdate() {
    emit checkUpdateClicked();
}

void OneSevenLiveMenuManager::updateDockVisibility(bool chatRoomVisible, bool broadcastVisible,
                                                   bool liveListVisible) {
    // Update visibility status variables
    isChatRoomVisible = chatRoomVisible;
    isBroadcastVisible = broadcastVisible;
    isLiveListVisible = liveListVisible;

    // Update menu item checked status
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

void OneSevenLiveMenuManager::updateMenuItemsEnabled() {
    // Update menu item enabled status based on login status
    chatRoomAction->setEnabled(isLoggedIn);
    broadcastAction->setEnabled(isLoggedIn);
    liveListAction->setEnabled(isLoggedIn);
}

void OneSevenLiveMenuManager::cleanup() {
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

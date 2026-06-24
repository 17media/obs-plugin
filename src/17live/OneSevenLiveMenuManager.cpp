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
      isLiveListVisible(false),
      isRockZoneVisible(false),
      isMultiRtmpVisible(false),
      isPreviewDockVisible(false),
      isCustomizedCartoonVisible(false) {
    // Create 17Live menu
    menu = mainWindow->menuBar()->addMenu(QString::fromUtf8(obs_module_text("17LIVE")));

    // Add submenu for dock menu
    dockSubMenu = menu->addMenu(QString::fromUtf8(obs_module_text("Menu.Dock")));

    // Add submenu items
    chatRoomAction = dockSubMenu->addAction(QString::fromUtf8(obs_module_text("Menu.ChatRoom")));
    connect(chatRoomAction, &QAction::triggered, this, [this]() { emit chatRoomClicked(); });

    broadcastAction = dockSubMenu->addAction(QString::fromUtf8(obs_module_text("Menu.Broadcast")));
    connect(broadcastAction, &QAction::triggered, this, [this]() { emit streamingClicked(); });

    rockZoneAction = dockSubMenu->addAction(QString::fromUtf8(obs_module_text("Menu.RockZone")));
    connect(rockZoneAction, &QAction::triggered, this, [this]() { emit rockZoneClicked(); });

    liveListAction = dockSubMenu->addAction(QString::fromUtf8(obs_module_text("Menu.LiveList")));
    connect(liveListAction, &QAction::triggered, this, [this]() { emit liveListClicked(); });

    multiRtmpAction =
        dockSubMenu->addAction(QString::fromUtf8(obs_module_text("MultiRTMP.Dock.Title")));
    connect(multiRtmpAction, &QAction::triggered, this, [this]() { emit multiRtmpClicked(); });

    previewDockAction =
        dockSubMenu->addAction(QString::fromUtf8(obs_module_text("Menu.PreviewDock")));
    connect(previewDockAction, &QAction::triggered, this, [this]() { emit previewDockClicked(); });

    customizedCartoonAction = dockSubMenu->addAction(obs_module_text("Menu.CustomizedCartoons"));
    connect(customizedCartoonAction, &QAction::triggered, this,
            [this]() { emit customizedCartoonClicked(); });

    menu->addSeparator();

    // Common menu
    helpAction = menu->addAction(QString::fromUtf8(obs_module_text("Menu.Help")));

    connect(helpAction, &QAction::triggered, this, [this]() {
        // open url obs_module_text("Menu.Help.Url");
        QUrl url = QUrl(obs_module_text("Menu.Help.Url"), QUrl::TolerantMode);
        QDesktopServices::openUrl(url);
    });

    // Create check update menu item
    checkUpdateAction = menu->addAction(QString::fromUtf8(obs_module_text("Menu.CheckUpdate")));
    connect(checkUpdateAction, &QAction::triggered, this, &OneSevenLiveMenuManager::checkUpdate);

    // Create diagnostics menu item
    diagnosticsAction = menu->addAction(QString::fromUtf8(obs_module_text("Menu.Diagnostics")));
    connect(diagnosticsAction, &QAction::triggered, this, [this]() { emit diagnosticsClicked(); });

    crashRecordsAction = menu->addAction(obs_module_text("Menu.CrashRecords"));
    connect(crashRecordsAction, &QAction::triggered, this, [this]() { emit crashRecordsClicked(); });

    settingsAction = menu->addAction(obs_module_text("Menu.Settings"));
    settingsAction->setMenuRole(QAction::NoRole);
    connect(settingsAction, &QAction::triggered, this, [this]() { emit settingsClicked(); });

    menu->addSeparator();

    // Create login menu item
    loginAction = menu->addAction(QString::fromUtf8(obs_module_text("Menu.SignIn")));
    connect(loginAction, &QAction::triggered, this, &OneSevenLiveMenuManager::handleLogin);

    // Initialize menu item enabled status
    updateMenuItemsEnabled();

    // Initialize menu item checked status
    chatRoomAction->setCheckable(true);
    broadcastAction->setCheckable(true);
    liveListAction->setCheckable(true);
    rockZoneAction->setCheckable(true);
    multiRtmpAction->setCheckable(true);
    previewDockAction->setCheckable(true);
    customizedCartoonAction->setCheckable(true);
    chatRoomAction->setChecked(false);
    broadcastAction->setChecked(false);
    liveListAction->setChecked(false);
    rockZoneAction->setChecked(false);
    multiRtmpAction->setChecked(false);
    previewDockAction->setChecked(false);
    customizedCartoonAction->setChecked(false);
}

OneSevenLiveMenuManager::~OneSevenLiveMenuManager() {}

void OneSevenLiveMenuManager::updateLoginStatus(bool logged, QString username) {
    isLoggedIn = logged;
    QString text = QString::fromUtf8(obs_module_text("Menu.SignIn"));
    if (isLoggedIn) {
        if (username.isEmpty()) {
            text = QString::fromUtf8(obs_module_text("Menu.SignOut"));
        } else {
            text = username + ": " + QString::fromUtf8(obs_module_text("Menu.SignOut"));
        }
    }
    loginAction->setText(text);

    disconnect(loginAction, &QAction::triggered, this, &OneSevenLiveMenuManager::handleLogin);
    disconnect(loginAction, &QAction::triggered, this, &OneSevenLiveMenuManager::handleLogout);
    if (isLoggedIn) {
        connect(loginAction, &QAction::triggered, this, &OneSevenLiveMenuManager::handleLogout,
                Qt::UniqueConnection);
    } else {
        connect(loginAction, &QAction::triggered, this, &OneSevenLiveMenuManager::handleLogin,
                Qt::UniqueConnection);
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
                                                   bool liveListVisible, bool rockZoneVisible,
                                                   bool multiRtmpVisible, bool previewDockVisible,
                                                   bool customizedCartoonVisible) {
    // Update visibility status variables
    isChatRoomVisible = chatRoomVisible;
    isBroadcastVisible = broadcastVisible;
    isLiveListVisible = liveListVisible;
    isRockZoneVisible = rockZoneVisible;
    isMultiRtmpVisible = multiRtmpVisible;
    isPreviewDockVisible = previewDockVisible;
    isCustomizedCartoonVisible = customizedCartoonVisible;

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

    if (rockZoneAction) {
        rockZoneAction->setCheckable(true);
        rockZoneAction->setChecked(isRockZoneVisible);
    }

    if (multiRtmpAction) {
        multiRtmpAction->setCheckable(true);
        multiRtmpAction->setChecked(isMultiRtmpVisible);
    }

    if (previewDockAction) {
        previewDockAction->setCheckable(true);
        previewDockAction->setChecked(isPreviewDockVisible);
    }

    if (customizedCartoonAction) {
        customizedCartoonAction->setCheckable(true);
        customizedCartoonAction->setChecked(isCustomizedCartoonVisible);
    }
}

void OneSevenLiveMenuManager::updateMenuItemsEnabled() {
    // Update menu item enabled status based on login status
    chatRoomAction->setEnabled(isLoggedIn);
    broadcastAction->setEnabled(isLoggedIn);
    liveListAction->setEnabled(isLoggedIn);
    rockZoneAction->setEnabled(isLoggedIn);
    multiRtmpAction->setEnabled(isLoggedIn);
    previewDockAction->setEnabled(isLoggedIn);
    customizedCartoonAction->setEnabled(isLoggedIn);
    if (dockSubMenu) {
        dockSubMenu->setEnabled(true);
        if (dockSubMenu->menuAction()) {
            dockSubMenu->menuAction()->setEnabled(true);
        }
    }
    // Diagnostics action is always enabled regardless of login status
    if (crashRecordsAction) {
        crashRecordsAction->setEnabled(true);
    }
    if (settingsAction) {
        settingsAction->setEnabled(true);
    }
}

void OneSevenLiveMenuManager::cleanup() {
    if (dockSubMenu) {
        dockSubMenu->deleteLater();
        dockSubMenu = nullptr;
    }

    if (menu) {
        menu->deleteLater();
        menu = nullptr;
    }

    chatRoomAction = nullptr;
    settingsAction = nullptr;
    broadcastAction = nullptr;
    liveListAction = nullptr;
    rockZoneAction = nullptr;
    multiRtmpAction = nullptr;
    previewDockAction = nullptr;
    customizedCartoonAction = nullptr;
    helpAction = nullptr;
    loginAction = nullptr;
    checkUpdateAction = nullptr;
    diagnosticsAction = nullptr;
    crashRecordsAction = nullptr;
}

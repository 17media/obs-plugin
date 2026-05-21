#include "DockOrchestrator.hpp"

#include <QLineEdit>
#include <QScrollArea>
#include <QTimer>

#include <obs-module.h>

#include "OneSevenLiveCoreContext.hpp"
#include "../OneSevenLiveConfigManager.hpp"
#include "../OneSevenLiveHttpServer.hpp"
#include "../OneSevenLiveMenuManager.hpp"
#include "../chat/OneSevenLiveChatWidget.hpp"
#include "../customized_cartoons/CustomizedCartoonDock.hpp"
#include "../multi-rtmp/ui/OneSevenLiveMultiRtmpDock.hpp"
#include "../preview/OneSevenLivePreviewDock.hpp"
#include "../rockzone/OneSevenLiveRockZoneDock.hpp"
#include "../streaming/OneSevenLiveStreamManager.hpp"
#include "../streaming/OneSevenLiveStreamingDock.hpp"
#include "../streamlist/OneSevenLiveStreamListDock.hpp"
#include "../utility/Common.hpp"
#include "../websocket/OneSevenLiveWebsocketServer.hpp"
#include "../websocket/WsMessage.hpp"
#include "plugin-support.h"

static const int INITIAL_DOCK_WIDTH = 450;
static const int INITIAL_DOCK_HEIGHT = 550;

DockOrchestrator::DockOrchestrator(OneSevenLiveCoreContext* core) : core_(core) {}

bool DockOrchestrator::isDockOpen(QDockWidget* dock) {
    return dock && dock->toggleViewAction() && dock->toggleViewAction()->isChecked();
}

void DockOrchestrator::centerDockOnMainWindow(QDockWidget* dock, QMainWindow* mainWindow) {
    if (!dock || !mainWindow) {
        return;
    }
    const QRect mainWindowGeometry = mainWindow->geometry();
    const int x = mainWindowGeometry.x() + (mainWindowGeometry.width() - dock->width()) / 2;
    const int y = mainWindowGeometry.y() + (mainWindowGeometry.height() - dock->height()) / 2;
    dock->move(x, y);
}

void DockOrchestrator::showDockAsFloating(QDockWidget* dock, QMainWindow* mainWindow,
                                          bool isStartupRestore) {
    if (!dock || isStartupRestore) {
        return;
    }
    dock->setFloating(true);
    dock->setVisible(true);
    centerDockOnMainWindow(dock, mainWindow);
}

void DockOrchestrator::syncMenuDockVisibility(OneSevenLiveMenuManager* menuManager,
                                               QDockWidget* chatDock, QDockWidget* streamingDock,
                                               QDockWidget* liveListDock, QDockWidget* rockZoneDock,
                                               QDockWidget* multiRtmpDock,
                                               QDockWidget* previewDock,
                                               QDockWidget* customizedCartoonDock) const {
    if (!menuManager) {
        return;
    }
    menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                      isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                      isDockOpen(multiRtmpDock), isDockOpen(previewDock),
                                      isDockOpen(customizedCartoonDock));
}

void DockOrchestrator::syncMenuDockVisibility() {
    if (!core_) {
        return;
    }
    syncMenuDockVisibility(core_->getMenuManager(), core_->getChatDock(), core_->getStreamingDock(),
                           core_->getLiveListDock(), core_->getRockZoneDock(),
                           core_->getMultiRtmpDock(), core_->getPreviewDock(),
                           core_->getCustomizedCartoonDock());
}

void DockOrchestrator::closeAllDocks() {
    if (!core_) {
        return;
    }
    auto* owner = core_->getUiOwner();
    auto* cfg = core_->getConfigManager();

    obs_log(LOG_INFO, "closeAllDocks");

    auto* streamingDock = core_->getStreamingDock();
    const bool streamingVisible = closeAndDeleteDock(streamingDock, owner);
    core_->setStreamingDock(streamingDock);
    if (cfg) {
        cfg->setDockVisibility("streaming", streamingVisible);
    }

    auto* liveListDock = core_->getLiveListDock();
    const bool liveListVisible = closeAndDeleteDock(liveListDock, owner);
    core_->setLiveListDock(liveListDock);
    if (cfg) {
        cfg->setDockVisibility("liveList", liveListVisible);
    }

    auto* rockZoneDock = core_->getRockZoneDock();
    const bool rockZoneVisible = closeAndDeleteDock(rockZoneDock, owner);
    core_->setRockZoneDock(rockZoneDock);
    if (cfg) {
        cfg->setDockVisibility("rockZone", rockZoneVisible);
    }

    bool chatRoomVisible = false;
    auto* chatDock = core_->getChatDock();
    if (chatDock) {
        chatRoomVisible = isDockOpen(chatDock);
        chatDock->disconnect(owner);
        if (auto* widget = qobject_cast<OneSevenLiveChatWidget*>(chatDock->widget())) {
            obs_log(LOG_INFO, "Shutting down chat widget in closeAllDocks");
            widget->shutdown();
            chatDock->setWidget(nullptr);
            delete widget;
        }
        chatDock->close();
        chatDock->deleteLater();
        chatDock = nullptr;
        core_->setChatDock(chatDock);
    }
    if (cfg) {
        cfg->setDockVisibility("chatRoom", chatRoomVisible);
    }

    auto* multiRtmpDock = core_->getMultiRtmpDock();
    const bool multiRtmpVisible = closeAndDeleteDock(multiRtmpDock, owner);
    core_->setMultiRtmpDock(multiRtmpDock);
    if (cfg) {
        cfg->setDockVisibility("multiRtmp", multiRtmpVisible);
    }

    auto* previewDock = core_->getPreviewDock();
    const bool previewDockVisible = closeAndDeleteDock(previewDock, owner);
    core_->setPreviewDock(previewDock);
    if (cfg) {
        cfg->setDockVisibility("previewDock", previewDockVisible);
    }

    auto* customizedCartoonDock = core_->getCustomizedCartoonDock();
    const bool customizedCartoonVisible = closeAndDeleteDock(customizedCartoonDock, owner);
    core_->setCustomizedCartoonDock(customizedCartoonDock);
    if (cfg) {
        cfg->setDockVisibility("customizedCartoon", customizedCartoonVisible);
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::handleStreamingClicked() {
    if (!core_) {
        return;
    }
    obs_log(LOG_INFO, "handleStreamingClicked");

    if (!core_->getStreamingDock()) {
        createStreamingDock();
    } else {
        core_->getStreamingDock()->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::createStreamingDock() {
    if (!core_ || core_->getStreamingDock()) {
        return;
    }
    if (!core_->getMainWindow() || !core_->getConfigManager()) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!core_->getConfigManager()->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    auto* dock = new OneSevenLiveStreamingDock(core_->getMainWindow(), core_->getStreamManager(),
                                               core_->getApiWrapper(), core_->getConfigManager());
    core_->setStreamingDock(dock);
    dock->setObjectName("OneSevenLiveStreamingDock");

    dock->setMaximumWidth(600);
    dock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    core_->getMainWindow()->addDockWidget(Qt::RightDockWidgetArea, dock);

    showDockAsFloating(dock, core_->getMainWindow(), core_->getStartupRestore());

    if (streamingDockFirstLoad_) {
        QObject::connect(dock, &OneSevenLiveStreamingDock::streamInfoSaved, core_->getUiOwner(),
                         [this]() {
                             if (core_ && core_->getLiveListDock()) {
                                 core_->getLiveListDock()->refreshStreamList();
                             }
                         });

        QObject::connect(dock, &QDockWidget::visibilityChanged, core_->getUiOwner(),
                         [this]() { this->syncMenuDockVisibility(); });

        streamingDockFirstLoad_ = false;
    }
}

void DockOrchestrator::handleRockZoneClicked() {
    if (!core_) {
        return;
    }
    obs_log(LOG_INFO, "handleRockZoneClicked");

    if (!core_->getRockZoneDock()) {
        createRockZoneDock();
    } else {
        core_->getRockZoneDock()->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::createRockZoneDock() {
    if (!core_ || core_->getRockZoneDock()) {
        return;
    }
    if (!core_->getMainWindow() || !core_->getConfigManager()) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!core_->getConfigManager()->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    auto* dock = new OneSevenLiveRockZoneDock(core_->getMainWindow(), core_->getApiWrapper(),
                                              core_->getConfigManager());
    core_->setRockZoneDock(dock);
    dock->setObjectName("OneSevenLiveRockZoneDock");

    dock->setMinimumWidth(300);
    dock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    core_->getMainWindow()->addDockWidget(Qt::RightDockWidgetArea, dock);

    if (core_->getStartupRestore()) {
        dock->setVisible(true);
    } else {
        showDockAsFloating(dock, core_->getMainWindow(), false);
    }

    if (core_->getStreamManager()) {
        QObject::connect(core_->getStreamManager(), &OneSevenLiveStreamManager::streamStatusChanged,
                         core_->getUiOwner(), [this](OneSevenLiveStreamingStatus status) {
                             if (status == OneSevenLiveStreamingStatus::NotStarted &&
                                 core_ && core_->getRockZoneDock()) {
                                 core_->getRockZoneDock()->clearUserList();
                             }
                         });
        QObject::connect(core_->getStreamManager(), &OneSevenLiveStreamManager::obsStreamStopped,
                         core_->getUiOwner(), [this](int, const QString&) {
                             if (core_ && core_->getRockZoneDock()) {
                                 core_->getRockZoneDock()->clearUserList();
                             }
                         });
    }

    if (rockZoneDockFirstLoad_) {
        QObject::connect(dock, &QDockWidget::visibilityChanged, core_->getUiOwner(),
                         [this]() { this->syncMenuDockVisibility(); });

        rockZoneDockFirstLoad_ = false;
    }
}

void DockOrchestrator::handleLiveListClicked() {
    if (!core_) {
        return;
    }
    if (!core_->getMainWindow() || !core_->getConfigManager()) {
        return;
    }
    obs_log(LOG_INFO, "handleLiveListClicked");

    if (!core_->getLiveListDock()) {
        auto* dock = new OneSevenLiveStreamListDock(core_->getMainWindow(), core_->getConfigManager(),
                                                    core_->getStreamingStatus());
        core_->setLiveListDock(dock);
        dock->setObjectName("OneSevenLiveStreamListDock");
        dock->setMinimumWidth(300);
        dock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        core_->getMainWindow()->addDockWidget(Qt::RightDockWidgetArea, dock);

        showDockAsFloating(dock, core_->getMainWindow(), core_->getStartupRestore());

        QObject::connect(dock, &OneSevenLiveStreamListDock::startLiveClicked, core_->getUiOwner(),
                         [this](const OneSevenLiveRtmpRequest& request) {
                             if (!core_) {
                                 return;
                             }
                             if (!core_->getStreamingDock()) {
                                 createStreamingDock();
                             }
                             auto* streamingDock = core_->getStreamingDock();
                             if (!streamingDock) {
                                 return;
                             }
                             streamingDock->setFloating(true);
                             streamingDock->setVisible(true);
                             streamingDock->raise();
                             streamingDock->activateWindow();
                             centerDockOnMainWindow(streamingDock, core_->getMainWindow());
                             streamingDock->createLiveWithRequest(request);
                         });

        QObject::connect(dock, &OneSevenLiveStreamListDock::editLiveClicked, core_->getUiOwner(),
                         [this](const OneSevenLiveStreamInfo& info) {
                             if (!core_) {
                                 return;
                             }
                             if (!core_->getStreamingDock()) {
                                 createStreamingDock();
                             }
                             auto* streamingDock = core_->getStreamingDock();
                             if (!streamingDock) {
                                 return;
                             }
                             streamingDock->editLiveWithInfo(info);
                             streamingDock->setFloating(true);
                             streamingDock->setVisible(true);
                             streamingDock->raise();
                             streamingDock->activateWindow();
                             centerDockOnMainWindow(streamingDock, core_->getMainWindow());

                             QTimer::singleShot(100, core_->getUiOwner(), [this]() {
                                 if (core_ && core_->getStreamingDock()) {
                                     QScrollArea* scrollArea =
                                         core_->getStreamingDock()->findChild<QScrollArea*>();
                                     QLineEdit* titleEdit =
                                         core_->getStreamingDock()->findChild<QLineEdit*>("titleEdit");
                                     if (scrollArea && titleEdit) {
                                         scrollArea->ensureWidgetVisible(titleEdit);
                                         titleEdit->setFocus();
                                         titleEdit->selectAll();
                                     }
                                 }
                             });
                         });

        QObject::connect(dock, &QDockWidget::visibilityChanged, core_->getUiOwner(),
                         [this]() { this->syncMenuDockVisibility(); });
    } else {
        core_->getLiveListDock()->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::saveDockState() {
    if (!core_ || !core_->getMainWindow() || !core_->getConfigManager()) {
        return;
    }

    QByteArray state = core_->getMainWindow()->saveState();
    core_->getConfigManager()->setDockState(state);

    obs_log(LOG_INFO, "Dock state saved successfully");
}

void DockOrchestrator::handleChatRoomClicked() {
    if (!core_) {
        return;
    }
    if (!core_->getMainWindow() || !core_->getConfigManager()) {
        return;
    }
    obs_log(LOG_INFO, "handleChatRoomClicked");

    OneSevenLiveLoginData loginData;
    if (!core_->getConfigManager()->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    auto* http = core_->getHttpServer();
    auto* ws = core_->getWebsocketServer();
    if (!http || !ws) {
        obs_log(LOG_ERROR, "[17Live Core] Local servers not available for chat dock");
        return;
    }

    std::string locale = GetCurrentLocale();
    QString wsUrl = QString::fromStdString("ws://127.0.0.1:%1").arg(ws->getPort());
    QString chatUrl =
        QString("http://localhost:%1/%2.html?roomID=%3&userID=%4&ws=%5")
            .arg(QString::number(http->getPort()), QString::fromStdString(locale),
                 QString::number(loginData.userInfo.roomID), loginData.userInfo.userID, wsUrl);

    obs_log(LOG_INFO, "Chat URL: %s", chatUrl.toStdString().c_str());

    auto* owner = core_->getUiOwner();
    auto* existingChatDock = core_->getChatDock();
    if (!existingChatDock) {
        obs_log(LOG_INFO, "Creating new chatDock instance");
        auto* dock = new QDockWidget(obs_module_text("ChatRoom.Title"), core_->getMainWindow());
        core_->setChatDock(dock);
        dock->setObjectName("OneSevenLiveChatDock");
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setAttribute(Qt::WA_DeleteOnClose, false);
        dock->installEventFilter(owner);
        dock->setMinimumWidth(300);

        OneSevenLiveChatWidget* chatWidget = new OneSevenLiveChatWidget(dock, chatUrl);
        dock->setWidget(chatWidget);

        core_->getMainWindow()->addDockWidget(Qt::RightDockWidgetArea, dock);

        if (core_->getStartupRestore()) {
            dock->setVisible(true);
        } else {
            obs_log(LOG_INFO, "Setting chatDock to floating mode");
            bool hadChatStored =
                core_->getConfigManager() ? core_->getConfigManager()->getDockVisibility("chatRoom") : false;
            if (!hadChatStored) {
                dock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);
            }
            showDockAsFloating(dock, core_->getMainWindow(), false);
        }

        QObject::connect(dock, &QDockWidget::visibilityChanged, owner, [this](bool visible) {
                             this->syncMenuDockVisibility();
                             if (visible && core_) {
                                 core_->requestFlushChatEventQueue();
                             }
                         });
    } else {
        auto* dock = existingChatDock;
        if (!dock->widget()) {
            OneSevenLiveChatWidget* chatWidget = new OneSevenLiveChatWidget(dock, chatUrl);
            dock->setWidget(chatWidget);
        } else if (auto* chatWidget =
                       qobject_cast<OneSevenLiveChatWidget*>(dock->widget())) {
            chatWidget->setUrl(chatUrl);
        }

        obs_log(LOG_INFO, "Toggling existing chatDock visibility. Current: %s",
                dock->isVisible() ? "visible" : "hidden");
        dock->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::handleMultiRtmpClicked() {
    if (!core_) {
        return;
    }
    obs_log(LOG_INFO, "handleMultiRtmpClicked");

    if (!core_->getMultiRtmpDock()) {
        createMultiRtmpDock();
    } else {
        core_->getMultiRtmpDock()->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::createMultiRtmpDock() {
    if (!core_ || core_->getMultiRtmpDock()) {
        return;
    }
    if (!core_->getMainWindow() || !core_->getConfigManager()) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!core_->getConfigManager()->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    auto* dock = new OneSevenLiveMultiRtmpDock(core_->getMainWindow());
    core_->setMultiRtmpDock(dock);
    dock->setObjectName("OneSevenLiveMultiRtmpDock");

    dock->setMaximumWidth(600);
    dock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    core_->getMainWindow()->addDockWidget(Qt::RightDockWidgetArea, dock);

    showDockAsFloating(dock, core_->getMainWindow(), core_->getStartupRestore());

    if (multiRtmpDockFirstLoad_) {
        QObject::connect(dock, &QDockWidget::visibilityChanged, core_->getUiOwner(),
                         [this]() { this->syncMenuDockVisibility(); });

        multiRtmpDockFirstLoad_ = false;
    }
}

void DockOrchestrator::handlePreviewDockClicked() {
    if (!core_) {
        return;
    }
    obs_log(LOG_INFO, "handlePreviewDockClicked");

    if (!core_->getPreviewDock()) {
        createPreviewDock();
    } else {
        core_->getPreviewDock()->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::createPreviewDock() {
    if (!core_ || core_->getPreviewDock()) {
        return;
    }
    if (!core_->getMainWindow()) {
        return;
    }

    auto* http = core_->getHttpServer();
    auto* ws = core_->getWebsocketServer();
    if (!http || !ws) {
        obs_log(LOG_ERROR, "[17Live Core] Local servers not available for preview dock");
        return;
    }

    QString wsUrl = QString::fromStdString("ws://127.0.0.1:%1").arg(ws->getPort());

    QString cartoonUrl = QString("http://localhost:%1/vff/?ws=%2")
                             .arg(QString::number(http->getPort()), wsUrl);
    obs_log(LOG_INFO, "cartoonUrl: %s", cartoonUrl.toStdString().c_str());

    std::string locale = GetCurrentLocale();
    QString enterAnimUrl = QString("http://localhost:%1/%2/enter_animation.html?ws=%3")
                             .arg(QString::number(http->getPort()), QString::fromStdString(locale), wsUrl);
    obs_log(LOG_INFO, "enterAnimUrl: %s", enterAnimUrl.toStdString().c_str());

    auto* dock = new OneSevenLivePreviewDock(core_->getMainWindow(), cartoonUrl, enterAnimUrl);
    core_->setPreviewDock(dock);
    dock->setObjectName("OneSevenLivePreviewDock");

    dock->setMaximumWidth(800);
    dock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    core_->getMainWindow()->addDockWidget(Qt::RightDockWidgetArea, dock);

    showDockAsFloating(dock, core_->getMainWindow(), core_->getStartupRestore());

    if (previewDockFirstLoad_) {
        QObject::connect(dock, &QDockWidget::visibilityChanged, core_->getUiOwner(),
                         [this]() { this->syncMenuDockVisibility(); });

        previewDockFirstLoad_ = false;
    }
}

void DockOrchestrator::handleCustomizedCartoonClicked() {
    if (!core_) {
        return;
    }
    obs_log(LOG_INFO, "handleCustomizedCartoonClicked");

    if (!core_->getCustomizedCartoonDock()) {
        createCustomizedCartoonDock();
    } else {
        core_->getCustomizedCartoonDock()->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::createCustomizedCartoonDock() {
    if (!core_ || core_->getCustomizedCartoonDock()) {
        return;
    }
    if (!core_->getMainWindow() || !core_->getConfigManager()) {
        return;
    }
    if (!core_->getCustomizedCartoonService()) {
        obs_log(LOG_WARNING, "CustomizedCartoonService not available");
        return;
    }

    auto* dock = new CustomizedCartoonDock(core_->getMainWindow(), core_->getCustomizedCartoonService());
    core_->setCustomizedCartoonDock(dock);
    dock->setObjectName("CustomizedCartoonDock");

    dock->setMaximumWidth(1200);
    dock->setMinimumSize(465, 500);
    dock->resize(500, 820);

    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    core_->getMainWindow()->addDockWidget(Qt::RightDockWidgetArea, dock);

    showDockAsFloating(dock, core_->getMainWindow(), core_->getStartupRestore());

    if (customizedCartoonDockFirstLoad_) {
        QObject::connect(dock, &QDockWidget::visibilityChanged, core_->getUiOwner(),
                         [this]() { this->syncMenuDockVisibility(); });

        customizedCartoonDockFirstLoad_ = false;
    }
}

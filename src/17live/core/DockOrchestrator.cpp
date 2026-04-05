#include "DockOrchestrator.hpp"

#include <QLineEdit>
#include <QScrollArea>
#include <QTimer>

#include <obs-module.h>

#include "../OneSevenLiveConfigManager.hpp"
#include "../OneSevenLiveCoreManager.hpp"
#include "../OneSevenLiveHttpServer.hpp"
#include "../OneSevenLiveMenuManager.hpp"
#include "../chat/OneSevenLiveChatWidget.hpp"
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

DockOrchestrator::DockOrchestrator(OneSevenLiveCoreManager* coreManager)
    : coreManager_(coreManager) {}

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
                                               QDockWidget* previewDock) const {
    if (!menuManager) {
        return;
    }
    menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                      isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                      isDockOpen(multiRtmpDock), isDockOpen(previewDock));
}

void DockOrchestrator::syncMenuDockVisibility() {
    if (!coreManager_) {
        return;
    }
    syncMenuDockVisibility(coreManager_->menuManager.get(), coreManager_->chatDock,
                           coreManager_->streamingDock, coreManager_->liveListDock,
                           coreManager_->rockZoneDock, coreManager_->multiRtmpDock,
                           coreManager_->previewDock);
}

void DockOrchestrator::closeAllDocks() {
    if (!coreManager_) {
        return;
    }
    auto* cfg = coreManager_->configManager.get();

    obs_log(LOG_INFO, "closeAllDocks");

    const bool streamingVisible = closeAndDeleteDock(coreManager_->streamingDock, coreManager_);
    if (cfg) {
        cfg->setDockVisibility("streaming", streamingVisible);
    }

    const bool liveListVisible = closeAndDeleteDock(coreManager_->liveListDock, coreManager_);
    if (cfg) {
        cfg->setDockVisibility("liveList", liveListVisible);
    }

    const bool rockZoneVisible = closeAndDeleteDock(coreManager_->rockZoneDock, coreManager_);
    if (cfg) {
        cfg->setDockVisibility("rockZone", rockZoneVisible);
    }

    bool chatRoomVisible = false;
    if (coreManager_->chatDock) {
        chatRoomVisible = isDockOpen(coreManager_->chatDock);
        coreManager_->chatDock->disconnect(coreManager_);
        if (auto* widget = qobject_cast<OneSevenLiveChatWidget*>(coreManager_->chatDock->widget())) {
            obs_log(LOG_INFO, "Shutting down chat widget in closeAllDocks");
            widget->shutdown();
            coreManager_->chatDock->setWidget(nullptr);
            delete widget;
        }
        coreManager_->chatDock->close();
        coreManager_->chatDock->deleteLater();
        coreManager_->chatDock = nullptr;
    }
    if (cfg) {
        cfg->setDockVisibility("chatRoom", chatRoomVisible);
    }

    const bool multiRtmpVisible = closeAndDeleteDock(coreManager_->multiRtmpDock, coreManager_);
    if (cfg) {
        cfg->setDockVisibility("multiRtmp", multiRtmpVisible);
    }

    const bool previewDockVisible = closeAndDeleteDock(coreManager_->previewDock, coreManager_);
    if (cfg) {
        cfg->setDockVisibility("previewDock", previewDockVisible);
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::handleStreamingClicked() {
    if (!coreManager_) {
        return;
    }
    obs_log(LOG_INFO, "handleStreamingClicked");

    if (!coreManager_->streamingDock) {
        createStreamingDock();
    } else {
        coreManager_->streamingDock->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::createStreamingDock() {
    if (!coreManager_ || coreManager_->streamingDock) {
        return;
    }
    if (!coreManager_->mainWindow || !coreManager_->configManager) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!coreManager_->configManager->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    coreManager_->streamingDock =
        new OneSevenLiveStreamingDock(coreManager_->mainWindow, coreManager_->streamManager.get(),
                                      coreManager_->apiWrapper.get(), coreManager_->configManager.get());
    coreManager_->streamingDock->setObjectName("OneSevenLiveStreamingDock");

    coreManager_->streamingDock->setMaximumWidth(600);
    coreManager_->streamingDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    coreManager_->streamingDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    coreManager_->mainWindow->addDockWidget(Qt::RightDockWidgetArea, coreManager_->streamingDock);

    showDockAsFloating(coreManager_->streamingDock, coreManager_->mainWindow, coreManager_->isStartupRestore);

    if (coreManager_->streamingDockFirstLoad) {
        QObject::connect(coreManager_->streamingDock, &OneSevenLiveStreamingDock::streamInfoSaved,
                         coreManager_, [this]() {
                             if (coreManager_ && coreManager_->liveListDock) {
                                 coreManager_->liveListDock->refreshStreamList();
                             }
                         });

        QObject::connect(coreManager_->streamingDock, &QDockWidget::visibilityChanged, coreManager_,
                         [this]() { syncMenuDockVisibility(); });

        coreManager_->streamingDockFirstLoad = false;
    }
}

void DockOrchestrator::handleRockZoneClicked() {
    if (!coreManager_) {
        return;
    }
    obs_log(LOG_INFO, "handleRockZoneClicked");

    if (!coreManager_->rockZoneDock) {
        createRockZoneDock();
    } else {
        coreManager_->rockZoneDock->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::createRockZoneDock() {
    if (!coreManager_ || coreManager_->rockZoneDock) {
        return;
    }
    if (!coreManager_->mainWindow || !coreManager_->configManager) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!coreManager_->configManager->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    coreManager_->rockZoneDock =
        new OneSevenLiveRockZoneDock(coreManager_->mainWindow, coreManager_->apiWrapper.get(),
                                     coreManager_->configManager.get());
    coreManager_->rockZoneDock->setObjectName("OneSevenLiveRockZoneDock");

    coreManager_->rockZoneDock->setMinimumWidth(300);
    coreManager_->rockZoneDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    coreManager_->rockZoneDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    coreManager_->mainWindow->addDockWidget(Qt::RightDockWidgetArea, coreManager_->rockZoneDock);

    if (coreManager_->isStartupRestore) {
        coreManager_->rockZoneDock->setVisible(true);
    } else {
        showDockAsFloating(coreManager_->rockZoneDock, coreManager_->mainWindow, false);
    }

    if (coreManager_->streamManager) {
        QObject::connect(coreManager_->streamManager.get(), &OneSevenLiveStreamManager::streamStatusChanged,
                         coreManager_, [this](OneSevenLiveStreamingStatus status) {
                             if (status == OneSevenLiveStreamingStatus::NotStarted &&
                                 coreManager_ && coreManager_->rockZoneDock) {
                                 coreManager_->rockZoneDock->clearUserList();
                             }
                         });
        QObject::connect(coreManager_->streamManager.get(), &OneSevenLiveStreamManager::obsStreamStopped,
                         coreManager_, [this](int, const QString&) {
                             if (coreManager_ && coreManager_->rockZoneDock) {
                                 coreManager_->rockZoneDock->clearUserList();
                             }
                         });
    }

    if (coreManager_->rockZoneDockFirstLoad) {
        QObject::connect(coreManager_->rockZoneDock, &QDockWidget::visibilityChanged, coreManager_,
                         [this]() { syncMenuDockVisibility(); });

        coreManager_->rockZoneDockFirstLoad = false;
    }
}

void DockOrchestrator::handleLiveListClicked() {
    if (!coreManager_) {
        return;
    }
    if (!coreManager_->mainWindow || !coreManager_->configManager) {
        return;
    }
    obs_log(LOG_INFO, "handleLiveListClicked");

    if (!coreManager_->liveListDock) {
        coreManager_->liveListDock =
            new OneSevenLiveStreamListDock(coreManager_->mainWindow, coreManager_->configManager.get(),
                                           coreManager_->status);
        coreManager_->liveListDock->setObjectName("OneSevenLiveStreamListDock");
        coreManager_->liveListDock->setMinimumWidth(300);
        coreManager_->liveListDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

        coreManager_->liveListDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        coreManager_->mainWindow->addDockWidget(Qt::RightDockWidgetArea, coreManager_->liveListDock);

        showDockAsFloating(coreManager_->liveListDock, coreManager_->mainWindow, coreManager_->isStartupRestore);

        QObject::connect(coreManager_->liveListDock, &OneSevenLiveStreamListDock::startLiveClicked,
                         coreManager_, [this](const OneSevenLiveRtmpRequest& request) {
                             if (!coreManager_) {
                                 return;
                             }
                             if (!coreManager_->streamingDock) {
                                 createStreamingDock();
                             }
                             coreManager_->streamingDock->setFloating(true);
                             coreManager_->streamingDock->setVisible(true);
                             coreManager_->streamingDock->raise();
                             coreManager_->streamingDock->activateWindow();
                             centerDockOnMainWindow(coreManager_->streamingDock, coreManager_->mainWindow);
                             coreManager_->streamingDock->createLiveWithRequest(request);
                         });

        QObject::connect(coreManager_->liveListDock, &OneSevenLiveStreamListDock::editLiveClicked,
                         coreManager_, [this](const OneSevenLiveStreamInfo& info) {
                             if (!coreManager_) {
                                 return;
                             }
                             if (!coreManager_->streamingDock) {
                                 createStreamingDock();
                             }
                             coreManager_->streamingDock->editLiveWithInfo(info);
                             coreManager_->streamingDock->setFloating(true);
                             coreManager_->streamingDock->setVisible(true);
                             coreManager_->streamingDock->raise();
                             coreManager_->streamingDock->activateWindow();
                             centerDockOnMainWindow(coreManager_->streamingDock, coreManager_->mainWindow);

                             QTimer::singleShot(100, coreManager_, [this]() {
                                 if (coreManager_ && coreManager_->streamingDock) {
                                     QScrollArea* scrollArea =
                                         coreManager_->streamingDock->findChild<QScrollArea*>();
                                     QLineEdit* titleEdit =
                                         coreManager_->streamingDock->findChild<QLineEdit*>("titleEdit");
                                     if (scrollArea && titleEdit) {
                                         scrollArea->ensureWidgetVisible(titleEdit);
                                         titleEdit->setFocus();
                                         titleEdit->selectAll();
                                     }
                                 }
                             });
                         });

        QObject::connect(coreManager_->liveListDock, &QDockWidget::visibilityChanged, coreManager_,
                         [this]() { syncMenuDockVisibility(); });
    } else {
        coreManager_->liveListDock->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::saveDockState() {
    if (!coreManager_ || !coreManager_->initialized || !coreManager_->mainWindow ||
        !coreManager_->configManager) {
        return;
    }

    QByteArray state = coreManager_->mainWindow->saveState();
    coreManager_->configManager->setDockState(state);

    obs_log(LOG_INFO, "Dock state saved successfully");
}

void DockOrchestrator::handleChatRoomClicked() {
    if (!coreManager_) {
        return;
    }
    if (!coreManager_->mainWindow || !coreManager_->configManager) {
        return;
    }
    obs_log(LOG_INFO, "handleChatRoomClicked");

    OneSevenLiveLoginData loginData;
    if (!coreManager_->configManager->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    auto* http = coreManager_->getHttpServer();
    auto* ws = coreManager_->getWebsocketServer();
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

    if (!coreManager_->chatDock) {
        obs_log(LOG_INFO, "Creating new chatDock instance");
        coreManager_->chatDock = new QDockWidget(obs_module_text("ChatRoom.Title"), coreManager_->mainWindow);
        coreManager_->chatDock->setObjectName("OneSevenLiveChatDock");
        coreManager_->chatDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        coreManager_->chatDock->setAttribute(Qt::WA_DeleteOnClose, false);
        coreManager_->chatDock->installEventFilter(coreManager_);
        coreManager_->chatDock->setMinimumWidth(300);

        OneSevenLiveChatWidget* chatWidget = new OneSevenLiveChatWidget(coreManager_->chatDock, chatUrl);
        coreManager_->chatDock->setWidget(chatWidget);

        coreManager_->mainWindow->addDockWidget(Qt::RightDockWidgetArea, coreManager_->chatDock);

        if (coreManager_->isStartupRestore) {
            coreManager_->chatDock->setVisible(true);
        } else {
            obs_log(LOG_INFO, "Setting chatDock to floating mode");
            bool hadChatStored =
                coreManager_->configManager ? coreManager_->configManager->getDockVisibility("chatRoom") : false;
            if (!hadChatStored) {
                coreManager_->chatDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);
            }
            showDockAsFloating(coreManager_->chatDock, coreManager_->mainWindow, false);
        }

        QObject::connect(coreManager_->chatDock, &QDockWidget::visibilityChanged, coreManager_,
                         [this](bool visible) {
                             syncMenuDockVisibility();
                             if (visible && coreManager_) {
                                 coreManager_->flushChatEventQueue();
                             }
                         });
    } else {
        if (!coreManager_->chatDock->widget()) {
            OneSevenLiveChatWidget* chatWidget = new OneSevenLiveChatWidget(coreManager_->chatDock, chatUrl);
            coreManager_->chatDock->setWidget(chatWidget);
        } else if (auto* chatWidget =
                       qobject_cast<OneSevenLiveChatWidget*>(coreManager_->chatDock->widget())) {
            chatWidget->setUrl(chatUrl);
        }

        obs_log(LOG_INFO, "Toggling existing chatDock visibility. Current: %s",
                coreManager_->chatDock->isVisible() ? "visible" : "hidden");
        coreManager_->chatDock->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::handleMultiRtmpClicked() {
    if (!coreManager_) {
        return;
    }
    obs_log(LOG_INFO, "handleMultiRtmpClicked");

    if (!coreManager_->multiRtmpDock) {
        createMultiRtmpDock();
    } else {
        coreManager_->multiRtmpDock->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::createMultiRtmpDock() {
    if (!coreManager_ || coreManager_->multiRtmpDock) {
        return;
    }
    if (!coreManager_->mainWindow || !coreManager_->configManager) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!coreManager_->configManager->getLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to get login data");
        return;
    }

    coreManager_->multiRtmpDock = new OneSevenLiveMultiRtmpDock(coreManager_->mainWindow);
    coreManager_->multiRtmpDock->setObjectName("OneSevenLiveMultiRtmpDock");

    coreManager_->multiRtmpDock->setMaximumWidth(600);
    coreManager_->multiRtmpDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    coreManager_->multiRtmpDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    coreManager_->mainWindow->addDockWidget(Qt::RightDockWidgetArea, coreManager_->multiRtmpDock);

    showDockAsFloating(coreManager_->multiRtmpDock, coreManager_->mainWindow, coreManager_->isStartupRestore);

    if (coreManager_->multiRtmpDockFirstLoad) {
        QObject::connect(coreManager_->multiRtmpDock, &QDockWidget::visibilityChanged, coreManager_,
                         [this]() { syncMenuDockVisibility(); });

        coreManager_->multiRtmpDockFirstLoad = false;
    }
}

void DockOrchestrator::handlePreviewDockClicked() {
    if (!coreManager_) {
        return;
    }
    obs_log(LOG_INFO, "handlePreviewDockClicked");

    if (!coreManager_->previewDock) {
        createPreviewDock();
    } else {
        coreManager_->previewDock->toggleViewAction()->trigger();
    }

    syncMenuDockVisibility();
}

void DockOrchestrator::createPreviewDock() {
    if (!coreManager_ || coreManager_->previewDock) {
        return;
    }
    if (!coreManager_->mainWindow) {
        return;
    }

    auto* http = coreManager_->getHttpServer();
    auto* ws = coreManager_->getWebsocketServer();
    if (!http || !ws) {
        obs_log(LOG_ERROR, "[17Live Core] Local servers not available for preview dock");
        return;
    }

    QString wsUrl = QString::fromStdString("ws://127.0.0.1:%1").arg(ws->getPort());

    QString cartoonUrl = QString("http://localhost:%1/vff/?ws=%2")
                             .arg(QString::number(http->getPort()), wsUrl);
    obs_log(LOG_INFO, "cartoonUrl: %s", cartoonUrl.toStdString().c_str());

    coreManager_->previewDock = new OneSevenLivePreviewDock(coreManager_->mainWindow, cartoonUrl);
    coreManager_->previewDock->setObjectName("OneSevenLivePreviewDock");

    coreManager_->previewDock->setMaximumWidth(800);
    coreManager_->previewDock->resize(INITIAL_DOCK_WIDTH, INITIAL_DOCK_HEIGHT);

    coreManager_->previewDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    coreManager_->mainWindow->addDockWidget(Qt::RightDockWidgetArea, coreManager_->previewDock);

    showDockAsFloating(coreManager_->previewDock, coreManager_->mainWindow, coreManager_->isStartupRestore);

    if (coreManager_->previewDockFirstLoad) {
        QObject::connect(coreManager_->previewDock, &QDockWidget::visibilityChanged, coreManager_,
                         [this]() { syncMenuDockVisibility(); });

        coreManager_->previewDockFirstLoad = false;
    }
}

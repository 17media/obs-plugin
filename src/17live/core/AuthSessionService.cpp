#include "AuthSessionService.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

#include <obs-module.h>

#include "../OneSevenLiveCoreManager.hpp"
#include "../OneSevenLiveConfigManager.hpp"
#include "../OneSevenLiveLoginDialog.hpp"
#include "../OneSevenLiveMenuManager.hpp"
#include "../api/OneSevenLiveApiWrappers.hpp"
#include "../multi-rtmp/ui/OneSevenLiveMultiRtmpDock.hpp"
#include "../preview/OneSevenLivePreviewDock.hpp"
#include "../rockzone/OneSevenLiveRockZoneDock.hpp"
#include "../streaming/OneSevenLiveStreamManager.hpp"
#include "../streaming/OneSevenLiveStreamingDock.hpp"
#include "../streamlist/OneSevenLiveStreamListDock.hpp"
#include "../websocket/OneSevenLiveWebsocketServer.hpp"
#include "../twitch/OneSevenLiveTwitchAuth.hpp"
#include "plugin-support.h"
#include "ChatBridgeService.hpp"

AuthSessionService::AuthSessionService(OneSevenLiveCoreManager* coreManager, QObject* parent)
    : QObject(parent), coreManager_(coreManager) {}

SessionState AuthSessionService::getState() const {
    return state_.load();
}

bool AuthSessionService::isPendingLogout() const {
    return pendingLogout_.load();
}

void AuthSessionService::setPendingLogout(bool pending) {
    pendingLogout_.store(pending);
}

bool AuthSessionService::handleLoginClicked() {
    coreManager_->m_cancelFlag.store(false);
    OneSevenLiveLoginDialog dialog(coreManager_->mainWindow, coreManager_->getApiWrapper());

    // Connect login success signal to slot function
    QObject::connect(&dialog, &OneSevenLiveLoginDialog::loginSuccess, this,
                     &AuthSessionService::handleLoginSuccess);

    return dialog.exec() == QDialog::Accepted;
}

void AuthSessionService::handleLoginSuccess(const OneSevenLiveLoginData& loginData) {
    obs_log(LOG_INFO, "handleLoginSuccess");

    if (!coreManager_->configManager->setLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to save login data");
        return;
    }

    QPointer<AuthSessionService> self = this;
    QMetaObject::invokeMethod(
        this,
        [self, loginData]() {
            if (self)
                self->handleLoginStateChanged(true, loginData);
        },
        Qt::QueuedConnection);
}

void AuthSessionService::handleLoginStateChanged(bool isLoggedIn,
                                                 const OneSevenLiveLoginData& loginData) {
    obs_log(LOG_INFO, "handleLoginStateChanged: %s", isLoggedIn ? "logged in" : "logged out");

    if (isLoggedIn) {
        state_.store(SessionState::LoggingIn);
        performLoginOperations(loginData);
        state_.store(SessionState::LoggedIn);
    } else {
        state_.store(SessionState::LoggingOut);
        performLogoutOperations();
        state_.store(SessionState::Idle);
    }
}

void AuthSessionService::performLoginOperations(const OneSevenLiveLoginData& loginData) {
    obs_log(LOG_INFO, "performLoginOperations");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    // if apiWrappers token is empty or not equal to loginData.accessToken.toStdString(), update it
    if (coreManager_->apiWrapper->getToken().empty() ||
        coreManager_->apiWrapper->getToken() != loginData.jwtAccessToken.toStdString()) {
        coreManager_->apiWrapper->setToken(loginData.jwtAccessToken.toStdString());
    }

    // Initialize stream manager (after apiWrapper is ready)
    coreManager_->streamManager =
        std::make_unique<OneSevenLiveStreamManager>(coreManager_->apiWrapper.get(), coreManager_->configManager.get(), coreManager_);
    if (!coreManager_->streamManager) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to create stream manager instance");
        return;
    }

    QPointer<OneSevenLiveCoreManager> core = coreManager_;
    QTimer::singleShot(0, coreManager_, [core]() {
        if (core)
            core->loadGifts();
    });

    // Update menu with user info
    QString username = loginData.userInfo.displayName;
    if (username.isEmpty()) {
        username = loginData.userInfo.openID;
    }
    coreManager_->menuManager->updateLoginStatus(true, username);

    QTimer::singleShot(0, coreManager_, [core, loginData]() {
        if (core)
            core->load17LiveConfig(loginData);
    });

    // Restore dock states if this is during startup and there are saved states
    if (coreManager_->isStartupRestore) {
        QPointer<AuthSessionService> self = this;
        QTimer::singleShot(0, this, [self, core]() {
            if (self && core) {
                self->restoreDockStatesOnLogin();
                core->isStartupRestore = false;
            }
        });
    }

    QTimer::singleShot(0, coreManager_, [core]() {
        if (!core)
            return;
        core->createYouTubeChatClient();
        core->createTwitchChatClient();
        core->setConnection();
    });

    QTimer::singleShot(0, coreManager_, [core]() {
        if (!core)
            return;
        if (core->streamManager && core->apiWrapper) {
            const qint64 rid = core->streamManager->getRoomID();
            if (rid > 0) {
                core->m_cancelFlag.store(false);
                core->connectAblyChat(QString::number(rid), QString());
            }
        }
    });

    // discovery is managed by YouTubeChatClient
}

void AuthSessionService::performLogoutOperations() {
    obs_log(LOG_INFO, "performLogoutOperations");
    coreManager_->m_cancelFlag.store(true);

    {
        auto* ws = coreManager_->getWebsocketServer();
        if (ws && ws->is_running()) {
            ws->closeAllClients();
        }
    }

    coreManager_->destroyTwitchChatClient();
    coreManager_->destroyAblyChatClient();

    if (coreManager_->streamCheckTimer) {
        coreManager_->streamCheckTimer->stop();
        coreManager_->streamCheckTimer->deleteLater();
        coreManager_->streamCheckTimer = nullptr;
        coreManager_->streamCheckInFlight.store(false);
    }

    coreManager_->closeAllDocks();

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    coreManager_->streamManager.reset();

    // Reset login status in menu
    coreManager_->menuManager->updateLoginStatus(false, "");

    // Clear login data
    coreManager_->configManager->clearLoginData();

    // Clear third-party platform authorization data
    coreManager_->configManager->clearTwitchTokens();
    coreManager_->configManager->clearTwitchUserInfo();

    // Clear streaming configuration
    coreManager_->configManager->clearStreamingInfo();
    coreManager_->configManager->clearWhipStreamingInfo();
    coreManager_->configManager->clearStreamingPullUrl();

    // Clear in-memory auth states
    if (coreManager_->twitchAuth) {
        coreManager_->twitchAuth->clearTokens();
    }
    if (coreManager_->apiWrapper) {
        coreManager_->apiWrapper->shutdown();
    }

    if (coreManager_->ytChatDiscoverTimer) {
        coreManager_->ytChatDiscoverTimer->stop();
        coreManager_->ytChatDiscoverTimer->deleteLater();
        coreManager_->ytChatDiscoverTimer = nullptr;
    }

    if (coreManager_->chatBridgeService_) {
        coreManager_->chatBridgeService_->clear();
    }

    if (coreManager_->apiWrapper) {
        coreManager_->apiWrapper->setToken(std::string());
    }
}

void AuthSessionService::restoreDockStatesOnLogin() {
    obs_log(LOG_INFO, "restoreDockStatesOnLogin");

    // Check if there are saved dock states and restore them
    QByteArray dockState = coreManager_->configManager->getDockState();
    if (!dockState.isEmpty() && coreManager_->mainWindow && coreManager_->mainWindow->isVisible()) {
        // Restore streaming dock if it was previously shown
        if (coreManager_->configManager->getDockVisibility("streaming")) {
            coreManager_->createStreamingDock();
        }

        // Restore live list dock if it was previously shown
        if (coreManager_->configManager->getDockVisibility("liveList")) {
            coreManager_->handleLiveListClicked();
        }

        // Restore chat room dock if it was previously shown
        if (coreManager_->configManager->getDockVisibility("chatRoom")) {
            coreManager_->handleChatRoomClicked();
        }

        // Restore rock zone dock if it was previously shown
        if (coreManager_->configManager->getDockVisibility("rockZone")) {
            coreManager_->handleRockZoneClicked();
        }

        // Restore multi-RTMP dock if it was previously shown
        if (coreManager_->configManager->getDockVisibility("multiRtmp")) {
            coreManager_->handleMultiRtmpClicked();
        }

        // Restore preview dock if it was previously shown
        if (coreManager_->configManager->getDockVisibility("previewDock")) {
            coreManager_->handlePreviewDockClicked();
        }

        // Apply the saved dock layout
        coreManager_->mainWindow->restoreState(dockState);

        QPointer<OneSevenLiveCoreManager> core = coreManager_;
        QTimer::singleShot(0, coreManager_, [core]() {
            if (!core || !core->mainWindow)
                return;

            QList<QDockWidget*> docks;
            QList<int> sizes;
            int initialHeight = 550; // INITIAL_DOCK_HEIGHT
            if (core->streamingDock) {
                docks << core->streamingDock.data();
                sizes << initialHeight;
            }
            if (core->liveListDock) {
                docks << core->liveListDock.data();
                sizes << initialHeight;
            }
            if (core->rockZoneDock) {
                docks << core->rockZoneDock.data();
                sizes << initialHeight;
            }
            if (core->multiRtmpDock) {
                docks << core->multiRtmpDock.data();
                sizes << initialHeight;
            }
            if (core->previewDock) {
                docks << core->previewDock.data();
                sizes << initialHeight;
            }
            if (core->chatDock) {
                docks << core->chatDock.data();
                sizes << initialHeight;
            }
            if (!docks.isEmpty())
                core->mainWindow->resizeDocks(docks, sizes, Qt::Vertical);
        });

        // Update menu visibility status after restoration
        coreManager_->syncMenuDockVisibility();
    }
}

void AuthSessionService::handleLogoutClicked() {
    obs_log(LOG_INFO, "handleLogoutClicked");

    if (coreManager_->status == OneSevenLiveStreamingStatus::Streaming) {
        auto* msgBox = new QMessageBox(coreManager_->mainWindow);
        msgBox->setWindowTitle(obs_module_text("Logout.Warning.Title"));
        msgBox->setText(obs_module_text("Logout.Warning.Message"));
        QPushButton* confirmButton =
            msgBox->addButton(obs_module_text("Logout.Warning.Button.Yes"), QMessageBox::YesRole);
        QPushButton* cancelButton =
            msgBox->addButton(obs_module_text("Logout.Warning.Button.No"), QMessageBox::NoRole);
        msgBox->setDefaultButton(cancelButton);

        QPointer<AuthSessionService> self = this;
        connect(msgBox, &QMessageBox::finished, this, [self, msgBox, confirmButton](int) {
            if (msgBox->clickedButton() != confirmButton) {
                msgBox->deleteLater();
                return;
            }
            if (self) {
                QMetaObject::invokeMethod(
                    self,
                    [self]() {
                        if (self) {
                            self->setPendingLogout(true);
                            self->coreManager_->closeLive(false);
                        }
                    },
                    Qt::QueuedConnection);
            }
            msgBox->deleteLater();
        });
        msgBox->open();
        return;
    }

    QPointer<AuthSessionService> self = this;
    QMetaObject::invokeMethod(
        this,
        [self]() {
            if (self)
                self->handleLoginStateChanged(false);
        },
        Qt::QueuedConnection);
}

bool AuthSessionService::checkLoginStatus() {
    OneSevenLiveLoginData loginData;
    if (!coreManager_->apiWrapper->GetSelfInfo(loginData)) {
        coreManager_->configManager->clearLoginData();
        return false;
    }

    // TODO: update loginData: displayName

    return true;
}

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
#include "../customized_cartoons/CustomizedCartoonDock.hpp"
#include "../customized_cartoons/CustomizedCartoonService.hpp"
#include "../websocket/OneSevenLiveWebsocketServer.hpp"
#include "../twitch/OneSevenLiveTwitchAuth.hpp"
#include "plugin-support.h"
#include "ChatBridgeService.hpp"
#include "CrashUploadService.hpp"

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
    const auto s = state_.load();
    if (s != SessionState::Idle) {
        obs_log(LOG_INFO, "Login click ignored due to session state=%d", static_cast<int>(s));
        return false;
    }

    bool expected = false;
    if (!loginDialogOpen_.compare_exchange_strong(expected, true)) {
        obs_log(LOG_INFO, "Login click ignored due to login dialog open");
        return false;
    }

    coreManager_->setSessionCancel(false);
    OneSevenLiveLoginDialog dialog(coreManager_->mainWindow, coreManager_->getApiWrapper());

    // Connect login success signal to slot function
    QObject::connect(&dialog, &OneSevenLiveLoginDialog::loginSuccess, this,
                     &AuthSessionService::handleLoginSuccess);

    const bool ok = dialog.exec() == QDialog::Accepted;
    loginDialogOpen_.store(false);
    return ok;
}

void AuthSessionService::handleLoginSuccess(const OneSevenLiveLoginData& loginData) {
    obs_log(LOG_INFO, "handleLoginSuccess");

    SessionState expected = SessionState::Idle;
    if (!state_.compare_exchange_strong(expected, SessionState::LoggingIn)) {
        obs_log(LOG_WARNING, "Login success ignored due to session state=%d",
                static_cast<int>(expected));
        return;
    }

    if (!coreManager_->configManager->setLoginData(loginData)) {
        obs_log(LOG_ERROR, "Failed to save login data");
        state_.store(SessionState::Idle);
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
        const auto s = state_.load();
        if (s != SessionState::LoggingIn && s != SessionState::Idle) {
            obs_log(LOG_WARNING, "Login state change ignored due to session state=%d",
                    static_cast<int>(s));
            return;
        }
        performLoginOperations(loginData);
        state_.store(SessionState::LoggedIn);
    } else {
        const auto s = state_.load();
        if (s != SessionState::LoggedIn) {
            obs_log(LOG_INFO, "Logout state change ignored due to session state=%d",
                    static_cast<int>(s));
            return;
        }
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

    coreManager_->customizedCartoonService_ =
        std::make_unique<CustomizedCartoonService>(coreManager_->mainWindow, coreManager_->apiWrapper.get(),
                                                   coreManager_->configManager.get(),
                                                   coreManager_->streamManager.get(), coreManager_);

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
                core->setSessionCancel(false);
                core->connectAblyChat(QString::number(rid), QString());
            }
        }
    });

    QTimer::singleShot(0, coreManager_, [core, loginData]() {
        if (!core || !core->crashUploadService_) {
            return;
        }
        core->crashUploadService_->onLogin(loginData);
    });

    // discovery is managed by YouTubeChatClient
}

void AuthSessionService::performLogoutOperations() {
    obs_log(LOG_INFO, "performLogoutOperations");
    coreManager_->setSessionCancel(true);

    {
        auto* ws = coreManager_->getWebsocketServer();
        if (ws && ws->is_running()) {
            ws->closeAllClients();
        }
    }

    coreManager_->destroyTwitchChatClient();
    coreManager_->destroyAblyChatClient();
    coreManager_->customizedCartoonService_.reset();

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
    if (!coreManager_->configManager->clearTwitchTokens()) {
        const auto err = coreManager_->configManager->getLastError();
        obs_log(LOG_WARNING, "Failed to clear Twitch tokens: %s %s", err.code.c_str(),
                err.message.c_str());
    }
    if (!coreManager_->configManager->clearTwitchUserInfo()) {
        const auto err = coreManager_->configManager->getLastError();
        obs_log(LOG_WARNING, "Failed to clear Twitch user info: %s %s", err.code.c_str(),
                err.message.c_str());
    }

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

        // Restore customized cartoon dock if it was previously shown
        if (coreManager_->configManager->getDockVisibility("customizedCartoon")) {
            coreManager_->handleCustomizedCartoonClicked();
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
            if (core->customizedCartoonDock) {
                docks << core->customizedCartoonDock.data();
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

    const auto s = state_.load();
    if (s != SessionState::LoggedIn) {
        obs_log(LOG_INFO, "Logout click ignored due to session state=%d", static_cast<int>(s));
        return;
    }

    if (coreManager_->status == OneSevenLiveStreamingStatus::Streaming) {
        if (logoutConfirmBox_) {
            obs_log(LOG_INFO, "Logout click ignored due to existing confirm dialog");
            return;
        }
        auto* msgBox = new QMessageBox(coreManager_->mainWindow);
        msgBox->setWindowTitle(obs_module_text("Logout.Warning.Title"));
        msgBox->setText(obs_module_text("Logout.Warning.Message"));
        QPushButton* confirmButton =
            msgBox->addButton(obs_module_text("Logout.Warning.Button.Yes"), QMessageBox::YesRole);
        QPushButton* cancelButton =
            msgBox->addButton(obs_module_text("Logout.Warning.Button.No"), QMessageBox::NoRole);
        msgBox->setDefaultButton(cancelButton);

        QPointer<AuthSessionService> self = this;
        logoutConfirmBox_ = msgBox;
        connect(msgBox, &QMessageBox::finished, this,
                [self, msgBox, confirmButton](int) {
                    if (self) {
                        self->logoutConfirmBox_.clear();
                    }
            if (msgBox->clickedButton() != confirmButton) {
                msgBox->deleteLater();
                return;
            }
            if (self) {
                QMetaObject::invokeMethod(
                    self,
                    [self]() {
                        if (self) {
                            if (self->isPendingLogout()) {
                                return;
                            }
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

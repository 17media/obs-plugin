#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <atomic>
#include "api/OneSevenLiveModels.hpp"

class OneSevenLiveCoreManager;

enum class SessionState {
    Idle,
    LoggingIn,
    LoggedIn,
    LoggingOut
};

class AuthSessionService : public QObject {
    Q_OBJECT
public:
    explicit AuthSessionService(OneSevenLiveCoreManager* coreManager, QObject* parent = nullptr);
    ~AuthSessionService() override = default;

    SessionState getState() const;
    bool isPendingLogout() const;
    void setPendingLogout(bool pending);

    bool checkLoginStatus();
    bool handleLoginClicked();
    void handleLogoutClicked();

    void handleLoginStateChanged(bool isLoggedIn, const OneSevenLiveLoginData& loginData = OneSevenLiveLoginData());

public slots:
    void handleLoginSuccess(const OneSevenLiveLoginData& loginData);

private:
    void performLoginOperations(const OneSevenLiveLoginData& loginData);
    void performLogoutOperations();
    void restoreDockStatesOnLogin();

    OneSevenLiveCoreManager* coreManager_;
    std::atomic<SessionState> state_{SessionState::Idle};
    std::atomic<bool> pendingLogout_{false};
};

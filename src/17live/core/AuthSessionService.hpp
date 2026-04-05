#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include "api/OneSevenLiveModels.hpp"

class OneSevenLiveCoreManager;

class AuthSessionService : public QObject {
    Q_OBJECT
public:
    explicit AuthSessionService(OneSevenLiveCoreManager* coreManager, QObject* parent = nullptr);
    ~AuthSessionService() override = default;

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
};

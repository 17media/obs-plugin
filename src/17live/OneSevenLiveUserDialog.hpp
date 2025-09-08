#pragma once

// Qt headers
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>

#include "api/OneSevenLiveModels.hpp"

// Forward declarations
class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;

// User information dialog class
class OneSevenLiveUserDialog : public QDialog {
    Q_OBJECT

public:
    OneSevenLiveUserDialog(QWidget* parent = nullptr, 
                          OneSevenLiveApiWrappers* apiWrapper = nullptr,
                          OneSevenLiveConfigManager* configManager = nullptr);
    ~OneSevenLiveUserDialog();

    // Set user information and display dialog
    void setUserInfo(const OneSevenLiveRockZoneViewer& user);

private slots:
    void onPokeUserClicked();
    void onCloseClicked();

private:
    void setupUi();
    void createConnections();
    void updateUserAvatar();

    // UI elements
    QLabel* avatarLabel;
    QLabel* usernameLabel;
    QLabel* userIdLabel;
    QPushButton* pokeButton;
    QPushButton* closeButton;

    OneSevenLiveRockZoneViewer viewer;

    // API wrapper
    OneSevenLiveApiWrappers* apiWrapper;
    OneSevenLiveConfigManager* configManager = nullptr;
};

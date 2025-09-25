#pragma once

// Qt headers
#include <QPointer>
#include <QThread>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "api/OneSevenLiveModels.hpp"

// Forward declarations
class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;

// User information dialog class
class OneSevenLiveUserDialog : public QDialog {
    Q_OBJECT

   public:
    OneSevenLiveUserDialog(QWidget* parent = nullptr, OneSevenLiveApiWrappers* apiWrapper = nullptr,
                           OneSevenLiveConfigManager* configManager = nullptr);
    ~OneSevenLiveUserDialog();

    // Set user information and display dialog
    void setUserInfo(const OneSevenLiveRockZoneViewer& user);

   private slots:
    void onPokeUserClicked();
    void onCloseClicked();

   protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

   private:
    void setupUi();
    void createConnections();
    void updateUserAvatar();
    void fetchUserInfo();
    void updateUserStats(const OneSevenLiveUserInfo& userInfo);

    // UI elements
    QLabel* avatarLabel;
    QLabel* usernameLabel;
    QLabel* userIdLabel;
    QLabel* followersLabel;
    QLabel* followingLabel;
    QLabel* likesLabel;
    QPushButton* pokeButton;
    QPushButton* closeButton;

    OneSevenLiveRockZoneViewer viewer;

    // API wrapper
    OneSevenLiveApiWrappers* apiWrapper;
    OneSevenLiveConfigManager* configManager = nullptr;

    // Dragging functionality
    bool dragging = false;
    QPoint dragStartPosition;
};

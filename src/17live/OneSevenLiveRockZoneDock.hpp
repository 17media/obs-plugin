#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

#include "OneSevenLiveUserDialog.hpp"

#include "api/OneSevenLiveModels.hpp"

class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;

// 摇滚区用户信息结构体
struct RockZoneUser {
    QString userId;       // 用户ID
    QString username;     // 用户名
    QString avatarUrl;    // 头像URL
    QByteArray avatarData; // 头像数据
};

class OneSevenLiveRockZoneDock : public QDockWidget {
    Q_OBJECT

   public:
    OneSevenLiveRockZoneDock(QWidget* parent = nullptr,
                           OneSevenLiveApiWrappers* apiWrapper = nullptr,
                           OneSevenLiveConfigManager* configManager = nullptr);
    ~OneSevenLiveRockZoneDock();

    void refreshUserList();

   protected:
    void resizeEvent(QResizeEvent* event) override;

   signals:
    void viewAllFriendsClicked();

   private slots:
    void onViewAllFriendsClicked();
    void handleTopLevelChanged(bool topLevel);
    void onUserItemClicked(QListWidgetItem* item);

   private:
    void setupUi();
    void createConnections();
    void updateUserItem(QListWidgetItem* item, const RockZoneUser& user);
    void showEmptyListMessage();
    void loadUserAvatar(const QString& url, RockZoneUser& user);

    QListWidget* userList;
    QLabel* titleLabel;
    QLabel* userCountLabel;
    QPushButton* viewAllFriendsButton;
    QWidget* emptyContainer = nullptr;

    OneSevenLiveApiWrappers* apiWrapper = nullptr;
    OneSevenLiveConfigManager* configManager = nullptr;

    QList<RockZoneUser> usersList;  // 存储用户列表
    
    // 用户信息对话框
    OneSevenLiveUserDialog* userDialog = nullptr;
};

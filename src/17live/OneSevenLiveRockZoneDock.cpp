#include "OneSevenLiveRockZoneDock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QPainter>

#include "utility/RemoteTextThread.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "plugin-support.h"

OneSevenLiveRockZoneDock::OneSevenLiveRockZoneDock(QWidget* parent,
                                               OneSevenLiveApiWrappers* apiWrapper_,
                                               OneSevenLiveConfigManager* configManager_)
    : QDockWidget(obs_module_text("Live.RockZone"), parent),
      apiWrapper(apiWrapper_),
      configManager(configManager_) {
    setupUi();
    createConnections();
    refreshUserList();

    // 添加延迟初始化以确保UI元素大小正确计算
    QTimer::singleShot(0, this, [this]() {
        if (emptyContainer && emptyContainer->isVisible()) {
            emptyContainer->setGeometry(widget()->rect());
        }
    });

    connect(this, &QDockWidget::topLevelChanged, this,
            &OneSevenLiveRockZoneDock::handleTopLevelChanged);
}

OneSevenLiveRockZoneDock::~OneSevenLiveRockZoneDock() {
    if (userDialog) {
        userDialog->deleteLater();
        userDialog = nullptr;
    }
}

void OneSevenLiveRockZoneDock::setupUi() {
    QWidget* container = new QWidget(this);
    container->setStyleSheet(
        "QWidget#container {"
        "    background-color: #000000;"
        "    border: none;"
        "    font-family: 'Inter';"
        "    color: #FFFFFF;"
        "    font-style: normal;"
        "}");
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 创建标题栏
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(5);

    titleLabel = new QLabel(obs_module_text("Live.RockZone"));
    titleLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 16px;"
        "}");

    userCountLabel = new QLabel();
    userCountLabel->setStyleSheet(
        "QLabel {"
        "    color: gray;"
        "    font-size: 12px;"
        "}");

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(userCountLabel);
    titleLayout->addStretch();

    mainLayout->addLayout(titleLayout);

    // 创建用户列表
    userList = new QListWidget();
    userList->setStyleSheet(
        "QListWidget {"
        "   background-color: transparent;"
        "   border: none;"
        "}"
        "QListWidget::item {"
        "   background-color: #000000;"
        "   border-radius: 0px;"
        "   padding: 5px;"
        "   margin: 0px;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #3a3a4a;"
        "   border: 1px solid #5a5a6a;"
        "   color: white;"
        "}"
        "QListWidget::item:hover:!selected {"
        "    background-color: #1a1a1a;"
        "}");
    userList->setResizeMode(QListWidget::Adjust);
    userList->setWordWrap(true);
    userList->setSpacing(1);
    mainLayout->addWidget(userList);

    // 创建底部按钮
    viewAllFriendsButton = new QPushButton(obs_module_text("Live.RockZone.ViewAllFriends"));
    viewAllFriendsButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #FF0001;"
        "    color: white;"
        "    border-radius: 4px;"
        "    padding: 8px;"
        "   font-weight: 600;"
        "   font-size: 16px;"
        "   line-height: 24px;"
        "}");
    viewAllFriendsButton->setFixedWidth(250);
    mainLayout->addWidget(viewAllFriendsButton, 0, Qt::AlignHCenter);

    setWidget(container);
}

void OneSevenLiveRockZoneDock::createConnections() {
    connect(viewAllFriendsButton, &QPushButton::clicked, this,
            &OneSevenLiveRockZoneDock::onViewAllFriendsClicked);
    
    // 连接用户列表项点击信号
    connect(userList, &QListWidget::itemClicked, this,
            &OneSevenLiveRockZoneDock::onUserItemClicked);
}

void OneSevenLiveRockZoneDock::updateUserItem(QListWidgetItem* item, const RockZoneUser& user) {
    QWidget* itemContainer = new QWidget(this);

    QHBoxLayout* mainLayout = new QHBoxLayout(itemContainer);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(10);

    // 用户头像
    QLabel* avatarLabel = new QLabel();
    avatarLabel->setFixedSize(40, 40);
    avatarLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #333333;"
        "    border-radius: 20px;"
        "}");
    
    // 如果有头像数据，则显示头像
    if (!user.avatarData.isEmpty()) {
        QPixmap avatar;
        avatar.loadFromData(user.avatarData);
        avatar = avatar.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
        // 创建圆形头像
        QPixmap roundedAvatar(40, 40);
        roundedAvatar.fill(Qt::transparent);
        
        QPainter painter(&roundedAvatar);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(avatar));
        painter.drawEllipse(0, 0, 40, 40);
        
        avatarLabel->setPixmap(roundedAvatar);
    }
    
    mainLayout->addWidget(avatarLabel);

    // 用户信息（用户名和ID）
    QVBoxLayout* userInfoLayout = new QVBoxLayout();
    userInfoLayout->setContentsMargins(0, 0, 0, 0);
    userInfoLayout->setSpacing(2);

    QLabel* usernameLabel = new QLabel(user.username);
    usernameLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "}");

    QLabel* userIdLabel = new QLabel(user.userId);
    userIdLabel->setStyleSheet(
        "QLabel {"
        "    color: #d9d9d9;"
        "    font-size: 12px;"
        "}");

    userInfoLayout->addWidget(usernameLabel);
    userInfoLayout->addWidget(userIdLabel);

    mainLayout->addLayout(userInfoLayout);
    mainLayout->addStretch();

    itemContainer->setLayout(mainLayout);
    item->setSizeHint(QSize(-1, 50));
    userList->setItemWidget(item, itemContainer);
}

void OneSevenLiveRockZoneDock::showEmptyListMessage() {
    // 隐藏列表
    userList->setVisible(false);

    // 如果空状态容器已存在，先删除它
    if (emptyContainer) {
        emptyContainer->deleteLater();
    }

    // 创建空状态容器
    emptyContainer = new QWidget(widget());
    emptyContainer->setStyleSheet(
        "QWidget {"
        "    background-color: #1e1e1e;"
        "    border-radius: 4px;"
        "}");

    // 设置空状态容器填充整个Dock区域
    emptyContainer->setGeometry(widget()->rect());

    // 创建布局管理器
    QVBoxLayout* emptyLayout = new QVBoxLayout(emptyContainer);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(20);
    emptyLayout->setContentsMargins(20, 20, 20, 20);

    // 创建提示标签
    QLabel* emptyLabel = new QLabel(obs_module_text("Live.RockZone.Empty"));
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setWordWrap(true);
    emptyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    emptyLabel->setStyleSheet(
        "QLabel {"
        "    color: #888888;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    padding: 0 10px;"
        "}");

    // 添加到布局
    emptyLayout->addWidget(emptyLabel);

    // 显示空状态容器
    emptyContainer->show();
    emptyContainer->raise();
}

void OneSevenLiveRockZoneDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);

    if (emptyContainer && emptyContainer->isVisible()) {
        emptyContainer->setGeometry(widget()->rect());
    }

    for (int i = 0; i < userList->count(); ++i) {
        QListWidgetItem* item = userList->item(i);
        QWidget* widget = userList->itemWidget(item);
        if (widget)
            widget->resize(userList->viewport()->width(), widget->height());
        item->setSizeHint(widget->sizeHint());
    }
}

void OneSevenLiveRockZoneDock::refreshUserList() {
    userList->clear();

    if (emptyContainer) {
        // 如果空状态容器已存在，先删除它
        emptyContainer->deleteLater();
        emptyContainer = nullptr;
    }

    // 模拟获取用户列表数据
    // 实际应用中，这里应该调用API获取真实数据
    usersList.clear();
    
    // 添加模拟数据
    for (int i = 0; i < 5; i++) {
        RockZoneUser user;
        user.userId = QString("12345678901234567890").left(3 + i * 5);
        user.username = QString("DamonDamonDamon123...");
        // 实际应用中应该设置真实的头像URL
        user.avatarUrl = "";
        usersList.append(user);
    }

    if (usersList.isEmpty()) {
        // 显示空列表提示
        showEmptyListMessage();
    } else {
        userList->setVisible(true);

        // 更新用户数量提示
        userCountLabel->setText(QString(obs_module_text("Live.RockZone.UserCount")).arg(usersList.size()));

        // 显示用户列表
        for (const auto& user : usersList) {
            QListWidgetItem* item = new QListWidgetItem(userList);
            updateUserItem(item, user);
            userList->addItem(item);
        }
    }
}

void OneSevenLiveRockZoneDock::loadUserAvatar(const QString& url, RockZoneUser& user) {
    if (url.isEmpty()) {
        return;
    }

    RemoteTextThread *thread = new RemoteTextThread(url.toStdString(), "image/png", "", 0, true);
        
    connect(thread, &RemoteTextThread::ImageResult, this, [this, &user](const QByteArray &imageData, const QString &error) {
        if (error.isEmpty() && !imageData.isEmpty()) {
            user.avatarData = imageData;
            refreshUserList();
        }
    });
    
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    
    thread->start();
}

void OneSevenLiveRockZoneDock::onViewAllFriendsClicked() {
    // 发出查看所有朋友的信号
    emit viewAllFriendsClicked();
}

void OneSevenLiveRockZoneDock::handleTopLevelChanged(bool topLevel) {
    if (!topLevel) {
        // 停靠状态
        adjustSize();
        // 可能还需要强制更新布局或子部件大小
        for (int i = 0; i < userList->count(); ++i) {
            QListWidgetItem* item = userList->item(i);
            QWidget* itemWidget = userList->itemWidget(item);
            if (itemWidget) {
                itemWidget->adjustSize();
                item->setSizeHint(itemWidget->sizeHint());
            }
        }
    }
}

void OneSevenLiveRockZoneDock::onUserItemClicked(QListWidgetItem* item) {
    // 获取点击的项目索引
    int index = userList->row(item);
    if (index < 0 || index >= usersList.size()) {
        return;
    }
    
    // 获取用户信息
    const RockZoneUser& user = usersList.at(index);
    
    // 创建用户信息对话框（如果不存在）
    if (!userDialog) {
        userDialog = new OneSevenLiveUserDialog(this, apiWrapper);
    }
    
    // 设置用户信息并显示对话框
    userDialog->setUserInfo(user.userId, user.username, user.avatarData);
    userDialog->exec();
}

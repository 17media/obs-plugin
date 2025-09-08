#pragma once

// 标准库头文件
#include <string>
#include <vector>
#include <type_traits>

// Qt头文件
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>

#include "api/OneSevenLiveModels.hpp"

// 前向声明
class OneSevenLiveApiWrappers;

// 用户信息对话框类
class OneSevenLiveUserDialog : public QDialog {
    Q_OBJECT

public:
    OneSevenLiveUserDialog(QWidget* parent = nullptr, 
                          OneSevenLiveApiWrappers* apiWrapper = nullptr);
    ~OneSevenLiveUserDialog();

    // 设置用户信息并显示对话框
    void setUserInfo(const QString& userId, const QString& username, const QByteArray& avatarData);

private slots:
    void onPokeUserClicked();
    void onCloseClicked();

private:
    void setupUi();
    void createConnections();
    void updateUserAvatar();

    // UI元素
    QLabel* avatarLabel;
    QLabel* usernameLabel;
    QLabel* userIdLabel;
    QPushButton* pokeButton;
    QPushButton* closeButton;

    // 用户数据
    QString userId;
    QString username;
    QByteArray avatarData;

    // API包装器
    OneSevenLiveApiWrappers* apiWrapper;
};

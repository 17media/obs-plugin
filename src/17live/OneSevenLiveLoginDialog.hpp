#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

struct OneSevenLiveLoginData;

class OneSevenLiveApiWrappers;

class OneSevenLiveLoginDialog : public QDialog {
    Q_OBJECT

public:
    explicit OneSevenLiveLoginDialog(QWidget* parent = nullptr, OneSevenLiveApiWrappers* apiWrapper_ = nullptr);
    ~OneSevenLiveLoginDialog();

private:
    void setupUi();
    void handleLogin();

signals:
    /**
     * @brief 登录成功信号
     * @param loginData 登录信息
     */
    void loginSuccess(const OneSevenLiveLoginData& loginData);

private:
    QLabel* titleLabel;
    QLineEdit* usernameEdit;
    QLineEdit* passwordEdit;
    QPushButton* showPasswordButton;
    QPushButton* loginButton;
    QLabel* errorLabel;
    QLabel* forgotPasswordLabel;
    QLabel* registerLabel;
    QLabel* disclaimerLabel;
    QLabel* passwordLabel;
    QPushButton* passwordQuestionButton;
    QLabel* forgotPasswordLinkLabel;
    OneSevenLiveApiWrappers* apiWrapper;
};

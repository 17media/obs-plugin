#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

struct SeventeenLiveLoginData;

class SeventeenLiveLoginDialog : public QDialog {
    Q_OBJECT

public:
    explicit SeventeenLiveLoginDialog(QWidget* parent = nullptr);
    ~SeventeenLiveLoginDialog();

private:
    void setupUi();
    void handleLogin();

signals:
    /**
     * @brief 登录成功信号
     * @param loginData 登录信息
     */
    void loginSuccess(const SeventeenLiveLoginData& loginData);

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
};

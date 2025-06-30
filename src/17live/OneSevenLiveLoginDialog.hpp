#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

struct OneSevenLiveLoginData;

class OneSevenLiveApiWrappers;

class OneSevenLiveLoginDialog : public QDialog {
    Q_OBJECT

   public:
    explicit OneSevenLiveLoginDialog(QWidget* parent = nullptr,
                                     OneSevenLiveApiWrappers* apiWrapper_ = nullptr);
    ~OneSevenLiveLoginDialog();

   private:
    void setupUi();
    void handleLogin();

   signals:
    /**
     * @brief Login success signal
     * @param loginData Login information
     */
    void loginSuccess(const OneSevenLiveLoginData& loginData);

   private:
    QLabel* titleLabel;
    QLineEdit* usernameEdit;
    QLineEdit* passwordEdit;
    QPushButton* showPasswordButton;
    QPushButton* loginButton;
    QLabel* errorLabel;
    QWidget* errorContainer;
    QLabel* forgotPasswordLabel;
    QLabel* registerLabel;
    QLabel* disclaimerLabel;
    QLabel* passwordLabel;
    QPushButton* passwordQuestionButton;
    QLabel* forgotPasswordLinkLabel;
    OneSevenLiveApiWrappers* apiWrapper;
};

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
    QLineEdit* usernameEdit = nullptr;
    QLineEdit* passwordEdit = nullptr;
    QPushButton* showPasswordButton = nullptr;
    QPushButton* loginButton = nullptr;
    QLabel* errorLabel = nullptr;
    QWidget* errorContainer = nullptr;
    QLabel* registerLabel = nullptr;
    QLabel* disclaimerLabel = nullptr;
    QLabel* passwordLabel = nullptr;
    QPushButton* passwordQuestionButton = nullptr;
    QLabel* forgotPasswordLinkLabel = nullptr;
    OneSevenLiveApiWrappers* apiWrapper = nullptr;
};

#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

namespace seventeenlive {

class LoginDialog : public QDialog {
    Q_OBJECT

public:
    explicit LoginDialog(QWidget* parent = nullptr);
    ~LoginDialog();

private:
    void setupUi();
    void handleLogin();

private:
    QLabel* titleLabel;
    QLineEdit* usernameEdit;
    QLineEdit* passwordEdit;
    QPushButton* loginButton;
    QLabel* errorLabel;
    QLabel* forgotPasswordLabel;
    QLabel* registerLabel;
    QLabel* disclaimerLabel;
};

} // namespace seventeenlive

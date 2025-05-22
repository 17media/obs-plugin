#include <obs-module.h>
#include <plugin-support.h>

#include "SeventeenLiveLoginDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QStyle>
#include <QMessageBox>

#include "api/SeventeenLiveApiWrappers.hpp"

#include "moc_SeventeenLiveLoginDialog.cpp"

SeventeenLiveLoginDialog::SeventeenLiveLoginDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(obs_module_text("Auth.SignIn"));
    setFixedSize(400, 600);
}

SeventeenLiveLoginDialog::~SeventeenLiveLoginDialog()
{
}

void SeventeenLiveLoginDialog::setupUi()
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // Logo
    titleLabel = new QLabel(this);
    QPixmap logo(":/resources/17live-logo.svg");
    titleLabel->setPixmap(logo.scaled(200, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // 用户名输入框
    usernameEdit = new QLineEdit(this);
    usernameEdit->setPlaceholderText(obs_module_text("Auth.Username"));
    usernameEdit->setMinimumHeight(40);
    mainLayout->addWidget(usernameEdit);

    // 密码输入框
    passwordEdit = new QLineEdit(this);
    passwordEdit->setPlaceholderText(obs_module_text("Auth.Password"));
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setMinimumHeight(40);
    mainLayout->addWidget(passwordEdit);

    // 错误提示
    errorLabel = new QLabel(this);
    errorLabel->setStyleSheet("color: red;");
    errorLabel->setText(obs_module_text("Auth.Error01"));
    errorLabel->setVisible(false);
    mainLayout->addWidget(errorLabel);

    // 登录按钮
    loginButton = new QPushButton(obs_module_text("Auth.SignIn"), this);
    loginButton->setMinimumHeight(40);
    loginButton->setStyleSheet("background-color: red; color: white; border: none; border-radius: 5px;");
    connect(loginButton, &QPushButton::clicked, this, &SeventeenLiveLoginDialog::handleLogin);
    mainLayout->addWidget(loginButton);

    // 忘记密码链接
    // TODO: set real link to forgot password page
    QString forgotPasswordLinkTemplate = obs_module_text("Auth.ForgotPassword");
    QString forgotPasswordLink = forgotPasswordLinkTemplate.arg("#");
    forgotPasswordLabel = new QLabel(forgotPasswordLink, this);
    forgotPasswordLabel->setAlignment(Qt::AlignRight);
    forgotPasswordLabel->setOpenExternalLinks(true);
    mainLayout->addWidget(forgotPasswordLabel);

    // 注册新用户链接
    // TODO: set real link to register page
    QString registerLinkTemplate = obs_module_text("Auth.Register");
    QString registerLink = registerLinkTemplate.arg("#");
    registerLabel = new QLabel(registerLink, this);
    registerLabel->setAlignment(Qt::AlignCenter);
    registerLabel->setOpenExternalLinks(true);
    mainLayout->addWidget(registerLabel);

    mainLayout->addStretch();

    // 免责声明
    disclaimerLabel = new QLabel(obs_module_text("Auth.Hint01"), this);
    disclaimerLabel->setWordWrap(true);
    disclaimerLabel->setAlignment(Qt::AlignCenter);
    disclaimerLabel->setStyleSheet("color: gray; font-size: 12px;");
    mainLayout->addWidget(disclaimerLabel);
}

void SeventeenLiveLoginDialog::handleLogin()
{
    obs_log(LOG_INFO, "SeventeenLiveLoginDialog::handle login");
    
    // 验证逻辑
    if (usernameEdit->text().isEmpty() || passwordEdit->text().isEmpty()) {
        errorLabel->setVisible(true);
        return;
    }
    
    // 创建API包装器实例
    SeventeenLiveApiWrappers apiWrapper;
    SeventeenLiveLoginData loginData;
    
    // 调用登录接口
    if (!apiWrapper.Login(usernameEdit->text(), passwordEdit->text(), loginData)) {
        QString errorMessageTemplate = obs_module_text("Auth.Error02");
        QString errorMessage = errorMessageTemplate.arg(apiWrapper.getLastErrorMessage());
        errorLabel->setText(errorMessage);
        errorLabel->setVisible(true);
        return;
    }

    obs_log(LOG_INFO, "login success");
    obs_log(LOG_INFO, "userID: %s", loginData.userInfo.userID.toStdString().c_str());
    obs_log(LOG_INFO, "displayName: %s", loginData.userInfo.displayName.toStdString().c_str());
    obs_log(LOG_INFO, "roomID: %d", loginData.userInfo.roomID);

    emit loginSuccess(loginData);

    // log access token
    obs_log(LOG_INFO, "access token: %s", loginData.accessToken.toStdString().c_str());
    
    // 显示登录成功消息框
    QMessageBox::information(this, obs_module_text("Auth.LoginSuccess"), QString(obs_module_text("Auth.LoginSuccess.Tip")).arg(loginData.userInfo.openID));
    
    // 登录成功
    accept();
}

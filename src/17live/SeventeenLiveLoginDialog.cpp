#include <obs-module.h>
#include <plugin-support.h>

#include "SeventeenLiveLoginDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QStyle>

#include "api/SeventeenLiveApiWrappers.hpp"

#include "moc_SeventeenLiveLoginDialog.cpp"

namespace seventeenlive {

SeventeenLiveLoginDialog::SeventeenLiveLoginDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(tr("登録"));
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
    usernameEdit->setPlaceholderText(tr("用戶名"));
    usernameEdit->setMinimumHeight(40);
    mainLayout->addWidget(usernameEdit);

    // 密码输入框
    passwordEdit = new QLineEdit(this);
    passwordEdit->setPlaceholderText(tr("密碼"));
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setMinimumHeight(40);
    mainLayout->addWidget(passwordEdit);

    // 错误提示
    errorLabel = new QLabel(this);
    errorLabel->setStyleSheet("color: red;");
    errorLabel->setText(tr("用戶名或密碼不正確"));
    errorLabel->setVisible(false);
    mainLayout->addWidget(errorLabel);

    // 登录按钮
    loginButton = new QPushButton(tr("登録"), this);
    loginButton->setMinimumHeight(40);
    loginButton->setStyleSheet("background-color: red; color: white; border: none; border-radius: 5px;");
    connect(loginButton, &QPushButton::clicked, this, &SeventeenLiveLoginDialog::handleLogin);
    mainLayout->addWidget(loginButton);

    // 忘记密码链接
    forgotPasswordLabel = new QLabel("<a href='#'>忘記密碼？</a>", this);
    forgotPasswordLabel->setAlignment(Qt::AlignRight);
    forgotPasswordLabel->setOpenExternalLinks(true);
    mainLayout->addWidget(forgotPasswordLabel);

    // 注册新用户链接
    registerLabel = new QLabel("<a href='#'>註冊新用戶</a>", this);
    registerLabel->setAlignment(Qt::AlignCenter);
    registerLabel->setOpenExternalLinks(true);
    mainLayout->addWidget(registerLabel);

    mainLayout->addStretch();

    // 免责声明
    disclaimerLabel = new QLabel(tr("請提高警覺：17LIVE 不會以任何分期付款失敗等名義要求您提供帳戶資訊、金ATM操作或提供信用卡等資料。"), this);
    disclaimerLabel->setWordWrap(true);
    disclaimerLabel->setAlignment(Qt::AlignCenter);
    disclaimerLabel->setStyleSheet("color: gray; font-size: 12px;");
    mainLayout->addWidget(disclaimerLabel);
}

void SeventeenLiveLoginDialog::handleLogin()
{
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
        errorLabel->setText(tr("登录失败：") + apiWrapper.getLastErrorMessage());
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
    
    // 登录成功
    accept();
}

} // namespace seventeenlive

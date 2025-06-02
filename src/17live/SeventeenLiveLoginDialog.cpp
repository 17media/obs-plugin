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
    // 设置对话框背景为黑色
    setStyleSheet(
        "QDialog {"
        "    background-color: #000000;"
        "    color: white;"
        "}"
    );
    
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(40, 40, 40, 40);

    // Logo - 使用白色17LIVE文字
    titleLabel = new QLabel(this);
    titleLabel->setText("17LIVE");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-size: 48px;"
        "    font-weight: bold;"
        "    margin-bottom: 20px;"
        "}"
    );
    mainLayout->addWidget(titleLabel);
    
    // 添加"17LIVE ID 登入"标题
    QLabel* loginTitleLabel = new QLabel(obs_module_text("Auth.Caption"), this);
    loginTitleLabel->setAlignment(Qt::AlignLeft);
    loginTitleLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-size: 24px;"
        "    font-weight: bold;"
        "    margin-bottom: 10px;"
        "}"
    );
    mainLayout->addWidget(loginTitleLabel);
    
    // 添加用户头像区域（可选）
    QWidget* avatarContainer = new QWidget(this);
    QHBoxLayout* avatarLayout = new QHBoxLayout(avatarContainer);
    avatarLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* idLabel = new QLabel("ID", this);
    idLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-size: 16px;"
        "    margin-right: 10px;"
        "}"
    );
    avatarLayout->addWidget(idLabel);
    mainLayout->addWidget(avatarContainer);

    // 用户名输入框
    usernameEdit = new QLineEdit(this);
    usernameEdit->setPlaceholderText("name@company.com");
    usernameEdit->setMinimumHeight(50);
    usernameEdit->setStyleSheet(
        "QLineEdit {"
        "    background-color: #333333;"
        "    border: 1px solid #555555;"
        "    border-radius: 8px;"
        "    padding: 0 15px;"
        "    color: white;"
        "    font-size: 16px;"
        "}"
        "QLineEdit::placeholder {"
        "    color: #888888;"
        "}"
        "QLineEdit:focus {"
        "    border: 2px solid #4A90E2;"
        "}"
    );
    mainLayout->addWidget(usernameEdit);

    // 密码标签容器
    QWidget* passwordLabelContainer = new QWidget(this);
    QHBoxLayout* passwordLabelLayout = new QHBoxLayout(passwordLabelContainer);
    passwordLabelLayout->setContentsMargins(0, 0, 0, 0);
    passwordLabelLayout->setSpacing(5);
    
    // 密码标签
    passwordLabel = new QLabel(obs_module_text("Auth.Password"), passwordLabelContainer);
    passwordLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-size: 14px;"
        "}"
    );
    
    // Question icon按钮
    passwordQuestionButton = new QPushButton("?", passwordLabelContainer);
    passwordQuestionButton->setFixedSize(16, 16);
    passwordQuestionButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #666;"
        "    color: white;"
        "    border: 1px solid #888;"
        "    border-radius: 8px;"
        "    font-size: 10px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #777;"
        "}"
    );
    
    // 设置tooltip提示
    passwordQuestionButton->setToolTip(
        obs_module_text("Auth.Password.Hint")
    );
    
    // 添加到标签布局
    passwordLabelLayout->addWidget(passwordLabel);
    passwordLabelLayout->addWidget(passwordQuestionButton);
    passwordLabelLayout->addStretch(); // 添加弹性空间，使内容左对齐
    
    mainLayout->addWidget(passwordLabelContainer);

    // 密码输入框容器
    QWidget* passwordContainer = new QWidget(this);
    QHBoxLayout* passwordLayout = new QHBoxLayout(passwordContainer);
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->setSpacing(0);
    
    // 密码输入框
    passwordEdit = new QLineEdit(passwordContainer);
    passwordEdit->setPlaceholderText(obs_module_text("Auth.Password.Placeholder"));
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setMinimumHeight(40);
    passwordEdit->setStyleSheet(
        "QLineEdit {"
        "    border: 1px solid #ccc;"
        "    border-radius: 5px 0 0 5px;"
        "    padding: 0 10px;"
        "}"
    );
    
    // 显示/隐藏密码按钮
    showPasswordButton = new QPushButton(passwordContainer);
    showPasswordButton->setText("👁");
    showPasswordButton->setFixedSize(40, 40);
    showPasswordButton->setStyleSheet(
        "QPushButton {"
        "    border: 1px solid #ccc;"
        "    border-left: none;"
        "    border-radius: 0 5px 5px 0;"
        "    background-color: #f5f5f5;"
        "}"
        "QPushButton:hover {"
        "    background-color: #e0e0e0;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #d0d0d0;"
        "}"
    );
    
    // 连接按钮点击事件
    connect(showPasswordButton, &QPushButton::clicked, this, [this]() {
        if (passwordEdit->echoMode() == QLineEdit::Password) {
            passwordEdit->setEchoMode(QLineEdit::Normal);
            showPasswordButton->setText("🙈");
        } else {
            passwordEdit->setEchoMode(QLineEdit::Password);
            showPasswordButton->setText("👁");
        }
    });
    
    // 添加到布局
    passwordLayout->addWidget(passwordEdit);
    passwordLayout->addWidget(showPasswordButton);
    
    mainLayout->addWidget(passwordContainer);

    // 错误提示
    errorLabel = new QLabel(this);
    errorLabel->setText(obs_module_text("Auth.Error01"));
    errorLabel->setStyleSheet(
        "QLabel {"
        "    color: #FF6B6B;"
        "    font-size: 14px;"
        "    margin: 10px 0;"
        "}"
    );
    errorLabel->setVisible(false);
    mainLayout->addWidget(errorLabel);

    // 登录按钮
    loginButton = new QPushButton("Auth.SignIn", this);
    loginButton->setMinimumHeight(50);
    loginButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #FF4444;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 8px;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #FF6666;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #CC3333;"
        "}"
    );
    connect(loginButton, &QPushButton::clicked, this, &SeventeenLiveLoginDialog::handleLogin);
    mainLayout->addWidget(loginButton);

    // 注册新用户链接
    registerLabel = new QLabel(obs_module_text("Auth.Register"), this);
    registerLabel->setAlignment(Qt::AlignLeft);
    registerLabel->setOpenExternalLinks(true);
    registerLabel->setStyleSheet(
        "QLabel {"
        "    margin: 10px 0;"
        "}"
    );
    mainLayout->addWidget(registerLabel);
    
    // 更多登入幫助
    QLabel* helpLabel = new QLabel("Auth.Help", this);
    helpLabel->setAlignment(Qt::AlignLeft);
    helpLabel->setOpenExternalLinks(false);
    helpLabel->setStyleSheet(
        "QLabel {"
        "    margin-bottom: 20px;"
        "}"
    );
    mainLayout->addWidget(helpLabel);

    mainLayout->addStretch();

    // 免責聲明
    disclaimerLabel = new QLabel(obs_module_text("Auth.Hint01"), this);
    disclaimerLabel->setWordWrap(true);
    disclaimerLabel->setAlignment(Qt::AlignCenter);
    disclaimerLabel->setStyleSheet(
        "QLabel {"
        "    color: #666666;"
        "    font-size: 12px;"
        "    line-height: 1.4;"
        "    margin: 20px 0;"
        "}"
    );
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

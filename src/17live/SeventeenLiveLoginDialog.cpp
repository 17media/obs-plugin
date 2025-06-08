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

SeventeenLiveLoginDialog::SeventeenLiveLoginDialog(QWidget* parent,  SeventeenLiveApiWrappers* apiWrapper_)
    : QDialog(parent),  apiWrapper(apiWrapper_)
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

    // Logo - 添加一个加载resource中 17live-logo-whith.svg 图片
    QLabel* logoLabel = new QLabel(this);
    QPixmap logoPixmap(":/resources/17live-logo-white.svg");
    // 设置合适的缩放大小
    logoLabel->setPixmap(logoPixmap.scaled(200, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(logoLabel);
    
    
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
    
    QWidget* idLabelContainer = new QWidget(this);
    QHBoxLayout* idLabelLayout = new QHBoxLayout(idLabelContainer);
    idLabelLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* idLabel = new QLabel("ID", this);
    idLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-size: 16px;"
        "    margin-right: 10px;"
        "}"
    );
    idLabelLayout->addWidget(idLabel);
    mainLayout->addWidget(idLabelContainer);

    // 用户名输入框
    usernameEdit = new QLineEdit(this);
    usernameEdit->setMinimumHeight(40);
    usernameEdit->setStyleSheet(
        "QLineEdit {"
        "    border: none;"
        "    border-radius: 2px;"
        "    padding: 0 15px;"
        "    font-size: 14px;"
        "}"
        "QLineEdit::placeholder {"
        "    color: #888888;"
        "}"
        "QLineEdit:focus {"
        "    border: 2px solid #4A90E2;"
        "}"
    );
    mainLayout->addWidget(usernameEdit);

    // 密码标签
    QWidget* passwordLabelContainer = new QWidget(this);
    QHBoxLayout* passwordLabelLayout = new QHBoxLayout(passwordLabelContainer);
    passwordLabelLayout->setContentsMargins(0, 0, 0, 0);
    // passwordLabelLayout->setSpacing(0);
    passwordLabel = new QLabel(obs_module_text("Auth.Password"), passwordLabelContainer);
    passwordLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-size: 14px;"
        "}"
    );
    
    // Question icon按钮
    passwordQuestionButton = new QPushButton(passwordLabelContainer);
    
    // 设置问号图标
    QIcon questionIcon(":/resources/question.svg");
    passwordQuestionButton->setIcon(questionIcon);
    passwordQuestionButton->setIconSize(QSize(16, 16));
    passwordQuestionButton->setFixedSize(16, 16);
    
    // 设置透明背景样式
    passwordQuestionButton->setStyleSheet(
        "QPushButton {"
        "    background: transparent;"
        "    border: none;"
        "    padding: 0px;"
        "}"
        "QPushButton:hover {"
        "    background: rgba(255, 255, 255, 0.1);"
        "}"
        "QPushButton:pressed {"
        "    background: rgba(255, 255, 255, 0.2);"
        "}"
    );
    
    // 设置tooltip提示
    passwordQuestionButton->setToolTip(
        QString("<div style='max-width: 200px; word-wrap: break-word; padding: 5px; border-radius: 5px;'>%1</div>")
        .arg(obs_module_text("Auth.Password.Tip"))
    );
    
    // 忘记密码链接
    forgotPasswordLinkLabel = new QLabel(passwordLabelContainer);
    forgotPasswordLinkLabel->setText(obs_module_text("Auth.ForgotPassword"));
    forgotPasswordLinkLabel->setOpenExternalLinks(true);
    forgotPasswordLinkLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 14px;"
        "}"
        "QLabel a {"
        "    color: #4A9EFF;"
        "    text-decoration: none;"
        "}"
        "QLabel a:hover {"
        "    text-decoration: underline;"
        "}"
    );
    
    // 添加到标签布局
    passwordLabelLayout->addWidget(passwordLabel);
    passwordLabelLayout->addWidget(passwordQuestionButton);
    passwordLabelLayout->addStretch(); // 添加弹性空间，将忘记密码链接推到右侧
    passwordLabelLayout->addWidget(forgotPasswordLinkLabel);
    
    mainLayout->addWidget(passwordLabelContainer);

    // 密码输入框容器
    QWidget* passwordContainer = new QWidget(this);
    QHBoxLayout* passwordLayout = new QHBoxLayout(passwordContainer);
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->setSpacing(0);

    // 密码输入框
    passwordEdit = new QLineEdit(passwordContainer);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setFixedHeight(40);
    passwordEdit->setStyleSheet(
        "QLineEdit {"
        "    border: none;"
        "    border-radius: 2px 0 0 2px;"
        "    padding: 0 15px;"
        "}"
        "QLineEdit:focus {"
        "    border: 2px solid #4A90E2;"
        "    border-right: none;"
        "}"
    );

    // 添加回车键处理，点击回车键相当于点击登录按钮
    connect(passwordEdit, &QLineEdit::returnPressed, this, &SeventeenLiveLoginDialog::handleLogin);
    
    // 显示/隐藏密码按钮
    showPasswordButton = new QPushButton(passwordContainer);

    // 设置初始图标为显示密码图标
    QIcon showIcon(":/resources/show-password.svg");
    showPasswordButton->setIcon(showIcon);
    showPasswordButton->setIconSize(QSize(20, 20));
    showPasswordButton->setFixedSize(40, 40);
    showPasswordButton->setStyleSheet(
        "QPushButton {"
        "    border: none;"
        "    border-radius: 0 2px 2px 0;"
        // "    background-color: #f8f8f8;"
        "}"
        "QPushButton:hover {"
        // "    background-color: #e8e8e8;"
        // "    border: none;"
        "}"
        "QPushButton:pressed {"
        // "    background-color: #d8d8d8;"
        "}"
    );
    showPasswordButton->setFlat(true); // 去除按钮边框，和passwordEdit紧密相连

    passwordLayout->addWidget(passwordEdit);
    passwordLayout->addWidget(showPasswordButton);

    mainLayout->addWidget(passwordContainer);
    
    // 连接按钮点击事件
    connect(showPasswordButton, &QPushButton::clicked, this, [this]() {
        if (passwordEdit->echoMode() == QLineEdit::Password) {
            passwordEdit->setEchoMode(QLineEdit::Normal);
            // 切换到隐藏密码图标
            QIcon hideIcon(":/resources/hide-password.svg");
            showPasswordButton->setIcon(hideIcon);
        } else {
            passwordEdit->setEchoMode(QLineEdit::Password);
            // 切换到显示密码图标
            QIcon showIcon(":/resources/show-password.svg");
            showPasswordButton->setIcon(showIcon);
        }
    });

    // 错误提示
    errorLabel = new QLabel(this);
    errorLabel->setText(obs_module_text("Auth.Error01"));
    errorLabel->setStyleSheet(
        "QLabel {"
        "    color: #FF6B6B;"
        "    font-size: 14px;"
        "    margin: 10px 0;"
        "    min-height: 20px;"
        "}"
    );
    errorLabel->setVisible(false);
    // 修改：允许标签自动调整高度，但保持固定的最小高度
    errorLabel->setMinimumHeight(20);
    // 修改：允许标签在需要时垂直扩展
    errorLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    // 修改：启用自动换行
    errorLabel->setWordWrap(true);
    mainLayout->addWidget(errorLabel);

    QWidget *loginContainer = new QWidget(this);
    QHBoxLayout *loginLayout = new QHBoxLayout(loginContainer);
    loginLayout->setContentsMargins(0, 0, 0, 0);

    // 登录按钮
    loginButton = new QPushButton(obs_module_text("Auth.SignIn"), this);
    loginButton->setMinimumHeight(40);
    loginButton->setMinimumWidth(150);
    loginButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #FF4444;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 2px;"
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

    QVBoxLayout *loginLeftLayout = new QVBoxLayout();
    loginLeftLayout->setContentsMargins(0, 0, 0, 0);

    // 注册新用户链接
    registerLabel = new QLabel(obs_module_text("Auth.Register"), this);
    registerLabel->setAlignment(Qt::AlignLeft);
    registerLabel->setOpenExternalLinks(true);
    registerLabel->setStyleSheet(
        "QLabel {"
        "    margin: 10px 0;"
        "}"
    );
    
    // 更多登入幫助
    QLabel* helpLabel = new QLabel(obs_module_text("Auth.Help"), this);
    helpLabel->setAlignment(Qt::AlignLeft);
    helpLabel->setOpenExternalLinks(true);
    helpLabel->setStyleSheet(
        "QLabel {"
        "    margin-bottom: 20px;"
        "}"
    );

    loginLeftLayout->addWidget(registerLabel);
    loginLeftLayout->addWidget(helpLabel);
    loginLayout->addLayout(loginLeftLayout);
    loginLayout->addStretch();
    loginLayout->addWidget(loginButton);

    loginLayout->setAlignment(loginLeftLayout, Qt::AlignTop);
    loginLayout->setAlignment(loginButton, Qt::AlignTop);

    mainLayout->addWidget(loginContainer);

    mainLayout->addStretch();

    // 免責聲明
    disclaimerLabel = new QLabel(obs_module_text("Auth.Hint01"), this);
    disclaimerLabel->setWordWrap(true);
    disclaimerLabel->setAlignment(Qt::AlignCenter);
    disclaimerLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
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
    SeventeenLiveLoginData loginData;
    
    // 调用登录接口
    if (!apiWrapper->Login(usernameEdit->text(), passwordEdit->text(), loginData)) {
        // QString errorMessageTemplate = obs_module_text("Auth.Error02");
        // QString errorMessage = errorMessageTemplate.arg(apiWrapper.getLastErrorMessage());
        // errorLabel->setText(errorMessage);
        errorLabel->setVisible(true);
        return;
    }

    // obs_log(LOG_INFO, "login success");
    // obs_log(LOG_INFO, "userID: %s", loginData.userInfo.userID.toStdString().c_str());
    // obs_log(LOG_INFO, "displayName: %s", loginData.userInfo.displayName.toStdString().c_str());
    // obs_log(LOG_INFO, "roomID: %d", loginData.userInfo.roomID);

    emit loginSuccess(loginData);

    // log access token
    // obs_log(LOG_INFO, "access token: %s", loginData.accessToken.toStdString().c_str());
    
    // 显示登录成功消息框
    QMessageBox::information(this, obs_module_text("Auth.LoginSuccess"), QString(obs_module_text("Auth.LoginSuccess.Tip")).arg(loginData.userInfo.openID));
    
    // 登录成功
    accept();
}

#include <obs-module.h>
#include <plugin-support.h>

#include "OneSevenLiveLoginDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QStyle>
#include <QMessageBox>

#include "api/OneSevenLiveApiWrappers.hpp"

#include "moc_OneSevenLiveLoginDialog.cpp"

OneSevenLiveLoginDialog::OneSevenLiveLoginDialog(QWidget* parent,  OneSevenLiveApiWrappers* apiWrapper_)
    : QDialog(parent),  apiWrapper(apiWrapper_)
{
    setupUi();
    setWindowTitle(obs_module_text("Auth.SignIn"));
    // setFixedSize(400, 600);
    setFixedWidth(400);
}

OneSevenLiveLoginDialog::~OneSevenLiveLoginDialog()
{
}

void OneSevenLiveLoginDialog::setupUi()
{
    // Set dialog background to black
    setStyleSheet(
        "QDialog {"
        "    background-color: #000000;"
        "    color: white;"
        "}"
    );
    
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(40, 40, 40, 40);

    // Logo - add loading of 17live-logo-whith.svg image from resources
    QLabel* logoLabel = new QLabel(this);
    QPixmap logoPixmap(":/resources/17live-logo-white.svg");
    // Set appropriate scaling size
    logoLabel->setPixmap(logoPixmap.scaled(200, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(logoLabel);
    
    
    // Add "17LIVE ID Login" title
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
    idLabelLayout->setSpacing(0);
    
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

    mainLayout->addSpacing(5);

    // Username input field
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

    // Password label
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
    
    // Question icon button
    passwordQuestionButton = new QPushButton(passwordLabelContainer);
    
    // Set question mark icon
    QIcon questionIcon(":/resources/question.svg");
    passwordQuestionButton->setIcon(questionIcon);
    passwordQuestionButton->setIconSize(QSize(16, 16));
    passwordQuestionButton->setFixedSize(16, 16);
    
    // Set transparent background style
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
    
    // Set tooltip hint
    passwordQuestionButton->setToolTip(
        QString("<div style='max-width: 200px; word-wrap: break-word; padding: 5px; border-radius: 5px;'>%1</div>")
        .arg(obs_module_text("Auth.Password.Tip"))
    );
    
    // Forgot password link
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
    
    // Add to label layout
    passwordLabelLayout->addWidget(passwordLabel);
    passwordLabelLayout->addWidget(passwordQuestionButton);
    passwordLabelLayout->addStretch(); // Add flexible space to push forgot password link to right
    passwordLabelLayout->addWidget(forgotPasswordLinkLabel);
    
    mainLayout->addWidget(passwordLabelContainer);

    // Password input field container
    QWidget* passwordContainer = new QWidget(this);
    QHBoxLayout* passwordLayout = new QHBoxLayout(passwordContainer);
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->setSpacing(0);

    // Password input field
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
        // "    background-color: #2E2E2E;"
        "    border-right: none;"
        "}"
    );

    // Add Enter key handling, pressing Enter is equivalent to clicking login button
    connect(passwordEdit, &QLineEdit::returnPressed, this, &OneSevenLiveLoginDialog::handleLogin);
    
    // Show/hide password button
    showPasswordButton = new QPushButton(passwordContainer);

    // Set initial icon to show password icon
    QIcon showIcon(":/resources/show-password.svg");
    showPasswordButton->setIcon(showIcon);
    showPasswordButton->setIconSize(QSize(20, 20));
    showPasswordButton->setFixedSize(40, 40);
    showPasswordButton->setStyleSheet(
        "QPushButton {"
        // "    border: none;"
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
    showPasswordButton->setFlat(true); // Remove button border, closely connected to passwordEdit

    passwordLayout->addWidget(passwordEdit);
    passwordLayout->addWidget(showPasswordButton);
    // passwordLayout->setAlignment(Qt::AlignVCenter);
    passwordLayout->setAlignment(Qt::AlignTop);

    mainLayout->addWidget(passwordContainer);
    
    // Connect button click event
    connect(showPasswordButton, &QPushButton::clicked, this, [this]() {
        if (passwordEdit->echoMode() == QLineEdit::Password) {
            passwordEdit->setEchoMode(QLineEdit::Normal);
            // Switch to hide password icon
            QIcon hideIcon(":/resources/hide-password.svg");
            showPasswordButton->setIcon(hideIcon);
        } else {
            passwordEdit->setEchoMode(QLineEdit::Password);
            // Switch to show password icon
            QIcon showIcon(":/resources/show-password.svg");
            showPasswordButton->setIcon(showIcon);
        }
    });

    // Error message
    errorLabel = new QLabel(this);
    errorLabel->setText(obs_module_text("Auth.Error01"));
    errorLabel->setStyleSheet(
        "QLabel {"
        "    color: #FF0001;"
        "    font-size: 14px;"
        "    margin: 10px 0;"
        "    min-height: 20px;"
        "}"
    );
    errorLabel->setVisible(false);
    // Modified: allow label to auto-adjust height while maintaining fixed minimum height
    errorLabel->setMinimumHeight(20);
    // Modified: allow label to expand vertically when needed
    errorLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    // Modified: enable word wrapping
    errorLabel->setWordWrap(true);
    mainLayout->addWidget(errorLabel);

    QWidget *loginContainer = new QWidget(this);
    QHBoxLayout *loginLayout = new QHBoxLayout(loginContainer);
    loginLayout->setContentsMargins(0, 0, 0, 0);

    // Login button
    loginButton = new QPushButton(obs_module_text("Auth.SignIn"), this);
    loginButton->setMinimumHeight(40);
    loginButton->setMinimumWidth(150);
    loginButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #FF0001;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 2px;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "}"
    );
    connect(loginButton, &QPushButton::clicked, this, &OneSevenLiveLoginDialog::handleLogin);

    QVBoxLayout *loginLeftLayout = new QVBoxLayout();
    loginLeftLayout->setContentsMargins(0, 0, 0, 0);

    // Register new user link
    registerLabel = new QLabel(obs_module_text("Auth.Register"), this);
    registerLabel->setAlignment(Qt::AlignLeft);
    registerLabel->setOpenExternalLinks(true);
    registerLabel->setStyleSheet(
        "QLabel {"
        "    margin: 10px 0;"
        "}"
    );
    
    // More login help
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

    // Disclaimer
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

void OneSevenLiveLoginDialog::handleLogin()
{
    obs_log(LOG_INFO, "OneSevenLiveLoginDialog::handle login");
    
    // Validation logic
    if (usernameEdit->text().isEmpty() || passwordEdit->text().isEmpty()) {
        errorLabel->setVisible(true);
        adjustSize(); // resize dialog to fit error message
        return;
    }
    
    // Create API wrapper instance
    OneSevenLiveLoginData loginData;
    
    // Call login interface
    if (!apiWrapper->Login(usernameEdit->text(), passwordEdit->text(), loginData)) {
        // QString errorMessageTemplate = obs_module_text("Auth.Error02");
        // QString errorMessage = errorMessageTemplate.arg(apiWrapper.getLastErrorMessage());
        // errorLabel->setText(errorMessage);
        errorLabel->setVisible(true);
        adjustSize(); // resize dialog to fit error message
        return;
    }

    // obs_log(LOG_INFO, "login success");
    // obs_log(LOG_INFO, "userID: %s", loginData.userInfo.userID.toStdString().c_str());
    // obs_log(LOG_INFO, "displayName: %s", loginData.userInfo.displayName.toStdString().c_str());
    // obs_log(LOG_INFO, "roomID: %d", loginData.userInfo.roomID);

    emit loginSuccess(loginData);

    // log access token
    // obs_log(LOG_INFO, "access token: %s", loginData.accessToken.toStdString().c_str());
    
    // Show login success message box
    QMessageBox::information(this, obs_module_text("Auth.LoginSuccess"), QString(obs_module_text("Auth.LoginSuccess.Tip")).arg(loginData.userInfo.openID));
    
    // Login successful
    accept();
}

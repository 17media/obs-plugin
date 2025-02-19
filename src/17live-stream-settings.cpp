#include "17live-stream-settings.hpp"
#include <QFormLayout>
#include <QMessageBox>
#include <QSettings>
#include "17live-api.hpp"

SeventeenLiveStreamSettings::SeventeenLiveStreamSettings(QWidget *parent)
    : QDialog(parent)
    , isAuthenticated(false)
{
    setupUi();
    loadSettings();
}

void SeventeenLiveStreamSettings::Register()
{
    obs_frontend_add_tools_menu_item(
        "17LIVE Settings",
        []() {
            auto settings = new SeventeenLiveStreamSettings();
            settings->setAttribute(Qt::WA_DeleteOnClose);
            settings->show();
        });
}

void SeventeenLiveStreamSettings::setupUi()
{
    setWindowTitle("17LIVE Stream Settings");
    setMinimumWidth(400);

    auto layout = new QFormLayout(this);

    // Authentication section
    usernameEdit = new QLineEdit(this);
    passwordEdit = new QLineEdit(this);
    passwordEdit->setEchoMode(QLineEdit::Password);
    loginButton = new QPushButton("Login", this);

    layout->addRow("Username:", usernameEdit);
    layout->addRow("Password:", passwordEdit);
    layout->addRow("", loginButton);

    // Stream settings section
    titleEdit = new QLineEdit(this);
    hashtagsEdit = new QLineEdit(this);
    streamKeyEdit = new QLineEdit(this);
    streamKeyEdit->setReadOnly(true);
    generateKeyButton = new QPushButton("Generate Stream Key", this);
    saveButton = new QPushButton("Save Settings", this);

    layout->addRow("Stream Title:", titleEdit);
    layout->addRow("Hashtags:", hashtagsEdit);
    layout->addRow("Stream Key:", streamKeyEdit);
    layout->addRow("", generateKeyButton);

    // Add browser source creation button
    auto createBrowserSourceButton = new QPushButton("Create Chat Browser Source", this);
    layout->addRow("", createBrowserSourceButton);
    layout->addRow("", saveButton);

    // Connect signals
    connect(loginButton, &QPushButton::clicked, this, &SeventeenLiveStreamSettings::onLoginClicked);
    connect(saveButton, &QPushButton::clicked, this, &SeventeenLiveStreamSettings::onSaveSettings);
    connect(generateKeyButton, &QPushButton::clicked, this, &SeventeenLiveStreamSettings::updateStreamKey);
    connect(createBrowserSourceButton, &QPushButton::clicked, this, &SeventeenLiveStreamSettings::onCreateBrowserSourceClicked);

    // Initially disable stream settings until authenticated
    titleEdit->setEnabled(false);
    hashtagsEdit->setEnabled(false);
    streamKeyEdit->setEnabled(false);
    generateKeyButton->setEnabled(false);
    saveButton->setEnabled(false);
}

void SeventeenLiveStreamSettings::loadSettings()
{
    QSettings settings("obs-17live", "settings");
    usernameEdit->setText(settings.value("username").toString());
    titleEdit->setText(settings.value("title").toString());
    hashtagsEdit->setText(settings.value("hashtags").toString());
    streamKeyEdit->setText(settings.value("streamKey").toString());
    authToken = settings.value("authToken").toString();

    if (!authToken.isEmpty()) {
        isAuthenticated = true;
        titleEdit->setEnabled(true);
        hashtagsEdit->setEnabled(true);
        streamKeyEdit->setEnabled(true);
        generateKeyButton->setEnabled(true);
        saveButton->setEnabled(true);
        loginButton->setText("Logout");
    }
}

void SeventeenLiveStreamSettings::saveSettings()
{
    QSettings settings("obs-17live", "settings");
    settings.setValue("username", usernameEdit->text());
    settings.setValue("title", titleEdit->text());
    settings.setValue("hashtags", hashtagsEdit->text());
    settings.setValue("streamKey", streamKeyEdit->text());
    settings.setValue("authToken", authToken);
}

void SeventeenLiveStreamSettings::onLoginClicked()
{
    if (isAuthenticated) {
        // Logout
        isAuthenticated = false;
        authToken.clear();
        loginButton->setText("Login");
        titleEdit->setEnabled(false);
        hashtagsEdit->setEnabled(false);
        streamKeyEdit->setEnabled(false);
        generateKeyButton->setEnabled(false);
        saveButton->setEnabled(false);
        saveSettings();
        return;
    }

    // Login
    QString username = usernameEdit->text();
    QString password = passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter both username and password.");
        return;
    }

    // TODO: Implement actual authentication using 17live-api
    // For now, simulate successful authentication
    isAuthenticated = true;
    authToken = "dummy_token";
    loginButton->setText("Logout");
    titleEdit->setEnabled(true);
    hashtagsEdit->setEnabled(true);
    streamKeyEdit->setEnabled(true);
    generateKeyButton->setEnabled(true);
    saveButton->setEnabled(true);
    saveSettings();

    QMessageBox::information(this, "Success", "Successfully logged in to 17LIVE!");
}

void SeventeenLiveStreamSettings::onSaveSettings()
{
    if (!isAuthenticated) {
        QMessageBox::warning(this, "Error", "Please login first.");
        return;
    }

    saveSettings();
    QMessageBox::information(this, "Success", "Settings saved successfully!");
}

void SeventeenLiveStreamSettings::onCreateBrowserSourceClicked()
{
    obs_source_t *source = obs_source_create("browser_source", "17LIVE Chat", nullptr, nullptr);
    if (!source) {
        QMessageBox::warning(this, "Error", "Failed to create browser source.");
        return;
    }

    obs_data_t *settings = obs_data_create();
    QString chatPath = QString("file://") + obs_get_module_data_path(obs_get_module()) + "/chat/index.html";
    obs_data_set_string(settings, "url", chatPath.toUtf8().constData());
    obs_data_set_bool(settings, "is_local_file", true);
    obs_data_set_int(settings, "width", 300);
    obs_data_set_int(settings, "height", 600);

    obs_source_update(source, settings);
    obs_scene_t *scene = obs_scene_from_source(obs_frontend_get_current_scene());
    if (scene) {
        obs_sceneitem_t *item = obs_scene_add(scene, source);
        if (item) {
            QMessageBox::information(this, "Success", "Chat browser source created successfully!");
        }
    }

    obs_data_release(settings);
    obs_source_release(source);
}

void SeventeenLiveStreamSettings::updateStreamKey()
{
    if (!isAuthenticated) {
        QMessageBox::warning(this, "Error", "Please login first.");
        return;
    }

    // TODO: Implement actual stream key generation using 17live-api
    // For now, generate a dummy stream key
    QString newKey = QString("dummy_stream_key_%1").arg(QDateTime::currentSecsSinceEpoch());
    streamKeyEdit->setText(newKey);
    saveSettings();

    QMessageBox::information(this, "Success", "New stream key generated successfully!");
}
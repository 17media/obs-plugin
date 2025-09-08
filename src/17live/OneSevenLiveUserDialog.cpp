#include "OneSevenLiveUserDialog.hpp"

#include <QMessageBox>
#include <QPainter>
#include <QPointer>

#include <obs-module.h>

#include "utility/RemoteTextThread.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "plugin-support.h"

OneSevenLiveUserDialog::OneSevenLiveUserDialog(QWidget* parent, 
                                           OneSevenLiveApiWrappers* apiWrapper_,
                                           OneSevenLiveConfigManager* configManager_)
    : QDialog(parent),
      apiWrapper(apiWrapper_),
      configManager(configManager_) {
    setWindowTitle(obs_module_text("Live.UserInfo"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumSize(300, 400);
    setStyleSheet(
        "QDialog {"
        "    background-color: #000000;"
        "    color: #FFFFFF;"
        "    font-family: 'Inter';"
        "    font-style: normal;"
        "}");

    setupUi();
    createConnections();
}

OneSevenLiveUserDialog::~OneSevenLiveUserDialog() = default;

void OneSevenLiveUserDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);
    mainLayout->setAlignment(Qt::AlignCenter);

    // User avatar
    avatarLabel = new QLabel();
    avatarLabel->setFixedSize(120, 120);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #333333;"
        "    border-radius: 60px;"
        "}");
    mainLayout->addWidget(avatarLabel, 0, Qt::AlignHCenter);

    // Username
    usernameLabel = new QLabel();
    usernameLabel->setAlignment(Qt::AlignCenter);
    usernameLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 18px;"
        "}");
    mainLayout->addWidget(usernameLabel);

    // User ID
    userIdLabel = new QLabel();
    userIdLabel->setAlignment(Qt::AlignCenter);
    userIdLabel->setStyleSheet(
        "QLabel {"
        "    color: #d9d9d9;"
        "    font-size: 14px;"
        "}");
    mainLayout->addWidget(userIdLabel);

    // Button area
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    // Poke button
    pokeButton = new QPushButton(obs_module_text("Live.PokeUser"));
    pokeButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #FF0001;"
        "    color: white;"
        "    border-radius: 4px;"
        "    padding: 8px 16px;"
        "    font-weight: 600;"
        "    font-size: 16px;"
        "    line-height: 24px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #D10001;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #B00001;"
        "}");

    // Close button
    closeButton = new QPushButton(obs_module_text("Close"));
    closeButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #333333;"
        "    color: white;"
        "    border-radius: 4px;"
        "    padding: 8px 16px;"
        "    font-weight: 600;"
        "    font-size: 16px;"
        "    line-height: 24px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #444444;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #222222;"
        "}");

    buttonLayout->addWidget(pokeButton);
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch();
}

void OneSevenLiveUserDialog::createConnections() {
    connect(pokeButton, &QPushButton::clicked, this, &OneSevenLiveUserDialog::onPokeUserClicked);
    connect(closeButton, &QPushButton::clicked, this, &OneSevenLiveUserDialog::onCloseClicked);
}

void OneSevenLiveUserDialog::setUserInfo(const OneSevenLiveRockZoneViewer& user) {
    viewer = user;

    // Update UI
    usernameLabel->setText(viewer.displayUser.displayName);
//    userIdLabel->setText(QString(obs_module_text("Live.UserInfo.ID")).arg(viewer.displayUser.openID));
    updateUserAvatar();
}

void OneSevenLiveUserDialog::updateUserAvatar() {

    QString url = "https://cdn.17app.co/" + viewer.displayUser.picture;
    RemoteTextThread *thread = new RemoteTextThread(url.toStdString(), "image/png", "", 0, true);
        
    QPointer<QLabel> safeAvatarLabel = avatarLabel;
    connect(thread, &RemoteTextThread::ImageResult, this, [this, safeAvatarLabel](const QByteArray &imageData, const QString &error) {
        if (error.isEmpty() && !imageData.isEmpty()) {
            QPixmap avatar;
            avatar.loadFromData(imageData);
            avatar = avatar.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
            // Create rounded avatar
            QPixmap roundedAvatar(120, 120);
            roundedAvatar.fill(Qt::transparent);
        
            QPainter painter(&roundedAvatar);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(avatar));
            painter.drawEllipse(0, 0, 120, 120);
                
            if (safeAvatarLabel) {
                safeAvatarLabel->setPixmap(roundedAvatar);
            }
        }
    });
    
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    
    thread->start();
}

void OneSevenLiveUserDialog::onPokeUserClicked() {
    if (!apiWrapper) {
        return;
    }
    std::string roomID;
    configManager->getConfigValue("RoomID", roomID);

    // Create poke request
    OneSevenLivePokeRequest request;
    OneSevenLivePokeResponse response;
    request.userID = viewer.displayUser.userID;
    request.srcID = QString::fromStdString(roomID);
    request.isPokeBack = false;

    // Send request
    bool success = apiWrapper->PokeOne(request, response);

    if (success) {
        QMessageBox::information(this, 
                               obs_module_text("Live.PokeSuccess"), 
                               obs_module_text("Live.PokeSuccessMessage"));
    } else {
        QMessageBox::warning(this, 
                           obs_module_text("Live.PokeError"), 
                           obs_module_text("Live.PokeErrorMessage"));
    }
}

void OneSevenLiveUserDialog::onCloseClicked() {
    close();
}

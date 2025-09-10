#include "OneSevenLiveUserDialog.hpp"

#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QIcon>
#include <QPixmap>

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
    setWindowTitle(QString());
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedSize(250, 300);
    setStyleSheet("QDialog { background-color: transparent; }");

    setupUi();
    createConnections();
 }

 OneSevenLiveUserDialog::~OneSevenLiveUserDialog() = default;

void OneSevenLiveUserDialog::setupUi() {
    // Root layout (no margins/spacings to fit 250x300 exactly)
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Card container with rounded corners and background color
    QWidget* card = new QWidget(this);
    card->setObjectName("card");
    card->setStyleSheet("#card { background-color: #3C404C; border-radius: 2px; }");
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    // Top background area (128px)
    // QWidget* topBg = new QWidget(card);
    // topBg->setObjectName("topBg");
    // topBg->setFixedHeight(128);
    // topBg->setStyleSheet("#topBg { background-color: #3C404C; background-image: url(:/resources/user_images/user_bg.png); background-position: center; background-repeat: no-repeat; border-top-left-radius: 2px; border-top-right-radius: 2px; }");
    // QHBoxLayout* topBgLayout = new QHBoxLayout(topBg);
    // topBgLayout->setContentsMargins(0, 0, 6, 0);
    // topBgLayout->addStretch();

    // Close icon button at top-right
    // closeButton = new QPushButton(topBg);
    closeButton = new QPushButton(card);
    closeButton->setFlat(true);
    closeButton->setIcon(QIcon(":/resources/close.svg"));
    closeButton->setIconSize(QSize(20, 20));
    closeButton->setFixedSize(30, 30);
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setStyleSheet("QPushButton { background: transparent; border: none; } QPushButton:hover { background: rgba(255,255,255,0.08); border-radius: 4px; }");
    // topBgLayout->addWidget(closeButton, 0, Qt::AlignTop | Qt::AlignRight);
    cardLayout->addWidget(closeButton, 0, Qt::AlignTop | Qt::AlignRight);

    // cardLayout->addWidget(topBg);

    // Body area (overlap into top area so avatar is ~20px from dialog top)
    QWidget* body = new QWidget(card);
    QVBoxLayout* bodyLayout = new QVBoxLayout(body);
    // Move body content upward so avatar top sits ~20px from dialog top
    // const int topBgHeight = 128; // must match topBg->setFixedHeight(128)
    // const int desiredTopFromDialog = 20;
    // const int overlapIntoTop = topBgHeight - desiredTopFromDialog; // 128 - 20 = 108
    // bodyLayout->setContentsMargins(20, -overlapIntoTop, 20, 20);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0); // control exact gaps manually

    // User avatar (120x120, ~20px from top)
    avatarLabel = new QLabel();
    avatarLabel->setFixedSize(120, 120);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setStyleSheet("QLabel { background-color: transparent; border-radius: 60px; }");
    bodyLayout->addWidget(avatarLabel, 0, Qt::AlignHCenter);

    // Ensure ~10px spacing between avatar and username
    bodyLayout->addSpacing(10);

    // Username (10px below avatar by explicit spacing)
    usernameLabel = new QLabel();
    usernameLabel->setAlignment(Qt::AlignCenter);
    usernameLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 18px;"
        "}");
    bodyLayout->addWidget(usernameLabel, 0, Qt::AlignHCenter);

    // Optional: User ID (kept from original, centered)
    userIdLabel = new QLabel();
    userIdLabel->setAlignment(Qt::AlignCenter);
    userIdLabel->setStyleSheet("QLabel { color: #d9d9d9; font-size: 14px; }");
    bodyLayout->addWidget(userIdLabel);

    // Button area - center the poke button
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    buttonLayout->setAlignment(Qt::AlignHCenter);

    // Poke button (centered)
    pokeButton = new QPushButton(obs_module_text("RockZone.PokeUser"));
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
    buttonLayout->addWidget(pokeButton);
    bodyLayout->addLayout(buttonLayout);
    bodyLayout->addStretch();

    cardLayout->addWidget(body);
    mainLayout->addWidget(card);
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

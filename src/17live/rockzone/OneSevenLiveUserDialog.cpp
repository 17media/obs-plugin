#include "OneSevenLiveUserDialog.hpp"

#include <obs-module.h>

#include <QIcon>
#include <QHash>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QThread>

#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "plugin-support.h"
#include "rockzone/OneSevenLiveUserMemoDialog.hpp"
#include "utility/Common.hpp"
#include "utility/RemoteTextThread.hpp"

namespace {
constexpr int kAvatarSizePx = 120;
constexpr int kAvatarRequestTimeoutSec = 8;
constexpr int kAvatarConnectTimeoutSec = 3;

QPixmap MakeRoundedAvatar(const QPixmap& source) {
    if (source.isNull()) {
        return QPixmap();
    }

    QPixmap scaled = source.scaled(kAvatarSizePx, kAvatarSizePx, Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation);
    QPixmap rounded(kAvatarSizePx, kAvatarSizePx);
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(scaled));
    painter.drawEllipse(0, 0, kAvatarSizePx, kAvatarSizePx);
    return rounded;
}

QPixmap DefaultAvatarPixmap() {
    static QPixmap avatar = MakeRoundedAvatar(QPixmap(":/resources/avatar.png"));
    return avatar;
}

QMutex& AvatarCacheMutex() {
    static QMutex mutex;
    return mutex;
}

QHash<QString, QPixmap>& AvatarPixmapCache() {
    static QHash<QString, QPixmap> cache;
    return cache;
}
}  // namespace

OneSevenLiveUserDialog::OneSevenLiveUserDialog(QWidget* parent,
                                               OneSevenLiveApiWrappers* apiWrapper_,
                                               OneSevenLiveConfigManager* configManager_)
    : QDialog(parent), apiWrapper(apiWrapper_), configManager(configManager_) {
    setWindowTitle(QString());
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedSize(250, 320);
    setStyleSheet("QDialog { background-color: transparent; }");
    setModal(false);

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

    closeButton = new QPushButton(card);
    closeButton->setFlat(true);
    closeButton->setIcon(QIcon(":/resources/close.svg"));
    closeButton->setIconSize(QSize(20, 20));
    closeButton->setFixedSize(30, 30);
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setStyleSheet(
        "QPushButton { background: transparent; border: none; } QPushButton:hover { background: "
        "rgba(255,255,255,0.08); border-radius: 4px; }");
    cardLayout->addWidget(closeButton, 0, Qt::AlignTop | Qt::AlignRight);

    // Body area (overlap into top area so avatar is ~20px from dialog top)
    QWidget* body = new QWidget(card);
    QVBoxLayout* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);  // control exact gaps manually

    avatarLabel = new QLabel();
    avatarLabel->setFixedSize(120, 120);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setStyleSheet("QLabel { background-color: transparent; border-radius: 60px; }");
    avatarLabel->setPixmap(DefaultAvatarPixmap());
    bodyLayout->addWidget(avatarLabel, 0, Qt::AlignHCenter);

    // Ensure ~10px spacing between avatar and username
    bodyLayout->addSpacing(10);

    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->setContentsMargins(0, 0, 0, 0);
    nameLayout->setSpacing(6);
    nameLayout->setAlignment(Qt::AlignHCenter);

    usernameLabel = new QLabel();
    usernameLabel->setAlignment(Qt::AlignCenter);
    usernameLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 18px;"
        "}");
    nameLayout->addWidget(usernameLabel);

    userMemoButton = new QPushButton();
    userMemoButton->setFlat(true);
    userMemoButton->setIcon(QIcon(":/resources/memo-edit.svg"));
    userMemoButton->setIconSize(QSize(20, 20));
    userMemoButton->setFixedSize(24, 24);
    userMemoButton->setCursor(Qt::PointingHandCursor);
    userMemoButton->setToolTip(obs_module_text("RockZone.UserMemo.Tooltip"));
    userMemoButton->setStyleSheet(
        "QPushButton { background: transparent; border: none; }"
        "QPushButton:hover { background: rgba(255,255,255,0.08); border-radius: 4px; }");
    nameLayout->addWidget(userMemoButton);

    bodyLayout->addLayout(nameLayout);

    // User stats area
    bodyLayout->addSpacing(15);

    QHBoxLayout* statsLayout = new QHBoxLayout();
    statsLayout->setSpacing(20);
    statsLayout->setAlignment(Qt::AlignHCenter);

    // Followers
    QVBoxLayout* followersLayout = new QVBoxLayout();
    followersLayout->setSpacing(2);
    followersLayout->setAlignment(Qt::AlignCenter);

    followersLabel = new QLabel("--");
    followersLabel->setAlignment(Qt::AlignCenter);
    followersLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 16px;"
        "}");

    QLabel* followersText = new QLabel(obs_module_text("RockZone.Followers"));
    followersText->setAlignment(Qt::AlignCenter);
    followersText->setStyleSheet(
        "QLabel {"
        "    color: #CCCCCC;"
        "    font-size: 12px;"
        "}");

    followersLayout->addWidget(followersLabel);
    followersLayout->addWidget(followersText);

    // Following
    QVBoxLayout* followingLayout = new QVBoxLayout();
    followingLayout->setSpacing(2);
    followingLayout->setAlignment(Qt::AlignCenter);

    followingLabel = new QLabel("--");
    followingLabel->setAlignment(Qt::AlignCenter);
    followingLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 16px;"
        "}");

    QLabel* followingText = new QLabel(obs_module_text("RockZone.Following"));
    followingText->setAlignment(Qt::AlignCenter);
    followingText->setStyleSheet(
        "QLabel {"
        "    color: #CCCCCC;"
        "    font-size: 12px;"
        "}");

    followingLayout->addWidget(followingLabel);
    followingLayout->addWidget(followingText);

    // Likes
    QVBoxLayout* likesLayout = new QVBoxLayout();
    likesLayout->setSpacing(2);
    likesLayout->setAlignment(Qt::AlignCenter);

    likesLabel = new QLabel("--");
    likesLabel->setAlignment(Qt::AlignCenter);
    likesLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 16px;"
        "}");

    QLabel* likesText = new QLabel(obs_module_text("RockZone.Likes"));
    likesText->setAlignment(Qt::AlignCenter);
    likesText->setStyleSheet(
        "QLabel {"
        "    color: #CCCCCC;"
        "    font-size: 12px;"
        "}");

    likesLayout->addWidget(likesLabel);
    likesLayout->addWidget(likesText);

    // Add to stats layout
    statsLayout->addLayout(followersLayout);
    statsLayout->addLayout(followingLayout);
    statsLayout->addLayout(likesLayout);

    bodyLayout->addLayout(statsLayout);
    bodyLayout->addSpacing(15);

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
    pokeButton->setFixedHeight(30);
    pokeButton->setFixedWidth(200);
    buttonLayout->addWidget(pokeButton);
    bodyLayout->addLayout(buttonLayout);
    bodyLayout->addStretch();

    cardLayout->addWidget(body);
    mainLayout->addWidget(card);
}

void OneSevenLiveUserDialog::createConnections() {
    connect(pokeButton, &QPushButton::clicked, this, &OneSevenLiveUserDialog::onPokeUserClicked);
    connect(closeButton, &QPushButton::clicked, this, &OneSevenLiveUserDialog::onCloseClicked);
    connect(userMemoButton, &QPushButton::clicked, this, &OneSevenLiveUserDialog::onUserMemoClicked);
}

void OneSevenLiveUserDialog::setUserInfo(const OneSevenLiveRockZoneViewer& user) {
    viewer = user;

    // Update UI
    usernameLabel->setText(viewer.displayUser.displayName);
    updateUserAvatar();

    // Fetch detailed user information asynchronously
    fetchUserInfo();
}

void OneSevenLiveUserDialog::updateUserAvatar() {
    currentAvatarUrl.clear();
    avatarLabel->setPixmap(DefaultAvatarPixmap());

    const QString picturePath = viewer.displayUser.picture.trimmed();
    if (picturePath.isEmpty()) {
        return;
    }

    const QString url = "https://cdn.17app.co/" + picturePath;
    currentAvatarUrl = url;

    {
        QMutexLocker locker(&AvatarCacheMutex());
        const auto it = AvatarPixmapCache().constFind(url);
        if (it != AvatarPixmapCache().constEnd()) {
            avatarLabel->setPixmap(it.value());
            return;
        }
    }

    std::vector<std::string> headers;
    headers.emplace_back("Accept: image/*");
    RemoteTextThread* thread =
        new RemoteTextThread(url.toStdString(), std::move(headers), "", "", kAvatarRequestTimeoutSec,
                             true, kAvatarConnectTimeoutSec);

    QPointer<QLabel> safeAvatarLabel = avatarLabel;
    QPointer<OneSevenLiveUserDialog> safeThis = this;
    connect(thread, &RemoteTextThread::ImageResult, this,
            [safeThis, safeAvatarLabel, url](const QByteArray& imageData, const QString& error) {
                if (!safeThis || !safeAvatarLabel) {
                    return;
                }

                if (safeThis->currentAvatarUrl != url) {
                    return;
                }

                if (!error.isEmpty()) {
                    obs_log(LOG_WARNING, "Failed to load avatar: %s [url: %s]",
                            error.toStdString().c_str(), url.toStdString().c_str());
                    safeAvatarLabel->setPixmap(DefaultAvatarPixmap());
                    return;
                }

                QPixmap avatar;
                if (!avatar.loadFromData(imageData)) {
                    obs_log(LOG_WARNING, "Failed to decode avatar image data [url: %s]",
                            url.toStdString().c_str());
                    safeAvatarLabel->setPixmap(DefaultAvatarPixmap());
                    return;
                }

                const QPixmap roundedAvatar = MakeRoundedAvatar(avatar);
                if (roundedAvatar.isNull()) {
                    safeAvatarLabel->setPixmap(DefaultAvatarPixmap());
                    return;
                }

                {
                    QMutexLocker locker(&AvatarCacheMutex());
                    AvatarPixmapCache().insert(url, roundedAvatar);
                }

                safeAvatarLabel->setPixmap(roundedAvatar);
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

    if (!success) {
        obs_log(LOG_ERROR, "Failed to poke user %s",
                apiWrapper->getLastErrorMessage().toStdString().c_str());
    }
}

void OneSevenLiveUserDialog::onCloseClicked() {
    close();
}

void OneSevenLiveUserDialog::onUserMemoClicked() {
    if (!apiWrapper || viewer.displayUser.userID.isEmpty()) {
        return;
    }

    OneSevenLiveUserMemoDialog dialog(this, apiWrapper, viewer.displayUser.userID);
    dialog.exec();
}

void OneSevenLiveUserDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        dragStartPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void OneSevenLiveUserDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - dragStartPosition);
        event->accept();
    }
}

void OneSevenLiveUserDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        event->accept();
    }
}

void OneSevenLiveUserDialog::fetchUserInfo() {
    if (!apiWrapper || viewer.displayUser.userID.isEmpty()) {
        return;
    }

    // Create a worker thread to fetch user info
    QThread* workerThread = new QThread();

    // Get user ID and region from config
    std::string region;
    std::string language = GetCurrentLanguage();
    configManager->getConfigValue("Region", region);

    if (region.empty()) {
        region = "TW";  // Default region
    }

    QString userID = viewer.displayUser.userID;

    // Use QPointer to safely access this object
    QPointer<OneSevenLiveUserDialog> safeThis = this;

    // Connect worker thread to perform API call
    connect(workerThread, &QThread::started, [=]() {
        OneSevenLiveUserInfo userInfo;
        bool success = apiWrapper->GetUserInfo(userID.toStdString(), region, language, userInfo);

        // Post result back to main thread
        QMetaObject::invokeMethod(
            safeThis,
            [=]() {
                if (safeThis && success) {
                    safeThis->updateUserStats(userInfo);
                }
            },
            Qt::QueuedConnection);

        // Clean up thread
        workerThread->quit();
    });

    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    workerThread->start();
}

void OneSevenLiveUserDialog::updateUserStats(const OneSevenLiveUserInfo& userInfo) {
    // Format numbers with K/M suffixes for large values
    auto formatNumber = [](int number) -> QString {
        if (number >= 1000000) {
            return QString("%1m").arg(number / 1000000.0, 0, 'f', 1);
        } else if (number >= 1000) {
            return QString("%1k").arg(number / 1000.0, 0, 'f', 1);
        } else {
            return QString::number(number);
        }
    };

    // Update UI labels with formatted numbers
    followersLabel->setText(formatNumber(userInfo.followerCount));
    followingLabel->setText(formatNumber(userInfo.followingCount));
    likesLabel->setText(formatNumber(userInfo.likeCount));
}

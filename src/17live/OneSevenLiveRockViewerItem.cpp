#include "OneSevenLiveRockViewerItem.hpp"

#include <QMouseEvent>

#include <obs-module.h>

#include "utility/RemoteTextThread.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveUtility.hpp"

#include "moc_OneSevenLiveRockViewerItem.cpp"
OneSevenLiveRockViewerItem::OneSevenLiveRockViewerItem(const OneSevenLiveRockZoneViewer &u,
                                                       OneSevenLiveApiWrappers *apiWrapper_,
                                                       OneSevenLiveConfigManager *configManager_,
                                                       QWidget *parent)
    : QWidget(parent), user(u), apiWrapper(apiWrapper_), configManager(configManager_) {
    setupUi();
}

QSize OneSevenLiveRockViewerItem::sizeHint() const { return QSize(-1, 74); }

QString OneSevenLiveRockViewerItem::buildUrl(const QString &path) {
    if (path.isEmpty()) return QString();
    if (path.startsWith("http://") || path.startsWith("https://")) return path;
    return QString("https://cdn.17app.co/") + path;
}

void OneSevenLiveRockViewerItem::setupUi() {
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(10);

    // Make the whole item look clickable
    setCursor(Qt::PointingHandCursor);

    // Left: Avatar with overlay frame
    QLabel *avatarLabel = new QLabel(this);
    avatarLabel->setFixedSize(40, 40);
    avatarLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #333333;"
        "    border-radius: 20px;"
        "}");
    // Forward clicks to parent widget so any click inside the item triggers
    avatarLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // Keep pixmaps across async loads
    auto avatarReady = QSharedPointer<bool>::create(false);
    auto frameReady = QSharedPointer<bool>::create(false);
    auto roundedAvatar = QSharedPointer<QPixmap>::create();
    auto framePixmap = QSharedPointer<QPixmap>::create();

    QPointer<QLabel> safeAvatarLabel = avatarLabel;

    // Compose function: draw frame over avatar if available
    auto composeAndSet = [safeAvatarLabel, avatarReady, frameReady, roundedAvatar, framePixmap]() {
        if (!safeAvatarLabel)
            return;
        if (!(*avatarReady))
            return; // need avatar first

        if (*frameReady && !framePixmap->isNull()) {
            QPixmap result(roundedAvatar->size());
            result.fill(Qt::transparent);
            QPainter painter(&result);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.drawPixmap(0, 0, *roundedAvatar);
            painter.drawPixmap(0, 0, *framePixmap);
            painter.end();
            safeAvatarLabel->setPixmap(result);
        } else {
            safeAvatarLabel->setPixmap(*roundedAvatar);
        }
    };

    // Load avatar image
    {
        const QString avatarUrl = buildUrl(user.displayUser.picture);
        RemoteTextThread *thread = new RemoteTextThread(avatarUrl.toStdString(), "image/png", "", 0, true);
        connect(thread, &RemoteTextThread::ImageResult, this, [avatarReady, roundedAvatar, composeAndSet](const QByteArray &imageData, const QString &error) {
            if (error.isEmpty() && !imageData.isEmpty()) {
                QPixmap avatar;
                avatar.loadFromData(imageData);
                avatar = avatar.scaled(40, 40, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

                // Make circle
                QPixmap rounded(40, 40);
                rounded.fill(Qt::transparent);
                QPainter painter(&rounded);
                painter.setRenderHint(QPainter::Antialiasing);
                QPainterPath path;
                path.addEllipse(0, 0, 40, 40);
                painter.setClipPath(path);
                painter.drawPixmap(0, 0, avatar);
                painter.end();

                *roundedAvatar = rounded;
                *avatarReady = true;
                composeAndSet();
            }
        });
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
    }

    {
        const QString frameRes = OneSevenLiveUtility::avatarFrameResource(user);
        if (!frameRes.isEmpty()) {
            QPixmap overlay;
            overlay.load(frameRes);
            if (!overlay.isNull()) {
                overlay = overlay.scaled(40, 40, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                *framePixmap = overlay;
                *frameReady = true;
                composeAndSet();
            }
        }
    }

    mainLayout->addWidget(avatarLabel);

    // Right side: 3 vertical sections
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);

    // 1) Username + Level badge
    {
        QHBoxLayout *nameRow = new QHBoxLayout();
        nameRow->setContentsMargins(0, 0, 0, 0);
        nameRow->setSpacing(6);

        QLabel *usernameLabel = new QLabel(user.displayUser.displayName, this);
        usernameLabel->setStyleSheet(
            "QLabel {"
            "    color: #FFFFFF;"
            "    font-weight: bold;"
            "    font-size: 14px;"
            "}");
        usernameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        usernameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        QLabel *levelLabel = new QLabel(QString("Lv %1").arg(user.displayUser.level), this);
        levelLabel->setStyleSheet(
            "QLabel {"
            "    color: #000000;"
            "    background-color: #FFD54F;"
            "    border-radius: 6px;"
            "    padding: 1px 6px;"
            "    font-size: 11px;"
            "    font-weight: 600;"
            "}");
        levelLabel->setFixedHeight(16);
        levelLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        nameRow->addWidget(usernameLabel, 1);
        nameRow->addWidget(levelLabel, 0, Qt::AlignVCenter);
        rightLayout->addLayout(nameRow);
    }

    // 2) Medals list
    {
        QHBoxLayout *medalsRow = new QHBoxLayout();
        medalsRow->setContentsMargins(0, 0, 0, 0);
        medalsRow->setSpacing(6);
        auto addMedalResource = [&](const QString &resPath) {
            if (resPath.isEmpty()) return;
            QLabel *icon = new QLabel(this);
            icon->setFixedSize(18, 18);
            icon->setStyleSheet("QLabel { background-color: transparent; }");
            icon->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            QPixmap pm;
            pm.load(resPath);
            if (!pm.isNull()) {
                pm = pm.scaled(icon->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                icon->setPixmap(pm);
            }
            medalsRow->addWidget(icon);
        };

        // Use local resource images to unify style and reduce network requests
        addMedalResource(OneSevenLiveUtility::mLevelBadgeResource(user));
        addMedalResource(OneSevenLiveUtility::checkingLevelBadgeResource(user));

        rightLayout->addLayout(medalsRow);
    }

    // 3) Invested points
    {
        int points = user.armyInfo.pointContribution; // invest points
        QLabel *pointsLabel = new QLabel(QString::number(points), this);
        pointsLabel->setStyleSheet(
            "QLabel {"
            "    color: #D9D9D9;"
            "    font-size: 12px;"
            "}");
        pointsLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        rightLayout->addWidget(pointsLabel);
    }

    mainLayout->addLayout(rightLayout, 1);

    // Set minimum height for better spacing
    setMinimumHeight(74);
    setLayout(mainLayout);
}

void OneSevenLiveRockViewerItem::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(user);
    }
    QWidget::mousePressEvent(event);
}

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

QSize OneSevenLiveRockViewerItem::sizeHint() const { return QSize(350, 80); }

QString OneSevenLiveRockViewerItem::buildUrl(const QString &path) {
    if (path.isEmpty()) return QString();
    if (path.startsWith("http://") || path.startsWith("https://")) return path;
    return QString("https://cdn.17app.co/")  path;
}

void OneSevenLiveRockViewerItem::setupUi() {

    // Root layout centers a fixed-size inner card to achieve visual width=350 while
    // allowing the outer widget to stretch with the QListWidget viewport
    QHBoxLayout *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setAlignment(Qt::AlignCenter);

    QWidget *card = new QWidget(this);
    card->setFixedSize(350, 80);
    QHBoxLayout *mainLayout = new QHBoxLayout(card);
    mainLayout->setContentsMargins(10, 6, 10, 6); // item padding ~10
    mainLayout->setSpacing(10);
    mainLayout->setAlignment(Qt::AlignCenter); // center content

    // Make the whole item look clickable
    setCursor(Qt::PointingHandCursor);

    // Left: Avatar with overlay frame
    QLabel *avatarLabel = new QLabel(this);
    avatarLabel->setFixedSize(65, 67); // avatar area 65x67
    avatarLabel->setStyleSheet("QLabel { background-color: transparent; }");
    // Forward clicks to parent widget so any click inside the item triggers
    avatarLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // Keep pixmaps across async loads
    auto avatarReady = QSharedPointer<bool>::create(false);
    auto frameReady = QSharedPointer<bool>::create(false);
    auto composedPixmap = QSharedPointer<QPixmap>::create(); // final 65x67 canvas
    auto framePixmap = QSharedPointer<QPixmap>::create();

    QPointer<QLabel> safeAvatarLabel = avatarLabel;

    // Compose function: draw frame over avatar if available
    auto composeAndSet = [safeAvatarLabel, avatarReady, frameReady, composedPixmap, framePixmap]() {
        if (!safeAvatarLabel) return;
        if (!(*avatarReady)) return; // need avatar first

        QPixmap canvas = *composedPixmap;
        if (*frameReady && !framePixmap->isNull()) {
            QPainter painter(&canvas);
            painter.setRenderHint(QPainter::Antialiasing);
            // Overlay scaled to full canvas to match visual frame
            painter.drawPixmap(0, 0, framePixmap->scaled(canvas.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            painter.end();
        }
        safeAvatarLabel->setPixmap(canvas);
    };

    // Load avatar image
    {
        const QString avatarUrl = buildUrl(user.displayUser.picture);
        RemoteTextThread *thread = new RemoteTextThread(avatarUrl.toStdString(), "image/png", "", 0, true);
        connect(thread, &RemoteTextThread::ImageResult, this, [avatarReady, composedPixmap, composeAndSet](const QByteArray &imageData, const QString &error) {
            if (error.isEmpty() && !imageData.isEmpty()) {
                // Prepare 65x67 canvas and draw 55x57 image centered with 5px padding
                QPixmap src;
                src.loadFromData(imageData);
                QPixmap scaled = src.scaled(55, 57, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

                QPixmap canvas(65, 67);
                canvas.fill(Qt::transparent);
                QPainter painter(&canvas);
                painter.setRenderHint(QPainter::Antialiasing);
                // top-left at (5,5) to leave 5px padding on all sides
                painter.drawPixmap(5, 5, scaled);
                painter.end();

                *composedPixmap = canvas;
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
                *framePixmap = overlay; // will be scaled during compose
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
    rightLayout->setAlignment(Qt::AlignCenter); // center content in right column

    // 1) Username  Level badge
    {
        QHBoxLayout *nameRow = new QHBoxLayout();
        nameRow->setContentsMargins(0, 0, 0, 0);
        nameRow->setSpacing(6);
        nameRow->setAlignment(Qt::AlignCenter);

        QLabel *usernameLabel = new QLabel(user.displayUser.displayName, this);
        usernameLabel->setStyleSheet(
            "QLabel {"
            "    color: #FFFFFF;"
            "    font-weight: bold;"
            "    font-size: 14px;"
            "}");
        usernameLabel->setAlignment(Qt::AlignCenter);
        usernameLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        usernameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        // Replace level text badge with checking-level background image (if available)
        QString checkingRes = OneSevenLiveUtility::checkingLevelBadgeResource(user);
        QLabel *checkingLabel = nullptr;
        if (!checkingRes.isEmpty()) {
            QPixmap checkingPm;
            if (checkingPm.load(checkingRes)) {
                checkingLabel = new QLabel(this);
                checkingLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                checkingLabel->setStyleSheet("QLabel { background-color: transparent; }");
                int badgeHeight = 16; // match previous visual height
                checkingLabel->setPixmap(checkingPm.scaledToHeight(badgeHeight, Qt::SmoothTransformation));
                checkingLabel->setFixedHeight(badgeHeight);
                checkingLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            }
        }

        nameRow->addWidget(usernameLabel, 0, Qt::AlignCenter);
        if (checkingLabel) {
            nameRow->addWidget(checkingLabel, 0, Qt::AlignVCenter);
        }
        rightLayout->addLayout(nameRow);
    }

    // 2) Medals list
    {
        QHBoxLayout *medalsRow = new QHBoxLayout();
        medalsRow->setContentsMargins(0, 0, 0, 0);
        medalsRow->setSpacing(6);
        medalsRow->setAlignment(Qt::AlignCenter);
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
    // {
    //     int points = user.armyInfo.pointContribution; // invest points
    //     QLabel *pointsLabel = new QLabel(QString::number(points), this);
    //     pointsLabel->setStyleSheet(
    //         "QLabel {"
    //         "    color: #D9D9D9;"
    //         "    font-size: 12px;"
    //         "}");
    //     pointsLabel->setAlignment(Qt::AlignCenter);
    //     pointsLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    //     rightLayout->addWidget(pointsLabel, 0, Qt::AlignCenter);
    // }

    mainLayout->addLayout(rightLayout, 1);

-    // Fix item size to avoid overlap and enforce design width/height
-    setFixedSize(350, 80);
-    setLayout(mainLayout);
-    // Mount card to root centered layout
-    rootLayout->addWidget(card, 0, Qt::AlignCenter);
-    setLayout(rootLayout);
    // Mount card to root centered layout
    rootLayout->addWidget(card, 0, Qt::AlignCenter);
    setLayout(rootLayout);
}

void OneSevenLiveRockViewerItem::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(user);
    }
    QWidget::mousePressEvent(event);
}

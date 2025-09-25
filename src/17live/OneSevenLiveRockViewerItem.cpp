#include "OneSevenLiveRockViewerItem.hpp"

#include <obs-module.h>

#include <QColor>
#include <QEvent>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "api/OneSevenLiveUtility.hpp"
#include "moc_OneSevenLiveRockViewerItem.cpp"
#include "utility/RemoteTextThread.hpp"

OneSevenLiveRockViewerItem::OneSevenLiveRockViewerItem(
    const OneSevenLiveRockZoneViewer &u, OneSevenLiveApiWrappers *apiWrapper_,
    OneSevenLiveConfigManager *configManager_,
    const OneSevenLiveArmyNameResponse &armyNameResponse_, QWidget *parent)
    : QWidget(parent),
      user(u),
      apiWrapper(apiWrapper_),
      configManager(configManager_),
      armyNameResponse(armyNameResponse_) {
    setupUi();
}

QSize OneSevenLiveRockViewerItem::sizeHint() const {
    return QSize(350, 80);
}

QString OneSevenLiveRockViewerItem::buildUrl(const QString &path) {
    if (path.isEmpty())
        return QString();
    if (path.startsWith("http://") || path.startsWith("https://"))
        return path;
    return QString("https://cdn.17app.co/") + path;
}

void OneSevenLiveRockViewerItem::setupUi() {
    // Root layout centers a fixed-size inner card to achieve visual width=300 while
    // allowing the outer widget to stretch with the QListWidget viewport
    QHBoxLayout *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setAlignment(Qt::AlignLeft);

    QWidget *card = new QWidget(this);
    card->setFixedSize(300, 80);
    QHBoxLayout *mainLayout = new QHBoxLayout(card);
    mainLayout->setContentsMargins(0, 0, 0, 0);  // item padding ~10
    mainLayout->setSpacing(5);
    mainLayout->setAlignment(Qt::AlignLeft);

    // Make the whole item look clickable
    setCursor(Qt::PointingHandCursor);

    // Setup avatar area
    QLabel *avatarLabel = setupAvatar();
    mainLayout->addWidget(avatarLabel, 0, Qt::AlignVCenter);

    // Right side: 3 vertical sections
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(0, 5, 0, 5);
    rightLayout->setSpacing(4);  // reduce spacing between components
    rightLayout->setAlignment(Qt::AlignTop);

    // Setup name row
    QHBoxLayout *nameRow = setupNameRow();
    rightLayout->addLayout(nameRow);

    // Setup badge row
    QHBoxLayout *badgeRow = setupBadgeRow();
    if (badgeRow) {
        rightLayout->addLayout(badgeRow);
    }

    // Add stretch to push all components to the top
    // rightLayout->addStretch();

    // 3) Invested points
    // {
    //     int points = user.armyInfo.pointContribution; // invest points
    //     QLabel *pointsLabel = new QLabel(QString::number(points), this);
    //     pointsLabel->setStyleSheet(
    //         "QLabel {"
    //         "    color: #D9D9D9;"
    //         "    font-size: 12px;"
    //         "}");
    //     pointsLabel->setAlignment(Qt::AlignLeft);
    //     pointsLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    //     rightLayout->addWidget(pointsLabel, 0, Qt::AlignLeft);
    // }

    mainLayout->addLayout(rightLayout, 1);

    // Mount card to root centered layout
    rootLayout->addWidget(card, 0, Qt::AlignLeft);
    setLayout(rootLayout);
}

QLabel *OneSevenLiveRockViewerItem::setupAvatar() {
    // Left: Avatar with overlay frame
    QLabel *avatarLabel = new QLabel(this);
    avatarLabel->setFixedSize(55, 57);  // avatar area 55x57
    avatarLabel->setStyleSheet("QLabel { background-color: transparent; }");
    // Forward clicks to parent widget so any click inside the item triggers
    avatarLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // Keep pixmaps across async loads
    auto avatarReady = QSharedPointer<bool>::create(false);
    auto frameReady = QSharedPointer<bool>::create(false);
    auto composedPixmap = QSharedPointer<QPixmap>::create();  // final 65x67 canvas
    auto framePixmap = QSharedPointer<QPixmap>::create();
    auto levelReady = QSharedPointer<bool>::create(false);
    auto levelPixmap = QSharedPointer<QPixmap>::create();

    QPointer<QLabel> safeAvatarLabel = avatarLabel;

    // Compose function: draw frame over avatar if available
    auto composeAndSet = [safeAvatarLabel, avatarReady, frameReady, composedPixmap, framePixmap,
                          levelReady, levelPixmap]() {
        if (!safeAvatarLabel)
            return;
        if (!(*avatarReady))
            return;  // need avatar first

        QPixmap canvas = *composedPixmap;
        if (*frameReady && !framePixmap->isNull()) {
            QPainter painter(&canvas);
            painter.setRenderHint(QPainter::Antialiasing);
            // Overlay scaled to full canvas to match visual frame
            painter.drawPixmap(0, 0,
                               framePixmap->scaled(canvas.size(), Qt::IgnoreAspectRatio,
                                                   Qt::SmoothTransformation));

            // Draw mLevel icon (12x12) at bottom-right, on top of frame
            if (*levelReady && !levelPixmap->isNull()) {
                const int badgeW = 12;
                const int badgeH = 12;
                const int x = canvas.width() - badgeW;
                const int y = canvas.height() - badgeH;
                painter.drawPixmap(x, y,
                                   levelPixmap->scaled(badgeW, badgeH, Qt::IgnoreAspectRatio,
                                                       Qt::SmoothTransformation));
            }
            painter.end();
        }
        // If there is no frame, still draw mLevel icon
        if (!(*frameReady) && *levelReady && !levelPixmap->isNull()) {
            QPainter painter(&canvas);
            painter.setRenderHint(QPainter::Antialiasing);
            const int badgeW = 12;
            const int badgeH = 12;
            const int x = canvas.width() - badgeW;
            const int y = canvas.height() - badgeH;
            painter.drawPixmap(x, y,
                               levelPixmap->scaled(badgeW, badgeH, Qt::IgnoreAspectRatio,
                                                   Qt::SmoothTransformation));
            painter.end();
        }
        safeAvatarLabel->setPixmap(canvas);
    };

    // Load avatar image
    {
        const QString avatarUrl = buildUrl(user.displayUser.picture);
        RemoteTextThread *thread =
            new RemoteTextThread(avatarUrl.toStdString(), "image/png", "", 0, true);
        connect(thread, &RemoteTextThread::ImageResult, this,
                [avatarReady, composedPixmap, composeAndSet](const QByteArray &imageData,
                                                             const QString &error) {
                    if (error.isEmpty() && !imageData.isEmpty()) {
                        // Prepare 65x67 canvas and draw 55x57 image centered with 5px padding
                        QPixmap src;
                        src.loadFromData(imageData);
                        QPixmap scaled =
                            src.scaled(45, 47, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

                        QPixmap canvas(55, 57);
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
                *framePixmap = overlay;  // will be scaled during compose
                *frameReady = true;
                composeAndSet();
            }
        }
    }

    // add mLevel icon if available
    {
        const QString levelRes = OneSevenLiveUtility::mLevelBadgeResource(user);
        if (!levelRes.isEmpty()) {
            QPixmap lvl;
            if (lvl.load(levelRes)) {
                *levelPixmap = lvl;
                *levelReady = true;
                composeAndSet();
            }
        }
    }

    return avatarLabel;
}

QHBoxLayout *OneSevenLiveRockViewerItem::setupNameRow() {
    // 1) Username  Level badge
    QHBoxLayout *nameRow = new QHBoxLayout();
    nameRow->setContentsMargins(0, 0, 0, 0);
    nameRow->setSpacing(6);
    nameRow->setAlignment(Qt::AlignLeft);

    usernameLabel = new QLabel(user.displayUser.displayName, this);
    usernameLabel->setStyleSheet(
        "QLabel {"
        "    color: #FFFFFF;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "}");
    usernameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    usernameLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    usernameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    nameRow->addWidget(usernameLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);

    // Replace level text badge with checking-level background image (if available)
    QString checkingRes = OneSevenLiveUtility::checkingLevelBadgeResource(user);
    if (!checkingRes.isEmpty()) {
        QPixmap checkingPm;
        if (checkingPm.load(checkingRes)) {
            QLabel *checkingLabel = new QLabel(this);
            checkingLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            checkingLabel->setStyleSheet("QLabel { background-color: transparent; }");
            int badgeHeight = 16;  // match previous visual height
            checkingLabel->setPixmap(
                checkingPm.scaledToHeight(badgeHeight, Qt::SmoothTransformation));
            checkingLabel->setFixedHeight(badgeHeight);
            checkingLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            nameRow->addWidget(checkingLabel, 0, Qt::AlignVCenter);
        }
    }

    return nameRow;
}

QHBoxLayout *OneSevenLiveRockViewerItem::setupBadgeRow() {
    // 2) Badge list
    // Badge labels based on merged badgeTypes; skip if none or all empty
    QHBoxLayout *badgeRow = nullptr;
    for (int t : user.badgeTypes) {
        const QString labelText =
            OneSevenLiveUtility::badgeLabel(t, user.armyInfo.rank, &armyNameResponse);
        if (labelText.isEmpty()) {
            continue;
        }
        if (!badgeRow) {
            badgeRow = new QHBoxLayout();
            badgeRow->setContentsMargins(0, 0, 0, 0);
            badgeRow->setSpacing(6);
            badgeRow->setAlignment(Qt::AlignLeft);
        }
        // Build a composite badge: [Gradient text label] + [Right image]
        QWidget *badge = new QWidget(this);
        QHBoxLayout *badgeLayout = new QHBoxLayout(badge);
        badgeLayout->setContentsMargins(0, 0, 0, 0);
        badgeLayout->setSpacing(0);  // no gap between left and right parts

        // Left: text label with gradient background and rounded left corners
        QLabel *leftLbl = new QLabel(labelText, badge);
        leftLbl->setStyleSheet(
            "QLabel {"
            "    color: #FFFFFF;"
            "    padding: 2px 6px;"
            "    font-size: 11px;"
            "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #F69355, stop:1 "
            "#F5487D);"
            "    border-top-left-radius: 6px;"
            "    border-bottom-left-radius: 6px;"
            "    border-top-right-radius: 0px;"
            "    border-bottom-right-radius: 0px;"
            "}");
        leftLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        leftLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        const int targetH = leftLbl->sizeHint().height();
        leftLbl->setFixedHeight(targetH);

        // Right: fixed image piece to complete the badge shape
        QLabel *rightImg = new QLabel(badge);
        QIcon badgeIcon(":/resources/user_images/ig_rock_viewer_badge.svg");
        const int iconH = targetH;                       // keep exact same height as left label
        const int iconW = qRound(iconH * (8.0 / 14.0));  // svg aspect 8x14
        rightImg->setPixmap(badgeIcon.pixmap(iconW, iconH));
        rightImg->setFixedSize(iconW, iconH);
        rightImg->setAlignment(Qt::AlignCenter);
        rightImg->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        badgeLayout->addWidget(leftLbl);
        badgeLayout->addWidget(rightImg);
        badgeRow->addWidget(badge, 0, Qt::AlignLeft);
    }

    return badgeRow;
}

void OneSevenLiveRockViewerItem::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(user);
    }
    QWidget::mousePressEvent(event);
}

void OneSevenLiveRockViewerItem::updateData(const OneSevenLiveRockZoneViewer &user,
                                            const OneSevenLiveArmyNameResponse &armyNameResponse) {
    this->user = user;
    this->armyNameResponse = armyNameResponse;

    // TODO:

    // this->updateGeometry();
    // this->repaint();
}

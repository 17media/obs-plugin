#include "OneSevenLiveRockViewerItem.hpp"

#include <obs-module.h>

#include <QColor>
#include <QEvent>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "api/OneSevenLiveUtility.hpp"
#include "moc_OneSevenLiveRockViewerItem.cpp"
#include "utility/Common.hpp"
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
    // Return fixed size 280x80
    return QSize(280, 80);
}

QString OneSevenLiveRockViewerItem::buildUrl(const QString &path) {
    if (path.isEmpty())
        return QString();
    if (path.startsWith("http://") || path.startsWith("https://"))
        return path;
    return QString("https://cdn.17app.co/") + path;
}

void OneSevenLiveRockViewerItem::setupUi() {
    // Root layout allows the card to stretch with the QListWidget viewport
    QHBoxLayout *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setAlignment(Qt::AlignLeft);

    QWidget *card = new QWidget(this);
    card->setFixedSize(280, 80);  // Set fixed size to 280x80
    QHBoxLayout *mainLayout = new QHBoxLayout(card);
    mainLayout->setContentsMargins(0, 0, 0, 0);  // Add horizontal padding
    mainLayout->setSpacing(5);
    mainLayout->setAlignment(Qt::AlignLeft);

    // Make the whole item look clickable
    setCursor(Qt::PointingHandCursor);

    // Setup avatar area
    setupAvatar();
    mainLayout->addWidget(avatarLabel, 0, Qt::AlignVCenter);

    // Right side: 3 vertical sections
    rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);  // reduce spacing between components
    rightLayout->setAlignment(Qt::AlignTop);

    // Setup name row
    nameRowLayout = setupNameRow();
    rightLayout->addLayout(nameRowLayout);

    // Setup badge row
    badgeRowLayout = setupBadgeRow();
    if (badgeRowLayout) {
        rightLayout->addLayout(badgeRowLayout);
    }

    // Setup points row
    QHBoxLayout *pointsRow = setupPointsRow();
    rightLayout->addLayout(pointsRow);

    mainLayout->addLayout(rightLayout, 1);

    // Mount card to root layout with fixed size
    rootLayout->addWidget(card, 0, Qt::AlignLeft);
    setLayout(rootLayout);
}

void OneSevenLiveRockViewerItem::setupAvatar() {
    // Left: Avatar with overlay frame
    avatarLabel = new QLabel(this);
    avatarLabel->setFixedSize(55, 57);  // avatar area 55x57
    avatarLabel->setStyleSheet("QLabel { background-color: transparent; }");
    // Forward clicks to parent widget so any click inside the item triggers
    avatarLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    reloadAvatar();
}

void OneSevenLiveRockViewerItem::reloadAvatar() {
    if (!avatarLabel)
        return;

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

            if (*levelReady && !levelPixmap->isNull()) {
                const int badgeW = 18;
                const int badgeH = 18;
                const int x = 5;
                const int y = 5;
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
            const int badgeW = 18;
            const int badgeH = 18;
            const int x = 5;
            const int y = 5;
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
    QHBoxLayout *badgeRow = new QHBoxLayout();
    badgeRow->setContentsMargins(0, 0, 0, 0);
    badgeRow->setSpacing(6);
    badgeRow->setAlignment(Qt::AlignLeft);

    bool hasBadges = false;
    for (int t : user.badgeTypes) {
        const QString labelText =
            OneSevenLiveUtility::badgeLabel(t, user.armyInfo.rank, &armyNameResponse);
        if (labelText.isEmpty()) {
            continue;
        }
        hasBadges = true;

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

    if (!hasBadges) {
        badgeRow->deleteLater();
        return nullptr;
    }

    return badgeRow;
}

void OneSevenLiveRockViewerItem::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(user);
    }
    QWidget::mousePressEvent(event);
}

QHBoxLayout *OneSevenLiveRockViewerItem::setupPointsRow() {
    int points = user.userAttr.sentPoint;  // sent points

    // Create horizontal layout for icon and points
    QHBoxLayout *pointsLayout = new QHBoxLayout();
    pointsLayout->setContentsMargins(0, 0, 0, 0);
    pointsLayout->setSpacing(4);  // Small spacing between icon and text

    // Add baobaobi icon
    QLabel *iconLabel = new QLabel(this);
    iconLabel->setFixedSize(16, 16);
    iconLabel->setStyleSheet("QLabel { background-color: transparent; }");
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // Load SVG icon
    QPixmap iconPixmap(":/resources/baobaobi.svg");
    if (!iconPixmap.isNull()) {
        iconLabel->setPixmap(
            iconPixmap.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // Format points with thousand separators based on current language
    QLocale locale;
    std::string currentLang = GetCurrentLanguage();
    if (currentLang == "TW") {
        locale = QLocale(QLocale::Chinese, QLocale::Taiwan);
    } else if (currentLang == "JP") {
        locale = QLocale(QLocale::Japanese, QLocale::Japan);
    } else {  // US
        locale = QLocale(QLocale::English, QLocale::UnitedStates);
    }
    QString formattedPoints = locale.toString(points);

    pointsLabel = new QLabel(formattedPoints, this);
    pointsLabel->setStyleSheet(
        "QLabel {"
        "    color: #D9D9D9;"
        "    font-size: 12px;"
        "}");
    pointsLabel->setAlignment(Qt::AlignLeft);
    pointsLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    pointsLayout->addWidget(iconLabel);
    pointsLayout->addWidget(pointsLabel);
    pointsLayout->addStretch();  // Push content to the left

    return pointsLayout;
}

void OneSevenLiveRockViewerItem::updateData(
    const OneSevenLiveRockZoneViewer &newUser,
    const OneSevenLiveArmyNameResponse &newArmyNameResponse) {
    const auto &oldUser = this->user;

    bool avatarChanged = (newUser.displayUser.picture != oldUser.displayUser.picture) ||
                         (OneSevenLiveUtility::avatarFrameResource(newUser) !=
                          OneSevenLiveUtility::avatarFrameResource(oldUser)) ||
                         (OneSevenLiveUtility::mLevelBadgeResource(newUser) !=
                          OneSevenLiveUtility::mLevelBadgeResource(oldUser));

    bool nameChanged = (newUser.displayUser.displayName != oldUser.displayUser.displayName) ||
                       (OneSevenLiveUtility::checkingLevelBadgeResource(newUser) !=
                        OneSevenLiveUtility::checkingLevelBadgeResource(oldUser));

    bool pointsChanged = (newUser.userAttr.sentPoint != oldUser.userAttr.sentPoint);

    bool badgesChanged = (newUser.badgeTypes != oldUser.badgeTypes) ||
                         (newUser.armyInfo.rank != oldUser.armyInfo.rank);

    this->user = newUser;
    this->armyNameResponse = newArmyNameResponse;

    if (avatarChanged) {
        reloadAvatar();
    }

    if (nameChanged) {
        updateNameRow();
    }

    if (pointsChanged) {
        QLocale locale;
        std::string currentLang = GetCurrentLanguage();
        if (currentLang == "TW") {
            locale = QLocale(QLocale::Chinese, QLocale::Taiwan);
        } else if (currentLang == "JP") {
            locale = QLocale(QLocale::Japanese, QLocale::Japan);
        } else {  // US
            locale = QLocale(QLocale::English, QLocale::UnitedStates);
        }
        QString formattedPoints = locale.toString(user.userAttr.sentPoint);
        if (pointsLabel)
            pointsLabel->setText(formattedPoints);
    }

    if (badgesChanged) {
        updateBadges();
    }

    updateGeometry();
}

void OneSevenLiveRockViewerItem::updateNameRow() {
    if (!nameRowLayout)
        return;

    // Clear existing items
    QLayoutItem *child;
    while ((child = nameRowLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    // Rebuild
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

    nameRowLayout->addWidget(usernameLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);

    QString checkingRes = OneSevenLiveUtility::checkingLevelBadgeResource(user);
    if (!checkingRes.isEmpty()) {
        QPixmap checkingPm;
        if (checkingPm.load(checkingRes)) {
            QLabel *checkingLabel = new QLabel(this);
            checkingLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            checkingLabel->setStyleSheet("QLabel { background-color: transparent; }");
            int badgeHeight = 16;
            checkingLabel->setPixmap(
                checkingPm.scaledToHeight(badgeHeight, Qt::SmoothTransformation));
            checkingLabel->setFixedHeight(badgeHeight);
            checkingLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            nameRowLayout->addWidget(checkingLabel, 0, Qt::AlignVCenter);
        }
    }
}

void OneSevenLiveRockViewerItem::updateBadges() {
    // If badge row didn't exist but now might, we have a problem because we need to insert it into
    // rightLayout But rightLayout is available.

    if (badgeRowLayout) {
        // Clear existing
        QLayoutItem *child;
        while ((child = badgeRowLayout->takeAt(0)) != nullptr) {
            if (child->widget()) {
                child->widget()->deleteLater();
            }
            delete child;
        }

        // Check if we still need badges
        // If not, we should ideally remove the layout, but keeping an empty layout is okay-ish
        // (just extra spacing) Or we can delete badgeRowLayout and set to nullptr.

        // Let's see setupBadgeRow logic.
        bool hasBadges = false;
        for (int t : user.badgeTypes) {
            const QString labelText =
                OneSevenLiveUtility::badgeLabel(t, user.armyInfo.rank, &armyNameResponse);
            if (!labelText.isEmpty()) {
                hasBadges = true;
                break;
            }
        }

        if (!hasBadges) {
            // Remove layout from rightLayout
            rightLayout->removeItem(badgeRowLayout);
            badgeRowLayout->deleteLater();
            badgeRowLayout = nullptr;
            return;
        }

        // Rebuild
        for (int t : user.badgeTypes) {
            const QString labelText =
                OneSevenLiveUtility::badgeLabel(t, user.armyInfo.rank, &armyNameResponse);
            if (labelText.isEmpty()) {
                continue;
            }

            QWidget *badge = new QWidget(this);
            QHBoxLayout *badgeLayout = new QHBoxLayout(badge);
            badgeLayout->setContentsMargins(0, 0, 0, 0);
            badgeLayout->setSpacing(0);

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

            QLabel *rightImg = new QLabel(badge);
            QIcon badgeIcon(":/resources/user_images/ig_rock_viewer_badge.svg");
            const int iconH = targetH;
            const int iconW = qRound(iconH * (8.0 / 14.0));
            rightImg->setPixmap(badgeIcon.pixmap(iconW, iconH));
            rightImg->setFixedSize(iconW, iconH);
            rightImg->setAlignment(Qt::AlignCenter);
            rightImg->setAttribute(Qt::WA_TransparentForMouseEvents, true);

            badgeLayout->addWidget(leftLbl);
            badgeLayout->addWidget(rightImg);
            badgeRowLayout->addWidget(badge, 0, Qt::AlignLeft);
        }

    } else {
        // Create new if needed
        badgeRowLayout = setupBadgeRow();
        if (badgeRowLayout) {
            // Insert between nameRow (index 0) and pointsRow (index 1 or 2?)
            // rightLayout has: nameRow, [badgeRow], pointsRow
            // If badgeRow was null, pointsRow is at index 1.
            // So insert at index 1.
            rightLayout->insertLayout(1, badgeRowLayout);
        }
    }
}

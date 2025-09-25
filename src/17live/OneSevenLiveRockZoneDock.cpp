#include "OneSevenLiveRockZoneDock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSharedPointer>
#include <QTimer>
#include <QVBoxLayout>

#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveRockViewerItem.hpp"
#include "OneSevenLiveUserDialog.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "plugin-support.h"
#include "utility/RemoteTextThread.hpp"

OneSevenLiveRockZoneDock::OneSevenLiveRockZoneDock(QWidget* parent,
                                                   OneSevenLiveApiWrappers* apiWrapper_,
                                                   OneSevenLiveConfigManager* configManager_)
    : QDockWidget(obs_module_text("RockZone.Title"), parent),
      apiWrapper(apiWrapper_),
      configManager(configManager_) {
    setupUi();
    createConnections();

    // Initialize auto refresh timer
    refreshTimer = new QTimer(this);
    refreshTimer->setInterval(5000);  // 5 seconds
    connect(refreshTimer, &QTimer::timeout, this, &OneSevenLiveRockZoneDock::refreshUserList);

    // Initialize cooldown timer
    cooldownTimer = new QTimer(this);
    cooldownTimer->setInterval(1000);  // 1 second
    connect(cooldownTimer, &QTimer::timeout, this,
            &OneSevenLiveRockZoneDock::onCooldownTimerTimeout);

    refreshUserList();
    refreshTimer->start();

    connect(this, &QDockWidget::topLevelChanged, this,
            &OneSevenLiveRockZoneDock::handleTopLevelChanged);

    connect(userList, &QObject::destroyed, this, [this]() { userItemMap.clear(); });
}

OneSevenLiveRockZoneDock::~OneSevenLiveRockZoneDock() {
    if (userDialog) {
        userDialog->deleteLater();
        userDialog = nullptr;
    }
}

void OneSevenLiveRockZoneDock::setupUi() {
    QWidget* container = new QWidget(this);
    container->setObjectName("container");
    container->setStyleSheet(
        "QWidget#container {"
        "    background-color: #000000;"
        "    border: none;"
        "    font-family: 'Inter';"
        "    color: #FFFFFF;"
        "    font-style: normal;"
        "}");
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Create hint bar
    {
        QHBoxLayout* hintLayout = new QHBoxLayout();
        hintLayout->setContentsMargins(0, 10, 0, 10);
        hintLayout->setSpacing(8);
        hintLayout->setAlignment(Qt::AlignHCenter);

        QLabel* icon = new QLabel(container);
        icon->setFixedSize(20, 20);
        icon->setPixmap(QPixmap(":/resources/exclaimark.svg")
                            .scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        QLabel* hintText = new QLabel(obs_module_text("RockZone.Hint"), container);
        hintText->setStyleSheet("color: #FFFFFF; font-size: 14px;");

        // Add leading stretch to center contents
        hintLayout->addStretch();
        hintLayout->addWidget(icon);
        hintLayout->addWidget(hintText);
        hintLayout->addStretch();

        QWidget* hintContainer = new QWidget(container);
        hintContainer->setLayout(hintLayout);
        mainLayout->addWidget(hintContainer);
    }

    // Create user list
    userList = new QListWidget();
    userList->setStyleSheet(
        "QListWidget {"
        "   background-color: transparent;"
        "   border: none;"
        "}"
        "QListWidget::item {"
        "   background-color: #000000;"
        "   border-radius: 0px;"
        // "   padding: 10px;"
        "   margin: 0px;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #3a3a4a;"
        "   border: 1px solid #5a5a6a;"
        "   color: white;"
        "}"
        "QListWidget::item:hover:!selected {"
        "    background-color: #1a1a1a;"
        "}");
    userList->setResizeMode(QListWidget::Adjust);
    userList->setWordWrap(true);
    userList->setSpacing(1);
    mainLayout->addWidget(userList);

    mainLayout->addSpacing(40);

    // Create bottom button
    pokeAllButton = new QPushButton(obs_module_text("RockZone.PokeAll"));
    pokeAllButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #FF0001;"
        "    color: white;"
        "    border-radius: 2px;"
        "    padding: 8px;"
        "   font-weight: 600;"
        "   font-size: 16px;"
        "   line-height: 24px;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #808080;"
        "    color: #C0C0C0;"
        "}");
    pokeAllButton->setMaximumWidth(250);
    pokeAllButton->setMinimumWidth(150);
    pokeAllButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mainLayout->addWidget(pokeAllButton, 0, Qt::AlignHCenter);

    // Save original button text
    originalButtonText = pokeAllButton->text();

    // Set dock size constraints to allow width adjustment with maximum width of 450
    setMaximumWidth(450);
    setMinimumWidth(380);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setWidget(container);
}

void OneSevenLiveRockZoneDock::createConnections() {
    connect(pokeAllButton, &QPushButton::clicked, this,
            &OneSevenLiveRockZoneDock::onPokeAllClicked);

    // Connect user list item click signal
    // Disabled because OneSevenLiveRockViewerItem handles click and opens the dialog
    // connect(userList, &QListWidget::itemClicked, this,
    //         &OneSevenLiveRockZoneDock::onUserItemClicked);
}

void OneSevenLiveRockZoneDock::updateUserItem(
    QListWidgetItem* item, const OneSevenLiveRockZoneViewer& user,
    const OneSevenLiveArmyNameResponse& armyNameResponse) {
    OneSevenLiveRockViewerItem* w =
        qobject_cast<OneSevenLiveRockViewerItem*>(userList->itemWidget(item));

    if (!w) {
        w = new OneSevenLiveRockViewerItem(user, apiWrapper, configManager, armyNameResponse, this);
        item->setSizeHint(w->sizeHint());
        userList->setItemWidget(item, w);

        connect(w, &OneSevenLiveRockViewerItem::clicked, this,
                [this](const OneSevenLiveRockZoneViewer& viewer) {
                    OneSevenLiveUserDialog* dialog =
                        new OneSevenLiveUserDialog(this, apiWrapper, configManager);
                    dialog->setAttribute(Qt::WA_DeleteOnClose);
                    dialog->setUserInfo(viewer);
                    dialog->show();
                });
    } else {
        w->updateData(user, armyNameResponse);
    }
}

void OneSevenLiveRockZoneDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);

    if (!userList)
        return;

    for (int i = 0; i < userList->count(); ++i) {
        QListWidgetItem* item = userList->item(i);
        QWidget* widget = userList->itemWidget(item);
        if (widget)
            widget->resize(userList->viewport()->width(), widget->height());
        item->setSizeHint(widget->sizeHint());
    }
}

void OneSevenLiveRockZoneDock::refreshUserList() {
    std::string roomID;
    configManager->getConfigValue("RoomID", roomID);

    std::string userID;
    configManager->getConfigValue("UserID", userID);

    // Create new thread for API call to avoid UI blocking
    QThread* thread = new QThread;
    QObject* worker = new QObject;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [this, worker, thread, roomID, userID]() {
        // Execute API call in new thread
        Json response;
        bool success = apiWrapper->GetRockViewers(roomID, response);

        OneSevenLiveArmyNameResponse armyNameResponse;

        // Only call GetArmyName if not cached
        if (!armyNameCached) {
            apiWrapper->GetArmyName(userID, armyNameResponse);
            cachedArmyNameResponse = armyNameResponse;
            armyNameCached = true;
        } else {
            armyNameResponse = cachedArmyNameResponse;
        }

        // Use Qt::QueuedConnection to ensure UI updates happen on the main thread
        QMetaObject::invokeMethod(
            this,
            [this, success, response, armyNameResponse, userID]() {
                if (success) {
                    QList<OneSevenLiveRockZoneViewer> users;
                    JsonToOneSevenLiveRockViewers(response, users);

                    // Merge viewers by userID and collect their types into badgeTypes
                    QHash<QString, int> idIndex;  // userID -> index in viewersList
                    viewersList.clear();
                    for (const auto& user : users) {
                        const QString uid = user.displayUser.userID.isEmpty()
                                                ? user.giftRankOne.userID
                                                : user.displayUser.userID;
                        if (uid.isEmpty()) {
                            continue;
                        }
                        if (uid == QString::fromStdString(userID)) {
                            continue;
                        }
                        if (user.userAttr.sentPoint <= 0) {
                            continue;
                        }
                        if (idIndex.contains(uid)) {
                            auto& existing = viewersList[idIndex.value(uid)];
                            if (!existing.badgeTypes.contains(user.type)) {
                                existing.badgeTypes.append(user.type);
                            }
                            if (!existing.giftRankOne.userID.isEmpty()) {
                                existing.displayUser = user.displayUser;
                            }
                        } else {
                            OneSevenLiveRockZoneViewer base = user;
                            if (base.displayUser.userID.isEmpty()) {
                                base.displayUser.userID = base.giftRankOne.userID;
                                base.displayUser.displayName = base.giftRankOne.displayName;
                                base.displayUser.picture = base.giftRankOne.picture;
                            }

                            base.badgeTypes.clear();
                            base.badgeTypes.append(user.type);
                            viewersList.push_back(base);
                            idIndex.insert(uid, viewersList.size() - 1);
                        }
                    }

                    // Update UI
                    userList->setVisible(true);

                    // --- Incremental Update Section ---
                    QSet<QString> newUserIDs;
                    for (const auto& user : viewersList) {
                        QString uid = user.displayUser.userID;
                        newUserIDs.insert(uid);

                        if (userItemMap.contains(uid)) {
                            // Existing user, update item
                            QListWidgetItem* item = userItemMap.value(uid);
                            updateUserItem(item, user, armyNameResponse);
                        } else {
                            // New user
                            QListWidgetItem* item = new QListWidgetItem(userList);
                            updateUserItem(item, user, armyNameResponse);
                            userList->addItem(item);
                            userItemMap.insert(uid, item);
                        }
                    }

                    // Remove users that no longer exist
                    auto it = userItemMap.begin();
                    while (it != userItemMap.end()) {
                        if (!newUserIDs.contains(it.key())) {
                            QListWidgetItem* item = it.value();
                            int row = userList->row(item);
                            if (row >= 0) {
                                QListWidgetItem* removed = userList->takeItem(row);
                                delete removed;
                            }
                            it = userItemMap.erase(it);
                        } else {
                            ++it;
                        }
                    }
                } else {
                    // Show error message
                    obs_log(LOG_ERROR, "Failed to refresh rock viewers list: %s",
                            apiWrapper->getLastErrorMessage().toStdString().c_str());
                }
            },
            Qt::QueuedConnection);

        // Clean up after completion
        thread->quit();
        worker->deleteLater();
    });

    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void OneSevenLiveRockZoneDock::clearArmyNameCache() {
    armyNameCached = false;
    cachedArmyNameResponse = OneSevenLiveArmyNameResponse();
}

void OneSevenLiveRockZoneDock::onPokeAllClicked() {
    if (!apiWrapper) {
        return;
    }

    // Check if button is already in cooldown
    if (!pokeAllButton->isEnabled()) {
        return;
    }

    std::string roomID;
    configManager->getConfigValue("RoomID", roomID);

    // Create poke request
    OneSevenLivePokeAllRequest request;
    OneSevenLivePokeResponse response;
    request.liveStreamID = QString::fromStdString(roomID);
    request.receiverGroup = 2;

    // Send request
    bool success = apiWrapper->PokeAll(request, response);

    if (success) {
        // Start cooldown timer
        cooldownSeconds = 20;
        pokeAllButton->setEnabled(false);
        pokeAllButton->setText(QString("0:%1").arg(cooldownSeconds, 2, 10, QChar('0')));
        cooldownTimer->start();
    } else {
        obs_log(LOG_WARNING, "PokeAll failed %s",
                apiWrapper->getLastErrorMessage().toStdString().c_str());
    }
}

void OneSevenLiveRockZoneDock::handleTopLevelChanged(bool topLevel) {
    if (!topLevel) {
        // Docked state
        adjustSize();
        // May need to force update layout or child widget sizes
        for (int i = 0; i < userList->count(); ++i) {
            QListWidgetItem* item = userList->item(i);
            QWidget* itemWidget = userList->itemWidget(item);
            if (itemWidget) {
                itemWidget->adjustSize();
                item->setSizeHint(itemWidget->sizeHint());
            }
        }
    }
}

void OneSevenLiveRockZoneDock::onUserItemClicked(QListWidgetItem* item) {
    // Get clicked item index
    int index = userList->row(item);
    if (index < 0 || index >= viewersList.size()) {
        return;
    }

    // Get user information
    const OneSevenLiveRockZoneViewer& user = viewersList.at(index);

    // Create user information dialog (if it doesn't exist)
    if (!userDialog) {
        userDialog = new OneSevenLiveUserDialog(this, apiWrapper, configManager);
    }

    // Set user information and display dialog
    userDialog->setUserInfo(user);
    userDialog->exec();
}

void OneSevenLiveRockZoneDock::onCooldownTimerTimeout() {
    cooldownSeconds--;

    if (cooldownSeconds <= 0) {
        // Cooldown finished
        cooldownTimer->stop();
        pokeAllButton->setEnabled(true);
        pokeAllButton->setText(originalButtonText);
    } else {
        // Update countdown display
        pokeAllButton->setText(QString("0:%1").arg(cooldownSeconds, 2, 10, QChar('0')));
    }
}

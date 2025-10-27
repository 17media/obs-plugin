#include "OneSevenLiveRockZoneDock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QFile>
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
    pokeAllButton->setFixedHeight(40);
    pokeAllButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mainLayout->addWidget(pokeAllButton, 0, Qt::AlignHCenter);

    // Save original button text
    originalButtonText = pokeAllButton->text();

    // Set dock size constraints to allow width adjustment with maximum width of 450
    // Minimum width adjusted to 320px to accommodate 280px RockViewerItem + margins
    setMaximumWidth(450);
    setMinimumWidth(320);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setWidget(container);
}

void OneSevenLiveRockZoneDock::createConnections() {
    connect(pokeAllButton, &QPushButton::clicked, this,
            &OneSevenLiveRockZoneDock::onPokeAllClicked);
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
                    obs_log(LOG_INFO, "OneSevenLiveRockZoneDock::userClicked %s",
                            viewer.displayUser.displayName.toStdString().c_str());
                    if (!userDialog) {
                        obs_log(LOG_INFO, "Creating user dialog");
                        userDialog = new OneSevenLiveUserDialog(this, apiWrapper, configManager);
                    }

                    userDialog->setAttribute(Qt::WA_DeleteOnClose);
                    userDialog->setUserInfo(viewer);
                    userDialog->show();
                });
    } else {
        w->updateData(user, armyNameResponse);
    }
}

void OneSevenLiveRockZoneDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);

    if (!userList)
        return;

    // Store count to avoid issues if list is modified during iteration
    int itemCount = userList->count();

    for (int i = 0; i < itemCount; ++i) {
        // Double-check count hasn't changed during iteration
        if (i >= userList->count())
            break;

        QListWidgetItem* item = userList->item(i);
        if (!item)
            continue;  // Skip null items

        QWidget* widget = userList->itemWidget(item);
        if (widget) {
            widget->resize(userList->viewport()->width(), widget->height());
            item->setSizeHint(widget->sizeHint());
        }
    }
}

static void mergeMockUsers(Json& originUsers, Json& mockUsers) {
    // Read mock users from local json file and merge with original users
    do {
        QFile file("/Users/zhuyu/workspace/mk/17live/dev/17live_dev/mock/test_viewers.json");
        if (!file.exists()) {
            mockUsers = originUsers;  // no mock data, return original
            break;
        }
        if (!file.open(QIODevice::ReadOnly)) {
            obs_log(LOG_WARNING, "mergeMockUsers: cannot open test viewers file");
            mockUsers = originUsers;  // fallback to original
            break;
        }
        QByteArray content = file.readAll();
        file.close();

        Json test_json;
        try {
            test_json = Json::parse(content.toStdString());
        } catch (const nlohmann::json::parse_error& e) {
            obs_log(LOG_WARNING, "mergeMockUsers: parse test viewers failed: %s", e.what());
            mockUsers = originUsers;  // fallback to original
            break;
        }

        // Extract mock users array from json
        Json mock_array_json;
        if (test_json.is_array()) {
            mock_array_json = test_json;
        } else if (test_json.is_object() && test_json["viewers"].is_array()) {
            mock_array_json = test_json["viewers"];
        } else {
            obs_log(LOG_WARNING,
                    "mergeMockUsers: mock json is neither array nor object with 'viewers'");
            mockUsers = originUsers;  // fallback to original
            break;
        }

        // Extract original users array
        Json merged = Json::array();
        bool original_is_array = false;
        if (originUsers.is_array()) {
            original_is_array = true;
            for (const auto& item : originUsers) {
                merged.push_back(item);
            }
        } else if (originUsers.is_object() && originUsers.contains("viewers") &&
                   originUsers["viewers"].is_array()) {
            for (const auto& item : originUsers["viewers"]) {
                merged.push_back(item);
            }
        } else if (originUsers.is_null()) {
            // no original data, start with empty
        } else {
            // unexpected type, try best effort
            obs_log(LOG_WARNING, "mergeMockUsers: unexpected originUsers format");
        }

        // Merge mock users into the array
        if (mock_array_json.is_array()) {
            for (const auto& item : mock_array_json) {
                merged.push_back(item);
            }
        }

        // Construct the result based on original format
        if (original_is_array) {
            mockUsers = merged;
        } else if (originUsers.is_object()) {
            Json obj = originUsers;
            obj["viewers"] = merged;
            mockUsers = obj;
        } else {
            mockUsers = merged;
        }

        size_t mock_count = mock_array_json.is_array() ? mock_array_json.size() : 0;
        size_t original_count = 0;
        if (original_is_array) {
            original_count = originUsers.is_array() ? originUsers.size() : 0;
        } else if (originUsers.is_object() && originUsers.contains("viewers") &&
                   originUsers["viewers"].is_array()) {
            original_count = originUsers["viewers"].size();
        }
        obs_log(LOG_INFO, "mergeMockUsers: merged %zu mock viewers with %zu original users",
                mock_count, original_count);
    } while (false);
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
        Json jsonResponse;
        bool success = apiWrapper->GetRockViewers(roomID, jsonResponse);

        Json response = jsonResponse;
        if (success) {
            mergeMockUsers(jsonResponse, response);
        }

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

                    QList<OneSevenLiveRockZoneViewer> sortedViewersList =
                        SortOneSevenLiveRockZoneViewers(viewersList);

                    // Update UI
                    // userList->setVisible(true);

                    // --- Incremental Update Section ---
                    QSet<QString> newUserIDs;

                    // Limit to first 50 viewers to improve performance
                    if (sortedViewersList.size() > 50) {
                        sortedViewersList = sortedViewersList.mid(0, 50);
                    }

                    // First pass: update existing items and create new ones
                    for (int i = 0; i < sortedViewersList.size(); ++i) {
                        const auto& user = sortedViewersList[i];
                        QString uid = user.displayUser.userID;
                        newUserIDs.insert(uid);

                        QListWidgetItem* item = nullptr;
                        if (userItemMap.contains(uid)) {
                            // Existing user, update item
                            item = userItemMap.value(uid);
                            updateUserItem(item, user, armyNameResponse);

                            // Move item to correct position if needed
                            int currentRow = userList->row(item);
                            if (currentRow != i && currentRow >= 0) {
                                // Use a more atomic operation to avoid temporary null items
                                QListWidgetItem* takenItem = userList->takeItem(currentRow);
                                if (takenItem == item) {
                                    userList->insertItem(i, item);
                                }
                            }
                        } else {
                            // New user
                            item = new QListWidgetItem();
                            updateUserItem(item, user, armyNameResponse);
                            userList->insertItem(i, item);
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

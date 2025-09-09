#include "OneSevenLiveRockZoneDock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QPainter>
#include <QPointer>
#include <QPainterPath>
#include <QSharedPointer>

#include "utility/RemoteTextThread.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "plugin-support.h"
#include "OneSevenLiveRockViewerItem.hpp"
#include "OneSevenLiveUserDialog.hpp"

OneSevenLiveRockZoneDock::OneSevenLiveRockZoneDock(QWidget* parent,
                                               OneSevenLiveApiWrappers* apiWrapper_,
                                               OneSevenLiveConfigManager* configManager_)
    : QDockWidget(obs_module_text("RockZone.Title"), parent),
      apiWrapper(apiWrapper_),
      configManager(configManager_) {
    setupUi();
    createConnections();
    refreshUserList();

    // Add delayed initialization to ensure UI element sizes are correctly calculated
    QTimer::singleShot(0, this, [this]() {
        if (emptyContainer && emptyContainer->isVisible()) {
            emptyContainer->setGeometry(widget()->rect());
        }
    });

    connect(this, &QDockWidget::topLevelChanged, this,
            &OneSevenLiveRockZoneDock::handleTopLevelChanged);
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
        hintLayout->setContentsMargins(0, 40, 0, 20);
        hintLayout->setSpacing(8);
        hintLayout->setAlignment(Qt::AlignHCenter);

        QLabel* icon = new QLabel(container);
        icon->setFixedSize(20, 20);
        icon->setPixmap(QPixmap(":/resources/alert.svg").scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        QLabel* hintText = new QLabel(obs_module_text("RockZone.Hint"), container);
        hintText->setStyleSheet("color: #FFFFFF; font-size: 14px;");

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
        "   padding: 10px;"
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
    userList->setSpacing(5);
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
        "}");
    pokeAllButton->setFixedWidth(250);
    mainLayout->addWidget(pokeAllButton, 0, Qt::AlignHCenter);

    // Create loading status UI
    loadingOverlay = new QWidget(container);
    loadingOverlay->setObjectName("loadingOverlay");
    loadingOverlay->setStyleSheet(
        "QWidget#loadingOverlay {"
        "    background-color: rgba(30, 30, 30, 0.8);"
        "    border-radius: 4px;"
        "}");
    loadingOverlay->setGeometry(container->rect());
    loadingOverlay->hide();

    QVBoxLayout* loadingLayout = new QVBoxLayout(loadingOverlay);
    loadingLayout->setAlignment(Qt::AlignCenter);
    loadingLayout->setSpacing(10);

    loadingLabel = new QLabel(obs_module_text("Live.Settings.Loading"));
    loadingLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "}");
    loadingLayout->addWidget(loadingLabel, 0, Qt::AlignHCenter);

    loadingProgress = new QProgressBar();
    loadingProgress->setRange(0, 0); // Set to indeterminate mode
    loadingProgress->setFixedSize(200, 10);
    loadingProgress->setTextVisible(false);
    loadingProgress->setStyleSheet(
        "QProgressBar {"
        "    background-color: #333333;"
        "    border-radius: 5px;"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #FF0001;"
        "    border-radius: 5px;"
        "}");
    loadingLayout->addWidget(loadingProgress, 0, Qt::AlignHCenter);

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

void OneSevenLiveRockZoneDock::updateUserItem(QListWidgetItem* item, const OneSevenLiveRockZoneViewer& user) {
    OneSevenLiveRockViewerItem *w = new OneSevenLiveRockViewerItem(user, apiWrapper, configManager, this);
    item->setSizeHint(w->sizeHint());
    userList->setItemWidget(item, w);
    
    // Click: open user dialog
    connect(w, &OneSevenLiveRockViewerItem::clicked, this, [this](const OneSevenLiveRockZoneViewer &viewer){
        OneSevenLiveUserDialog *dialog = new OneSevenLiveUserDialog(this, apiWrapper, configManager);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setUserInfo(viewer);
        dialog->show();
    });
}

void OneSevenLiveRockZoneDock::showEmptyListMessage() {
    // Hide list
    userList->setVisible(false);

    // If empty state container exists, delete it first
    if (emptyContainer) {
        emptyContainer->deleteLater();
    }

    // Create empty state container
    emptyContainer = new QWidget(widget());
    emptyContainer->setStyleSheet(
        "QWidget {"
        "    background-color: #1e1e1e;"
        "    border-radius: 4px;"
        "}");

    // Set empty state container to fill the entire Dock area
    emptyContainer->setGeometry(widget()->rect());

    // Create layout manager
    QVBoxLayout* emptyLayout = new QVBoxLayout(emptyContainer);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(20);
    emptyLayout->setContentsMargins(20, 20, 20, 20);

    // Create prompt label
    QLabel* emptyLabel = new QLabel(obs_module_text("Live.RockZone.Empty"));
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setWordWrap(true);
    emptyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    emptyLabel->setStyleSheet(
        "QLabel {"
        "    color: #888888;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    padding: 0 10px;"
        "}");

    // Add to layout
    emptyLayout->addWidget(emptyLabel);

    // Show empty state container
    emptyContainer->show();
    emptyContainer->raise();
}

void OneSevenLiveRockZoneDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);

    if (emptyContainer && emptyContainer->isVisible()) {
        emptyContainer->setGeometry(widget()->rect());
    }
    
    // Adjust loading overlay size
    if (loadingOverlay) {
        loadingOverlay->setGeometry(widget()->rect());
    }

    for (int i = 0; i < userList->count(); ++i) {
        QListWidgetItem* item = userList->item(i);
        QWidget* widget = userList->itemWidget(item);
        if (widget)
            widget->resize(userList->viewport()->width(), widget->height());
        item->setSizeHint(widget->sizeHint());
    }
}

void OneSevenLiveRockZoneDock::refreshUserList() {
    // Show loading status
    isLoading = true;
    loadingOverlay->setVisible(true);
    loadingOverlay->raise();  // Ensure overlay is on top
    loadingLabel->setText(obs_module_text("Live.Settings.Loading"));

    // Disable all controls
    QWidget* container = qobject_cast<QWidget*>(widget());
    if (container) {
        container->setEnabled(false);
    }

    userList->clear();

    if (emptyContainer) {
        // If empty state container exists, delete it first
        emptyContainer->deleteLater();
        emptyContainer = nullptr;
    }

    // Create new thread for API call to avoid UI blocking
    QThread* thread = new QThread;
    QObject* worker = new QObject;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [this, worker, thread]() {
        // Execute API call in new thread
        std::string roomID;
        configManager->getConfigValue("RoomID", roomID);

        Json response;
        bool success = apiWrapper->GetRockViewers(roomID, response);

        // Use Qt::QueuedConnection to ensure UI updates happen on the main thread
        QMetaObject::invokeMethod(
            this,
            [this, success, response]() {
                // Hide loading status
                isLoading = false;
                loadingOverlay->setVisible(false);

                QWidget* container = qobject_cast<QWidget*>(widget());
                if (container) {
                    container->setEnabled(true);
                }

                if (success) {
                    userList->clear();
                    viewersList.clear();

                    QList<OneSevenLiveRockZoneViewer> users;
                    JsonToOneSevenLiveRockViewers(response, users);

                    std::string userID;
                    configManager->getConfigValue("UserID", userID);

                    QList<QString> userIDs;
                    for (auto &user : users) {
                        if (user.displayUser.userID.isEmpty()) {
                            continue;
                        }
                        if (user.displayUser.userID == QString::fromStdString(userID)) {
                            continue;
                        }
                        if (user.userAttr.sentPoint > 0 && !userIDs.contains(user.displayUser.userID)) {
                            viewersList.push_back(user);
                            userIDs.push_back(user.displayUser.userID);
                        }
                    }
                    
                    // Update UI
                    if (viewersList.isEmpty()) {
                        // Show empty list message
                        showEmptyListMessage();
                    } else {
                        userList->setVisible(true);
                        
                        // Update user count label
                        userCountLabel->setText(QString(obs_module_text("Live.RockZone.UserCount")).arg(viewersList.size()));
    

                        // Display user list
                        for (const auto& user : viewersList) {
                            QListWidgetItem* item = new QListWidgetItem(userList);
                            updateUserItem(item, user);
                            userList->addItem(item);
                        }
                    }
                } else {
                    // Show error message
                    QMessageBox::warning(
                        this, obs_module_text("Live.Settings.Error"),
                        QString::fromStdString(obs_module_text("Live.Settings.LoadError"))
                            .arg(apiWrapper->getLastErrorMessage()));
                    
                    // Show empty list message
                    showEmptyListMessage();
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

void OneSevenLiveRockZoneDock::onPokeAllClicked() {
    if (!apiWrapper) {
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
        QMessageBox::information(this, 
                               obs_module_text("Live.PokeSuccess"), 
                               obs_module_text("Live.PokeSuccessMessage"));
    } else {
        QMessageBox::warning(this, 
                           obs_module_text("Live.PokeError"), 
                           obs_module_text("Live.PokeErrorMessage"));
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

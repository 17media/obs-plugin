#include "OneSevenLiveStreamListDock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>

#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveStreamListItem.hpp"
#include "moc_OneSevenLiveStreamListDock.cpp"
#include "plugin-support.h"

OneSevenLiveStreamListDock::OneSevenLiveStreamListDock(QWidget* parent,
                                                       OneSevenLiveConfigManager* configManager_,
                                                       OneSevenLiveStreamingStatus status_)
    : QDockWidget(obs_module_text("Live.StreamList"), parent),
      configManager(configManager_),
      status(status_) {
    setupUi();
    createConnections();
    refreshStreamList();

    // Add delayed initialization to ensure UI element sizes are correctly calculated
    QTimer::singleShot(0, this, [this]() {
        if (emptyContainer && emptyContainer->isVisible()) {
            emptyContainer->setGeometry(widget()->rect());
        }
    });

    connect(this, &QDockWidget::topLevelChanged, this,
            &OneSevenLiveStreamListDock::handleTopLevelChanged);
}

OneSevenLiveStreamListDock::~OneSevenLiveStreamListDock() = default;

void OneSevenLiveStreamListDock::setupUi() {
    QWidget* container = new QWidget(this);
    container->setStyleSheet(
        "QWidget#container {"
        "    background-color: #000000;"
        "    border: none;"
        "    font-family: 'Inter';"
        "    color: #FFFFFF;"
        "    font-style: normal;"
        "}");
    QVBoxLayout* mainLayout = new QVBoxLayout(container);

    // Create live stream list
    streamList = new QListWidget();
    streamList->setStyleSheet(
        "QListWidget {"
        "   background-color: transparent;"
        "   border: none;"
        "}"
        "QListWidget::item {"
        "   background-color: #3C404C;"
        "   border-radius: 6px;"
        "   padding: 0px;"
        "   margin: 0px;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #3a3a4a;"
        "   border: 2px solid #5a5a6a;"
        "   color: white;"
        "}"
        "QListWidget::item:hover:!selected {"
        "    background-color: #454b5a;"
        "}");
    streamList->setResizeMode(QListWidget::Adjust);
    streamList->setWordWrap(true);
    streamList->setSpacing(10);
    mainLayout->addWidget(streamList);

    // Create start streaming button
    startLiveButton = new QPushButton(obs_module_text("Live.Settings.StartLive"));
    startLiveButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #FF0001;"
        "    color: white;"
        "    border-radius: 4px;"
        "    padding: 8px;"
        "   font-weight: 600;"
        "   font-size: 16px;"
        "   line-height: 24px;"
        "}");
    startLiveButton->setFixedWidth(250);
    mainLayout->addWidget(startLiveButton, 0, Qt::AlignHCenter);

    setWidget(container);
}

void OneSevenLiveStreamListDock::createConnections() {
    connect(startLiveButton, &QPushButton::clicked, this,
            &OneSevenLiveStreamListDock::onStartLiveClicked);
}

void OneSevenLiveStreamListDock::updateStreamItem(QListWidgetItem* item,
                                                  const OneSevenLiveStreamInfo& info) {
    QWidget* itemContainer = new QWidget(this);

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    // mainLayout->setSpacing(0);

    // Left layout (title, category, time)
    QVBoxLayout* leftLayout = new QVBoxLayout();
    // leftLayout->setAlignment(Qt::AlignVCenter);
    leftLayout->setSpacing(5);

    QLabel* titleLabel = new QLabel(info.request.caption);
    titleLabel->setStyleSheet(
        "color: white; font-weight: bold; font-size: 14px; font-family: 'Inter'; line-height: "
        "20px;");
    titleLabel->setWordWrap(true);
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QLabel* categoryLabel = new QLabel(info.categoryName);
    categoryLabel->setStyleSheet(
        "color: #d9d9d9; font-weight: bold; font-size: 14px; font-family: 'Inter'; line-height: "
        "20px;");
    categoryLabel->setWordWrap(true);
    categoryLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QLabel* timeLabel = new QLabel(info.createdAt.toString("yyyy-MM-dd hh:mm:ss"));
    timeLabel->setStyleSheet(
        "color: #d9d9d9; font-weight: bold; font-size: 14px; font-family: 'Inter'; line-height: "
        "20px;");
    timeLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    leftLayout->addWidget(titleLabel);
    leftLayout->addWidget(categoryLabel);
    leftLayout->addWidget(timeLabel);

    // Right buttons (edit + delete)
    QWidget* buttonContainer = new QWidget();
    buttonContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(4);
    buttonLayout->setAlignment(Qt::AlignCenter);

    QPushButton* editButton = new QPushButton();
    editButton->setFixedSize(24, 24);
    editButton->setIcon(QIcon(":/resources/edit.svg"));
    editButton->setIconSize(QSize(16, 16));
    editButton->setStyleSheet("background: transparent; border: none;");

    QPushButton* deleteButton = new QPushButton();
    deleteButton->setFixedSize(24, 24);
    deleteButton->setIcon(QIcon(":/resources/delete.svg"));
    deleteButton->setIconSize(QSize(16, 16));
    deleteButton->setStyleSheet("background: transparent; border: none;");

    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deleteButton);

    // Add to main layout
    mainLayout->addLayout(leftLayout);
    mainLayout->addStretch();
    mainLayout->addWidget(buttonContainer);

    itemContainer->setLayout(mainLayout);
    // Adjust size to ensure all content is visible, especially after word wrap
    itemContainer->adjustSize();
    item->setSizeHint(QSize(-1, itemContainer->sizeHint().height()));
    streamList->setItemWidget(item, itemContainer);

    connect(editButton, &QPushButton::clicked, this,
            [this, item, info]() { this->onEditStreamClicked(item, info); });
    connect(deleteButton, &QPushButton::clicked, this,
            [this, item, info]() { this->onDeleteStreamClicked(item, info); });
}

void OneSevenLiveStreamListDock::showEmptyListMessage() {
    // Hide list and start streaming button
    streamList->setVisible(false);
    startLiveButton->setVisible(false);

    // If empty state container already exists, delete it first
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

    // Set empty state container to fill entire Dock area
    emptyContainer->setGeometry(widget()->rect());

    // Create layout manager
    QVBoxLayout* emptyLayout = new QVBoxLayout(emptyContainer);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(20);
    emptyLayout->setContentsMargins(20, 20, 20, 20);

    // Create hint label
    QLabel* emptyLabel = new QLabel(obs_module_text("Live.StreamList.Empty"));
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setWordWrap(true);  // Enable text wrapping
    emptyLabel->setSizePolicy(QSizePolicy::Expanding,
                              QSizePolicy::Preferred);  // Allow horizontal expansion
    emptyLabel->setStyleSheet(
        "QLabel {"
        "    color: #888888;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    padding: 0 10px;"
        "}");

    // Create button to navigate to start streaming
    goToStreamingButton = new QPushButton(obs_module_text("Live.Settings.StartLive"));
    goToStreamingButton->setFixedSize(200, 40);
    goToStreamingButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #4a90e2;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #5a96f8;"
        "}");
    goToStreamingButton->setCursor(Qt::PointingHandCursor);

    // Create button container for centered button display
    QWidget* buttonContainer = new QWidget();
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setAlignment(Qt::AlignCenter);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addWidget(goToStreamingButton);

    // Connect button click signal
    connect(goToStreamingButton, &QPushButton::clicked, this, [this]() {
        // Send signal to notify opening start streaming panel
        emit editLiveClicked(OneSevenLiveStreamInfo());
    });

    // Add to layout
    emptyLayout->addWidget(emptyLabel);
    emptyLayout->addWidget(
        buttonContainer);  // Use buttonContainer instead of adding button directly

    // Show empty state container
    emptyContainer->show();
    emptyContainer->raise();  // Ensure display on top layer
}

void OneSevenLiveStreamListDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);

    if (emptyContainer && emptyContainer->isVisible()) {
        emptyContainer->setGeometry(widget()->rect());
    }

    for (int i = 0; i < streamList->count(); ++i) {
        QListWidgetItem* item = streamList->item(i);
        QWidget* widget = streamList->itemWidget(item);
        if (widget)
            widget->resize(streamList->viewport()->width(), widget->height());
        item->setSizeHint(widget->sizeHint());
    }
}

void OneSevenLiveStreamListDock::setStatus(OneSevenLiveStreamingStatus status_) {
    status = status_;

    // Enable/disable buttons based on streaming status
    bool isNotStarted = (status == OneSevenLiveStreamingStatus::NotStarted);

    // Enable/disable start live button
    if (startLiveButton) {
        startLiveButton->setEnabled(isNotStarted);
    }

    // Enable/disable edit and delete buttons in stream list items
    for (int i = 0; i < streamList->count(); ++i) {
        QListWidgetItem* item = streamList->item(i);
        if (item) {
            QWidget* widget = streamList->itemWidget(item);
            if (widget) {
                // Find edit and delete buttons
                QList<QPushButton*> buttons = widget->findChildren<QPushButton*>();
                for (QPushButton* button : buttons) {
                    button->setEnabled(isNotStarted);
                }
            }
        }
    }

    if (emptyContainer && goToStreamingButton) {
        goToStreamingButton->setEnabled(isNotStarted);
    }
}

void OneSevenLiveStreamListDock::refreshStreamList() {
    streamList->clear();

    if (emptyContainer) {
        // If empty state container already exists, delete it first
        emptyContainer->deleteLater();
        emptyContainer = nullptr;
    }

    std::vector<OneSevenLiveStreamInfo> streamInfoList;
    configManager->loadAllLiveConfig(streamInfoList);

    if (streamInfoList.empty()) {
        // Show empty list hint and navigation button
        showEmptyListMessage();
        // Disable start streaming button as no streams are available for selection
        startLiveButton->setVisible(false);
    } else {
        streamList->setVisible(true);

        // Have stream info, display list normally
        for (const auto& info : streamInfoList) {
            QString title = info.request.caption;
            QString content = info.categoryName;
            QString timestamp = info.createdAt.toString("yyyy-MM-dd hh:mm:ss");

            OneSevenLiveStreamListItem* widget =
                new OneSevenLiveStreamListItem(title, content, timestamp);
            QListWidgetItem* widgetItem = new QListWidgetItem(streamList);
            streamList->setItemWidget(widgetItem, widget);
            widgetItem->setSizeHint(widget->sizeHint());
            widgetItem->setData(Qt::UserRole, QVariant::fromValue(info));
            streamList->addItem(widgetItem);

            connect(widget, &OneSevenLiveStreamListItem::editClicked, [=]() {
                if (status != OneSevenLiveStreamingStatus::NotStarted) {
                    QMessageBox::information(this, obs_module_text("Live.Common.Notice"),
                                             obs_module_text("Live.Common.StreamingInProgress"));
                    return;
                }

                obs_log(LOG_INFO, "onEditStreamClicked %s %s",
                        info.request.caption.toStdString().c_str(),
                        info.streamUuid.toStdString().c_str());
                emit editLiveClicked(info);
            });

            connect(widget, &OneSevenLiveStreamListItem::deleteClicked, [=]() {
                if (status != OneSevenLiveStreamingStatus::NotStarted) {
                    QMessageBox::information(this, obs_module_text("Live.Common.Notice"),
                                             obs_module_text("Live.Common.StreamingInProgress"));
                    return;
                }

                obs_log(LOG_INFO, "onDeleteStreamClicked %s %s",
                        info.request.caption.toStdString().c_str(),
                        info.streamUuid.toStdString().c_str());

                configManager->removeLiveConfig(info.streamUuid.toStdString());
                refreshStreamList();
            });
        }

        // Enable start streaming button
        startLiveButton->setVisible(true);
    }
}

void OneSevenLiveStreamListDock::onEditStreamClicked(
    [[maybe_unused]] QListWidgetItem* item, [[maybe_unused]] const OneSevenLiveStreamInfo& info) {
    if (status != OneSevenLiveStreamingStatus::NotStarted) {
        QMessageBox::information(this, obs_module_text("Live.Common.Notice"),
                                 obs_module_text("Live.Common.StreamingInProgress"));
        return;
    }

    obs_log(LOG_INFO, "onEditStreamClicked %s %s", info.request.caption.toStdString().c_str(),
            info.streamUuid.toStdString().c_str());
    emit editLiveClicked(info);
}

void OneSevenLiveStreamListDock::onDeleteStreamClicked([[maybe_unused]] QListWidgetItem* item,
                                                       const OneSevenLiveStreamInfo& info) {
    if (status != OneSevenLiveStreamingStatus::NotStarted) {
        QMessageBox::information(this, obs_module_text("Live.Common.Notice"),
                                 obs_module_text("Live.Common.StreamingInProgress"));
        return;
    }

    obs_log(LOG_INFO, "onDeleteStreamClicked %s %s", info.request.caption.toStdString().c_str(),
            info.streamUuid.toStdString().c_str());

    configManager->removeLiveConfig(info.streamUuid.toStdString());
    refreshStreamList();
}

void OneSevenLiveStreamListDock::onStartLiveClicked() {
    if (status != OneSevenLiveStreamingStatus::NotStarted) {
        QMessageBox::information(this, obs_module_text("Live.Common.Notice"),
                                 obs_module_text("Live.Common.StreamingInProgress"));
        return;
    }

    // Get currently selected list item
    QListWidgetItem* item = streamList->currentItem();
    if (item) {
        // Get item information
        OneSevenLiveStreamInfo info = item->data(Qt::UserRole).value<OneSevenLiveStreamInfo>();
        emit startLiveClicked(info.request);
    }
}

void OneSevenLiveStreamListDock::handleTopLevelChanged(bool topLevel) {
    if (!topLevel) {
        // Docked
        // Execute adjustSize() or other necessary operations here
        adjustSize();
        // May also need to force update layout or child widget sizes
        for (int i = 0; i < streamList->count(); ++i) {
            QListWidgetItem* item = streamList->item(i);
            QWidget* itemWidget = streamList->itemWidget(item);
            if (itemWidget) {
                itemWidget->adjustSize();
                item->setSizeHint(itemWidget->sizeHint());
            }
        }
    } else {
        // Floating
    }
}

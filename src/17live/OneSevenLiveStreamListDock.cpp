#include "OneSevenLiveStreamListDock.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QTimer>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include "plugin-support.h"

#include "moc_OneSevenLiveStreamListDock.cpp"

OneSevenLiveStreamListDock::OneSevenLiveStreamListDock(QWidget *parent,  OneSevenLiveConfigManager *configManager_)
    : QDockWidget(obs_module_text("Live.StreamList"), parent), configManager(configManager_)
{
    setupUi();
    createConnections();
    refreshStreamList();

    // Add delayed initialization to ensure UI element sizes are correctly calculated
    QTimer::singleShot(0, this, [this]() {
        if (emptyContainer && emptyContainer->isVisible()) {
            emptyContainer->setGeometry(widget()->rect());
        }
    });
}

OneSevenLiveStreamListDock::~OneSevenLiveStreamListDock() = default;

void OneSevenLiveStreamListDock::setupUi()
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    
    // Create live stream list
    streamList = new QListWidget();
    streamList->setStyleSheet(
        "QListWidget {"
        "    background-color: #1e1e1e;"
        "    border-radius: 4px;"
        "}"
        "QListWidget::item {"
        "    background-color: #3C404C;"
        "    border-radius: 6px;"
        "    margin: 4px;"
        "    padding: 2px;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #4a90e2;"
        "    border: 2px solid #6aa8ff;"
        "    color: white;"
        "}"
        "QListWidget::item:hover:!selected {"
        "    background-color: #454b5a;"
        "}"
    );
    mainLayout->addWidget(streamList);
    
    // Create start streaming button
    startLiveButton = new QPushButton(obs_module_text("Live.Settings.StartLive"));
    startLiveButton->setStyleSheet(
        "QPushButton {"
        "    background-color: red;"
        "    color: white;"
        "    border-radius: 4px;"
        "    padding: 8px;"
        "    font-weight: bold;"
        "}"
    );
    mainLayout->addWidget(startLiveButton);
    
    setWidget(container);
}

void OneSevenLiveStreamListDock::createConnections()
{
    connect(startLiveButton, &QPushButton::clicked, this, &OneSevenLiveStreamListDock::onStartLiveClicked);
}

void OneSevenLiveStreamListDock::updateStreamItem(QListWidgetItem* item, const OneSevenLiveStreamInfo& info)
{
    QFrame* frame = new QFrame();
    frame->setMinimumHeight(60);
    frame->setStyleSheet("background-color: #2c2c38; border-radius: 8px;");

    QHBoxLayout* mainLayout = new QHBoxLayout(frame);
    mainLayout->setContentsMargins(12, 8, 12, 8);
    mainLayout->setSpacing(8);

    // Left layout (title, category, time)
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setAlignment(Qt::AlignVCenter);

    QLabel* titleLabel = new QLabel(info.request.caption);
    titleLabel->setStyleSheet("color: white; font-weight: bold; font-size: 14px;");

    QLabel* categoryLabel = new QLabel(info.categoryName);
    categoryLabel->setStyleSheet("color: #aaaaaa; font-size: 12px;");

    QLabel* timeLabel = new QLabel(info.createdAt.toString("yyyy-MM-dd hh:mm:ss"));
    timeLabel->setStyleSheet("color: #888888; font-size: 12px;");

    leftLayout->addWidget(titleLabel);
    leftLayout->addWidget(categoryLabel);
    leftLayout->addWidget(timeLabel);

    // Right buttons (edit + delete)
    QWidget* buttonContainer = new QWidget();
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

    // Limit maximum width to avoid stretching
    frame->setMaximumWidth(streamList->viewport()->width() - 20);

    item->setSizeHint(frame->sizeHint());
    streamList->setItemWidget(item, frame);

    connect(editButton, &QPushButton::clicked, this, [this, item, info]() {
        this->onEditStreamClicked(item, info);
    });
    connect(deleteButton, &QPushButton::clicked, this, [this, item, info]() {
        this->onDeleteStreamClicked(item, info);
    });
}
void OneSevenLiveStreamListDock::showEmptyListMessage()
{
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
        "}"
    );
    
    // Set empty state container to fill entire Dock area
    emptyContainer->setGeometry(widget()->rect());

    // Create layout manager
    QVBoxLayout *emptyLayout = new QVBoxLayout(emptyContainer);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(20);
    emptyLayout->setContentsMargins(20, 20, 20, 20);
    
    // Create hint label
    QLabel *emptyLabel = new QLabel(obs_module_text("Live.StreamList.Empty"));
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setWordWrap(true); // Enable text wrapping
    emptyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); // Allow horizontal expansion
    emptyLabel->setStyleSheet(
        "QLabel {"
        "    color: #888888;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    padding: 0 10px;"
        "}"
    );
    
    // Create button to navigate to start streaming
    QPushButton *goToStreamingButton = new QPushButton(obs_module_text("Live.Settings.StartLive"));
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
        "}"
    );
    goToStreamingButton->setCursor(Qt::PointingHandCursor);
    
    // Create button container for centered button display
    QWidget *buttonContainer = new QWidget();
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setAlignment(Qt::AlignCenter);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addWidget(goToStreamingButton);
    
    // Connect button click signal
    connect(goToStreamingButton, &QPushButton::clicked, this, [this]() {
        // Send signal to notify opening start streaming panel
        emit startLiveClicked(OneSevenLiveRtmpRequest());
    });
    
    // Add to layout
    emptyLayout->addWidget(emptyLabel);
    emptyLayout->addWidget(buttonContainer); // Use buttonContainer instead of adding button directly
    
    // Show empty state container
    emptyContainer->show();
    emptyContainer->raise(); // Ensure display on top layer
}

void OneSevenLiveStreamListDock::resizeEvent(QResizeEvent *event)
{
    QDockWidget::resizeEvent(event);
    
    if (emptyContainer && emptyContainer->isVisible()) {
        emptyContainer->setGeometry(widget()->rect());
    }
}

void OneSevenLiveStreamListDock::refreshStreamList()
{
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
            QListWidgetItem* widgetItem = new QListWidgetItem(streamList);
            widgetItem->setData(Qt::UserRole, QVariant::fromValue(info));
            updateStreamItem(widgetItem, info);
        }
        // Enable start streaming button
        startLiveButton->setVisible(true);
    }
}

void OneSevenLiveStreamListDock::onEditStreamClicked([[maybe_unused]] QListWidgetItem* item, [[maybe_unused]] const OneSevenLiveStreamInfo& info)
{
    obs_log(LOG_INFO, "onEditStreamClicked %s %s", info.request.caption.toStdString().c_str(), info.streamUuid.toStdString().c_str());
    emit editLiveClicked(info);
}

void OneSevenLiveStreamListDock::onDeleteStreamClicked([[maybe_unused]] QListWidgetItem* item, const OneSevenLiveStreamInfo& info)
{
    obs_log(LOG_INFO, "onDeleteStreamClicked %s %s", info.request.caption.toStdString().c_str(), info.streamUuid.toStdString().c_str());

    configManager->removeLiveConfig(info.streamUuid.toStdString());
    refreshStreamList();
}

void OneSevenLiveStreamListDock::onStartLiveClicked()
{
    // Get currently selected list item
    QListWidgetItem* item = streamList->currentItem();
    if (item) {
        // Get item information
        OneSevenLiveStreamInfo info = item->data(Qt::UserRole).value<OneSevenLiveStreamInfo>();
        emit startLiveClicked(info.request);
    }
}

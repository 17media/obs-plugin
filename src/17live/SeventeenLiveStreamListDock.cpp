#include "SeventeenLiveStreamListDock.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include "plugin-support.h"

namespace seventeenlive {

SeventeenLiveStreamListDock::SeventeenLiveStreamListDock(QWidget *parent,  SeventeenLiveConfigManager *configManager_)
    : QDockWidget(obs_module_text("Live.StreamList"), parent), configManager(configManager_)
{
    setupUi();
    createConnections();
    refreshStreamList();
}

SeventeenLiveStreamListDock::~SeventeenLiveStreamListDock() = default;

void SeventeenLiveStreamListDock::setupUi()
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    
    // 创建直播列表
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
        "    background-color: #3d3d3d;"
        "}"
    );
    mainLayout->addWidget(streamList);
    
    // 创建开始直播按钮
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

void SeventeenLiveStreamListDock::createConnections()
{
    connect(startLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamListDock::onStartLiveClicked);
}

void SeventeenLiveStreamListDock::updateStreamItem(QListWidgetItem* item, const StreamInfo& info)
{
    QFrame* frame = new QFrame();
    frame->setMinimumHeight(60);
    frame->setStyleSheet("background-color: #2c2c38; border-radius: 8px;");

    QHBoxLayout* mainLayout = new QHBoxLayout(frame);
    mainLayout->setContentsMargins(12, 8, 12, 8);
    mainLayout->setSpacing(8);

    // 左侧布局（标题、类别、时间）
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setAlignment(Qt::AlignVCenter);

    QLabel* titleLabel = new QLabel(info.title);
    titleLabel->setStyleSheet("color: white; font-weight: bold; font-size: 14px;");

    QLabel* categoryLabel = new QLabel(info.category);
    categoryLabel->setStyleSheet("color: #aaaaaa; font-size: 12px;");

    QLabel* timeLabel = new QLabel(info.startTime.toString("yyyy-MM-dd hh:mm:ss"));
    timeLabel->setStyleSheet("color: #888888; font-size: 12px;");

    leftLayout->addWidget(titleLabel);
    leftLayout->addWidget(categoryLabel);
    leftLayout->addWidget(timeLabel);

    // 右侧按钮（编辑 + 删除）
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

    // 加入主布局
    mainLayout->addLayout(leftLayout);
    mainLayout->addStretch();
    mainLayout->addWidget(buttonContainer);

    // 限制最大宽度，避免拉伸
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
void SeventeenLiveStreamListDock::refreshStreamList()
{
    streamList->clear();
    
    std::vector<SeventeenLiveStreamInfo> streamInfoList;
    configManager->loadAllLiveConfig(streamInfoList);
    for (const auto& item : streamInfoList) {
      StreamInfo info;
      info.title = item.request.caption;
      info.category = item.categoryName;
      info.startTime = item.createdAt;
      info.streamId = item.streamUuid;
      info.request = item.request;

      QListWidgetItem* widgetItem = new QListWidgetItem(streamList);
      updateStreamItem(widgetItem, info);
    }

}

void SeventeenLiveStreamListDock::onEditStreamClicked([[maybe_unused]] QListWidgetItem* item, [[maybe_unused]] const StreamInfo& info)
{
    // TODO: 实现编辑直播的逻辑
}

void SeventeenLiveStreamListDock::onDeleteStreamClicked([[maybe_unused]] QListWidgetItem* item, const StreamInfo& info)
{
    obs_log(LOG_INFO, "onDeleteStreamClicked %s %s", info.title.toStdString().c_str(), info.streamId.toStdString().c_str());

    configManager->removeLiveConfig(info.streamId.toStdString());
    refreshStreamList();
}

void SeventeenLiveStreamListDock::onStartLiveClicked()
{
    // 获取当前选择的list item
    QListWidgetItem* item = streamList->currentItem();
    if (item) {
        // 获取item的信息
        StreamInfo info = item->data(Qt::UserRole).value<StreamInfo>();
        emit startLiveClicked(info.request);
    }
}

} // namespace seventeenlive

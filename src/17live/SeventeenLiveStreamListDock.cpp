#include "SeventeenLiveStreamListDock.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include "plugin-support.h"

#include "moc_SeventeenLiveStreamListDock.cpp"

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
        "    background-color: #4a90e2;"
        "    border: 2px solid #6aa8ff;"
        "    color: white;"
        "}"
        "QListWidget::item:hover:!selected {"
        "    background-color: #454b5a;"
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

void SeventeenLiveStreamListDock::updateStreamItem(QListWidgetItem* item, const SeventeenLiveStreamInfo& info)
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

    QLabel* titleLabel = new QLabel(info.request.caption);
    titleLabel->setStyleSheet("color: white; font-weight: bold; font-size: 14px;");

    QLabel* categoryLabel = new QLabel(info.categoryName);
    categoryLabel->setStyleSheet("color: #aaaaaa; font-size: 12px;");

    QLabel* timeLabel = new QLabel(info.createdAt.toString("yyyy-MM-dd hh:mm:ss"));
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
void SeventeenLiveStreamListDock::showEmptyListMessage()
{
    // 隐藏列表和开始直播按钮
    streamList->setVisible(false);
    startLiveButton->setVisible(false);
    
    // 如果已经存在空状态容器，先删除
    if (emptyContainer) {
        emptyContainer->deleteLater();
    }
    
    // 创建空状态容器
    emptyContainer = new QWidget(widget());
    emptyContainer->setStyleSheet(
        "QWidget {"
        "    background-color: #1e1e1e;"
        "    border-radius: 4px;"
        "}"
    );
    
    // 设置空状态容器填充整个 Dock 区域
    emptyContainer->setGeometry(widget()->rect());

    // 创建布局管理器
    QVBoxLayout *emptyLayout = new QVBoxLayout(emptyContainer);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(20);
    emptyLayout->setContentsMargins(20, 20, 20, 20);
    
    // 创建提示标签
    QLabel *emptyLabel = new QLabel(obs_module_text("Live.StreamList.Empty"));
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setWordWrap(true); // 添加文字换行
    emptyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); // 允许水平方向扩展
    emptyLabel->setStyleSheet(
        "QLabel {"
        "    color: #888888;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    padding: 0 10px;"
        "}"
    );
    
    // 创建跳转到开始直播的按钮
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
    
    // 创建按钮容器用于居中显示按钮
    QWidget *buttonContainer = new QWidget();
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setAlignment(Qt::AlignCenter);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addWidget(goToStreamingButton);
    
    // 连接按钮点击信号
    connect(goToStreamingButton, &QPushButton::clicked, this, [this]() {
        // 发送信号，通知需要打开开始直播面板
        emit startLiveClicked(SeventeenLiveRtmpRequest());
    });
    
    // 添加到布局
    emptyLayout->addWidget(emptyLabel);
    emptyLayout->addWidget(buttonContainer); // 使用buttonContainer代替直接添加按钮
    
    // 显示空状态容器
    emptyContainer->show();
    emptyContainer->raise(); // 确保显示在最上层
}

void SeventeenLiveStreamListDock::resizeEvent(QResizeEvent *event)
{
    QDockWidget::resizeEvent(event);
    
    if (emptyContainer && emptyContainer->isVisible()) {
        emptyContainer->setGeometry(widget()->rect());
    }
}

void SeventeenLiveStreamListDock::refreshStreamList()
{
    streamList->clear();

    if (emptyContainer) {
        // 如果已经存在空状态容器，先删除
        emptyContainer->deleteLater();
        emptyContainer = nullptr;
    }
    
    std::vector<SeventeenLiveStreamInfo> streamInfoList;
    configManager->loadAllLiveConfig(streamInfoList);
    
    if (streamInfoList.empty()) {
        // 显示空列表提示和跳转按钮
        showEmptyListMessage();
        // 禁用开始直播按钮，因为没有可选择的直播
        startLiveButton->setVisible(false);
    } else {
        streamList->setVisible(true);
        
        // 有直播信息，正常显示列表
        for (const auto& info : streamInfoList) {
            QListWidgetItem* widgetItem = new QListWidgetItem(streamList);
            widgetItem->setData(Qt::UserRole, QVariant::fromValue(info));
            updateStreamItem(widgetItem, info);
        }
        // 启用开始直播按钮
        startLiveButton->setVisible(true);
    }
}

void SeventeenLiveStreamListDock::onEditStreamClicked([[maybe_unused]] QListWidgetItem* item, [[maybe_unused]] const SeventeenLiveStreamInfo& info)
{
    obs_log(LOG_INFO, "onEditStreamClicked %s %s", info.request.caption.toStdString().c_str(), info.streamUuid.toStdString().c_str());
    emit editLiveClicked(info);
}

void SeventeenLiveStreamListDock::onDeleteStreamClicked([[maybe_unused]] QListWidgetItem* item, const SeventeenLiveStreamInfo& info)
{
    obs_log(LOG_INFO, "onDeleteStreamClicked %s %s", info.request.caption.toStdString().c_str(), info.streamUuid.toStdString().c_str());

    configManager->removeLiveConfig(info.streamUuid.toStdString());
    refreshStreamList();
}

void SeventeenLiveStreamListDock::onStartLiveClicked()
{
    // 获取当前选择的list item
    QListWidgetItem* item = streamList->currentItem();
    if (item) {
        // 获取item的信息
        SeventeenLiveStreamInfo info = item->data(Qt::UserRole).value<SeventeenLiveStreamInfo>();
        emit startLiveClicked(info.request);
    }
}

#include "SeventeenLiveStreamingDock.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

#include <obs-module.h>
#include "plugin-support.h"

#include "moc_SeventeenLiveStreamingDock.cpp"

namespace seventeenlive {

SeventeenLiveStreamingDock::SeventeenLiveStreamingDock(QWidget *parent, const SeventeenLiveRoomInfo &roomInfo_)
    : QDockWidget(tr("設定"), parent), roomInfo(roomInfo_)
{
    setupUi();
    createConnections();
}

SeventeenLiveStreamingDock::~SeventeenLiveStreamingDock() = default;

void SeventeenLiveStreamingDock::setupUi()
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    
    // 标题输入
    QFormLayout *formLayout = new QFormLayout();
    titleEdit = new QLineEdit();
    formLayout->addRow(tr("標題 (必填)"), titleEdit);
    
    // 类别选择
    categoryCombo = new QComboBox();
    formLayout->addRow(tr("類別"), categoryCombo);
    
    // 标签区域
    QHBoxLayout *tagLayout = new QHBoxLayout();
    tagEdit = new QLineEdit();
    addTagButton = new QPushButton(tr("Add"));
    tagLayout->addWidget(tagEdit);
    tagLayout->addWidget(addTagButton);
    formLayout->addRow(tr("標籤"), tagLayout);
    
    mainLayout->addLayout(formLayout);
    
    // 开播格式
    QGroupBox *streamFormatGroup = new QGroupBox(tr("開播格式"));
    QHBoxLayout *formatLayout = new QHBoxLayout(streamFormatGroup);
    normalStreamRadio = new QRadioButton(tr("標準播出"));
    verticalStreamRadio = new QRadioButton(tr("縱式播出"));
    formatLayout->addWidget(normalStreamRadio);
    formatLayout->addWidget(verticalStreamRadio);
    if (roomInfo.landscape) {
        normalStreamRadio->setChecked(true);
    } else {
        verticalStreamRadio->setChecked(true);
    }
    mainLayout->addWidget(streamFormatGroup);
    
    // 活动相关
    activityCombo = new QComboBox();
    // 添加一个默认选项
    activityCombo->addItem(tr("無特定"), -1);
    
    // 从roomInfo.eventList添加活动选项
    for (const auto& event : roomInfo.eventList) {
        QString eventName = event.name;
        if (eventName.isEmpty()) {
            eventName = tr("活動 ") + QString::number(event.ID);
        }
        activityCombo->addItem(eventName, event.ID);
    }
    
    formLayout->addRow(tr("活動"), activityCombo);
    
    
    customActivityCombo = new QComboBox();
    formLayout->addRow(tr("自訂活動 (選填)"), customActivityCombo);
    
    viewerLimitCombo = new QComboBox();
    formLayout->addRow(tr("觀眾限定觀看"), viewerLimitCombo);
    
    // 开关选项
    archiveStreamCheck = new QCheckBox(tr("典藏直播"));
    archiveStreamCheck->setToolTip(tr("儲存直播內容7天，並且只有您本人可以觀看。\n(限制：不超過8小時，PK/群聊內容皆不支持。)"));
    archiveStreamCheck->setChecked(roomInfo.archiveConfig.autoRecording);
    mainLayout->addWidget(archiveStreamCheck);
    
    autoPreviewCheck = new QCheckBox(tr("自動發布預覽"));
    autoPreviewCheck->setToolTip(tr("自動以影片的方式設定在個人頁面「如何收看」。\n17LIVE app的頁面也編輯該頁面的相關內容（如標題、標籤等）。"));
    autoPreviewCheck->setChecked(roomInfo.archiveConfig.autoPublish);
    mainLayout->addWidget(autoPreviewCheck);
    
    // 预览设置
//    QGroupBox *previewGroup = new QGroupBox(tr("預覽設定"));
//    QFormLayout *previewLayout = new QFormLayout(previewGroup);    
    
    // 剪辑身份
    clipIdentityCombo = new QComboBox();
    clipIdentityCombo->addItem(tr("關閉"), 0);
    clipIdentityCombo->addItem(tr("所有人"), 1);
    clipIdentityCombo->addItem(tr("粉絲"), 2);
    
    // 设置为不可编辑
    clipIdentityCombo->setEditable(false);
    
    // 设置默认值
    int clipPermission = roomInfo.archiveConfig.clipPermission;
    clipIdentityCombo->setCurrentIndex(clipIdentityCombo->findData(clipPermission));
    
    formLayout->addRow(tr("允許直播剪輯身份"), clipIdentityCombo);
    
    // 虚拟主播选项
    virtualStreamerCheck = new QCheckBox(tr("是，我是虛擬主播。"));
    mainLayout->addWidget(virtualStreamerCheck);
    
    // 底部按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    createStreamButton = new QPushButton(tr("建立直播"));
    createAndStartButton = new QPushButton(tr("建立直播並開始推流"));
    createAndStartButton->setStyleSheet("background-color: red; color: white;");
    buttonLayout->addWidget(createStreamButton);
    buttonLayout->addWidget(createAndStartButton);
    mainLayout->addLayout(buttonLayout);
    
    setWidget(container);
}

void SeventeenLiveStreamingDock::createConnections()
{
    connect(addTagButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onAddTagClicked);
    connect(createStreamButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateStreamClicked);
    connect(createAndStartButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateAndStartStreamClicked);
}

void SeventeenLiveStreamingDock::onAddTagClicked()
{
    // TODO: 实现添加标签的逻辑
}

void SeventeenLiveStreamingDock::onCreateStreamClicked()
{
    SeventeenLiveRtmpRequest request;
    gatherRtmpRequest(request);

    emit createStreamClicked(request);
}

void SeventeenLiveStreamingDock::onCreateAndStartStreamClicked()
{
    SeventeenLiveRtmpRequest request;
    gatherRtmpRequest(request);

    emit createAndStartStreamClicked(request);
}

void SeventeenLiveStreamingDock::onStopStreamingClicked()
{
    emit stopStreamingClicked();
}

void SeventeenLiveStreamingDock::onStopPushStreamingClicked()
{
    emit stopPushStreamingClicked();
}

void SeventeenLiveStreamingDock::gatherRtmpRequest(SeventeenLiveRtmpRequest &request)
{
    request.userID = roomInfo.userID;
    request.caption = titleEdit->text();
    request.device = "OBS";
    int eventID = activityCombo->currentData().toInt();
    request.eventID = eventID;
    request.hashtags = tagEdit->text().split(",");
    request.landscape = normalStreamRadio->isChecked();
    request.streamerType = roomInfo.streamerType;
    request.subtabID = categoryCombo->currentText();
    request.archiveConfig.autoRecording = archiveStreamCheck->isChecked();
    request.archiveConfig.autoPublish = autoPreviewCheck->isChecked();
    request.archiveConfig.clipPermission = clipIdentityCombo->currentData().toInt();
    request.vliverInfo.vliverModel = virtualStreamerCheck->isChecked();
}

void SeventeenLiveStreamingDock::updateStreamingStatus(bool streaming)
{
    obs_log(LOG_INFO, "updateStreamingStatus: %d", streaming);
    if (streaming) {
        obs_log(LOG_INFO, "start streaming");
        // change createStreamButton text to "停止直播"
        createStreamButton->setText(tr("停止直播"));
        disconnect(createStreamButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateStreamClicked);
        connect(createStreamButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onStopStreamingClicked);
        // change createAndStartButton text to "停止推流"
        createAndStartButton->setText(tr("停止推流"));
        disconnect(createAndStartButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateAndStartStreamClicked);
        connect(createAndStartButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onStopPushStreamingClicked);
    } else {
        obs_log(LOG_INFO, "stop streaming");
        // change createStreamButton text to "建立直播"
        createStreamButton->setText(tr("建立直播"));
        disconnect(createStreamButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onStopStreamingClicked);
        connect(createStreamButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateStreamClicked);
        // change createAndStartButton text to "建立直播並開始推流"
        createAndStartButton->setText(tr("建立直播並開始推流"));
        disconnect(createAndStartButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onStopPushStreamingClicked);
        connect(createAndStartButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateAndStartStreamClicked);
    }
}

} // namespace seventeenlive

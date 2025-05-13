#include "SeventeenLiveStreamingDock.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include "plugin-support.h"

#include "api/SeventeenLiveApiWrappers.hpp"
#include "SeventeenLiveConfigManager.hpp"

#include "moc_SeventeenLiveStreamingDock.cpp"

namespace seventeenlive {

SeventeenLiveStreamingDock::SeventeenLiveStreamingDock(QWidget *parent, const SeventeenLiveRoomInfo &roomInfo_, SeventeenLiveApiWrappers *apiWrapper_, SeventeenLiveConfigManager *configManager_)
    : QDockWidget(tr("設定"), parent), roomInfo(roomInfo_), apiWrapper(apiWrapper_), configManager(configManager_) 
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
    SeventeenLiveConfigStreamerResponse response;
    configManager->getConfigStreamer(response);
    for (const auto& subtab : response.subtabs) {
        categoryCombo->addItem(subtab.displayName, subtab.ID);
    }
    categoryCombo->setCurrentIndex(0);
    
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
    
    
    // customActivityCombo = new QComboBox();
    // formLayout->addRow(tr("自訂活動 (選填)"), customActivityCombo);
    
    // viewerLimitCombo = new QComboBox();
    // formLayout->addRow(tr("觀眾限定觀看"), viewerLimitCombo);
    
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
    saveConfigButton = new QPushButton(tr("存儲設定"));
    createLiveButton = new QPushButton(tr("開始直播"));
    createLiveButton->setStyleSheet("background-color: red; color: white;");
    buttonLayout->addWidget(saveConfigButton);
    buttonLayout->addWidget(createLiveButton);
    mainLayout->addLayout(buttonLayout);
    
    setWidget(container);
}

void SeventeenLiveStreamingDock::createConnections()
{
    connect(addTagButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onAddTagClicked);
    connect(saveConfigButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onSaveConfigClicked);
    connect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateLiveClicked);
}

void SeventeenLiveStreamingDock::onAddTagClicked()
{
    // TODO: 实现添加标签的逻辑
}

void SeventeenLiveStreamingDock::onSaveConfigClicked()
{
    SeventeenLiveRtmpRequest request;
    gatherRtmpRequest(request);

    // TODO: 实现保存配置的逻辑
}

void SeventeenLiveStreamingDock::onCreateLiveClicked()
{
    obs_log(LOG_INFO, "onCreateLiveClicked");

    // 创建直播
    SeventeenLiveRtmpRequest request;
    gatherRtmpRequest(request);

    SeventeenLiveRtmpResponse response;
    if (!apiWrapper->CreateRtmp(request, response)) {
        obs_log(LOG_ERROR, "Failed to create stream");
        return;
    }

    QString streamUrl;
    QString streamKey;

    int questionIndex = response.rtmpURL.indexOf(request.userID + "?");
    if (questionIndex != -1) {
        streamUrl = response.rtmpURL.left(questionIndex - 1);
        streamKey = response.rtmpURL.mid(questionIndex);
    } else {
        obs_log(LOG_ERROR, "Failed to parse stream url");
        return;
    }
    
    obs_log(LOG_INFO, "streamUrl: %s", streamUrl.toStdString().c_str());
    obs_log(LOG_INFO, "streamKey: %s", streamKey.toStdString().c_str());

    configManager->setStreamingInfo(response.liveStreamID.toStdString(), streamUrl.toStdString(), streamKey.toStdString());

    saveStreamingSettings(response.liveStreamID.toStdString(), streamUrl.toStdString(), streamKey.toStdString());

    // 开始直播
    if (!apiWrapper->StartStream(response.liveStreamID.toStdString(), request.userID.toStdString())) {
        obs_log(LOG_ERROR, "Failed to start stream");
        return;
    }

    updateLiveStatus(SeventeenLiveStreamingStatus::Live);
}

void SeventeenLiveStreamingDock::onDeleteLiveClicked()
{
    obs_log(LOG_INFO, "onDeleteLiveClicked");

    // 处理停止流的逻辑
    stopStreaming();

    std::string currUserID;
    std::string currLiveStreamID;
    configManager->getConfigValue("UserID", currUserID);
    configManager->getConfigValue("LiveStreamID", currLiveStreamID);

    // 发送关闭直播请求
    SeventeenLiveCloseLiveRequest request;
    request.reason = "normalEnd";
    request.userID = QString::fromStdString(currUserID);

    if (!apiWrapper->StopStream(currLiveStreamID, request)) {
        obs_log(LOG_ERROR, "Failed to stop stream");
        return;
    }

    configManager->clearStreamingInfo();

    updateLiveStatus(SeventeenLiveStreamingStatus::NotStarted);
}

void SeventeenLiveStreamingDock::saveStreamingSettings(const std::string &liveStreamID, const std::string &streamUrl, const std::string &streamKey)
{
    // 处理开始流的逻辑
    obs_log(LOG_INFO, "saveStreamingSettings %s", liveStreamID.c_str());

    // 获取OBS服务
    obs_service_t* service = obs_service_create("rtmp_custom", "default_service", NULL, NULL);
    
    // 设置流媒体URL和密钥
    obs_data_t *settings = obs_service_get_settings(service);
    obs_log(LOG_INFO, "streamUrl: %s", streamUrl.c_str());
    obs_log(LOG_INFO, "streamKey: %s", streamKey.c_str());
    obs_data_set_string(settings, "server", streamUrl.c_str());
    obs_data_set_string(settings, "key", streamKey.c_str());
    
    // 应用设置
    obs_service_update(service, settings);
    obs_data_release(settings);

    obs_frontend_set_streaming_service(service);

    obs_frontend_save_streaming_service();

    // 释放资源
    obs_service_release(service);
}

void SeventeenLiveStreamingDock::stopStreaming()
{
    // 处理停止流的逻辑
    obs_log(LOG_INFO, "stopStreaming");

    if (!obs_frontend_streaming_active()) {
        obs_log(LOG_INFO, "Streaming is not active");
        return;
    }

    obs_frontend_streaming_stop();
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
    request.subtabID = "newbie"; // TODO: categoryCombo->currentData().toString();
    request.archiveConfig.autoRecording = archiveStreamCheck->isChecked();
    request.archiveConfig.autoPublish = autoPreviewCheck->isChecked();
    request.archiveConfig.clipPermission = clipIdentityCombo->currentData().toInt();
    request.vliverInfo.vliverModel = virtualStreamerCheck->isChecked();
}

void SeventeenLiveStreamingDock::updateLiveButton(bool isLive)
{   
    obs_log(LOG_INFO, "updateLiveButton: %d", isLive);
    if (isLive) {
        // change text to "停止直播"
        createLiveButton->setText(tr("停止直播"));
        disconnect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateLiveClicked);
        connect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onDeleteLiveClicked);
    } else {
        // change text to "建立直播"
        createLiveButton->setText(tr("開始直播"));
        disconnect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onDeleteLiveClicked);
        connect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateLiveClicked);
    }
}

void SeventeenLiveStreamingDock::updateLiveStatus(SeventeenLiveStreamingStatus status)
{
    obs_log(LOG_INFO, "updateLiveStatus: %d", static_cast<int>(status));

    updateLiveButton(status == SeventeenLiveStreamingStatus::Live);
}

} // namespace seventeenlive

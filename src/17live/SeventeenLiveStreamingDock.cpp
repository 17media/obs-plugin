#include "SeventeenLiveStreamingDock.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QUuid>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include "plugin-support.h"

#include "utility/Meta.hpp"
#include "api/SeventeenLiveApiWrappers.hpp"
#include "SeventeenLiveConfigManager.hpp"

#include "moc_SeventeenLiveStreamingDock.cpp"

namespace seventeenlive {

SeventeenLiveStreamingDock::SeventeenLiveStreamingDock(QWidget *parent, const SeventeenLiveRoomInfo &roomInfo_, SeventeenLiveApiWrappers *apiWrapper_, SeventeenLiveConfigManager *configManager_)
    : QDockWidget(obs_module_text("Live.Settings"), parent), roomInfo(roomInfo_), apiWrapper(apiWrapper_), configManager(configManager_) 
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
    // 设置为纵向布局，标签在字段上方
    formLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // 设置标签左对齐
    formLayout->setLabelAlignment(Qt::AlignLeft);
    // 设置字段增长方式
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    
    titleEdit = new QLineEdit();
    formLayout->addRow(obs_module_text("Live.Settings.Title"), titleEdit);
    
    // 类别选择
    categoryCombo = new QComboBox();
    
    SeventeenLiveConfigStreamerResponse response;
    configManager->getConfigStreamer(response);
    
    for (const auto& subtab : response.subtabs) {
        categoryCombo->addItem(subtab.displayName, subtab.ID);
    }
    categoryCombo->setCurrentIndex(0);

    formLayout->addRow(obs_module_text("Live.Settings.Category"), categoryCombo);
    
    // 标签区域
    QHBoxLayout *tagLayout = new QHBoxLayout();
    tagEdit = new QLineEdit();
    addTagButton = new QPushButton(obs_module_text("Live.Settings.AddTag"));
    tagLayout->addWidget(tagEdit);
    tagLayout->addWidget(addTagButton);
    formLayout->addRow(obs_module_text("Live.Settings.Tags"), tagLayout);

    mainLayout->addLayout(formLayout);
    
    // 开播格式
    QGroupBox *streamFormatGroup = new QGroupBox(obs_module_text("Live.Settings.Layout"));
    QHBoxLayout *formatLayout = new QHBoxLayout(streamFormatGroup);
    normalStreamRadio = new QRadioButton(obs_module_text("Live.Settings.Layout.Landscape"));
    verticalStreamRadio = new QRadioButton(obs_module_text("Live.Settings.Layout.Portrait"));
    formatLayout->addWidget(normalStreamRadio);
    formatLayout->addWidget(verticalStreamRadio);
    if (roomInfo.landscape) {
        normalStreamRadio->setChecked(true);
    } else {
        verticalStreamRadio->setChecked(true);
    }
    
    mainLayout->addWidget(streamFormatGroup);
    
    // 活动相关
    QVBoxLayout *eventContainer = new QVBoxLayout();
    
    // 标题
    QLabel *eventLabel = new QLabel(obs_module_text("Live.Settings.Event"));
    eventContainer->addWidget(eventLabel);
        
    // 下拉框
    activityCombo = new QComboBox();
    
    for (const auto& event : response.event.events) {
        QString eventName = event.name;
        if (eventName.isEmpty()) {
            continue; // Skip if name is empty or null
        }
        activityCombo->addItem(eventName, event.ID);
    }
    activityCombo->setCurrentIndex(0);
    eventContainer->addWidget(activityCombo);
        
    // 创建提示 Label 并靠右对齐
    QHBoxLayout *hintLayout = new QHBoxLayout();
    QLabel *hintLabel = new QLabel(obs_module_text("Live.Settings.Event.Tip"));
    hintLabel->setStyleSheet("color: gray; font-size: 12px;");
    hintLayout->addStretch(); // 添加弹性空间，使提示文字靠右
    hintLayout->addWidget(hintLabel);
    eventContainer->addLayout(hintLayout);
        
    mainLayout->addLayout(eventContainer);

    
    // 开关选项 - 修改为Switch组件
    QHBoxLayout *archiveLayout = new QHBoxLayout();
    QVBoxLayout *archiveLabelLayout = new QVBoxLayout();
    
    // 左侧标题和提示信息
    QLabel *archiveLabel = new QLabel(obs_module_text("Live.Settings.Archive.Record"));
    QLabel *archiveTip = new QLabel(obs_module_text("Live.Settings.Archive.Record.Tip"));
    archiveTip->setStyleSheet("color: gray; font-size: 12px;");
    
    archiveLabelLayout->addWidget(archiveLabel);
    archiveLabelLayout->addWidget(archiveTip);
    archiveLabelLayout->setSpacing(2); // 调整标题和提示之间的间距
    
    // 右侧Switch组件
    archiveStreamCheck = new QCheckBox();
    archiveStreamCheck->setChecked(roomInfo.archiveConfig.autoRecording);
    
    // 将左右两部分添加到水平布局中
    archiveLayout->addLayout(archiveLabelLayout);
    archiveLayout->addStretch(); // 添加弹性空间，使Switch靠右
    archiveLayout->addWidget(archiveStreamCheck);
    
    mainLayout->addLayout(archiveLayout);
    
    // 同样修改自动预览选项
    QHBoxLayout *previewLayout = new QHBoxLayout();
    QVBoxLayout *previewLabelLayout = new QVBoxLayout();
    
    QLabel *previewLabel = new QLabel(obs_module_text("Live.Settings.Archive.AutoPublish"));
    QLabel *previewTip = new QLabel(obs_module_text("Live.Settings.Archive.AutoPublish.Tip"));
    previewTip->setStyleSheet("color: gray; font-size: 12px;");
    
    previewLabelLayout->addWidget(previewLabel);
    previewLabelLayout->addWidget(previewTip);
    previewLabelLayout->setSpacing(2);
    
    autoPreviewCheck = new QCheckBox();
    autoPreviewCheck->setChecked(roomInfo.archiveConfig.autoPublish);
    
    previewLayout->addLayout(previewLabelLayout);
    previewLayout->addStretch();
    previewLayout->addWidget(autoPreviewCheck);
    
    mainLayout->addLayout(previewLayout);

    QVBoxLayout *clipLayout = new QVBoxLayout();
    QLabel *clipLabel = new QLabel(obs_module_text("Live.Settings.Archive.ClipPermission"));
    clipLayout->addWidget(clipLabel);

    QLabel *clipTip = new QLabel(obs_module_text("Live.Settings.Archive.ClipPermission.Tip"));
    clipTip->setStyleSheet("color: gray; font-size: 12px;");
    clipLayout->addWidget(clipTip);
    
    // 剪辑身份
    clipIdentityCombo = new QComboBox();
    QList<SeventeenLiveMetaValueLabel> clipIdentityList;
    getMetaValueLabelList("ClipPermissions", clipIdentityList);
    for (const auto& item : clipIdentityList) {
        clipIdentityCombo->addItem(item.label, item.value);
    }
    
    // 设置为不可编辑
    clipIdentityCombo->setEditable(false);
    
    // 设置默认值
    int clipPermission = roomInfo.archiveConfig.clipPermission;
    clipIdentityCombo->setCurrentIndex(clipIdentityCombo->findData(clipPermission));
    
    clipLayout->addWidget(clipIdentityCombo);
    clipLayout->setSpacing(2);

    mainLayout->addLayout(clipLayout);
    
    
    // 虚拟主播选项
    QVBoxLayout *vliverLayout = new QVBoxLayout();

    QLabel *vliverLabel = new QLabel(obs_module_text("Live.Settings.VirtualLiver.Title"));
    vliverLayout->addWidget(vliverLabel);

    QLabel *vliverTip = new QLabel(obs_module_text("Live.Settings.VirtualLiver.Tip"));
    vliverTip->setStyleSheet("color: gray; font-size: 12px;");
    vliverLayout->addWidget(vliverTip);

    virtualStreamerCheck = new QCheckBox(obs_module_text("Live.Settings.VirtualLiver"));
    vliverLayout->addWidget(virtualStreamerCheck);
    
    vliverLayout->setSpacing(2);

    mainLayout->addLayout(vliverLayout);

    // 底部按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    saveConfigButton = new QPushButton(obs_module_text("Live.Settings.Save"));
    saveConfigButton->setStyleSheet("background-color: red; color: white;");
    createLiveButton = new QPushButton(obs_module_text("Live.Settings.StartLive"));
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

    SeventeenLiveStreamInfo streamInfo;
    streamInfo.categoryName = categoryCombo->currentText();
    streamInfo.createdAt = QDateTime::currentDateTime();
    streamInfo.request = request;
    streamInfo.streamUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    if (!configManager->saveLiveConfig(streamInfo)) {
        obs_log(LOG_ERROR, "Failed to save stream info");
        return;
    }

    emit streamInfoSaved();

    // 弹出提示框说明保存成功
    QMessageBox::information(this, obs_module_text("Live.Settings.Save.Title"), obs_module_text("Live.Settings.Save.Success"));
}

void SeventeenLiveStreamingDock::onCreateLiveClicked()
{
    obs_log(LOG_INFO, "onCreateLiveClicked");

    // 创建直播
    SeventeenLiveRtmpRequest request;
    gatherRtmpRequest(request);

    startStreaming(request);
}

void SeventeenLiveStreamingDock::createLiveWithRequest(const SeventeenLiveRtmpRequest &request)
{
    obs_log(LOG_INFO, "createLiveWithRequest");

    populateRtmpRequest(request);

    startStreaming(request);
}

void SeventeenLiveStreamingDock::startStreaming(const SeventeenLiveRtmpRequest& request)
{
    obs_log(LOG_INFO, "startStreaming");

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

void SeventeenLiveStreamingDock::populateRtmpRequest(const SeventeenLiveRtmpRequest &request)
{
    // 注意：userID 和 streamerType 一般不可编辑，只展示在界面或保持同步
    roomInfo.userID = request.userID;
    roomInfo.streamerType = request.streamerType;

    titleEdit->setText(request.caption);
    
    // 如果你的 activityCombo 是用 setItemData 设置的 eventID，这里要查找对应索引
    int eventIndex = activityCombo->findData(QVariant(request.eventID));
    if (eventIndex >= 0) {
        activityCombo->setCurrentIndex(eventIndex);
    }

    tagEdit->setText(request.hashtags.join(","));

    if (request.landscape) {
        normalStreamRadio->setChecked(true);
    } else {
        verticalStreamRadio->setChecked(true);
    }

    int categoryIndex = categoryCombo->findData(QVariant(request.subtabID));
    if (categoryIndex >= 0) {
        categoryCombo->setCurrentIndex(categoryIndex);
    }

    archiveStreamCheck->setChecked(request.archiveConfig.autoRecording);
    autoPreviewCheck->setChecked(request.archiveConfig.autoPublish);

    int clipIndex = clipIdentityCombo->findData(QVariant(request.archiveConfig.clipPermission));
    if (clipIndex >= 0) {
        clipIdentityCombo->setCurrentIndex(clipIndex);
    }

    virtualStreamerCheck->setChecked(request.vliverInfo.vliverModel);
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
    request.subtabID = categoryCombo->currentData().toString();
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
        createLiveButton->setText(obs_module_text("Live.Settings.StopLive"));
        disconnect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateLiveClicked);
        connect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onDeleteLiveClicked);
    } else {
        // change text to "建立直播"
        createLiveButton->setText(obs_module_text("Live.Settings.StartLive"));
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

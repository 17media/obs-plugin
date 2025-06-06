#include "SeventeenLiveStreamingDock.hpp"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QScrollArea>
#include <QUuid>
#include <QThread>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include "plugin-support.h"

#include "utility/Meta.hpp"
#include "api/SeventeenLiveApiWrappers.hpp"
#include "SeventeenLiveConfigManager.hpp"
#include "utility/Common.hpp"

#include "moc_SeventeenLiveStreamingDock.cpp"

SeventeenLiveStreamingDock::SeventeenLiveStreamingDock(QWidget *parent, SeventeenLiveApiWrappers *apiWrapper_, SeventeenLiveConfigManager *configManager_)
    : QDockWidget(obs_module_text("Live.Settings"), parent), apiWrapper(apiWrapper_), configManager(configManager_) 
{
    setupUi();
    createConnections();
}

SeventeenLiveStreamingDock::~SeventeenLiveStreamingDock() = default;

void SeventeenLiveStreamingDock::setupUi()
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(container);

    // 创建滚动区域
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true); // 允许内容调整大小
    scrollArea->setFrameShape(QFrame::NoFrame); // 移除边框
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded); // 需要时显示垂直滚动条
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 禁用水平滚动条
    
    // 设置最大高度（可以根据需要调整）
    scrollArea->setMaximumHeight(800); // 设置最大高度为800像素
    
    // 创建加载状态覆盖层 - 注意这里将父部件改为scrollArea
    loadingOverlay = new QWidget(scrollArea->viewport());
    loadingOverlay->setStyleSheet("background-color: rgba(0, 0, 0, 120);");
    loadingOverlay->setAttribute(Qt::WA_TranslucentBackground);
    loadingOverlay->setVisible(false); // 初始不可见
    
    QVBoxLayout *overlayLayout = new QVBoxLayout(loadingOverlay);
    overlayLayout->setAlignment(Qt::AlignCenter);
    
    loadingLabel = new QLabel(obs_module_text("Live.Settings.Loading"));
    loadingLabel->setStyleSheet("color: white; font-size: 16px;");
    loadingLabel->setAlignment(Qt::AlignCenter);
    
    loadingProgress = new QProgressBar();
    loadingProgress->setRange(0, 0); // 设置为不确定进度
    loadingProgress->setTextVisible(false);
    loadingProgress->setFixedSize(200, 10);
    
    overlayLayout->addWidget(loadingLabel);
    overlayLayout->addWidget(loadingProgress);
    
    // 标题输入
    QFormLayout *formLayout = new QFormLayout();
    // 设置为纵向布局，标签在字段上方
    formLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // 设置标签左对齐
    formLayout->setLabelAlignment(Qt::AlignLeft);
    // 设置字段增长方式
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    
    QLabel* titleLabel = new QLabel();
    titleLabel->setText(QString("<span style='color:red;'>*</span><span style='color:white;'>%1</span>").arg(obs_module_text("Live.Settings.Title")));

    titleEdit = new QLineEdit();
    formLayout->addRow(titleLabel, titleEdit);
    
    // 类别选择
    categoryCombo = new QComboBox();

    QLabel* categoryLabel = new QLabel();
    categoryLabel->setText(QString("<span style='color:red;'>*</span><span style='color:white;'>%1</span>").arg(obs_module_text("Live.Settings.Category")));

    formLayout->addRow(categoryLabel, categoryCombo);
    
    // 标签区域
    QVBoxLayout *tagContainer = new QVBoxLayout();
    
    // 输入框和添加按钮
    QHBoxLayout *tagInputLayout = new QHBoxLayout();
    tagEdit = new QLineEdit();
    tagEdit->setPlaceholderText(obs_module_text("Live.Settings.Tags.Placeholder"));
    addTagButton = new QPushButton(obs_module_text("Live.Settings.AddTag"));
    tagInputLayout->addWidget(tagEdit);
    tagInputLayout->addWidget(addTagButton);
    tagContainer->addLayout(tagInputLayout);
    
    // 标签显示区域
    tagsContainer = new QWidget();
    tagsLayout = new QHBoxLayout(tagsContainer);
    tagsLayout->setContentsMargins(0, 5, 0, 0);
    tagsLayout->setSpacing(5);
    tagsLayout->setAlignment(Qt::AlignLeft);
    tagContainer->addWidget(tagsContainer);
    
    formLayout->addRow(obs_module_text("Live.Settings.Tags"), tagContainer);

    mainLayout->addLayout(formLayout);
    
    // 开播格式
    QGroupBox *streamFormatGroup = new QGroupBox(obs_module_text("Live.Settings.Layout"));
    QHBoxLayout *formatLayout = new QHBoxLayout(streamFormatGroup);
    normalStreamRadio = new QRadioButton(obs_module_text("Live.Settings.Layout.Landscape"));
    verticalStreamRadio = new QRadioButton(obs_module_text("Live.Settings.Layout.Portrait"));
    formatLayout->addWidget(verticalStreamRadio);
    formatLayout->addWidget(normalStreamRadio);
    verticalStreamRadio->setChecked(true);
    
    mainLayout->addWidget(streamFormatGroup);
    
    // 活动相关
    QVBoxLayout *eventContainer = new QVBoxLayout();
    
    // 标题
    QLabel *eventLabel = new QLabel(obs_module_text("Live.Settings.Event"));
    eventContainer->addWidget(eventLabel);
        
    // 下拉框
    activityCombo = new QComboBox();
    eventContainer->addWidget(activityCombo);
        
    // 创建提示 Label 并靠右对齐
    QHBoxLayout *hintLayout = new QHBoxLayout();
    QLabel *hintLabel = new QLabel(obs_module_text("Live.Settings.Event.Tip"));
    hintLabel->setStyleSheet("color: gray; font-size: 12px;");
    hintLayout->addStretch(); // 添加弹性空间，使提示文字靠右
    hintLayout->addWidget(hintLabel);
    eventContainer->addLayout(hintLayout);
        
    mainLayout->addLayout(eventContainer);

    // 直播模式
    QVBoxLayout *broadcastModeLayout = new QVBoxLayout();
    broadcastModeLabel = new QLabel(obs_module_text("Live.Settings.BroadcastMode"));
    broadcastModeLabel->setStyleSheet("font-weight: bold;");
    broadcastModeLayout->addWidget(broadcastModeLabel);
    
    // 战队限定观看 - 可折叠部分
    // 1. 头部（标题和折叠按钮）
    armyOnlyHeader = new QWidget();
    armyOnlyHeaderLayout = new QHBoxLayout(armyOnlyHeader);
    armyOnlyHeaderLayout->setContentsMargins(0, 10, 0, 10);
    
    armyOnlyLabel = new QLabel(obs_module_text("Live.Settings.ArmyOnly"));
    armyOnlyToggleButton = new QPushButton();
    armyOnlyToggleButton->setIcon(QIcon(":/resources/arrow-down.svg")); 
    armyOnlyToggleButton->setStyleSheet("QPushButton { border: none; background-color: transparent; }");
    armyOnlyToggleButton->setFixedSize(24, 24);
    
    armyOnlyHeaderLayout->addWidget(armyOnlyLabel);
    armyOnlyHeaderLayout->addStretch();
    armyOnlyHeaderLayout->addWidget(armyOnlyToggleButton);
    
    // 2. 内容容器（默认隐藏）
    armyOnlyContainer = new QWidget();
    armyOnlyContainerLayout = new QVBoxLayout(armyOnlyContainer);
    armyOnlyContainerLayout->setContentsMargins(20, 0, 0, 10); // 左侧缩进
    
    // 战队限定观看开关
    QHBoxLayout *armyOnlyCheckLayout = new QHBoxLayout();
    QLabel *armyOnlyCheckLabel = new QLabel(obs_module_text("Live.Settings.ArmyOnly"));
    armyOnlyCheck = new QCheckBox();
    
    armyOnlyCheckLayout->addWidget(armyOnlyCheckLabel);
    armyOnlyCheckLayout->addStretch();
    armyOnlyCheckLayout->addWidget(armyOnlyCheck);
    
    armyOnlyContainerLayout->addLayout(armyOnlyCheckLayout);
    
    // 用户条件
    QVBoxLayout *userConditionLayout = new QVBoxLayout();
    QLabel *userConditionLabel = new QLabel(obs_module_text("Live.Settings.UserCondition"));
    userConditionLayout->addWidget(userConditionLabel);
    
    requiredArmyRankCombo = new QComboBox();
    // requiredArmyRankCombo->addItem(obs_module_text("Live.Settings.UserCondition.AllLevels"), 1);
    requiredArmyRankCombo->setEditable(false);
    userConditionLayout->addWidget(requiredArmyRankCombo);
    
    armyOnlyContainerLayout->addLayout(userConditionLayout);
    
    // 显示在热门页
    QHBoxLayout *showInHotPageLayout = new QHBoxLayout();
    QLabel *showInHotPageLabel = new QLabel(obs_module_text("Live.Settings.ShowInHotPage"));
    showInHotPageCheck = new QCheckBox();
    
    showInHotPageLayout->addWidget(showInHotPageLabel);
    showInHotPageLayout->addStretch();
    showInHotPageLayout->addWidget(showInHotPageCheck);
    
    armyOnlyContainerLayout->addLayout(showInHotPageLayout);
    
    // 开播通知
    QHBoxLayout *liveNotificationLayout = new QHBoxLayout();
    QLabel *liveNotificationLabel = new QLabel(obs_module_text("Live.Settings.LiveNotification"));
    liveNotificationCheck = new QCheckBox();
    
    liveNotificationLayout->addWidget(liveNotificationLabel);
    liveNotificationLayout->addStretch();
    liveNotificationLayout->addWidget(liveNotificationCheck);
    
    armyOnlyContainerLayout->addLayout(liveNotificationLayout);
    
    // 初始状态：折叠
    armyOnlyContainer->setVisible(false);
    armyOnlyExpanded = false;
    
    // 添加到主布局
    broadcastModeLayout->addWidget(armyOnlyHeader);
    broadcastModeLayout->addWidget(armyOnlyContainer);
    
    mainLayout->addLayout(broadcastModeLayout);
    
    // 开关选项 
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
        clipIdentityCombo->addItem(item.label, item.value.toInt());
    }
    
    // 设置为不可编辑
    clipIdentityCombo->setEditable(false);
    
    // 选中第一个选项
    clipIdentityCombo->setCurrentIndex(0);
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
    
    scrollArea->setWidget(container); // 将container设置为scrollArea的内容
    setWidget(scrollArea); // 将滚动区域设置为dock的主要部件

    // 初始化loadingOverlay的大小和位置
    loadingOverlay->setGeometry(scrollArea->viewport()->rect());
    loadingOverlay->raise(); // 确保覆盖层在最上层
}

// 添加新方法，用于加载房间信息
void SeventeenLiveStreamingDock::loadRoomInfo(qint64 roomID)
{
    // 显示加载状态
    isLoading = true;
    loadingOverlay->setVisible(true);
    loadingOverlay->raise(); // 确保覆盖层在最上层
    loadingLabel->setText(obs_module_text("Live.Settings.Loading"));
    
    // 禁用所有控件
    QScrollArea *scrollArea = qobject_cast<QScrollArea*>(widget());
    if (scrollArea && scrollArea->widget()) {
        scrollArea->widget()->setEnabled(false);
    }
    
    // TODO: 以下代码需要优化，建立Worker类，将API调用放在Worker类中，在Worker类中发送信号，在主线程中接收信号，更新UI
    // 创建一个新线程来执行API调用，避免阻塞UI
    QThread *thread = new QThread;
    QObject *worker = new QObject;
    worker->moveToThread(thread);
    
    connect(thread, &QThread::started, worker, [this, roomID, worker, thread]() {
        // 在新线程中执行API调用
        bool roomInfoSuccess = apiWrapper->GetRoomInfo(roomID, roomInfo);
        
        std::string region;
        configManager->getConfigValue("Region", region);
        std::string language = GetCurrentLanguage();

        std::string userID;
        configManager->getConfigValue("UserID", userID);

        // 在同一线程中获取configStreamer信息
        bool configStreamerSuccess = apiWrapper->GetConfigStreamer(region, language, configStreamer);

        bool userInfoSuccess = apiWrapper->GetUserInfo(userID, region, language, userInfo);

        bool levelsSuccess = apiWrapper->GetArmySubscriptionLevels(region, language, levels);
        
        // 使用Qt::QueuedConnection确保在主线程中更新UI
        QMetaObject::invokeMethod(this, [this, roomInfoSuccess, configStreamerSuccess, userInfoSuccess, levelsSuccess]() {
            // 隐藏加载状态
            isLoading = false;
            loadingOverlay->setVisible(false);
            
            // 启用所有控件
            widget()->setEnabled(true);
            
            if (roomInfoSuccess) {
                // 更新UI
                updateUIWithRoomInfo();
            } else {
                // 显示错误消息
                QMessageBox::warning(this, 
                    obs_module_text("Live.Settings.Error"), 
                    QString::fromStdString(obs_module_text("Live.Settings.LoadError")).arg(apiWrapper->getLastErrorMessage()));
            }
            
            // 如果configStreamer获取失败，记录日志但不影响主流程
            if (!configStreamerSuccess) {
                obs_log(LOG_WARNING, "Failed to get config streamer in loadRoomInfo");
            }

            if (!userInfoSuccess) {
                obs_log(LOG_WARNING, "Failed to get user info in loadRoomInfo");
            }

            if (!levelsSuccess) {
                obs_log(LOG_WARNING, "Failed to get army subscription levels in loadRoomInfo");
            }
        }, Qt::QueuedConnection);
        
        // 完成后清理
        thread->quit();
        worker->deleteLater();
    });
    
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

// 添加新方法，用于根据roomInfo更新UI
void SeventeenLiveStreamingDock::updateUIWithRoomInfo()
{
    // obs_log(LOG_INFO, "Updating UI with room info");

    hashtagSelectLimit = configStreamer.hashtagSelectLimit;

    // 类别
    for (const auto& subtab : configStreamer.subtabs) {
        categoryCombo->addItem(subtab.displayName, subtab.ID);
    }

    // 活动
    for (const auto& event : configStreamer.event.events) {
        QString eventName = event.name;
        if (eventName.isEmpty()) {
            continue; // Skip if name is empty or null
        }
        activityCombo->addItem(eventName, event.ID);
    }

    // 设置开播格式
    if (roomInfo.landscape) {
        normalStreamRadio->setChecked(true);
    } else {
        verticalStreamRadio->setChecked(true);
    }

    // 战队设定
    armyOnlyHeader->setVisible(configStreamer.armyOnly==2 || userInfo.onliveInfo.premiumType != 1);

    if (configStreamer.armyOnly==2 || userInfo.onliveInfo.premiumType != 1) {
        obs_log(LOG_INFO, "armyOnlyHeader->setVisible(true)");
        updateRequiredArmyRankSelections();
    }
    
    // 设置存档配置
    archiveStreamCheck->setChecked(roomInfo.archiveConfig.autoRecording);
    autoPreviewCheck->setChecked(roomInfo.archiveConfig.autoPublish);
    // 设置剪辑权限
    clipIdentityCombo->setCurrentIndex(clipIdentityCombo->findData(roomInfo.archiveConfig.clipPermission));

    if (roomInfo.status == static_cast<int>(SeventeenLiveStreamingStatus::Live)
        || roomInfo.status == static_cast<int>(SeventeenLiveStreamingStatus::Streaming)) {
        updateUIValues();
    }

    // TODO: 当web端已经开始直播后，应如何处理？
    if (roomInfo.status == static_cast<int>(SeventeenLiveStreamingStatus::Live)) {
        // 添加用户提示框，询问用户接下来的操作
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(obs_module_text("Live.Settings.LiveCreated"));
        msgBox.setText(obs_module_text("Live.Settings.LiveCreated.Tip"));
        
        QPushButton *startLiveOnlyButton = msgBox.addButton(obs_module_text("Live.Settings.StartLiveOnly"), QMessageBox::ActionRole);
        QPushButton *closeLiveButton = msgBox.addButton(obs_module_text("Live.Settings.CloseLive"), QMessageBox::ActionRole);
        
        msgBox.setDefaultButton(startLiveOnlyButton);
        msgBox.exec();
        
        if (msgBox.clickedButton() == startLiveOnlyButton) {
            onCreateLiveClicked();
        } else if (msgBox.clickedButton() == closeLiveButton) {
            // 关闭直播
            // onDeleteLiveClicked();
        }
    } else if (roomInfo.status == static_cast<int>(SeventeenLiveStreamingStatus::Streaming)) {
        // TODO: 直播中如何处理？
    }
}

void SeventeenLiveStreamingDock::updateRequiredArmyRankSelections()
{
    // obs_log(LOG_INFO, "updateRequiredArmyRankSelections");

    SeventeenLiveConfig config;
    if (!configManager->getConfig(config)) {
        return;
    }

    // 初始化Combo Items
    requiredArmyRankCombo->clear();

    // 遍历levels，添加到Combo Items
    for (const auto& level : levels.subscriptionLevels) {
        QString rankValueTemplate = obs_module_text(QString("Live.Settings.Rank%1.%2").arg(QString::number(config.addOns.features["158"]), level.i18nToken.key).toStdString().c_str());

        if (level.i18nToken.key != "army_only_stream_level_setting_all_level") {
            if (config.addOns.features["158"] == 0) {
                QString name = obs_module_text(QString("Live.Settings.Rank0.army_rank_name_%1").arg(level.rank).toStdString().c_str());
    
                // string replace {name} in rankValueTemplate with rankTemplateValueName
                rankValueTemplate = rankValueTemplate.replace("{name}", name);
            } else if (config.addOns.features["158"] == 1) {
                // string replace {value} in rankValueTemplate with
                rankValueTemplate = rankValueTemplate.replace("{value}", level.i18nToken.params[0].value);
            }
        }
        
        // string replace {subscribersAmount} in rankValueTemplate with level.subscribersAmount
        rankValueTemplate = rankValueTemplate.replace("{subscribersAmount}", QString::number(level.subscribersAmount));

        requiredArmyRankCombo->addItem(rankValueTemplate, level.rank);
    }
}

void SeventeenLiveStreamingDock::updateUIValues()
{
    // 设置虚拟主播选项
    virtualStreamerCheck->setChecked(configStreamer.lastStreamState.vliverInfo.vliverModel == 3);

    if (roomInfo.status == static_cast<int>(SeventeenLiveStreamingStatus::Live)
        || roomInfo.status == static_cast<int>(SeventeenLiveStreamingStatus::Streaming)) {
        titleEdit->setText(roomInfo.caption);

        int currentCategoryIndex = 0;
        if (roomInfo.subtabs.size() > 0) {
            currentCategoryIndex = categoryCombo->findData(roomInfo.subtabs[0]);
        }
        categoryCombo->setCurrentIndex(currentCategoryIndex);

        // 将 roomInfo.lastUsedHashtags 添加到 hashtagEdit
        for (const auto& tag : roomInfo.lastUsedHashtags) {
            addTag(tag.text);
        }

        int currentEventIndex = 0;
        if (roomInfo.eventList.size() > 0) {
            // find the type=2 event
            for (int i = 0; i < roomInfo.eventList.size(); i++) {
                if (roomInfo.eventList[i].type == 2) {
                    qint64 eventID = roomInfo.eventList[i].ID;
                    currentEventIndex = activityCombo->findData(eventID);
                    break;
                }
            }
        }
        activityCombo->setCurrentIndex(currentEventIndex);
    }
}

void SeventeenLiveStreamingDock::createConnections()
{
    // 标签相关连接
    connect(addTagButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onAddTagClicked);
    connect(tagEdit, &QLineEdit::returnPressed, this, &SeventeenLiveStreamingDock::onTagEnterPressed);
    
    // 其他按钮连接
    connect(saveConfigButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onSaveConfigClicked);
    connect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateLiveClicked);

    // 战队限定观看折叠/展开按钮
    connect(armyOnlyToggleButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onArmyOnlyToggleClicked);

    connect(armyOnlyCheck, &QCheckBox::stateChanged, this, &SeventeenLiveStreamingDock::onArmyOnlyCheckChanged);
}

void SeventeenLiveStreamingDock::onArmyOnlyToggleClicked()
{
    armyOnlyExpanded = !armyOnlyExpanded;
    armyOnlyContainer->setVisible(armyOnlyExpanded);
    
    // 更新按钮图标
    if (armyOnlyExpanded) {
        armyOnlyToggleButton->setIcon(QIcon(":/resources/arrow-up.svg"));
    } else {
        armyOnlyToggleButton->setIcon(QIcon(":/resources/arrow-down.svg"));
    }
}

void SeventeenLiveStreamingDock::onArmyOnlyCheckChanged(int state)
{
    if (state == Qt::Checked) {
        archiveStreamCheck->setChecked(false);
        autoPreviewCheck->setChecked(false);
        clipIdentityCombo->setCurrentIndex(0);
    } else {
        // 设置存档配置
        archiveStreamCheck->setChecked(roomInfo.archiveConfig.autoRecording);
        autoPreviewCheck->setChecked(roomInfo.archiveConfig.autoPublish);
        // 设置剪辑权限
        clipIdentityCombo->setCurrentIndex(clipIdentityCombo->findData(roomInfo.archiveConfig.clipPermission));
    }

    archiveStreamCheck->setEnabled(state != Qt::Checked);
    autoPreviewCheck->setEnabled(state != Qt::Checked);
    clipIdentityCombo->setEnabled(state != Qt::Checked);
}

void SeventeenLiveStreamingDock::onAddTagClicked()
{
    QString tag = tagEdit->text().trimmed();
    if (!tag.isEmpty()) {
        addTag(tag);
        tagEdit->clear();
    }
}

void SeventeenLiveStreamingDock::onTagEnterPressed()
{
    onAddTagClicked(); // 复用添加标签的逻辑
}

void SeventeenLiveStreamingDock::onRemoveTagClicked()
{
    // 获取发送信号的按钮
    QPushButton *removeButton = qobject_cast<QPushButton*>(sender());
    if (!removeButton) return;
    
    // 获取标签文本（存储在按钮的属性中）
    QString tag = removeButton->property("tag").toString();
    
    // 从列表中移除标签
    tagsList.removeOne(tag);
    
    // 更新标签显示
    updateTagsFromList();
}

void SeventeenLiveStreamingDock::addTag(const QString &tag)
{
    if (tagsList.size() >= hashtagSelectLimit) {
        return;
    }

    // 检查标签是否已存在
    if (!tagsList.contains(tag)) {
        tagsList.append(tag);
        updateTagsFromList();
    }
}

void SeventeenLiveStreamingDock::updateTagsFromList()
{
    // 清除现有标签显示
    QLayoutItem *child;
    while ((child = tagsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
    
    // 重新创建标签显示
    for (const QString &tag : tagsList) {
        // 创建标签容器
        QWidget *tagWidget = new QWidget();
        tagWidget->setStyleSheet("background-color: #3D3D3D; border-radius: 4px; padding: 2px;");
        
        QHBoxLayout *tagWidgetLayout = new QHBoxLayout(tagWidget);
        tagWidgetLayout->setContentsMargins(5, 2, 5, 2);
        tagWidgetLayout->setSpacing(3);
        
        // 创建标签文本
        QLabel *tagLabel = new QLabel("#" + tag);
        tagLabel->setStyleSheet("color: white;");
        
        // 创建删除按钮
        QPushButton *removeButton = new QPushButton("x");
        removeButton->setProperty("tag", tag);
        removeButton->setFixedSize(16, 16);
        removeButton->setStyleSheet("QPushButton { background-color: transparent; color: white; border: none; font-size: 12px; } QPushButton:hover { color: red; }");
        connect(removeButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onRemoveTagClicked);
        
        tagWidgetLayout->addWidget(tagLabel);
        tagWidgetLayout->addWidget(removeButton);
        
        tagsLayout->addWidget(tagWidget);
    }
    
    // 添加弹性空间，使标签靠左对齐
    tagsLayout->addStretch();
}

void SeventeenLiveStreamingDock::onSaveConfigClicked()
{
    SeventeenLiveRtmpRequest request;
    if (!gatherRtmpRequest(request)) {
        obs_log(LOG_ERROR, "Failed to gather rtmp request");
        return;
    }

    SeventeenLiveStreamInfo streamInfo;
    streamInfo.categoryName = categoryCombo->currentText();
    streamInfo.createdAt = QDateTime::currentDateTime();
    streamInfo.request = request;
    if (!currentInfoUuid.isEmpty()) {
        streamInfo.streamUuid = currentInfoUuid;
    } else {
        streamInfo.streamUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    
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
    if (!gatherRtmpRequest(request)) {
        obs_log(LOG_ERROR, "Failed to gather rtmp request");
        return;
    }

    startStreaming(request);
}

void SeventeenLiveStreamingDock::createLiveWithRequest(const SeventeenLiveRtmpRequest &request)
{
    obs_log(LOG_INFO, "createLiveWithRequest");

    populateRtmpRequest(request);

    if (request.caption.isEmpty() || request.subtabID.isEmpty()) {
        return;
    }

    startStreaming(request);
}

void SeventeenLiveStreamingDock::editLiveWithInfo(const SeventeenLiveStreamInfo &info)
{
    obs_log(LOG_INFO, "editLiveWithInfo");
    populateRtmpRequest(info.request);
    currentInfoUuid = info.streamUuid;
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

    // archive
    if (request.archiveConfig.autoRecording) {
        if (!apiWrapper->EnableStreamArchive(response.liveStreamID.toStdString(), 1)) {
            obs_log(LOG_ERROR, "Failed to enable archive %s", apiWrapper->getLastErrorMessage().toStdString().c_str());
        }
    }

    updateLiveStatus(SeventeenLiveStreamingStatus::Streaming);
    
    // 询问是否同时开始串流
    QMessageBox msgBox;
    msgBox.setWindowTitle(obs_module_text("Live.Settings.StartStreaming"));
    msgBox.setText(obs_module_text("Live.Settings.StartStreaming.Tip"));
    
    // 使用本地化的按钮文本
    QPushButton *yesButton = msgBox.addButton(obs_module_text("Live.Settings.Yes"), QMessageBox::YesRole);
    /* QPushButton *noButton = */ msgBox.addButton(obs_module_text("Live.Settings.No"), QMessageBox::NoRole);
    msgBox.setDefaultButton(yesButton);
    
    msgBox.exec();
    if (msgBox.clickedButton() == yesButton) {
        // 启动OBS串流
        obs_frontend_streaming_start();
    }
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

    // 清空并重新加载标签列表
    tagsList.clear();
    tagsList = request.hashtags;
    updateTagsFromList();
    tagEdit->clear();

    if (request.landscape) {
        normalStreamRadio->setChecked(true);
    } else {
        verticalStreamRadio->setChecked(true);
    }

    int categoryIndex = categoryCombo->findData(QVariant(request.subtabID));
    if (categoryIndex >= 0) {
        categoryCombo->setCurrentIndex(categoryIndex);
    }

    // 战队限定观看设置
    armyOnlyCheck->setChecked(request.armyOnly.enable);
    
    int userConditionIndex = requiredArmyRankCombo->findData(QVariant(request.armyOnly.requiredArmyRank));
    if (userConditionIndex >= 0) {
        requiredArmyRankCombo->setCurrentIndex(userConditionIndex);
    }
    
    showInHotPageCheck->setChecked(request.armyOnly.showOnHotPage);
    liveNotificationCheck->setChecked(request.armyOnly.armyOnlyPN);

    archiveStreamCheck->setChecked(request.archiveConfig.autoRecording);
    autoPreviewCheck->setChecked(request.archiveConfig.autoPublish);

    int clipIndex = clipIdentityCombo->findData(QVariant(request.archiveConfig.clipPermission));
    if (clipIndex >= 0) {
        clipIdentityCombo->setCurrentIndex(clipIndex);
    }

    virtualStreamerCheck->setChecked(request.vliverInfo.vliverModel == 3);
}

bool SeventeenLiveStreamingDock::gatherRtmpRequest(SeventeenLiveRtmpRequest &request)
{
    obs_log(LOG_INFO, "gatherRtmpRequest");
    QString caption = titleEdit->text();
    if (caption.isEmpty()) {
        // 弹出提示框, 提示用户输入标题
        QMessageBox::warning(this, obs_module_text("Live.Settings.Save.Title"), obs_module_text("Live.Settings.Save.Title.Empty"));
        return false;
    }
    QString subtabID = categoryCombo->currentData().toString();
    if (subtabID.isEmpty()) {
        // 弹出提示框, 提示用户选择分类
        QMessageBox::warning(this, obs_module_text("Live.Settings.Save.Title"), obs_module_text("Live.Settings.Save.Category.Empty"));
        return false;
    }
    request.userID = roomInfo.userID;
    request.caption = titleEdit->text();
    request.device = "OBS";
    int eventID = activityCombo->currentData().toInt();
    request.eventID = eventID;
    request.hashtags = tagsList;
    request.landscape = normalStreamRadio->isChecked();
    request.streamerType = roomInfo.streamerType;
    request.subtabID = categoryCombo->currentData().toString();

    // 战队限定观看设置
    request.armyOnly.enable = armyOnlyCheck->isChecked();
    request.armyOnly.requiredArmyRank = requiredArmyRankCombo->currentData().toInt();
    request.armyOnly.showOnHotPage = showInHotPageCheck->isChecked();
    request.armyOnly.armyOnlyPN = liveNotificationCheck->isChecked();

    request.archiveConfig.autoRecording = archiveStreamCheck->isChecked();
    request.archiveConfig.autoPublish = autoPreviewCheck->isChecked();
    request.archiveConfig.clipPermission = clipIdentityCombo->currentData().toInt();
    request.vliverInfo.vliverModel = virtualStreamerCheck->isChecked() ? 3 : 0;

    return true;
}

void SeventeenLiveStreamingDock::updateLiveButton(bool isLive)
{   
    // obs_log(LOG_INFO, "updateLiveButton: %d", isLive);
    if (isLive) {
        // change text to "停止直播"
        createLiveButton->setText(obs_module_text("Live.Settings.StopLive"));
        // 设置为绿色背景，表示当前正在直播
        createLiveButton->setStyleSheet("background-color: green; color: white;");
        disconnect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateLiveClicked);
        connect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onDeleteLiveClicked);
    } else {
        // change text to "建立直播"
        createLiveButton->setText(obs_module_text("Live.Settings.StartLive"));
        // 设置为红色背景，表示当前未直播
        createLiveButton->setStyleSheet("background-color: red; color: white;");
        disconnect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onDeleteLiveClicked);
        connect(createLiveButton, &QPushButton::clicked, this, &SeventeenLiveStreamingDock::onCreateLiveClicked);
    }
}

void SeventeenLiveStreamingDock::updateLiveStatus(SeventeenLiveStreamingStatus status)
{
    currentLiveStatus = status;

    updateLiveButton(status != SeventeenLiveStreamingStatus::NotStarted);
}

void SeventeenLiveStreamingDock::resizeEvent(QResizeEvent *event)
{
    QDockWidget::resizeEvent(event);
    
    // 更新加载覆盖层的大小和位置，使其始终覆盖整个可见区域
    if (loadingOverlay && widget()) {
        QScrollArea *scrollArea = qobject_cast<QScrollArea*>(widget());
        if (scrollArea) {
            // 直接使用viewport的rect()，因为loadingOverlay的父部件已经是viewport
            loadingOverlay->setGeometry(QRect(0, 0, scrollArea->viewport()->width(), scrollArea->viewport()->height()));
            loadingOverlay->raise(); // 确保覆盖层在最上层
        }
    }
}

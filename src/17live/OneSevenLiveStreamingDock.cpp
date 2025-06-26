#include "OneSevenLiveStreamingDock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QScrollArea>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "moc_OneSevenLiveStreamingDock.cpp"
#include "plugin-support.h"
#include "utility/Common.hpp"
#include "utility/Meta.hpp"

OneSevenLiveStreamingDock::OneSevenLiveStreamingDock(QWidget *parent,
                                                     OneSevenLiveApiWrappers *apiWrapper_,
                                                     OneSevenLiveConfigManager *configManager_)
    : QDockWidget(obs_module_text("Live.Settings"), parent),
      apiWrapper(apiWrapper_),
      configManager(configManager_) {
    setupUi();
    createConnections();
}

OneSevenLiveStreamingDock::~OneSevenLiveStreamingDock() = default;

void OneSevenLiveStreamingDock::setupUi() {
    QWidget *container = new QWidget(this);
    container->setStyleSheet(
        "QWidget {"
        "    color: white;"
        "    font-family: 'Inter';"
        "    font-style: normal;"
        "}"
        "QToolTip {"
        "   background-color: #333333;"
        "   color: #FFFFFF;"
        "   font-weight: 400;"
        "   font-size: 12px;"
        "   line-height: 16px;"
        "   padding: 5px;"
        "   border: none;"
        "   border-radius: 4px;"
        "}");
    QVBoxLayout *mainLayout = new QVBoxLayout(container);

    // Create scroll area
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);        // Allow content resizing
    scrollArea->setFrameShape(QFrame::NoFrame);  // Remove border
    scrollArea->setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded);  // Show vertical scrollbar when needed
    scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);  // Disable horizontal scrollbar

    // Set maximum height (adjustable as needed)
    scrollArea->setMaximumHeight(800);  // Set maximum height to 800 pixels

    // Create loading state overlay - note that parent widget is changed to scrollArea
    loadingOverlay = new QWidget(scrollArea->viewport());
    loadingOverlay->setStyleSheet("background-color: rgba(0, 0, 0, 120);");
    loadingOverlay->setAttribute(Qt::WA_TranslucentBackground);
    loadingOverlay->setVisible(false);  // Initially invisible

    QVBoxLayout *overlayLayout = new QVBoxLayout(loadingOverlay);
    overlayLayout->setAlignment(Qt::AlignCenter);

    loadingLabel = new QLabel(obs_module_text("Live.Settings.Loading"));
    loadingLabel->setStyleSheet("color: white; font-size: 16px;");
    loadingLabel->setMaximumWidth(350);
    loadingLabel->setWordWrap(true);
    loadingLabel->setAlignment(Qt::AlignCenter);

    loadingProgress = new QProgressBar();
    loadingProgress->setRange(0, 0);  // Set to indeterminate progress
    loadingProgress->setTextVisible(false);
    loadingProgress->setFixedSize(200, 10);

    overlayLayout->addWidget(loadingLabel);
    overlayLayout->addWidget(loadingProgress);

    // Title input
    QFormLayout *formLayout = new QFormLayout();
    // Set to vertical layout, labels above fields
    formLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    formLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    QLabel *titleLabel = new QLabel();
    titleLabel->setText(
        QString("<span style='color:red;'>*</span><span style='color:white;'>%1</span>")
            .arg(obs_module_text("Live.Settings.Title")));

    titleEdit = new QLineEdit();
    titleEdit->setPlaceholderText(obs_module_text("Live.Settings.Title.Placeholder"));
    formLayout->addRow(titleLabel, titleEdit);

    // Category selection
    categoryCombo = new QComboBox();
    QLabel *categoryLabel = new QLabel();
    categoryLabel->setText(
        QString("<span style='color:red;'>*</span><span style='color:white;'>%1</span>")
            .arg(obs_module_text("Live.Settings.Category")));
    formLayout->addRow(categoryLabel, categoryCombo);

    // Tags area
    QVBoxLayout *tagContainer = new QVBoxLayout();

    // Input box and add button
    QHBoxLayout *tagInputLayout = new QHBoxLayout();
    tagEdit = new QLineEdit();
    tagEdit->setPlaceholderText(obs_module_text("Live.Settings.Tags.Placeholder"));
    addTagButton = new QPushButton(obs_module_text("Live.Settings.AddTag"));
    addTagButton->setStyleSheet("background-color: #FF0001;");
    tagInputLayout->addWidget(tagEdit);
    tagInputLayout->addWidget(addTagButton);
    tagContainer->addLayout(tagInputLayout);

    // Tags display area
    tagsContainer = new QWidget();
    tagsLayout = new QHBoxLayout(tagsContainer);
    tagsLayout->setContentsMargins(0, 5, 0, 0);
    tagsLayout->setSpacing(5);
    tagsLayout->setAlignment(Qt::AlignLeft);
    tagContainer->addWidget(tagsContainer);

    formLayout->addRow(obs_module_text("Live.Settings.Tags"), tagContainer);

    mainLayout->addLayout(formLayout);

    // Stream format
    QGroupBox *streamFormatGroup = new QGroupBox(obs_module_text("Live.Settings.Layout"));
    QHBoxLayout *formatLayout = new QHBoxLayout(streamFormatGroup);
    landscapeStreamRadio = new QRadioButton(obs_module_text("Live.Settings.Layout.Landscape"));
    portraitStreamRadio = new QRadioButton(obs_module_text("Live.Settings.Layout.Portrait"));

    formatLayout->addWidget(portraitStreamRadio);
    formatLayout->addWidget(landscapeStreamRadio);

    portraitStreamRadio->setChecked(true);

    mainLayout->addWidget(streamFormatGroup);

    // Event related
    QVBoxLayout *eventContainer = new QVBoxLayout();

    // Title
    QLabel *eventLabel = new QLabel(obs_module_text("Live.Settings.Event"));
    eventContainer->addWidget(eventLabel);

    // Dropdown box
    activityCombo = new QComboBox();
    eventContainer->addWidget(activityCombo);

    // Create hint label and align right
    QHBoxLayout *hintLayout = new QHBoxLayout();
    QLabel *hintLabel = new QLabel(obs_module_text("Live.Settings.Event.Tip"));
    hintLabel->setStyleSheet("color: gray; font-size: 12px;");
    hintLayout->addStretch();  // Add flexible space to align hint text to the right
    hintLayout->addWidget(hintLabel);
    eventContainer->addLayout(hintLayout);

    mainLayout->addLayout(eventContainer);

    // Broadcast mode
    QVBoxLayout *broadcastModeLayout = new QVBoxLayout();
    broadcastModeLabel = new QLabel(obs_module_text("Live.Settings.BroadcastMode"));
    broadcastModeLabel->setStyleSheet("font-weight: bold;");
    broadcastModeLayout->addWidget(broadcastModeLabel);

    // Army-only viewing - collapsible section
    // 1. Header (title and collapse button)
    armyOnlyHeader = new QWidget();
    armyOnlyHeader->setStyleSheet("QLabel:disabled { color: #808080; }");
    armyOnlyHeaderLayout = new QHBoxLayout(armyOnlyHeader);
    armyOnlyHeaderLayout->setContentsMargins(0, 10, 0, 10);

    armyOnlyLabel = new QLabel(obs_module_text("Live.Settings.ArmyOnly"));
    armyOnlyToggleButton = new QPushButton();
    armyOnlyToggleButton->setIcon(QIcon(":/resources/arrow-down.svg"));
    armyOnlyToggleButton->setStyleSheet(
        "QPushButton { border: none; background-color: transparent; }");
    armyOnlyToggleButton->setFixedSize(24, 24);

    armyOnlyHeaderLayout->addWidget(armyOnlyLabel);
    armyOnlyHeaderLayout->addStretch();
    armyOnlyHeaderLayout->addWidget(armyOnlyToggleButton);

    // 2. Content container (hidden by default)
    armyOnlyContainer = new QWidget();
    armyOnlyContainerLayout = new QVBoxLayout(armyOnlyContainer);
    armyOnlyContainerLayout->setContentsMargins(20, 0, 0, 10);  // Left indent

    // Army-only viewing switch
    QHBoxLayout *armyOnlyCheckLayout = new QHBoxLayout();
    QLabel *armyOnlyCheckLabel = new QLabel(obs_module_text("Live.Settings.ArmyOnly"));
    armyOnlyCheck = new QCheckBox();

    armyOnlyCheckLayout->addWidget(armyOnlyCheckLabel);
    armyOnlyCheckLayout->addStretch();
    armyOnlyCheckLayout->addWidget(armyOnlyCheck);

    armyOnlyContainerLayout->addLayout(armyOnlyCheckLayout);

    // User conditions
    QVBoxLayout *userConditionLayout = new QVBoxLayout();
    QLabel *userConditionLabel = new QLabel(obs_module_text("Live.Settings.UserCondition"));
    userConditionLayout->addWidget(userConditionLabel);

    requiredArmyRankCombo = new QComboBox();
    // requiredArmyRankCombo->addItem(obs_module_text("Live.Settings.UserCondition.AllLevels"), 1);
    requiredArmyRankCombo->setEditable(false);
    userConditionLayout->addWidget(requiredArmyRankCombo);

    armyOnlyContainerLayout->addLayout(userConditionLayout);

    // Show in hot page
    QHBoxLayout *showInHotPageLayout = new QHBoxLayout();
    QLabel *showInHotPageLabel = new QLabel(obs_module_text("Live.Settings.ShowInHotPage"));
    showInHotPageCheck = new QCheckBox();

    showInHotPageLayout->addWidget(showInHotPageLabel);
    showInHotPageLayout->addStretch();
    showInHotPageLayout->addWidget(showInHotPageCheck);

    armyOnlyContainerLayout->addLayout(showInHotPageLayout);

    // Live notification
    QHBoxLayout *liveNotificationLayout = new QHBoxLayout();
    QLabel *liveNotificationLabel = new QLabel(obs_module_text("Live.Settings.LiveNotification"));
    liveNotificationCheck = new QCheckBox();

    liveNotificationLayout->addWidget(liveNotificationLabel);
    liveNotificationLayout->addStretch();
    liveNotificationLayout->addWidget(liveNotificationCheck);

    armyOnlyContainerLayout->addLayout(liveNotificationLayout);

    // Initial state: collapsed
    armyOnlyContainer->setVisible(false);
    armyOnlyExpanded = false;

    // Add to main layout
    broadcastModeLayout->addWidget(armyOnlyHeader);
    broadcastModeLayout->addWidget(armyOnlyContainer);

    mainLayout->addLayout(broadcastModeLayout);

    // Switch options
    QHBoxLayout *archiveLayout = new QHBoxLayout();
    QVBoxLayout *archiveLabelLayout = new QVBoxLayout();

    // Left side title and hint information
    QLabel *archiveLabel = new QLabel(obs_module_text("Live.Settings.Archive.Record"));
    QLabel *archiveTip = new QLabel(obs_module_text("Live.Settings.Archive.Record.Tip"));
    archiveTip->setStyleSheet("color: gray; font-size: 12px;");
    archiveTip->setMaximumWidth(580);
    archiveTip->setWordWrap(true);

    archiveLabelLayout->addWidget(archiveLabel);
    archiveLabelLayout->addWidget(archiveTip);
    archiveLabelLayout->setSpacing(2);  // Adjust spacing between title and hint

    // Right side Switch component
    archiveStreamCheck = new QCheckBox();

    // Add left and right parts to horizontal layout
    archiveLayout->addLayout(archiveLabelLayout);
    archiveLayout->addStretch();  // Add flexible space to align Switch to the right
    archiveLayout->addWidget(archiveStreamCheck);

    mainLayout->addLayout(archiveLayout);

    // Similarly modify auto preview options
    QHBoxLayout *previewLayout = new QHBoxLayout();
    QVBoxLayout *previewLabelLayout = new QVBoxLayout();

    QLabel *previewLabel = new QLabel(obs_module_text("Live.Settings.Archive.AutoPublish"));
    QLabel *previewTip = new QLabel(obs_module_text("Live.Settings.Archive.AutoPublish.Tip"));
    previewTip->setStyleSheet("color: gray; font-size: 12px;");
    previewTip->setMaximumWidth(580);
    previewTip->setWordWrap(true);

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
    clipTip->setMaximumWidth(580);
    clipTip->setWordWrap(true);
    clipLayout->addWidget(clipTip);

    // Clip identity
    clipIdentityCombo = new QComboBox();
    QList<OneSevenLiveMetaValueLabel> clipIdentityList;
    getMetaValueLabelList("ClipPermissions", clipIdentityList);
    for (const auto &item : clipIdentityList) {
        clipIdentityCombo->addItem(item.label, item.value.toInt());
    }

    // Set to non-editable
    clipIdentityCombo->setEditable(false);

    // Select first option
    clipIdentityCombo->setCurrentIndex(0);
    clipLayout->addWidget(clipIdentityCombo);
    clipLayout->setSpacing(2);

    mainLayout->addLayout(clipLayout);

    // Virtual streamer options
    QVBoxLayout *vliverLayout = new QVBoxLayout();

    QLabel *vliverLabel = new QLabel(obs_module_text("Live.Settings.VirtualLiver.Title"));
    vliverLayout->addWidget(vliverLabel);

    QLabel *vliverTip = new QLabel(obs_module_text("Live.Settings.VirtualLiver.Tip"));
    vliverTip->setStyleSheet("color: gray; font-size: 12px;");
    vliverTip->setMaximumWidth(580);
    vliverTip->setWordWrap(true);
    vliverLayout->addWidget(vliverTip);

    virtualStreamerCheck = new QCheckBox(obs_module_text("Live.Settings.VirtualLiver"));
    vliverLayout->addWidget(virtualStreamerCheck);

    vliverLayout->setSpacing(2);

    mainLayout->addLayout(vliverLayout);

    // Bottom buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    saveConfigButton = new QPushButton(obs_module_text("Live.Settings.Save"));
    saveConfigButton->setStyleSheet("background-color: red; color: white;");
    createLiveButton = new QPushButton(obs_module_text("Live.Settings.StartLive"));
    createLiveButton->setStyleSheet("background-color: red; color: white;");

    // Set saveConfigButton width to half of createLiveButton
    buttonLayout->addWidget(saveConfigButton, 1);
    buttonLayout->addWidget(createLiveButton, 2);
    mainLayout->addLayout(buttonLayout);

    scrollArea->setWidget(container);  // Set container as scrollArea content
    setWidget(scrollArea);             // Set scroll area as dock's main widget

    // Use delayed processing to ensure layout calculation is complete
    QTimer::singleShot(0, this, [this, scrollArea]() {
        if (loadingOverlay && scrollArea) {
            loadingOverlay->setGeometry(
                QRect(0, 0, scrollArea->viewport()->width(), scrollArea->viewport()->height()));
            loadingOverlay->raise();  // Ensure overlay is on top
        }
    });
}

// Add new method for loading room information
void OneSevenLiveStreamingDock::loadRoomInfo(qint64 roomID) {
    // Show loading state
    isLoading = true;
    loadingOverlay->setVisible(true);
    loadingOverlay->raise();  // Ensure overlay is on top
    loadingLabel->setText(obs_module_text("Live.Settings.Loading"));

    // Disable all controls
    QScrollArea *scrollArea = qobject_cast<QScrollArea *>(widget());
    if (scrollArea && scrollArea->widget()) {
        scrollArea->widget()->setEnabled(false);
    }

    // TODO: The following code needs optimization, establish Worker class, put API calls in Worker
    // class, send signals in Worker class, receive signals in main thread, update UI Create a new
    // thread to execute API calls, avoiding UI blocking
    QThread *thread = new QThread;
    QObject *worker = new QObject;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [this, roomID, worker, thread]() {
        // Execute API calls in new thread
        bool roomInfoSuccess = apiWrapper->GetRoomInfo(roomID, roomInfo);

        std::string region;
        configManager->getConfigValue("Region", region);
        std::string language = GetCurrentLanguage();

        std::string userID;
        configManager->getConfigValue("UserID", userID);

        // Get configStreamer information in the same thread
        bool configStreamerSuccess =
            apiWrapper->GetConfigStreamer(region, language, configStreamer);

        bool userInfoSuccess = apiWrapper->GetUserInfo(userID, region, language, userInfo);

        bool levelsSuccess = apiWrapper->GetArmySubscriptionLevels(region, language, levels);

        // Use Qt::QueuedConnection to ensure UI updates in main thread
        QMetaObject::invokeMethod(
            this,
            [this, roomInfoSuccess, configStreamerSuccess, userInfoSuccess, levelsSuccess]() {
                // Hide loading state
                isLoading = false;
                loadingOverlay->setVisible(false);

                // Enable all controls
                QScrollArea *scrollArea = qobject_cast<QScrollArea *>(widget());
                if (scrollArea && scrollArea->widget()) {
                    scrollArea->widget()->setEnabled(true);
                }

                if (configStreamerSuccess) {
                    // Update UI
                    updateUIWithRoomInfo();
                } else {
                    // Show error message
                    QMessageBox::warning(
                        this, obs_module_text("Live.Settings.Error"),
                        QString::fromStdString(obs_module_text("Live.Settings.LoadError"))
                            .arg(apiWrapper->getLastErrorMessage()));
                }

                if (!roomInfoSuccess) {
                    obs_log(LOG_WARNING, "Failed to get roomInfo in loadRoomInfo");
                }

                if (!userInfoSuccess) {
                    obs_log(LOG_WARNING, "Failed to get user info in loadRoomInfo");
                }

                if (!levelsSuccess) {
                    obs_log(LOG_WARNING, "Failed to get army subscription levels in loadRoomInfo");
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

// Add new method to update UI based on roomInfo
void OneSevenLiveStreamingDock::updateUIWithRoomInfo() {
    // obs_log(LOG_INFO, "Updating UI with room info");

    hashtagSelectLimit = configStreamer.hashtagSelectLimit;

    // Category
    for (const auto &subtab : configStreamer.subtabs) {
        categoryCombo->addItem(subtab.displayName, subtab.ID);
    }

    // Activity
    for (const auto &event : configStreamer.event.events) {
        QString eventName = event.name;
        if (eventName.isEmpty()) {
            continue;  // Skip if name is empty or null
        }
        activityCombo->addItem(eventName, event.ID);
    }

    // Set streaming format
    if (roomInfo.landscape) {
        landscapeStreamRadio->setChecked(true);
    } else {
        portraitStreamRadio->setChecked(true);
    }

    // Army settings

    armyOnlyHeader->setEnabled(configStreamer.armyOnly == 2 &&
                               userInfo.onliveInfo.premiumType != 1);

    if (configStreamer.armyOnly == 2 && userInfo.onliveInfo.premiumType != 1) {
        updateRequiredArmyRankSelections();
        armyOnlyHeader->setToolTip("");
    } else {
        armyOnlyHeader->setToolTip(obs_module_text("Live.Settings.ArmyOnly.Tip"));
    }

    // Set archive configuration
    archiveStreamCheck->setChecked(configStreamer.archiveConfig.autoRecording);
    autoPreviewCheck->setChecked(configStreamer.archiveConfig.autoPublish);
    // Set clip permissions
    clipIdentityCombo->setCurrentIndex(
        clipIdentityCombo->findData(configStreamer.archiveConfig.clipPermission));

    if (roomInfo.status == static_cast<int>(OneSevenLiveStreamingStatus::Live) ||
        roomInfo.status == static_cast<int>(OneSevenLiveStreamingStatus::Streaming)) {
        updateUIValues();
    }

    // How to handle when web has already started streaming
    if (roomInfo.status == static_cast<int>(OneSevenLiveStreamingStatus::Live)) {
        // Add user prompt dialog to ask for next operation
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(obs_module_text("Live.Settings.LiveCreated"));
        msgBox.setText(obs_module_text("Live.Settings.LiveCreated.Tip"));

        QPushButton *startLiveOnlyButton = msgBox.addButton(
            obs_module_text("Live.Settings.StartLiveOnly"), QMessageBox::ActionRole);
        QPushButton *closeLiveButton =
            msgBox.addButton(obs_module_text("Live.Settings.CloseLive"), QMessageBox::ActionRole);

        msgBox.setDefaultButton(startLiveOnlyButton);
        msgBox.exec();

        if (msgBox.clickedButton() == startLiveOnlyButton) {
            syncWithWeb(static_cast<OneSevenLiveStreamingStatus>(roomInfo.status));
        } else if (msgBox.clickedButton() == closeLiveButton) {
            closeLive(roomInfo.userInfo.userID.toStdString(),
                      QString::number(roomInfo.liveStreamID).toStdString());
        }
    } else if (roomInfo.status == static_cast<int>(OneSevenLiveStreamingStatus::Streaming)) {
        syncWithWeb(static_cast<OneSevenLiveStreamingStatus>(roomInfo.status));
    }
}

void OneSevenLiveStreamingDock::syncWithWeb(OneSevenLiveStreamingStatus status) {
    if (roomInfo.rtmpUrls.size() > 0) {
        QString provider = GetProviderNameByIndex(roomInfo.rtmpUrls[0].provider);
        OneSevenLiveRtmpResponse rtmpResponse;
        if (apiWrapper->GetRtmpByProvider(provider.toStdString(), rtmpResponse)) {
            rtmpResponse.liveStreamID = QString::number(roomInfo.liveStreamID);
            startLive(roomInfo.userInfo.userID.toStdString(), rtmpResponse,
                      roomInfo.archiveConfig.autoRecording,
                      status == OneSevenLiveStreamingStatus::Streaming);
        } else {
            QMessageBox::warning(
                this, obs_module_text("Live.Settings.Error"),
                QString::fromStdString(obs_module_text("Live.Settings.GetRtmpError"))
                    .arg(apiWrapper->getLastErrorMessage()));
        }
    } else {
        QMessageBox::warning(
            this, obs_module_text("Live.Settings.Error"),
            QString::fromStdString(obs_module_text("Live.Settings.GetRoomInfoError"))
                .arg(apiWrapper->getLastErrorMessage()));
    }
}

void OneSevenLiveStreamingDock::updateRequiredArmyRankSelections() {
    // obs_log(LOG_INFO, "updateRequiredArmyRankSelections");

    OneSevenLiveConfig config;
    if (!configManager->getConfig(config)) {
        return;
    }

    // Initialize Combo Items
    requiredArmyRankCombo->clear();

    // Iterate through levels, add to Combo Items
    for (const auto &level : levels.subscriptionLevels) {
        QString rankValueTemplate = obs_module_text(
            QString("Live.Settings.Rank%1.%2")
                .arg(QString::number(config.addOns.features["158"]), level.i18nToken.key)
                .toStdString()
                .c_str());

        if (level.i18nToken.key != "army_only_stream_level_setting_all_level") {
            if (config.addOns.features["158"] == 0) {
                QString name = obs_module_text(QString("Live.Settings.Rank0.army_rank_name_%1")
                                                   .arg(level.rank)
                                                   .toStdString()
                                                   .c_str());

                // string replace {name} in rankValueTemplate with rankTemplateValueName
                rankValueTemplate = rankValueTemplate.replace("{name}", name);
            } else if (config.addOns.features["158"] == 1) {
                // string replace {value} in rankValueTemplate with
                rankValueTemplate =
                    rankValueTemplate.replace("{value}", level.i18nToken.params[0].value);
            }
        }

        // string replace {subscribersAmount} in rankValueTemplate with level.subscribersAmount
        rankValueTemplate = rankValueTemplate.replace("{subscribersAmount}",
                                                      QString::number(level.subscribersAmount));

        requiredArmyRankCombo->addItem(rankValueTemplate, level.rank);
    }
}

void OneSevenLiveStreamingDock::updateUIValues() {
    // Set virtual streamer options
    virtualStreamerCheck->setChecked(configStreamer.lastStreamState.vliverInfo.vliverModel == 3);

    if (roomInfo.status == static_cast<int>(OneSevenLiveStreamingStatus::Live) ||
        roomInfo.status == static_cast<int>(OneSevenLiveStreamingStatus::Streaming)) {
        titleEdit->setText(roomInfo.caption);

        int currentCategoryIndex = 0;
        if (roomInfo.subtabs.size() > 0) {
            currentCategoryIndex = categoryCombo->findData(roomInfo.subtabs[0]);
        }
        categoryCombo->setCurrentIndex(currentCategoryIndex);

        // Add roomInfo.lastUsedHashtags to hashtagEdit
        for (const auto &tag : roomInfo.lastUsedHashtags) {
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

void OneSevenLiveStreamingDock::createConnections() {
    // Tag-related connections
    connect(addTagButton, &QPushButton::clicked, this, &OneSevenLiveStreamingDock::onAddTagClicked);
    connect(tagEdit, &QLineEdit::returnPressed, this,
            &OneSevenLiveStreamingDock::onTagEnterPressed);

    // Other button connections
    connect(saveConfigButton, &QPushButton::clicked, this,
            &OneSevenLiveStreamingDock::onSaveConfigClicked);
    connect(createLiveButton, &QPushButton::clicked, this,
            &OneSevenLiveStreamingDock::onCreateLiveClicked);

    // Army-only viewing collapse/expand button
    connect(armyOnlyToggleButton, &QPushButton::clicked, this,
            &OneSevenLiveStreamingDock::onArmyOnlyToggleClicked);

    connect(armyOnlyCheck, &QCheckBox::stateChanged, this,
            &OneSevenLiveStreamingDock::onArmyOnlyCheckChanged);
}

void OneSevenLiveStreamingDock::onArmyOnlyToggleClicked() {
    armyOnlyExpanded = !armyOnlyExpanded;
    armyOnlyContainer->setVisible(armyOnlyExpanded);

    // Update button icon
    if (armyOnlyExpanded) {
        armyOnlyToggleButton->setIcon(QIcon(":/resources/arrow-up.svg"));
    } else {
        armyOnlyToggleButton->setIcon(QIcon(":/resources/arrow-down.svg"));
    }
}

void OneSevenLiveStreamingDock::onArmyOnlyCheckChanged(int state) {
    if (state == Qt::Checked) {
        archiveStreamCheck->setChecked(false);
        autoPreviewCheck->setChecked(false);
        clipIdentityCombo->setCurrentIndex(0);
    } else {
        // Set archive configuration
        archiveStreamCheck->setChecked(roomInfo.archiveConfig.autoRecording);
        autoPreviewCheck->setChecked(roomInfo.archiveConfig.autoPublish);
        // Set clip permissions
        clipIdentityCombo->setCurrentIndex(
            clipIdentityCombo->findData(roomInfo.archiveConfig.clipPermission));
    }

    archiveStreamCheck->setEnabled(state != Qt::Checked);
    autoPreviewCheck->setEnabled(state != Qt::Checked);
    clipIdentityCombo->setEnabled(state != Qt::Checked);
}

void OneSevenLiveStreamingDock::onAddTagClicked() {
    QString tag = tagEdit->text().trimmed();
    if (!tag.isEmpty()) {
        addTag(tag);
        tagEdit->clear();
    }
}

void OneSevenLiveStreamingDock::onTagEnterPressed() {
    onAddTagClicked();  // Reuse add tag logic
}

void OneSevenLiveStreamingDock::onRemoveTagClicked() {
    // Get the button that sent the signal
    QPushButton *removeButton = qobject_cast<QPushButton *>(sender());
    if (!removeButton)
        return;

    // Get tag text (stored in button property)
    QString tag = removeButton->property("tag").toString();

    // Remove tag from list
    tagsList.removeOne(tag);

    // Update tag display
    updateTagsFromList();
}

void OneSevenLiveStreamingDock::addTag(const QString &tag) {
    if (tagsList.size() >= hashtagSelectLimit) {
        return;
    }

    // Check if tag length exceeds 24 bytes
    if (tag.toUtf8().size() > 24) {
        QMessageBox::warning(this, obs_module_text("Live.Settings.Error"),
                             obs_module_text("Live.Settings.Tags.LengthError"));
        return;
    }

    // Check if tag already exists
    if (!tagsList.contains(tag)) {
        tagsList.append(tag);
        updateTagsFromList();
    }
}

void OneSevenLiveStreamingDock::updateTagsFromList() {
    // Clear existing tag display
    QLayoutItem *child;
    while ((child = tagsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    // Recreate tag display
    for (const QString &tag : tagsList) {
        // Create tag container
        QWidget *tagWidget = new QWidget();
        tagWidget->setStyleSheet("background-color: #3D3D3D; border-radius: 4px; padding: 2px;");

        QHBoxLayout *tagWidgetLayout = new QHBoxLayout(tagWidget);
        tagWidgetLayout->setContentsMargins(5, 2, 5, 2);
        tagWidgetLayout->setSpacing(3);

        // Create tag text
        QLabel *tagLabel = new QLabel("#" + tag);
        tagLabel->setStyleSheet("color: white;");

        // Create delete button
        QPushButton *removeButton = new QPushButton("x");
        removeButton->setProperty("tag", tag);
        removeButton->setFixedSize(16, 16);
        removeButton->setStyleSheet(
            "QPushButton { background-color: transparent; color: white; border: none; font-size: "
            "12px; } QPushButton:hover { color: red; }");
        connect(removeButton, &QPushButton::clicked, this,
                &OneSevenLiveStreamingDock::onRemoveTagClicked);

        tagWidgetLayout->addWidget(tagLabel);
        tagWidgetLayout->addWidget(removeButton);

        tagsLayout->addWidget(tagWidget);
    }

    // Add flexible space to align tags to the left
    tagsLayout->addStretch();

    // Check if tags limit is reached and disable/enable UI accordingly
    bool limitReached = tagsList.size() >= hashtagSelectLimit;
    addTagButton->setEnabled(!limitReached);
    tagEdit->setEnabled(!limitReached);
}

void OneSevenLiveStreamingDock::onSaveConfigClicked() {
    OneSevenLiveRtmpRequest request;
    if (!gatherRtmpRequest(request)) {
        obs_log(LOG_ERROR, "Failed to gather rtmp request");
        return;
    }

    OneSevenLiveStreamInfo streamInfo;
    for (const auto &subtab : configStreamer.subtabs) {
        if (subtab.ID == request.subtabID) {
            streamInfo.categoryName = subtab.displayName;
            break;
        }
    }
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

    // Show success message dialog
    QMessageBox::information(this, obs_module_text("Live.Settings.Save.Title"),
                             obs_module_text("Live.Settings.Save.Success"));
}

void OneSevenLiveStreamingDock::onCreateLiveClicked() {
    obs_log(LOG_INFO, "onCreateLiveClicked");

    // Create live stream
    OneSevenLiveRtmpRequest request;
    if (!gatherRtmpRequest(request)) {
        obs_log(LOG_ERROR, "Failed to gather rtmp request");
        return;
    }

    createLive(request);
}

void OneSevenLiveStreamingDock::createLiveWithRequest(const OneSevenLiveRtmpRequest &request) {
    obs_log(LOG_INFO, "createLiveWithRequest");

    if (isLoading) {
        // loading roomInfo is in progress, waiting for it to finish
        obs_log(LOG_INFO, "Waiting for loading to complete before creating live");

        // Create a timer to periodically check if loading is complete
        QTimer *waitTimer = new QTimer(this);
        waitTimer->setSingleShot(false);
        waitTimer->setInterval(100);  // Check every 100ms

        connect(waitTimer, &QTimer::timeout, this, [this, request, waitTimer]() {
            if (!isLoading) {
                // Loading is complete, stop timer and proceed with creation
                waitTimer->stop();
                waitTimer->deleteLater();

                obs_log(LOG_INFO, "Loading completed, proceeding with live creation");

                populateRtmpRequest(request);

                if (request.caption.isEmpty() || request.subtabID.isEmpty()) {
                    return;
                }

                createLive(request);
            }
        });

        waitTimer->start();
        return;
    }

    populateRtmpRequest(request);

    if (request.caption.isEmpty() || request.subtabID.isEmpty()) {
        return;
    }

    createLive(request);
}

void OneSevenLiveStreamingDock::editLiveWithInfo(const OneSevenLiveStreamInfo &info) {
    obs_log(LOG_INFO, "editLiveWithInfo");
    populateRtmpRequest(info.request);
    currentInfoUuid = info.streamUuid;
}

void OneSevenLiveStreamingDock::createLive(const OneSevenLiveRtmpRequest &request) {
    obs_log(LOG_INFO, "createLive");

    OneSevenLiveRtmpResponse response;
    if (!apiWrapper->CreateRtmp(request, response)) {
        obs_log(LOG_ERROR, "Failed to create stream");
        return;
    }

    startLive(request.userID.toStdString(), response, request.archiveConfig.autoRecording);
}

void OneSevenLiveStreamingDock::startLive(const std::string userID,
                                          const OneSevenLiveRtmpResponse &response,
                                          bool autoRecording, bool skip) {
    QString streamUrl;
    QString streamKey;

    // Regular expression /(^.+:\/\/[^/]+\/[^/]+)\/(.+)$/ to parse response.rtmpURL
    // First captured group is streamUrl, second captured group is streamKey
    // Example: rtmp://live-push.bilivideo.com/live-bvc/1234567890?expire=1680000000&usign=abcdefg
    QRegularExpression re("(^.+://[^/]+/[^/]+)/(.+)$");
    QRegularExpressionMatch match = re.match(response.rtmpURL);
    if (match.hasMatch()) {
        streamUrl = match.captured(1);
        streamKey = match.captured(2);
    } else {
        obs_log(LOG_ERROR, "Failed to parse stream url");
        return;
    }

    // obs_log(LOG_INFO, "streamUrl: %s", streamUrl.toStdString().c_str());
    // obs_log(LOG_INFO, "streamKey: %s", streamKey.toStdString().c_str());

    configManager->setStreamingInfo(response.liveStreamID.toStdString(), streamUrl.toStdString(),
                                    streamKey.toStdString());

    saveStreamingSettings(response.liveStreamID.toStdString(), streamUrl.toStdString(),
                          streamKey.toStdString());

    // Start live stream
    if (!skip && !apiWrapper->StartStream(response.liveStreamID.toStdString(), userID)) {
        obs_log(LOG_ERROR, "Failed to start stream");
        return;
    }

    // archive
    if (!skip && autoRecording) {
        if (!apiWrapper->EnableStreamArchive(response.liveStreamID.toStdString(), 1)) {
            obs_log(LOG_ERROR, "Failed to enable archive %s",
                    apiWrapper->getLastErrorMessage().toStdString().c_str());
        }
    }

    updateLiveStatus(OneSevenLiveStreamingStatus::Streaming);
    emit streamStatusUpdated(OneSevenLiveStreamingStatus::Streaming);

    // Ask whether to start streaming simultaneously
    QMessageBox msgBox;
    msgBox.setWindowTitle(obs_module_text("Live.Settings.StartStreaming"));
    msgBox.setText(obs_module_text("Live.Settings.StartStreaming.Tip"));

    // Use localized button text
    QPushButton *yesButton =
        msgBox.addButton(obs_module_text("Live.Settings.Yes"), QMessageBox::YesRole);
    /* QPushButton *noButton = */ msgBox.addButton(obs_module_text("Live.Settings.No"),
                                                   QMessageBox::NoRole);
    msgBox.setDefaultButton(yesButton);

    msgBox.exec();
    if (msgBox.clickedButton() == yesButton) {
        // Start OBS streaming
        obs_frontend_streaming_start();
    }
}

void OneSevenLiveStreamingDock::onDeleteLiveClicked() {
    obs_log(LOG_INFO, "onDeleteLiveClicked");

    // Add confirmation dialog
    QMessageBox msgBox;
    msgBox.setWindowTitle(obs_module_text("Live.Settings.CloseLive"));
    msgBox.setText(obs_module_text("Live.Settings.CloseLive.Confirm"));

    // Use localized button text
    QPushButton *confirmButton = msgBox.addButton(
        obs_module_text("Live.Settings.CloseLive.Confirm.Button"), QMessageBox::YesRole);
    QPushButton *cancelButton =
        msgBox.addButton(obs_module_text("Live.Settings.No"), QMessageBox::NoRole);
    msgBox.setDefaultButton(cancelButton);

    msgBox.exec();
    if (msgBox.clickedButton() != confirmButton) {
        // User cancelled the operation
        return;
    }

    std::string currUserID;
    std::string currLiveStreamID;
    configManager->getConfigValue("UserID", currUserID);
    configManager->getConfigValue("LiveStreamID", currLiveStreamID);

    closeLive(currUserID, currLiveStreamID);
}

void OneSevenLiveStreamingDock::closeLive(const std::string &currUserID,
                                          const std::string &currLiveStreamID) {
    // Handle stop streaming logic
    stopStreaming();

    // Send close live stream request
    OneSevenLiveCloseLiveRequest request;
    request.reason = "normalEnd";
    request.userID = QString::fromStdString(currUserID);

    if (!apiWrapper->StopStream(currLiveStreamID, request)) {
        obs_log(LOG_ERROR, "Failed to stop stream");
        // return;
    }

    configManager->clearStreamingInfo();

    updateLiveStatus(OneSevenLiveStreamingStatus::NotStarted);
    emit streamStatusUpdated(OneSevenLiveStreamingStatus::NotStarted);
}

void OneSevenLiveStreamingDock::saveStreamingSettings(const std::string &liveStreamID,
                                                      const std::string &streamUrl,
                                                      const std::string &streamKey) {
    // Handle start streaming logic
    obs_log(LOG_INFO, "saveStreamingSettings %s", liveStreamID.c_str());

    // Get OBS service
    obs_service_t *service = obs_service_create("rtmp_custom", "default_service", NULL, NULL);

    // Set streaming URL and key
    obs_data_t *settings = obs_service_get_settings(service);
    obs_log(LOG_INFO, "streamUrl: %s", streamUrl.c_str());
    obs_log(LOG_INFO, "streamKey: %s", streamKey.c_str());
    obs_data_set_string(settings, "server", streamUrl.c_str());
    obs_data_set_string(settings, "key", streamKey.c_str());

    // Apply settings
    obs_service_update(service, settings);
    obs_data_release(settings);

    obs_frontend_set_streaming_service(service);

    obs_frontend_save_streaming_service();

    // Release resources
    obs_service_release(service);
}

void OneSevenLiveStreamingDock::stopStreaming() {
    // Handle stop streaming logic
    obs_log(LOG_INFO, "stopStreaming");

    if (!obs_frontend_streaming_active()) {
        obs_log(LOG_INFO, "Streaming is not active");
        return;
    }

    obs_frontend_streaming_stop();
}

void OneSevenLiveStreamingDock::populateRtmpRequest(const OneSevenLiveRtmpRequest &request) {
    // Note: userID and streamerType are generally not editable, only displayed in interface or kept
    // synchronized
    roomInfo.userID = request.userID;
    roomInfo.streamerType = request.streamerType;

    titleEdit->setText(request.caption);

    // If your activityCombo uses setItemData to set eventID, find the corresponding index here
    int eventIndex = activityCombo->findData(QVariant(request.eventID));
    if (eventIndex >= 0) {
        activityCombo->setCurrentIndex(eventIndex);
    }

    // Clear and reload tag list
    tagsList.clear();
    tagsList = request.hashtags;
    updateTagsFromList();
    tagEdit->clear();

    if (request.landscape) {
        landscapeStreamRadio->setChecked(true);
    } else {
        portraitStreamRadio->setChecked(true);
    }

    int categoryIndex = categoryCombo->findData(QVariant(request.subtabID));
    if (categoryIndex >= 0) {
        categoryCombo->setCurrentIndex(categoryIndex);
    }

    // Army-only viewing settings
    armyOnlyCheck->setChecked(request.armyOnly.enable);

    int userConditionIndex =
        requiredArmyRankCombo->findData(QVariant(request.armyOnly.requiredArmyRank));
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

bool OneSevenLiveStreamingDock::gatherRtmpRequest(OneSevenLiveRtmpRequest &request) {
    obs_log(LOG_INFO, "gatherRtmpRequest");
    QString caption = titleEdit->text();
    if (caption.isEmpty()) {
        // Show dialog to prompt user to enter title
        QMessageBox::warning(this, obs_module_text("Live.Settings.Save.Title"),
                             obs_module_text("Live.Settings.Save.Title.Empty"));
        return false;
    }
    QString subtabID = categoryCombo->currentData().toString();
    if (subtabID.isEmpty()) {
        // Show dialog to prompt user to select category
        QMessageBox::warning(this, obs_module_text("Live.Settings.Save.Title"),
                             obs_module_text("Live.Settings.Save.Category.Empty"));
        return false;
    }
    request.userID = roomInfo.userID;
    request.caption = titleEdit->text();
    request.device = "OBS";
    int eventID = activityCombo->currentData().toInt();
    request.eventID = eventID;
    request.hashtags = tagsList;
    request.landscape = landscapeStreamRadio->isChecked();
    request.streamerType = roomInfo.streamerType;
    request.subtabID = categoryCombo->currentData().toString();

    // Army-only viewing settings
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

void OneSevenLiveStreamingDock::updateLiveButton(bool isLive) {
    obs_log(LOG_INFO, "updateLiveButton: %d", isLive);
    if (isLive) {
        // change text to "Stop Live"
        createLiveButton->setText(obs_module_text("Live.Settings.StopLive"));
        // Set green background to indicate currently live
        createLiveButton->setStyleSheet("background-color: #215EBC; color: white;");
        disconnect(createLiveButton, &QPushButton::clicked, this,
                   &OneSevenLiveStreamingDock::onCreateLiveClicked);
        connect(createLiveButton, &QPushButton::clicked, this,
                &OneSevenLiveStreamingDock::onDeleteLiveClicked);
    } else {
        // change text to "Start Live"
        createLiveButton->setText(obs_module_text("Live.Settings.StartLive"));
        // Set red background to indicate not currently live
        createLiveButton->setStyleSheet("background-color: red; color: white;");
        disconnect(createLiveButton, &QPushButton::clicked, this,
                   &OneSevenLiveStreamingDock::onDeleteLiveClicked);
        connect(createLiveButton, &QPushButton::clicked, this,
                &OneSevenLiveStreamingDock::onCreateLiveClicked);
    }
}

void OneSevenLiveStreamingDock::updateLiveStatus(OneSevenLiveStreamingStatus status) {
    currentLiveStatus = status;

    updateLiveButton(status != OneSevenLiveStreamingStatus::NotStarted);
}

void OneSevenLiveStreamingDock::resizeEvent(QResizeEvent *event) {
    QDockWidget::resizeEvent(event);

    // Update loading overlay size and position to always cover the entire visible area
    if (loadingOverlay && widget()) {
        QScrollArea *scrollArea = qobject_cast<QScrollArea *>(widget());
        if (scrollArea) {
            // Use viewport's rect() directly since loadingOverlay's parent is already viewport
            loadingOverlay->setGeometry(
                QRect(0, 0, scrollArea->viewport()->width(), scrollArea->viewport()->height()));
            loadingOverlay->raise();  // Ensure overlay is on top
        }
    }
}

#include "OneSevenLiveStreamingDock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QAction>
#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveCoreManager.hpp"
#include "OneSevenLiveCustomEventDialog.hpp"
#include "OneSevenLiveLoadRoomInfoWorker.hpp"
#include "OneSevenLiveStreamingDock.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "moc_OneSevenLiveStreamingDock.cpp"
#include "multi-rtmp/OneSevenLiveMultiRtmpManager.hpp"
#include "plugin-support.h"
#include "streaming/OneSevenLiveStreamManager.hpp"
#include "utility/Common.hpp"
#include "utility/Meta.hpp"

OneSevenLiveStreamingDock::OneSevenLiveStreamingDock(QWidget *parent,
                                                     OneSevenLiveStreamManager *streamManager_,
                                                     OneSevenLiveApiWrappers *apiWrappers_,
                                                     OneSevenLiveConfigManager *configManager_)
    : QDockWidget(obs_module_text("Live.Settings"), parent),
      streamManager(streamManager_),
      apiWrapper(apiWrappers_),
      configManager(configManager_),
      eventCooldownTimer(new QTimer(this)),
      eventCooldownRemaining(0) {
    // Initialize category cooldown timer
    eventCooldownTimer->setSingleShot(false);
    eventCooldownTimer->setInterval(1000);  // 1 second interval
    connect(eventCooldownTimer, &QTimer::timeout, this,
            &OneSevenLiveStreamingDock::onEventCooldownTimeout);

    connect(streamManager, &OneSevenLiveStreamManager::streamStatusChanged, this,
            &OneSevenLiveStreamingDock::updateLiveStatus);

    // Monitor OBS output signals for disconnection
    // We cannot directly connect to obs signals here easily as they are C callbacks.
    // Instead, we rely on StreamManager to monitor OBS signals or use a timer/status check.
    // However, StreamManager already has some status monitoring.
    // Let's add a signal from StreamManager when OBS stream stops unexpectedly.

    connect(streamManager, &OneSevenLiveStreamManager::obsStreamStopped, this,
            [this](int code, const QString &lastError) {
                obs_log(LOG_WARNING, "OBS stream stopped unexpectedly with code %d: %s", code,
                        lastError.toStdString().c_str());
                // If this was an unexpected stop (network error etc), we might want to reflect that
                // in UI For now, just ensure our internal status is updated if it wasn't already
                if (streamManager->getCurrentStreamingStatus() ==
                    OneSevenLiveStreamingStatus::Streaming) {
                    // If we were streaming, but OBS stopped, we should probably consider it as
                    // stopped or trying to reconnect? OBS has its own reconnection logic. If OBS
                    // completely gives up (e.g. after max retries), it stops. We should sync our
                    // status to NotStarted in that case.
                    streamManager->stopStream(false);
                }
            });

    connect(streamManager, &OneSevenLiveStreamManager::createRtmpFinished, this,
            [this](bool success, const QString &error) {
                createLiveButton->setEnabled(true);

                if (success) {
                    startEventCooldown();
                    startLive();
                } else {
                    QString msg = error;
                    if (msg.isEmpty())
                        msg = obs_module_text("Live.Create.Failed");
                    QMessageBox::warning(this, obs_module_text("Live.Create.Title"), msg);
                }
            });

    connect(streamManager, &OneSevenLiveStreamManager::startStreamFinished, this,
            [this](bool success, const QString &error) {
                createLiveButton->setEnabled(true);

                if (success) {
                    // Ask whether to start streaming simultaneously
                    QMessageBox msgBox(this);
                    msgBox.setWindowTitle(obs_module_text("Live.Settings.StartStreaming"));
                    msgBox.setText(obs_module_text("Live.Settings.StartStreaming.Tip"));

                    QPushButton *yesButton = msgBox.addButton(obs_module_text("Live.Settings.Yes"),
                                                              QMessageBox::YesRole);
                    msgBox.addButton(obs_module_text("Live.Settings.No"), QMessageBox::NoRole);
                    msgBox.setDefaultButton(yesButton);

                    msgBox.exec();
                    if (msgBox.clickedButton() == yesButton) {
                        streamManager->startOBSStreaming();
                    }
                } else {
                    QString msg = error;
                    if (msg.isEmpty())
                        msg = obs_module_text("Live.Start.Failed");
                    QMessageBox::warning(this, obs_module_text("Live.Settings.Error"), msg);
                }
            });

    connect(streamManager, &OneSevenLiveStreamManager::changeEventFinished, this,
            [this](bool success, const QString &error) {
                if (success) {
                    startEventCooldown();
                } else {
                    obs_log(LOG_ERROR, "Failed to change event: %s", error.toStdString().c_str());
                    QMessageBox::warning(this, obs_module_text("Live.Common.Notice"),
                                         obs_module_text("Live.ChangeEvent.Failed"));

                    if (!isEventInCooldown()) {
                        eventCombo->setEnabled(true);
                        if (previousEventIndex >= 0 && previousEventIndex < eventCombo->count()) {
                            eventCombo->blockSignals(true);
                            eventCombo->setCurrentIndex(previousEventIndex);
                            eventCombo->blockSignals(false);
                        }
                    }
                }
            });

    setupUi();
    createConnections();

    QTimer::singleShot(0, this, [this]() { loadRoomInfo(); });
}

OneSevenLiveStreamingDock::~OneSevenLiveStreamingDock() {
    if (streamManager) {
        disconnect(streamManager, &OneSevenLiveStreamManager::roomInfoLoaded, this, nullptr);
        disconnect(streamManager, &OneSevenLiveStreamManager::streamStatusChanged, this, nullptr);
    }

    // Disconnect all QComboBox signals to prevent crashes during destruction
    if (eventCombo) {
        disconnect(eventCombo, nullptr, this, nullptr);
    }
    if (categoryCombo) {
        disconnect(categoryCombo, nullptr, this, nullptr);
    }
    if (requiredArmyRankCombo) {
        disconnect(requiredArmyRankCombo, nullptr, this, nullptr);
    }
    if (clipIdentityCombo) {
        disconnect(clipIdentityCombo, nullptr, this, nullptr);
    }

    if (eventCooldownTimer->isActive()) {
        eventCooldownTimer->stop();
    }
}

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
    titleEdit->setObjectName("titleEdit");
    titleEdit->setPlaceholderText(obs_module_text("Live.Settings.Title.Placeholder"));
    formLayout->addRow(titleLabel, titleEdit);

    // Category selection
    categoryCombo = new QComboBox(this);  // Set parent to ensure proper cleanup
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
    eventCombo = new QComboBox(this);  // Set parent to ensure proper cleanup
    eventContainer->addWidget(eventCombo);

    // Create hint label and align right
    QHBoxLayout *hintLayout = new QHBoxLayout();
    hintLabel = new QLabel(obs_module_text("Live.Settings.Event.Tip"));
    hintLabel->setStyleSheet("color: gray; font-size: 12px;");
    hintLayout->addStretch();  // Add flexible space to align hint text to the right
    hintLayout->addWidget(hintLabel);
    eventContainer->addLayout(hintLayout);

    mainLayout->addLayout(eventContainer);

    // Custom Event (Optional)
    customEventHeader = new QWidget();
    customEventHeaderLayout = new QHBoxLayout(customEventHeader);
    customEventHeaderLayout->setContentsMargins(0, 10, 0, 10);

    customEventLabel = new QLabel(obs_module_text("CustomEvent.Dialog.Title"));
    customEventToggleButton = new QPushButton();
    customEventToggleButton->setIcon(QIcon(":/resources/arrow-down.svg"));
    customEventToggleButton->setStyleSheet(
        "QPushButton { border: none; background-color: transparent; }");
    customEventToggleButton->setFixedSize(24, 24);

    customEventHeaderLayout->addWidget(customEventLabel);
    customEventHeaderLayout->addStretch();
    customEventHeaderLayout->addWidget(customEventToggleButton);

    mainLayout->addWidget(customEventHeader);

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

    requiredArmyRankCombo = new QComboBox(this);  // Set parent to ensure proper cleanup
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

    // Party Live section
    // Create party live header with label, help button and switch
    GroupCallContainer = new QWidget();
    GroupCallContainerLayout = new QHBoxLayout(GroupCallContainer);
    GroupCallContainerLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *groupCallLabelLayout = new QHBoxLayout();
    GroupCallLabel = new QLabel(obs_module_text("Live.Settings.GroupCall"));
    GroupCallLabel->setStyleSheet("font-weight: bold;");

    // Help button with question icon
    GroupCallHelpButton = new QPushButton();
    GroupCallHelpButton->setIcon(QIcon(":/resources/question.svg"));
    GroupCallHelpButton->setFixedSize(20, 20);
    GroupCallHelpButton->setStyleSheet("QPushButton { border: none; background: transparent; }");
    GroupCallHelpButton->setToolTip(obs_module_text("Live.Settings.GroupCall.Help.Tooltip"));

    groupCallLabelLayout->addWidget(GroupCallLabel);
    groupCallLabelLayout->addWidget(GroupCallHelpButton);
    groupCallLabelLayout->addStretch();

    QVBoxLayout *groupCallLeftLayout = new QVBoxLayout();

    QLabel *groupCallTip = new QLabel(obs_module_text("Live.Settings.GroupCall.Tip"));
    groupCallTip->setStyleSheet("color: gray; font-size: 12px;");
    groupCallTip->setMaximumWidth(580);
    groupCallTip->setWordWrap(true);

    groupCallLeftLayout->addLayout(groupCallLabelLayout);
    groupCallLeftLayout->addWidget(groupCallTip);
    groupCallLeftLayout->setSpacing(2);  // Adjust spacing between title and hint

    GroupCallCheck = new QCheckBox();

    GroupCallContainerLayout->addLayout(groupCallLeftLayout);
    GroupCallContainerLayout->addStretch();
    GroupCallContainerLayout->addWidget(GroupCallCheck);

    mainLayout->addWidget(GroupCallContainer);

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
    clipIdentityCombo = new QComboBox(this);  // Set parent to ensure proper cleanup
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
void OneSevenLiveStreamingDock::loadRoomInfo() {
    if (!streamManager) {
        return;
    }

    streamManager->loadRoomInfo();

    if (streamManager && streamManager->isRoomInfoLoading()) {
        loadingOverlay->setVisible(true);
        loadingOverlay->raise();
        loadingLabel->setText(obs_module_text("Live.Settings.Loading"));
        QScrollArea *scrollArea = qobject_cast<QScrollArea *>(widget());
        if (scrollArea && scrollArea->widget()) {
            scrollArea->widget()->setEnabled(false);
        }

        // connect streamManager's roomInfoLoaded signal to updateUIWithRoomInfo slot

        connect(streamManager, &OneSevenLiveStreamManager::roomInfoLoaded, this,
                [this](const OneSevenLiveLoadRoomInfoWorker::LoadResult &result) {
                    roomInfo = streamManager->getRoomInfo();
                    configStreamer = streamManager->getConfigStreamer();
                    userInfo = streamManager->getUserInfo();
                    levels = streamManager->getArmyLevels();

                    handleLoadingCompleted(result);
                });
    }
}

void OneSevenLiveStreamingDock::showEvent(QShowEvent *event) {
    QDockWidget::showEvent(event);
    if (streamManager && streamManager->isRoomInfoLoading()) {
        loadingOverlay->setVisible(true);
        loadingOverlay->raise();
        loadingLabel->setText(obs_module_text("Live.Settings.Loading"));
        QScrollArea *scrollArea = qobject_cast<QScrollArea *>(widget());
        if (scrollArea && scrollArea->widget()) {
            scrollArea->widget()->setEnabled(false);
        }
    } else {
        loadingOverlay->setVisible(false);
        QScrollArea *scrollArea = qobject_cast<QScrollArea *>(widget());
        if (scrollArea && scrollArea->widget()) {
            scrollArea->widget()->setEnabled(true);
        }
    }
}

// Add new method to update UI based on roomInfo
void OneSevenLiveStreamingDock::updateUIWithRoomInfo() {
    obs_log(LOG_INFO, "Updating UI with room info");

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
        eventCombo->addItem(eventName, event.ID);
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

    if (roomInfo.status != static_cast<int>(OneSevenLiveStreamingStatus::Live) &&
        roomInfo.status != static_cast<int>(OneSevenLiveStreamingStatus::Streaming)) {
        return;
    }

    updateUIValues();

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

        if (msgBox.clickedButton() == closeLiveButton) {
            streamManager->stopStream(false);
            return;
        }
    }

    connect(streamManager, &OneSevenLiveStreamManager::webStreamSettingsLoaded, this,
            [this](bool success) {
                disconnect(streamManager, &OneSevenLiveStreamManager::webStreamSettingsLoaded, this,
                           nullptr);
                if (success) {
                    startEventCooldown();
                    startLive(!(roomInfo.status ==
                                static_cast<int>(OneSevenLiveStreamingStatus::Streaming)));
                } else {
                    obs_log(LOG_ERROR, "Failed to load web stream settings");
                }
            });
    streamManager->startStreamWithWebAsync();
}

// Handle loading completion with comprehensive error handling
void OneSevenLiveStreamingDock::handleLoadingCompleted(
    const OneSevenLiveLoadRoomInfoWorker::LoadResult &result) {
    obs_log(LOG_INFO, "OneSevenLiveStreamingDock::handleLoadingCompleted");

    loadingOverlay->setVisible(false);

    // Enable all controls
    QScrollArea *scrollArea = qobject_cast<QScrollArea *>(widget());
    if (scrollArea && scrollArea->widget()) {
        scrollArea->widget()->setEnabled(true);
    }

    // Log any API failures
    if (!result.roomInfoSuccess) {
        obs_log(LOG_WARNING, "Failed to get roomInfo in loadRoomInfo");
    }
    if (!result.userInfoSuccess) {
        obs_log(LOG_WARNING, "Failed to get user info in loadRoomInfo");
    }
    if (!result.levelsSuccess) {
        obs_log(LOG_WARNING, "Failed to get army subscription levels in loadRoomInfo");
    }

    // Check for critical error message first
    if (!result.errorMessage.empty()) {
        // Critical failure - show non-blocking error with retry option
        QMessageBox *msgBox = new QMessageBox(this);
        msgBox->setIcon(QMessageBox::Critical);
        msgBox->setWindowTitle(obs_module_text("Live.Settings.Error"));
        msgBox->setText(QString::fromStdString(result.errorMessage));

        QPushButton *retryButton =
            msgBox->addButton(obs_module_text("Live.Settings.Retry"), QMessageBox::ActionRole);
        msgBox->addButton(QMessageBox::Cancel);
        msgBox->setDefaultButton(retryButton);
        msgBox->setAttribute(Qt::WA_DeleteOnClose);

        connect(msgBox, &QMessageBox::finished, this, [this, msgBox, retryButton]() {
            if (msgBox->clickedButton() == retryButton) {
                // Get current room ID and retry loading
                qint64 currentRoomID = streamManager->getRoomID();

                if (currentRoomID > 0) {
                    loadRoomInfo();
                }
            }
        });

        msgBox->show();
        return;
    }

    // Check if we have all required data for updateUIWithRoomInfo
    // Required: configStreamer, roomInfo, userInfo
    // levels is required only when configStreamer.armyOnly == 2 && userInfo.onliveInfo.premiumType
    // != 1
    bool hasRequiredData =
        result.configStreamerSuccess && result.roomInfoSuccess && result.userInfoSuccess;

    // Check if levels is required based on army settings
    bool levelsRequired = false;
    if (result.configStreamerSuccess && result.userInfoSuccess) {
        levelsRequired = (configStreamer.armyOnly == 2 && userInfo.onliveInfo.premiumType != 1);
        if (levelsRequired) {
            hasRequiredData = hasRequiredData && result.levelsSuccess;
        }
    }

    if (!hasRequiredData) {
        // Log detailed information for debugging
        QStringList missingDataDetails;
        if (!result.configStreamerSuccess) {
            missingDataDetails << "ConfigStreamer";
        }
        if (!result.roomInfoSuccess) {
            missingDataDetails << "RoomInfo";
        }
        if (!result.userInfoSuccess) {
            missingDataDetails << "UserInfo";
        }
        if (levelsRequired && !result.levelsSuccess) {
            missingDataDetails << "Levels (required for army settings)";
        }

        obs_log(LOG_WARNING,
                "[17Live] Failed to load required streaming configuration data. Missing: %s",
                missingDataDetails.join(", ").toUtf8().constData());

        // Show simplified user message consistent with error message box above
        QMessageBox *msgBox = new QMessageBox(this);
        msgBox->setIcon(QMessageBox::Warning);
        msgBox->setWindowTitle(obs_module_text("Live.Settings.Warning"));
        msgBox->setText(obs_module_text("Live.Settings.RequiredDataMissing"));

        QPushButton *retryButton =
            msgBox->addButton(obs_module_text("Live.Settings.Retry"), QMessageBox::ActionRole);
        msgBox->addButton(QMessageBox::Cancel);
        msgBox->setDefaultButton(retryButton);
        msgBox->setAttribute(Qt::WA_DeleteOnClose);

        connect(msgBox, &QMessageBox::finished, this, [this, msgBox, retryButton]() {
            if (msgBox->clickedButton() == retryButton) {
                // Get current room ID and retry loading
                qint64 currentRoomID = streamManager->getRoomID();
                if (currentRoomID > 0) {
                    obs_log(LOG_INFO, "[17Live] User requested retry for room ID: %lld",
                            currentRoomID);
                    loadRoomInfo();
                }
            }
        });

        msgBox->show();
        return;
    }

    // Show warnings for non-critical failures (levels is optional when not required for army
    // settings)
    if (!levelsRequired && !result.levelsSuccess) {
        obs_log(LOG_INFO,
                "[17Live] Levels data failed to load but not required for current army settings");
        // No user notification needed when levels is not required
    }

    // All required data loaded successfully - update UI
    updateUIWithRoomInfo();
}

void OneSevenLiveStreamingDock::updateRequiredArmyRankSelections() {
    // obs_log(LOG_INFO, "updateRequiredArmyRankSelections");

    OneSevenLiveConfig config;

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
                    currentEventIndex = eventCombo->findData(eventID);
                    break;
                }
            }
        }
        eventCombo->setCurrentIndex(currentEventIndex);

        GroupCallCheck->setChecked(roomInfo.enableOBSGroupCall);
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

    // Custom event toggle button
    connect(customEventToggleButton, &QPushButton::clicked, this,
            &OneSevenLiveStreamingDock::onCustomEventToggleClicked);

    // Party live help button
    connect(GroupCallHelpButton, &QPushButton::clicked, this,
            &OneSevenLiveStreamingDock::onGroupCallHelpClicked);

    // Event change event
    connect(eventCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &OneSevenLiveStreamingDock::onEventChanged);
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

void OneSevenLiveStreamingDock::onCustomEventToggleClicked() {
    if (customEventDialog) {
        customEventToggleButton->setIcon(QIcon(":/resources/arrow-down.svg"));

        OneSevenLiveCustomEventDialog *dialog = customEventDialog;
        customEventDialog = nullptr;

        if (dialog) {
            dialog->close();
            dialog->deleteLater();
        }
    } else {
        OneSevenLiveCustomEventDialog *dialog =
            new OneSevenLiveCustomEventDialog(this, apiWrapper, configManager);
        customEventDialog = dialog;

        connect(dialog, &QDialog::finished, this, [this]() {
            customEventToggleButton->setIcon(QIcon(":/resources/arrow-down.svg"));
            customEventDialog = nullptr;
        });

        customEventToggleButton->setIcon(QIcon(":/resources/arrow-up.svg"));

        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    }
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
        if (QWidget *w = child->widget()) {
            w->setParent(nullptr);
            delete w;
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
    if (!streamManager)
        return;
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

    if (!streamManager->saveStreamConfiguration(streamInfo)) {
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

    startCreateLiveSequence(request);
}

void OneSevenLiveStreamingDock::createLiveWithRequest(const OneSevenLiveRtmpRequest &request) {
    obs_log(LOG_INFO, "createLiveWithRequest");

    if (!streamManager)
        return;

    if (streamManager && streamManager->isRoomInfoLoading()) {
        // loading roomInfo is in progress, waiting for it to finish
        obs_log(LOG_INFO, "Waiting for loading to complete before creating live");

        // connect streamManager's roomInfoLoaded signal to updateUIWithRoomInfo slot
        disconnect(streamManager, &OneSevenLiveStreamManager::roomInfoLoaded, this, nullptr);
        connect(streamManager, &OneSevenLiveStreamManager::roomInfoLoaded, this,
                [this, request](const OneSevenLiveLoadRoomInfoWorker::LoadResult &result) {
                    roomInfo = streamManager->getRoomInfo();
                    configStreamer = streamManager->getConfigStreamer();
                    userInfo = streamManager->getUserInfo();
                    levels = streamManager->getArmyLevels();

                    handleLoadingCompleted(result);

                    populateRtmpRequest(request);

                    if (request.caption.isEmpty() || request.subtabID.isEmpty()) {
                        return;
                    }

                    startCreateLiveSequence(request);
                });

        return;
    }

    populateRtmpRequest(request);

    if (request.caption.isEmpty() || request.subtabID.isEmpty()) {
        return;
    }

    startCreateLiveSequence(request);
}

void OneSevenLiveStreamingDock::editLiveWithInfo(const OneSevenLiveStreamInfo &info) {
    obs_log(LOG_INFO, "editLiveWithInfo");

    if (!streamManager)
        return;

    if (streamManager && streamManager->isRoomInfoLoading()) {
        // loading roomInfo is in progress, waiting for it to finish
        obs_log(LOG_INFO, "Waiting for loading to complete before editing live info");

        // connect streamManager's roomInfoLoaded signal to updateUIWithRoomInfo slot
        disconnect(streamManager, &OneSevenLiveStreamManager::roomInfoLoaded, this, nullptr);
        connect(streamManager, &OneSevenLiveStreamManager::roomInfoLoaded, this,
                [this, info](const OneSevenLiveLoadRoomInfoWorker::LoadResult &result) {
                    roomInfo = streamManager->getRoomInfo();
                    configStreamer = streamManager->getConfigStreamer();
                    userInfo = streamManager->getUserInfo();
                    levels = streamManager->getArmyLevels();

                    handleLoadingCompleted(result);

                    populateRtmpRequest(info.request);
                    currentInfoUuid = info.streamUuid;
                });

        return;
    }

    populateRtmpRequest(info.request);
    currentInfoUuid = info.streamUuid;
}

void OneSevenLiveStreamingDock::startCreateLiveSequence(const OneSevenLiveRtmpRequest &request_) {
    obs_log(LOG_INFO, "startCreateLiveSequence (Async)");

    if (!streamManager)
        return;

    // Disable button to prevent double click
    createLiveButton->setEnabled(false);

    // Show loading overlay
    loadingOverlay->setVisible(true);
    loadingOverlay->raise();
    loadingLabel->setText(obs_module_text("Live.Settings.Loading"));

    // Prepare data for async task
    std::string currentRegion;
    configManager->getConfigValue("Region", currentRegion);

    // Capture necessary pointers and data
    auto *api = this->apiWrapper;
    QPointer<OneSevenLiveStreamingDock> self = this;
    OneSevenLiveRtmpRequest requestCopy = request_;

    ScheduleOBSTask([self, api, currentRegion, requestCopy]() {
        OneSevenLiveLoginData loginData;
        nlohmann::json configJson;
        bool selfInfoSuccess = false;
        bool configSuccess = false;
        QString errorMsg;

        if (api) {
            selfInfoSuccess = api->GetSelfInfo(loginData);
            if (!selfInfoSuccess) {
                errorMsg = api->getLastErrorMessage();
            }

            if (selfInfoSuccess &&
                loginData.userInfo.region != QString::fromStdString(currentRegion)) {
                std::string language = GetCurrentLanguage();
                // Region changed, need to fetch config
                configSuccess = api->GetConfig(currentRegion, language, configJson);
            }
        }

        if (self) {
            QMetaObject::invokeMethod(
                self,
                [self, selfInfoSuccess, configSuccess, loginData, configJson, requestCopy,
                 errorMsg]() {
                    if (self) {
                        self->handleCreateLiveChecks(loginData, configJson, selfInfoSuccess,
                                                     errorMsg, requestCopy);
                    }
                },
                Qt::QueuedConnection);
        }
    });
}

void OneSevenLiveStreamingDock::handleCreateLiveChecks(const OneSevenLiveLoginData &loginData,
                                                       const nlohmann::json &configJson,
                                                       bool success, const QString &error,
                                                       const OneSevenLiveRtmpRequest &request_) {
    loadingOverlay->setVisible(false);
    createLiveButton->setEnabled(true);

    if (!success) {
        obs_log(LOG_ERROR, "GetSelfInfo failed: %s", error.toStdString().c_str());
        QMessageBox::warning(this, obs_module_text("Live.Create.Title"),
                             obs_module_text("Live.Create.GetSelfInfoFailed"));
        return;
    }

    // check current region changed?
    std::string currentRegion;
    if (!configManager->getConfigValue("Region", currentRegion)) {
        const auto err = configManager->getLastError();
        obs_log(LOG_WARNING, "Failed to read Region config: %s %s", err.code.c_str(),
                err.message.c_str());
        currentRegion.clear();
    }

    // Check feature 207 to control createLiveButton state
    OneSevenLiveConfig currentConfig;
    if (!configManager->getConfig(currentConfig)) {
        const auto err = configManager->getLastError();
        obs_log(LOG_WARNING, "Failed to read current config: %s %s", err.code.c_str(),
                err.message.c_str());
    }
    bool currentIsFeature207Enabled = (currentConfig.addOns.features["207"] == 1);

    if (loginData.userInfo.region != QString::fromStdString(currentRegion)) {
        obs_log(LOG_INFO, "Region changed, reload config");

        // Save configuration
        if (!configJson.empty()) {
            if (!configManager->setConfig(configJson)) {
                const auto err = configManager->getLastError();
                obs_log(LOG_ERROR, "Failed to save config: %s %s", err.code.c_str(),
                        err.message.c_str());
                QMessageBox::warning(this, obs_module_text("Live.Create.Title"),
                                     obs_module_text("Live.Create.Failed"));
                return;
            }
            obs_log(LOG_INFO, "Config saved successfully");
        } else {
            obs_log(LOG_ERROR, "Failed to load config from API (or it was empty)");
            QMessageBox::warning(this, obs_module_text("Live.Create.Title"),
                                 obs_module_text("Live.Create.Failed"));
            return;
        }

        OneSevenLiveConfig newConfig;
        JsonToOneSevenLiveConfig(configJson, newConfig);

        bool newIsFeature207Enabled = (newConfig.addOns.features["207"] == 1);

        if (!newIsFeature207Enabled) {
            QMessageBox::warning(this, obs_module_text("Live.Create.Title"),
                                 obs_module_text("Live.Create.Feature207Disabled"));
            return;
        } else if (!currentIsFeature207Enabled && request_.subtabID.isEmpty()) {
            // Feature 207 enabled now, but subtabID is empty, show warning
            // Show dialog to prompt user to select category
            QMessageBox::warning(this, obs_module_text("Live.Create.Title"),
                                 obs_module_text("Live.Settings.Save.Category.Empty"));

            return;
        }
    } else if (!currentIsFeature207Enabled) {
        QMessageBox::warning(this, obs_module_text("Live.Create.Title"),
                             obs_module_text("Live.Create.Feature207Disabled"));
        return;
    }

    // Validate request
    OneSevenLiveRtmpRequest request = request_;

    if (request.caption.isEmpty()) {
        // Show dialog to prompt user to enter title
        QMessageBox::warning(this, obs_module_text("Live.Settings.Save.Title"),
                             obs_module_text("Live.Settings.Save.Title.Empty"));
        return;
    }

    // Use stream manager to create live stream
    createLiveButton->setEnabled(false);
    streamManager->createRtmpAsync(request);
}

void OneSevenLiveStreamingDock::createLive(const OneSevenLiveRtmpRequest &request_) {
    startCreateLiveSequence(request_);
}

void OneSevenLiveStreamingDock::startLive(bool startStream) {
    obs_log(LOG_INFO, "Starting live stream");

    if (!streamManager)
        return;

    if (startStream) {
        // Start streaming (server-side)
        createLiveButton->setEnabled(false);
        streamManager->startStreamAsync();
    } else {
        // update streaming status
        streamManager->setCurrentStreamingStatus(OneSevenLiveStreamingStatus::Streaming);

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
            streamManager->startOBSStreaming();
        }
    }
}

void OneSevenLiveStreamingDock::onDeleteLiveClicked() {
    obs_log(LOG_INFO, "onDeleteLiveClicked");

    if (!streamManager)
        return;

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

    // check other live streams
    bool multiActive = false;
    if (OneSevenLiveMultiRtmpManager::peekInstance() &&
        OneSevenLiveMultiRtmpManager::peekInstance()->isInitialized()) {
        auto ids = OneSevenLiveMultiRtmpManager::peekInstance()->getActiveStreamIds();
        multiActive = !ids.empty();
    }
    if (multiActive) {
        QMessageBox::information(this, obs_module_text("Live.Common.Notice"),
                                 obs_module_text("MultiRTMP.CloseLive.InfoTip"));
    }

    // Stop streaming
    streamManager->stopStream(false);
}

void OneSevenLiveStreamingDock::populateRtmpRequest(const OneSevenLiveRtmpRequest &request) {
    // Note: userID and streamerType are generally not editable, only displayed in interface or kept
    // synchronized
    // roomInfo.userID = request.userID;
    // roomInfo.streamerType = request.streamerType;

    titleEdit->setText(request.caption);

    // If your eventCombo uses setItemData to set eventID, find the corresponding index here
    int eventIndex = eventCombo->findData(QVariant(request.eventID));
    if (eventIndex >= 0) {
        eventCombo->setCurrentIndex(eventIndex);
        previousEventIndex = eventIndex;  // Initialize previous event index
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

    GroupCallCheck->setChecked(request.enableOBSGroupCall);

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

    request.caption = titleEdit->text();
    request.device = "OBS";
    int eventID = eventCombo->currentData().toInt();
    request.eventID = eventID;
    request.hashtags = tagsList;
    request.landscape = landscapeStreamRadio->isChecked();
    request.subtabID = categoryCombo->currentData().toString();

    // Army-only viewing settings
    request.armyOnly.enable = armyOnlyCheck->isChecked();
    request.armyOnly.requiredArmyRank = requiredArmyRankCombo->currentData().toInt();
    request.armyOnly.showOnHotPage = showInHotPageCheck->isChecked();
    request.armyOnly.armyOnlyPN = liveNotificationCheck->isChecked();

    request.enableOBSGroupCall = GroupCallCheck->isChecked();

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
        disconnect(createLiveButton, &QPushButton::clicked, this,
                   &OneSevenLiveStreamingDock::onDeleteLiveClicked);
        connect(createLiveButton, &QPushButton::clicked, this,
                &OneSevenLiveStreamingDock::onDeleteLiveClicked, Qt::UniqueConnection);
    } else {
        // change text to "Start Live"
        createLiveButton->setText(obs_module_text("Live.Settings.StartLive"));
        // Set red background to indicate not currently live
        createLiveButton->setStyleSheet("background-color: red; color: white;");
        disconnect(createLiveButton, &QPushButton::clicked, this,
                   &OneSevenLiveStreamingDock::onDeleteLiveClicked);
        disconnect(createLiveButton, &QPushButton::clicked, this,
                   &OneSevenLiveStreamingDock::onCreateLiveClicked);
        connect(createLiveButton, &QPushButton::clicked, this,
                &OneSevenLiveStreamingDock::onCreateLiveClicked, Qt::UniqueConnection);
    }
}

void OneSevenLiveStreamingDock::updateLiveStatus(OneSevenLiveStreamingStatus status) {
    obs_log(LOG_INFO, "Updating live status to %d", status);

    currentLiveStatus = status;

    updateLiveButton(status != OneSevenLiveStreamingStatus::NotStarted);

    if (status == OneSevenLiveStreamingStatus::NotStarted) {
        if (eventCooldownTimer && eventCooldownTimer->isActive()) {
            eventCooldownTimer->stop();
            eventCooldownRemaining = 0;
            if (hintLabel) {
                hintLabel->setText(obs_module_text("Live.Settings.Event.Tip"));
                hintLabel->setStyleSheet("color: gray; font-size: 12px;");
            }
            emit eventCooldownUpdated(0);
        }
    }

    // Disable ALL controls above the bottom buttons when streaming is active
    // Only keep save and create live buttons enabled
    bool isStreaming = (status == OneSevenLiveStreamingStatus::Live ||
                        status == OneSevenLiveStreamingStatus::Streaming);

    auto setEnabledSafe = [&](QWidget *w, bool enabled) {
        if (w)
            w->setEnabled(enabled);
    };

    bool enable = !isStreaming;

    // Basic info
    setEnabledSafe(titleEdit, enable);
    setEnabledSafe(categoryCombo, enable);

    // Special handling for eventCombo during live streaming
    if (isStreaming) {
        // During live streaming, eventCombo should be enabled unless cooldown is active
        bool eventEnabled = !eventCooldownTimer || !eventCooldownTimer->isActive();
        setEnabledSafe(eventCombo, eventEnabled);
    } else {
        // For other states, use normal enabled logic
        setEnabledSafe(eventCombo, enable);
    }

    // Tags
    setEnabledSafe(tagEdit, enable);
    setEnabledSafe(addTagButton, enable);
    setEnabledSafe(tagsContainer, enable);

    // Layout (portrait/landscape)
    setEnabledSafe(portraitStreamRadio, enable);
    setEnabledSafe(landscapeStreamRadio, enable);

    // Army-only section
    setEnabledSafe(armyOnlyHeader, enable);
    setEnabledSafe(armyOnlyContainer, enable);
    setEnabledSafe(armyOnlyToggleButton, enable);
    setEnabledSafe(armyOnlyCheck, enable);
    setEnabledSafe(requiredArmyRankCombo, enable);
    setEnabledSafe(showInHotPageCheck, enable);
    setEnabledSafe(liveNotificationCheck, enable);

    // Party Live (Group Call)
    setEnabledSafe(GroupCallContainer, enable);
    setEnabledSafe(GroupCallHelpButton, enable);
    setEnabledSafe(GroupCallCheck, enable);

    // Archive / Preview / Clip / Virtual Liver
    setEnabledSafe(archiveStreamCheck, enable);
    setEnabledSafe(autoPreviewCheck, enable);
    setEnabledSafe(clipIdentityCombo, enable);
    setEnabledSafe(virtualStreamerCheck, enable);

    // Keep bottom buttons enabled regardless of streaming status
    setEnabledSafe(saveConfigButton, true);
    setEnabledSafe(createLiveButton, true);
}

void OneSevenLiveStreamingDock::onGroupCallHelpClicked() {
    QMessageBox helpDialog(this);
    helpDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    // helpDialog.setWindowTitle(obs_module_text("Live.Settings.GroupCall.Help.Title"));
    helpDialog.setIcon(QMessageBox::NoIcon);

    // Enable rich text format for HTML content
    helpDialog.setTextFormat(Qt::RichText);
    helpDialog.setText(obs_module_text("Live.Settings.GroupCall.Help.Content"));

    helpDialog.addButton(obs_module_text("Live.Settings.GroupCall.Help.Button"),
                         QMessageBox::AcceptRole);

    // Set dialog size to accommodate the content
    helpDialog.setMinimumWidth(500);
    helpDialog.setMinimumHeight(400);

    // Apply modern dark theme styling
    helpDialog.setStyleSheet(
        "QMessageBox {"
        "    background-color: #4A5568;"
        "    border-radius: 10px;"
        "    color: white;"
        "    font-size: 14px;"
        "}"
        "QMessageBox QLabel {"
        "    color: white;"
        "    background-color: transparent;"
        "    padding: 15px;"
        "    font-size: 14px;"
        "    font-weight: normal;"
        "    line-height: 1.4;"
        "    word-wrap: break-word;"
        "    max-width: 450px;"
        "}"
        "QMessageBox QDialogButtonBox {"
        "    text-align: center;"
        "    qproperty-centerButtons: true;"
        "    padding-top: 10px;"
        "}"
        "QMessageBox QPushButton {"
        "    background-color: #007AFF;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    width: 120px;"
        "    height: 35px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    margin: 5px;"
        "}"
        "QMessageBox QPushButton:hover {"
        "    background-color: #0056CC;"
        "}");

    helpDialog.exec();
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

void OneSevenLiveStreamingDock::onEventChanged(int index) {
    // Only handle event changes during live streaming
    if (currentLiveStatus != OneSevenLiveStreamingStatus::Live &&
        currentLiveStatus != OneSevenLiveStreamingStatus::Streaming) {
        previousEventIndex = index;
        return;
    }

    // If cooldown is active, ignore the change
    if (eventCooldownTimer && eventCooldownTimer->isActive()) {
        obs_log(LOG_INFO, "Event change ignored due to cooldown");
        // Restore previous selection
        if (previousEventIndex >= 0 && previousEventIndex < eventCombo->count()) {
            eventCombo->blockSignals(true);
            eventCombo->setCurrentIndex(previousEventIndex);
            eventCombo->blockSignals(false);
        }
        return;
    }

    // Show confirmation dialog
    QString eventName = eventCombo->itemText(index);
    QString title = obs_module_text("Live.EventChange.Confirm.Title");
    QString message = QString(obs_module_text("Live.EventChange.Confirm.Message")).arg(eventName);
    QString cancelText = obs_module_text("Live.EventChange.Confirm.Cancel");
    QString confirmText = obs_module_text("Live.EventChange.Confirm.Confirm");

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(title);
    msgBox.setText(message);
    msgBox.addButton(cancelText, QMessageBox::RejectRole);
    QPushButton *confirmButton = msgBox.addButton(confirmText, QMessageBox::AcceptRole);
    msgBox.setDefaultButton(confirmButton);

    int result = msgBox.exec();

    // If user cancels, restore previous selection
    if (result == QMessageBox::Rejected) {
        if (previousEventIndex >= 0 && previousEventIndex < eventCombo->count()) {
            eventCombo->blockSignals(true);
            eventCombo->setCurrentIndex(previousEventIndex);
            eventCombo->blockSignals(false);
        }
        return;
    }

    // User confirmed, proceed with event change
    previousEventIndex = index;

    // Get current event data
    QVariant eventIDVariant = eventCombo->itemData(index);
    if (!eventIDVariant.isValid()) {
        obs_log(LOG_WARNING, "No event ID found for event index %d", index);
        return;
    }

    qint64 eventID = eventIDVariant.toLongLong();
    if (eventID == 0) {
        obs_log(LOG_WARNING, "Invalid event ID for event index %d", index);
        return;
    }

    // Call ChangeEvent API
    changeEvent(eventID);
}

void OneSevenLiveStreamingDock::changeEvent(qint64 eventID) {
    obs_log(LOG_INFO, "Changing event to: %lld", eventID);

    if (!streamManager)
        return;

    // Check if we're in cooldown
    if (isEventInCooldown()) {
        obs_log(LOG_INFO, "Event change ignored due to cooldown");
        return;
    }

    eventCombo->setEnabled(false);

    // Call ChangeEvent API
    OneSevenLiveChangeEventRequest request;
    request.eventID = eventID;

    streamManager->changeEventAsync(request);
}

void OneSevenLiveStreamingDock::startEventCooldown(int duration) {
    eventCooldownRemaining = duration;
    eventCooldownTimer->start();

    emit eventCooldownUpdated(eventCooldownRemaining);
    obs_log(LOG_INFO, "Event cooldown started for %d seconds", duration);
    originalCategoryText = eventCombo->currentText();
    eventCombo->setEnabled(false);
}

bool OneSevenLiveStreamingDock::isEventInCooldown() const {
    return eventCooldownTimer->isActive();
}

int OneSevenLiveStreamingDock::getEventCooldownRemaining() const {
    return eventCooldownRemaining;
}

void OneSevenLiveStreamingDock::onEventCooldownTimeout() {
    if (eventCooldownRemaining > 0) {
        eventCooldownRemaining--;

        // Update hint label to show remaining time
        int minutes = eventCooldownRemaining / 60;
        int seconds = eventCooldownRemaining % 60;
        QString cooldownText = QString(obs_module_text("Live.EventChange.CoolDown"))
                                   .arg(minutes, 2, 10, QChar('0'))
                                   .arg(seconds, 2, 10, QChar('0'));

        hintLabel->setText(cooldownText);
        hintLabel->setStyleSheet("color: orange; font-size: 12px;");

        emit eventCooldownUpdated(eventCooldownRemaining);
    } else {
        // Cooldown finished
        eventCooldownTimer->stop();

        // Restore original hint text
        hintLabel->setText(obs_module_text("Live.Settings.Event.Tip"));
        hintLabel->setStyleSheet("color: gray; font-size: 12px;");

        // Re-enable event combo based on current live status
        // Call updateLiveStatus to ensure consistent state handling across all UI elements
        updateLiveStatus(currentLiveStatus);

        emit eventCooldownUpdated(0);
        obs_log(LOG_INFO, "Event cooldown finished");
    }
}

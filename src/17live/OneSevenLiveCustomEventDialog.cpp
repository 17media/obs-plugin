#include "OneSevenLiveCustomEventDialog.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPixmap>
#include <QStyle>
#include <QToolTip>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QTabWidget>
#include <QLabel>
#include <QGridLayout>

#include "api/OneSevenLiveApiWrappers.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveModels.hpp"
#include "moc_OneSevenLiveCustomEventDialog.cpp"

OneSevenLiveCustomEventDialog::OneSevenLiveCustomEventDialog(QWidget* parent,
                                                           OneSevenLiveApiWrappers* apiWrapper_,
                                                           OneSevenLiveConfigManager* configManager_)
    : QDialog(parent), apiWrapper(apiWrapper_), configManager(configManager_) {
    setupUi();
    setWindowTitle(obs_module_text("CustomEvent.Dialog.Title"));
    setFixedSize(400, 700);
    setModal(false);  // Set to non-modal
    
    // Ensure the dialog is properly initialized and visible
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    
    // Ensure all widgets are properly initialized
    update();
}

OneSevenLiveCustomEventDialog::~OneSevenLiveCustomEventDialog() = default;

void OneSevenLiveCustomEventDialog::setupUi() {
    // Set dialog background to black with modern styling
    setStyleSheet(
        "QDialog {"
        "    background-color: #1a1a1a;"
        "    color: #ffffff;"
        "    font-family: 'Inter', 'Microsoft YaHei', sans-serif;"
        "    font-style: normal;"
        "}"
        "QLabel {"
        "    color: #ffffff;"
        "    font-size: 14px;"
        "    font-weight: 400;"
        "    margin: 5px 0;"
        "}"
        "QLineEdit, QTextEdit, QSpinBox, QDateEdit {"
        "    background-color: #2d2d2d;"
        "    border: 1px solid #404040;"
        "    border-radius: 6px;"
        "    padding: 8px 12px;"
        "    color: #ffffff;"
        "    font-size: 14px;"
        "    min-height: 20px;"
        "}"
        "QLineEdit:focus, QTextEdit:focus, QSpinBox:focus, QDateEdit:focus {"
        "    border-color: #007acc;"
        "    outline: none;"
        "}"
        "QPushButton {"
        "    background-color: #2d2d2d;"
        "    border: 1px solid #404040;"
        "    border-radius: 6px;"
        "    padding: 8px 16px;"
        "    color: #ffffff;"
        "    font-size: 14px;"
        "    font-weight: 500;"
        "    min-height: 20px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #3d3d3d;"
        "    border-color: #505050;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1d1d1d;"
        "}"
        "QPushButton#createButton {"
        "    background-color: #ff4757;"
        "    border-color: #ff4757;"
        "    color: #ffffff;"
        "    font-weight: 600;"
        "}"
        "QPushButton#createButton:hover {"
        "    background-color: #ff3742;"
        "}"
        "QPushButton#createButton:pressed {"
        "    background-color: #e63946;"
        "}"
        "QCalendarWidget {"
        "    background-color: #2d2d2d;"
        "    border: 1px solid #404040;"
        "    border-radius: 6px;"
        "    color: #ffffff;"
        "}"
        "QCalendarWidget QToolButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    color: #ffffff;"
        "    font-size: 12px;"
        "    padding: 4px;"
        "}"
        "QCalendarWidget QToolButton:hover {"
        "    background-color: #3d3d3d;"
        "    border-radius: 4px;"
        "}"
        "QCalendarWidget QAbstractItemView {"
        "    background-color: #2d2d2d;"
        "    selection-background-color: #007acc;"
        "    color: #ffffff;"
        "}"
        "QScrollArea {"
        "    border: none;"
        "    background-color: #1a1a1a;"
        "}"
        "QScrollBar:vertical {"
        "    background-color: #2d2d2d;"
        "    width: 12px;"
        "    border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background-color: #555555;"
        "    border-radius: 6px;"
        "    min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background-color: #666666;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    border: none;"
        "    background: none;"
        "}"
        "QFrame {"
        "    background-color: #2d2d2d;"
        "    border: 1px solid #404040;"
        "    border-radius: 6px;"
        "}");

    // Create main dialog layout
    QVBoxLayout* dialogLayout = new QVBoxLayout(this);
    dialogLayout->setSpacing(0);
    dialogLayout->setContentsMargins(0, 0, 0, 0);

    // Create scroll area for main content
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    // Create content widget for scroll area
    QWidget* contentWidget = new QWidget();
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setSpacing(15);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    
    // Store the content layout for use in setup functions
    mainLayout = contentLayout;
    
    setupEventTitleSection();
    setupEventDateSection();
    setupEventGiftsSection();
    setupEventTargetsSection();
    setupEventDescriptionSection();
    
    // Add stretch to push content to top
    contentLayout->addStretch();
    
    // Set content widget to scroll area
    scrollArea->setWidget(contentWidget);
    
    // Add scroll area to dialog layout
    dialogLayout->addWidget(scrollArea);
    
    // Setup bottom buttons directly with dialog layout
    setupBottomButtons(dialogLayout);
}

void OneSevenLiveCustomEventDialog::setupEventTitleSection() {
    titleLabel = new QLabel();
    titleLabel->setText(
        QString("<span style='color:red;'>*</span><span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.Title")));
    
    eventTitleEdit = new QLineEdit(this);
    eventTitleEdit->setObjectName("eventTitleEdit");
    eventTitleEdit->setPlaceholderText("例：一起来玩你的日记吧！");
    eventTitleEdit->setStyleSheet(
        "QLineEdit {"
        "    background-color: #3a3a3a;"
        "    border: 1px solid #555555;"
        "    color: #ffffff;"
        "    placeholder-text-color: #888888;"
        "}");
    
    // Create form layout for title section
    QFormLayout* titleFormLayout = new QFormLayout();
    titleFormLayout->addRow(titleLabel, eventTitleEdit);
    
    mainLayout->addLayout(titleFormLayout);
}

void OneSevenLiveCustomEventDialog::setupEventDateSection() {
    dateLabel = new QLabel();
    dateLabel->setText(
        QString("<span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.EndDate")));
    dateLabel->setStyleSheet("font-weight: 600;");
    
    // Date input with format
    dateEdit = new QDateEdit(this);
    dateEdit->setDate(QDate::currentDate().addDays(1));
    dateEdit->setMinimumDate(QDate::currentDate().addDays(1));
    dateEdit->setMaximumDate(QDate::currentDate().addDays(30));
    dateEdit->setDisplayFormat("yyyy/MM/dd");
    dateEdit->setCalendarPopup(false);
    
    // Create form layout for date section
    QFormLayout* dateFormLayout = new QFormLayout();
    dateFormLayout->addRow(dateLabel, dateEdit);
    
    // Calendar widget
    calendarFrame = new QFrame(this);
    calendarFrame->setFixedHeight(200);
    
    QVBoxLayout* calendarLayout = new QVBoxLayout(calendarFrame);
    calendarLayout->setContentsMargins(5, 5, 5, 5);
    
    calendar = new QCalendarWidget(this);
    calendar->setMinimumDate(QDate::currentDate().addDays(1));
    calendar->setMaximumDate(QDate::currentDate().addDays(30));
    calendar->setSelectedDate(QDate::currentDate().addDays(1));
    calendar->setGridVisible(true);
    
    calendarLayout->addWidget(calendar);
    
    // Connect calendar to date edit
    connect(calendar, &QCalendarWidget::selectionChanged, this, &OneSevenLiveCustomEventDialog::onDateChanged);
    connect(dateEdit, &QDateEdit::dateChanged, this, [this](const QDate& date) {
        calendar->setSelectedDate(date);
    });
    
    mainLayout->addLayout(dateFormLayout);
    mainLayout->addWidget(calendarFrame);
}

void OneSevenLiveCustomEventDialog::setupEventGiftsSection() {
    giftsLabel = new QLabel();
    giftsLabel->setText(
        QString("<span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.Gifts")));
    giftsLabel->setStyleSheet("font-weight: 600;");
    
    // Initialize allowed gift categories
    allowedGiftCategories << "luckyBag" << "TreasureChest" << "Event" << "army" 
                         << "Birthday" << "Texture" << "LevelUp" << "Extravagant" 
                         << "Audio" << "Heavenly" << "default";
    
    // Create tab widget for gift categories
    giftTabWidget = new QTabWidget(this);
    giftTabWidget->setFixedHeight(180);
    giftTabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "    border: 1px solid #555555;"
        "    background-color: #2a2a2a;"
        "}"
        "QTabBar::tab {"
        "    background-color: #404040;"
        "    color: #ffffff;"
        "    padding: 8px 12px;"
        "    margin-right: 2px;"
        "    border: 1px solid #555555;"
        "    border-bottom: none;"
        "}"
        "QTabBar::tab:selected {"
        "    background-color: #007acc;"
        "    border-color: #0099ff;"
        "}"
        "QTabBar::tab:hover {"
        "    background-color: #505050;"
        "}");
    
    connect(giftTabWidget, &QTabWidget::currentChanged, this, &OneSevenLiveCustomEventDialog::onGiftTabChanged);
    
    // Load gift tabs data
    loadGiftTabs();
    
    // Create form layout for gifts section
    QFormLayout* giftsFormLayout = new QFormLayout();
    giftsFormLayout->addRow(giftsLabel, giftTabWidget);
    
    mainLayout->addLayout(giftsFormLayout);
}

void OneSevenLiveCustomEventDialog::setupEventTargetsSection() {
    // Daily target
    dailyTargetLabel = new QLabel();
    dailyTargetLabel->setText(
        QString("<span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.DailyGoal")));
    dailyTargetLabel->setStyleSheet("font-weight: 600;");
    
    dailyTargetSpinBox = new QSpinBox(this);
    dailyTargetSpinBox->setRange(1, 999999999);
    dailyTargetSpinBox->setValue(7777777);
    dailyTargetSpinBox->setStyleSheet(
        "QSpinBox {"
        "    background-color: #3a3a3a;"
        "    border: 1px solid #555555;"
        "    color: #ffffff;"
        "}");
    
    // Create form layout for daily target
    QFormLayout* dailyTargetFormLayout = new QFormLayout();
    dailyTargetFormLayout->addRow(dailyTargetLabel, dailyTargetSpinBox);
    
    // Total target
    totalTargetLabel = new QLabel();
    totalTargetLabel->setText(
        QString("<span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.OverallGoal")));
    totalTargetLabel->setStyleSheet("font-weight: 600;");
    
    totalTargetSpinBox = new QSpinBox(this);
    totalTargetSpinBox->setRange(1, 999999999);
    totalTargetSpinBox->setValue(888888888);
    totalTargetSpinBox->setStyleSheet(
        "QSpinBox {"
        "    background-color: #3a3a3a;"
        "    border: 1px solid #555555;"
        "    color: #ffffff;"
        "}");
    
    // Create form layout for total target
    QFormLayout* totalTargetFormLayout = new QFormLayout();
    totalTargetFormLayout->addRow(totalTargetLabel, totalTargetSpinBox);
    
    mainLayout->addLayout(dailyTargetFormLayout);
    mainLayout->addLayout(totalTargetFormLayout);
}

void OneSevenLiveCustomEventDialog::setupEventDescriptionSection() {
    descriptionLabel = new QLabel();
    descriptionLabel->setText(
        QString("<span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.Description")));
    descriptionLabel->setStyleSheet("font-weight: 600;");
    
    // Create container for description edit and character count
    QWidget* descriptionContainer = new QWidget(this);
    QVBoxLayout* descriptionContainerLayout = new QVBoxLayout(descriptionContainer);
    descriptionContainerLayout->setContentsMargins(0, 0, 0, 0);
    descriptionContainerLayout->setSpacing(5);
    
    descriptionEdit = new QTextEdit(this);
    descriptionEdit->setFixedHeight(80);
    descriptionEdit->setPlaceholderText("例：前三名可以获得主播的特殊奖励！");
    descriptionEdit->setStyleSheet(
        "QTextEdit {"
        "    background-color: #3a3a3a;"
        "    border: 1px solid #555555;"
        "    color: #ffffff;"
        "}");
    
    characterCountLabel = new QLabel("0 / 200", this);
    characterCountLabel->setAlignment(Qt::AlignRight);
    characterCountLabel->setStyleSheet("color: #888888; font-size: 12px;");
    
    descriptionContainerLayout->addWidget(descriptionEdit);
    descriptionContainerLayout->addWidget(characterCountLabel);
    
    // Connect text change to update character count
    connect(descriptionEdit, &QTextEdit::textChanged, this, [this]() {
        int length = descriptionEdit->toPlainText().length();
        characterCountLabel->setText(QString("%1 / %2").arg(length).arg(MAX_DESCRIPTION_LENGTH));
        
        if (length > MAX_DESCRIPTION_LENGTH) {
            characterCountLabel->setStyleSheet("color: #ff4757; font-size: 12px;");
        } else {
            characterCountLabel->setStyleSheet("color: #888888; font-size: 12px;");
        }
    });
    
    // Create form layout for description section
    QFormLayout* descriptionFormLayout = new QFormLayout();
    descriptionFormLayout->addRow(descriptionLabel, descriptionContainer);
    
    mainLayout->addLayout(descriptionFormLayout);
}

void OneSevenLiveCustomEventDialog::setupBottomButtons(QVBoxLayout* parentLayout) {
    // Create bottom buttons container
    QWidget* buttonContainer = new QWidget(this);
    buttonContainer->setStyleSheet("QWidget { background-color: #1a1a1a; border-top: 1px solid #404040; }");
    
    buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setSpacing(10);
    buttonLayout->setContentsMargins(20, 15, 20, 15);
    
    // Add spacer to push buttons to right
    buttonLayout->addStretch();
    
    cancelButton = new QPushButton(obs_module_text("CustomEvent.Cancel"), this);
    cancelButton->setFixedHeight(40);
    cancelButton->setFixedWidth(80);
    
    createButton = new QPushButton(obs_module_text("CustomEvent.Create"), this);
    createButton->setObjectName("createButton");
    createButton->setFixedHeight(40);
    createButton->setFixedWidth(80);
    
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(createButton);
    
    // Add button container to parent layout
    parentLayout->addWidget(buttonContainer);
    
    // Connect button signals
    connect(cancelButton, &QPushButton::clicked, this, &OneSevenLiveCustomEventDialog::handleCancel);
    connect(createButton, &QPushButton::clicked, this, &OneSevenLiveCustomEventDialog::handleCreateEvent);
}

void OneSevenLiveCustomEventDialog::onDateChanged() {
    QDate selectedDate = calendar->selectedDate();
    dateEdit->setDate(selectedDate);
}

void OneSevenLiveCustomEventDialog::onGiftSelected(int giftIndex) {
    // 检查礼物是否已经被选中
    int indexInSelected = selectedGiftIndices.indexOf(giftIndex);
    
    if (indexInSelected != -1) {
        // 如果已经选中，则取消选中
        selectedGiftIndices.removeAt(indexInSelected);
        giftButtons[giftIndex]->setChecked(false);
    } else {
        // 如果未选中，检查是否已达到最大选择数量
        if (selectedGiftIndices.size() >= MAX_SELECTED_GIFTS) {
            // 已达到最大选择数量，显示提示并取消当前操作
            QMessageBox::warning(this, "错误", QString("最多只能选择%1个礼物").arg(MAX_SELECTED_GIFTS));
            giftButtons[giftIndex]->setChecked(false);
            return;
        }
        
        // 添加到已选中列表
        selectedGiftIndices.append(giftIndex);
        giftButtons[giftIndex]->setChecked(true);
    }
}

void OneSevenLiveCustomEventDialog::handleCreateEvent() {
    // 验证必填字段
    if (eventTitleEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入活动标题");
        return;
    }
    
    // 验证活动结束日期（已在UI中限制为最多30天）
    
    // 验证礼物选择
    if (selectedGiftIndices.isEmpty()) {
        QMessageBox::warning(this, "错误", "请至少选择一个活动礼物");
        return;
    }
    
    if (selectedGiftIndices.size() > MAX_SELECTED_GIFTS) {
        QMessageBox::warning(this, "错误", QString("最多只能选择%1个礼物").arg(MAX_SELECTED_GIFTS));
        return;
    }
    
    // 验证活动描述
    if (descriptionEdit->toPlainText().trimmed().isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入活动描述");
        return;
    }
    
    if (descriptionEdit->toPlainText().length() > MAX_DESCRIPTION_LENGTH) {
        QMessageBox::warning(this, "错误", "活动描述不能超过200个字符");
        return;
    }
    
    // 创建事件数据
    OneSevenLiveCustomEvent eventData;
    eventData.title = eventTitleEdit->text().trimmed();
    eventData.endTime = QDateTime(dateEdit->date(), QTime(23, 59, 59)).toSecsSinceEpoch();
    eventData.status = 1; // 活动状态 - 激活
    eventData.description = descriptionEdit->toPlainText().trimmed();
    eventData.dailyTarget = dailyTargetSpinBox->value();
    eventData.totalTarget = totalTargetSpinBox->value();
    
    // 添加选中的礼物ID
    for (int index : selectedGiftIndices) {
        if (index >= 0 && index < giftButtons.size()) {
            // 获取礼物ID
            QString giftID = giftButtons[index]->property("giftID").toString();
            if (!giftID.isEmpty()) {
                eventData.giftIDs.append(giftID);
            }
        }
    }
    
    // 发送事件创建信号
    emit eventCreated(eventData);
    
    // 显示成功消息
    QMessageBox::information(this, "成功", "自定义活动创建成功！");
    
    // 关闭对话框
    accept();
}

void OneSevenLiveCustomEventDialog::handleCancel() {
    reject();
}

void OneSevenLiveCustomEventDialog::loadGiftTabs() {
    if (!apiWrapper) {
        obs_log(LOG_WARNING, "API wrapper not available, using placeholder gift tabs");
        setupGiftTabsUI();
        return;
    }
    
    // TODO: Get live stream ID from current session
    std::string liveStreamID;
    std::string region;

    if (!configManager->getConfigValue("LiveStreamID", liveStreamID)) {
        obs_log(LOG_ERROR, "Failed to get live stream ID from config manager");
        setupGiftTabsUI();
        return;
    }

    if (!configManager->getConfigValue("Region", region)) {
        obs_log(LOG_ERROR, "Failed to get region from config manager");
        setupGiftTabsUI();
        return;
    }
    
    Json giftTabsJson;
    if (apiWrapper->GetGiftTabs(liveStreamID, region, giftTabsJson)) {
        Json giftsJson;
        OneSevenLiveGiftsResponse giftsResponse;
        JsonToOneSevenLiveGiftsResponse(giftTabsJson, giftsResponse);
        QList<OneSevenLiveGift> gifts = giftsResponse.gifts;

        if (JsonToOneSevenLiveGiftTabsResponse(giftTabsJson, giftTabsData)) {
            // Filter tabs based on allowed categories
            filteredGiftTabs.clear();
            for (const auto& tab : giftTabsData.tabs) {
                if (allowedGiftCategories.contains(tab.id)) {
                    // Filter gifts based on rules
                    QList<OneSevenLiveGift> filteredGifts;
                    for (const auto& tabGift : tab.gifts) {
                        // find giftData from gifts by gift.id
                        OneSevenLiveGift gift;
                        for (const auto& giftItem : gifts) {
                            if (giftItem.id == tabGift.id) {
                                gift = giftItem;
                                break;
                            }
                        }

                        // Rule 1: Skip if isHidden = 1
                        if (gift.isHidden == 1) {
                            continue;
                        }
                        
                        // Rule 2: Filter based on regionMode
                        bool shouldShow = false;
                        switch (gift.regionMode) {
                            case 0:
                                // regionMode = 0: Don't show
                                shouldShow = false;
                                break;
                            case 1:
                                // regionMode = 1: Always show
                                shouldShow = true;
                                break;
                            case 2:
                                // regionMode = 2: Show if streamer region is in gift regions
                                shouldShow = gift.regions.contains(region);
                                break;
                            case 3:
                                // regionMode = 3: Don't show if streamer region is in gift regions
                                shouldShow = !gift.regions.contains(region);
                                break;
                            default:
                                // Default behavior for unknown regionMode
                                shouldShow = false;
                                break;
                        }
                        
                        if (shouldShow) {
                            filteredGifts.append(gift);
                        }
                    }
                    if (filteredGifts.size() > 0) {
                        tab.gifts = filteredGifts;
                        filteredGiftTabs.append(tab);
                    }
                }
            }
            setupGiftTabsUI();
        } else {
            obs_log(LOG_ERROR, "Failed to parse gift tabs response");
            setupGiftTabsUI(); // Setup with placeholder data
        }
    } else {
        obs_log(LOG_ERROR, "Failed to load gift tabs from API");
        setupGiftTabsUI(); // Setup with placeholder data
    }
}

void OneSevenLiveCustomEventDialog::setupGiftTabsUI() {
    // Clear existing tabs
    giftTabWidget->clear();
    
    if (filteredGiftTabs.isEmpty()) {
        // Create placeholder tab if no data available
        QWidget* placeholderTab = new QWidget();
        QVBoxLayout* placeholderLayout = new QVBoxLayout(placeholderTab);
        
        QLabel* placeholderLabel = new QLabel("Loading gifts...");
        placeholderLabel->setAlignment(Qt::AlignCenter);
        placeholderLabel->setStyleSheet("color: #ffffff; font-size: 14px;");
        placeholderLayout->addWidget(placeholderLabel);
        
        giftTabWidget->addTab(placeholderTab, "Gifts");
        return;
    }
    
    // Create tabs for each filtered gift category
    for (int i = 0; i < filteredGiftTabs.size(); ++i) {
        const auto& giftTab = filteredGiftTabs[i];
        
        QWidget* tabWidget = new QWidget();
        QVBoxLayout* tabLayout = new QVBoxLayout(tabWidget);
        tabLayout->setContentsMargins(8, 8, 8, 8);
        
        // Create scroll area for gifts in this tab
        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setWidgetResizable(true);
        scrollArea->setStyleSheet(
            "QScrollArea {"
            "    background-color: #2a2a2a;"
            "    border: none;"
            "}");
        
        QWidget* giftsContainer = new QWidget();
        QGridLayout* giftsLayout = new QGridLayout(giftsContainer);
        giftsLayout->setSpacing(8);
        giftsLayout->setContentsMargins(8, 8, 8, 8);
        
        populateGiftTab(giftTab, i);
        
        scrollArea->setWidget(giftsContainer);
        tabLayout->addWidget(scrollArea);
        
        giftTabWidget->addTab(tabWidget, giftTab.name);
        
        // Store the gifts layout for later use
        tabWidget->setProperty("giftsLayout", QVariant::fromValue(static_cast<void*>(giftsLayout)));
        tabWidget->setProperty("giftsContainer", QVariant::fromValue(static_cast<void*>(giftsContainer)));
        tabWidget->setProperty("tabIndex", i);
    }
}

void OneSevenLiveCustomEventDialog::populateGiftTab(const OneSevenLiveGiftTab& giftTab, int tabIndex) {
    QWidget* tabWidget = giftTabWidget->widget(tabIndex);
    if (!tabWidget) return;
    
    QGridLayout* giftsLayout = static_cast<QGridLayout*>(tabWidget->property("giftsLayout").value<void*>());
    QWidget* giftsContainer = static_cast<QWidget*>(tabWidget->property("giftsContainer").value<void*>());
    
    if (!giftsLayout || !giftsContainer) return;
    
    // Clear existing gift buttons for this tab
    QLayoutItem* item;
    while ((item = giftsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    
    // Create gift buttons for this tab
    int maxGiftsPerTab = 12; // Show up to 12 gifts per tab (3 rows x 4 columns)
    int giftsToShow = qMin(giftTab.gifts.size(), maxGiftsPerTab);
    
    for (int i = 0; i < giftsToShow; ++i) {
        const auto& gift = giftTab.gifts[i];
        
        // 创建一个容器widget来包含图片和文本
        QWidget* giftWidget = new QWidget();
        giftWidget->setFixedSize(60, 60);
        giftWidget->setStyleSheet(
            "QWidget {"
            "    background-color: #404040;"
            "    border: 2px solid #555555;"
            "    border-radius: 8px;"
            "}"
            "QWidget:hover {"
            "    background-color: #505050;"
            "    border-color: #666666;"
            "}");
        
        // 创建垂直布局
        QVBoxLayout* layout = new QVBoxLayout(giftWidget);
        layout->setContentsMargins(2, 2, 2, 2);
        layout->setSpacing(1);
        
        // 创建图片标签
        QLabel* imageLabel = new QLabel();
        imageLabel->setFixedSize(40, 40);
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setScaledContents(true);
        
        // 设置占位符，实际项目中应该使用QNetworkAccessManager异步加载网络图片
        imageLabel->setText(gift.name.left(1));
        imageLabel->setStyleSheet("background-color: #555555; color: white; font-size: 16px; font-weight: bold; border-radius: 5px;");
        
        // 存储图片URL作为属性，以便将来可以实现异步加载
        if (!gift.leaderboardIcon.isEmpty()) {
            QString iconUrl = "https://cdn.17app.co/" + gift.leaderboardIcon;
            imageLabel->setProperty("iconUrl", iconUrl);
        }
        
        // 创建名称标签
        QLabel* nameLabel = new QLabel(gift.name);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("color: white; font-size: 8px;");
        
        // 创建价格标签
        QLabel* priceLabel = new QLabel(QString::number(gift.point));
        priceLabel->setAlignment(Qt::AlignCenter);
        priceLabel->setStyleSheet("color: white; font-size: 8px;");
        
        // 添加到布局
        layout->addWidget(imageLabel);
        layout->addWidget(nameLabel);
        layout->addWidget(priceLabel);
        
        // 设置布局的拉伸因子
        layout->setStretch(0, 5); // 图片
        layout->setStretch(1, 2); // 名称
        layout->setStretch(2, 1); // 价格
        
        // 创建一个透明的按钮覆盖整个widget，用于处理点击事件
        QPushButton* giftButton = new QPushButton(giftWidget);
        giftButton->setFixedSize(60, 60);
        giftButton->setFlat(true);
        giftButton->setStyleSheet(
            "QPushButton {"
            "    background-color: transparent;"
            "    border: none;"
            "}"
            "QPushButton:checked {"
            "    background-color: rgba(0, 122, 204, 150);"
            "    border-radius: 8px;"
            "}");
        
        giftButton->setCheckable(true);
        giftButton->setToolTip(gift.description);
        
        int row = i / GIFT_GRID_COLUMNS;
        int col = i % GIFT_GRID_COLUMNS;
        giftsLayout->addWidget(giftWidget, row, col);
        
        // Store gift data in button
        giftButton->setProperty("giftID", gift.giftID);
        giftButton->setProperty("giftIndex", i);
        giftButton->setProperty("tabIndex", tabIndex);
        giftWidget->setProperty("giftID", gift.giftID);
        giftWidget->setProperty("giftIndex", i);
        giftWidget->setProperty("tabIndex", tabIndex);
        
        connect(giftButton, &QPushButton::clicked, this, [this, i, tabIndex]() {
            onGiftSelected(i);
        });
    }
    
    // Update the gifts container
    QScrollArea* scrollArea = tabWidget->findChild<QScrollArea*>();
    if (scrollArea) {
        scrollArea->setWidget(giftsContainer);
    }
}

void OneSevenLiveCustomEventDialog::onGiftTabChanged(int tabIndex) {
    // Handle tab change if needed
    obs_log(LOG_INFO, "Gift tab changed to index: %d", tabIndex);
    
    // Clear previous selection when switching tabs
    selectedGiftIndex = -1;
    
    // Uncheck all gift buttons in all tabs
    for (int i = 0; i < giftTabWidget->count(); ++i) {
        QWidget* tabWidget = giftTabWidget->widget(i);
        if (tabWidget) {
            QList<QPushButton*> buttons = tabWidget->findChildren<QPushButton*>();
            for (QPushButton* button : buttons) {
                button->setChecked(false);
            }
        }
    }
}

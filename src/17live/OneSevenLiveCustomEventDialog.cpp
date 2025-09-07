// Include project header first to ensure proper dependency resolution
#include "OneSevenLiveCustomEventDialog.hpp"

// C++ Standard Library includes
#include <type_traits>
#include <mutex>
#include <string>
#include <vector>

// System includes
#include <sys/types.h>
#include <stdio.h>

// OBS includes
#include <obs-module.h>
#include <plugin-support.h>

// Qt includes
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

// Project includes
#include "api/OneSevenLiveApiWrappers.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "utility/RemoteTextThread.hpp"

#include "moc_OneSevenLiveCustomEventDialog.cpp"

OneSevenLiveCustomEventDialog::OneSevenLiveCustomEventDialog(QWidget* parent,
                                                           OneSevenLiveApiWrappers* apiWrapper_,
                                                           OneSevenLiveConfigManager* configManager_,
                                                           OneSevenLiveCustomEvent* customEvent)
    : QDialog(parent), apiWrapper(apiWrapper_), configManager(configManager_), customEventData(customEvent) {
    setupUi();
    setWindowTitle(obs_module_text("CustomEvent.Dialog.Title"));
    setFixedWidth(450);
    // setFixedSize(450, 700);
    // setModal(false);  // Set to non-modal
    
    // Ensure the dialog is properly initialized and visible
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    
    // If customEvent is provided, populate the dialog with its data
    if (customEvent) {
        // Populate dialog fields with customEvent data
        eventTitleEdit->setText(customEvent->eventName);
        
        // Set date from endTime timestamp
        if (customEvent->endTime > 0) {
            QDateTime endDateTime = QDateTime::fromSecsSinceEpoch(customEvent->endTime);
            dateEdit->setDate(endDateTime.date());
        }
        
        // Set description
        descriptionEdit->setText(customEvent->description);
        
        // Set targets
        dailyTargetEdit->setText(QString::number(customEvent->dailyGoalPoints));
        totalTargetEdit->setText(QString::number(customEvent->goalPoints));
        
        // TODO: Handle gift selection based on customEvent->giftIDs
        // This will be implemented after loading gift tabs
    }
    
    // Ensure all widgets are properly initialized
    update();
}

OneSevenLiveCustomEventDialog::~OneSevenLiveCustomEventDialog() = default;

void OneSevenLiveCustomEventDialog::setupUi() {
    // Set dialog background to black with modern styling
    // setStyleSheet(
    //     "QDialog {"
    //     "    background-color: #1a1a1a;"
    //     "    color: #ffffff;"
    //     "    font-family: 'Inter', 'Microsoft YaHei', sans-serif;"
    //     "    font-style: normal;"
    //     "}"
        // "QLabel {"
        // "    color: #ffffff;"
        // "    font-size: 14px;"
        // "    font-weight: 400;"
        // "    margin: 5px 0;"
        // "}"
        // "QLineEdit, QTextEdit, QSpinBox, QDateEdit {"
        // "    background-color: #2d2d2d;"
        // "    border: 1px solid #404040;"
        // "    border-radius: 2px;"
        // "    padding: 8px 12px;"
        // "    color: #ffffff;"
        // "    font-size: 14px;"
        // "    min-height: 20px;"
        // "}"
        // "QLineEdit:focus, QTextEdit:focus, QSpinBox:focus, QDateEdit:focus {"
        // "    border-color: #007acc;"
        // "    outline: none;"
        // "}"
        // "QPushButton {"
        // "    background-color: #2d2d2d;"
        // "    border: 1px solid #404040;"
        // "    border-radius: 2px;"
        // "    padding: 8px 16px;"
        // "    color: #ffffff;"
        // "    font-size: 14px;"
        // "    font-weight: 500;"
        // "    min-height: 20px;"
        // "}"
        // "QPushButton:hover {"
        // "    background-color: #3d3d3d;"
        // "    border-color: #505050;"
        // "}"
        // "QPushButton:pressed {"
        // "    background-color: #1d1d1d;"
        // "}"
        // "QPushButton#createButton {"
        // "    background-color: #ff4757;"
        // "    border-color: #ff4757;"
        // "    color: #ffffff;"
        // "    font-weight: 600;"
        // "}"
        // "QPushButton#createButton:hover {"
        // "    background-color: #ff3742;"
        // "}"
        // "QPushButton#createButton:pressed {"
        // "    background-color: #e63946;"
        // "}"
        // "QCalendarWidget {"
        // "    background-color: #2d2d2d;"
        // "    border: 1px solid #404040;"
        // "    border-radius: 6px;"
        // "    color: #ffffff;"
        // "}"
        // "QCalendarWidget QToolButton {"
        // "    background-color: transparent;"
        // "    border: none;"
        // "    color: #ffffff;"
        // "    font-size: 12px;"
        // "    padding: 4px;"
        // "}"
        // "QCalendarWidget QToolButton:hover {"
        // "    background-color: #3d3d3d;"
        // "    border-radius: 4px;"
        // "}"
        // "QCalendarWidget QAbstractItemView {"
        // "    background-color: #2d2d2d;"
        // "    selection-background-color: #007acc;"
        // "    color: #ffffff;"
        // "}"
        // "QScrollArea {"
        // "    border: none;"
        // "    background-color: #1a1a1a;"
        // "}"
        // "QScrollBar:vertical {"
        // "    background-color: #2d2d2d;"
        // "    width: 12px;"
        // "    border-radius: 6px;"
        // "}"
        // "QScrollBar::handle:vertical {"
        // "    background-color: #555555;"
        // "    border-radius: 6px;"
        // "    min-height: 20px;"
        // "}"
        // "QScrollBar::handle:vertical:hover {"
        // "    background-color: #666666;"
        // "}"
        // "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        // "    border: none;"
        // "    background: none;"
        // "}");

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
    QWidget* contentWidget = new QWidget(this);
    contentWidget->setStyleSheet(
        "QWidget {"
        "    color: white;"
        "    font-family: 'Inter';"
        "    font-style: normal;"
        "}");

    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setSpacing(5);
    contentLayout->setContentsMargins(10, 10, 10, 10);
    
    // Store the content layout for use in setup functions
    mainLayout = contentLayout;
    
    setupEventTitleSection();
    setupEventDateSection();
    setupEventGiftsSection();
    setupEventTargetsSection();
    setupEventDescriptionSection();
    
    // Add stretch to push content to top
    // contentLayout->addStretch();

    setupBottomButtons(contentLayout);
    
    // Set content widget to scroll area
    scrollArea->setWidget(contentWidget);
    
    // Add scroll area to dialog layout
    dialogLayout->addWidget(scrollArea);
    
    // Setup bottom buttons directly with dialog layout
    // setupBottomButtons(dialogLayout);
}

void OneSevenLiveCustomEventDialog::setupEventTitleSection() {
    titleLabel = new QLabel();
    titleLabel->setText(
        QString("<span style='color:red;'>*</span><span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.Title")));
    
    eventTitleEdit = new QLineEdit(this);
    eventTitleEdit->setObjectName("eventTitleEdit");
    eventTitleEdit->setPlaceholderText(obs_module_text("CustomEvent.Title.Placeholder"));
    eventTitleEdit->setMaxLength(MAX_TITLE_LENGTH);
    // eventTitleEdit->setStyleSheet(
    //     "QLineEdit {"
    //     "    background-color: #3a3a3a;"
    //     "    border: 1px solid #555555;"
    //     "    color: #ffffff;"
    //     "    placeholder-text-color: #888888;"
    //     "}");
    eventTitleEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    // Create form layout for title section
    QFormLayout* titleFormLayout = new QFormLayout();
    // Set to vertical layout, labels above fields
    titleFormLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    titleFormLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy
    titleFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    titleFormLayout->addRow(titleLabel, eventTitleEdit);
    
    mainLayout->addLayout(titleFormLayout);
}

void OneSevenLiveCustomEventDialog::setupEventDateSection() {
    dateLabel = new QLabel();
    dateLabel->setText(
        QString("<span style='color:red;'>*</span><span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.EndDate")));
    dateLabel->setStyleSheet("font-weight: 600;");
    
    // Date input with format
    dateEdit = new QDateEdit(this);
    dateEdit->setDate(QDate::currentDate().addDays(1));
    dateEdit->setMinimumDate(QDate::currentDate().addDays(1));
    dateEdit->setMaximumDate(QDate::currentDate().addDays(30));
    dateEdit->setDisplayFormat("yyyy/MM/dd");
    dateEdit->setCalendarPopup(true);
    
    // Create form layout for date section
    QFormLayout* dateFormLayout = new QFormLayout();
    dateFormLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    dateFormLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy
    dateFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    dateFormLayout->addRow(dateLabel, dateEdit);
    
    // Calendar widget
    // calendarFrame = new QFrame(this);
    // calendarFrame->setFixedHeight(200);
    
    // QVBoxLayout* calendarLayout = new QVBoxLayout(calendarFrame);
    // calendarLayout->setContentsMargins(5, 5, 5, 5);
    
    // calendar = new QCalendarWidget(this);
    // calendar->setMinimumDate(QDate::currentDate().addDays(1));
    // calendar->setMaximumDate(QDate::currentDate().addDays(30));
    // calendar->setSelectedDate(QDate::currentDate().addDays(1));
    // calendar->setGridVisible(true);
    
    // calendarLayout->addWidget(calendar);
    
    // // Connect calendar to date edit
    // connect(calendar, &QCalendarWidget::selectionChanged, this, &OneSevenLiveCustomEventDialog::onDateChanged);
    // connect(dateEdit, &QDateEdit::dateChanged, this, [this](const QDate& date) {
    //     calendar->setSelectedDate(date);
    // });
    
    mainLayout->addLayout(dateFormLayout);
    // mainLayout->addWidget(calendarFrame);
}

void OneSevenLiveCustomEventDialog::setupEventGiftsSection() {
    giftsLabel = new QLabel();
    giftsLabel->setText(
        QString("<span style='color:red;'>*</span><span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.Gifts")));
    giftsLabel->setStyleSheet("font-weight: 600;");
    selectedGiftsEdit = new QLineEdit(this);
    selectedGiftsEdit->setObjectName("selectedGiftsEdit");
    selectedGiftsEdit->setPlaceholderText(obs_module_text("CustomEvent.SelectedGifts.Placeholder"));
    
    // Initialize allowed gift categories
    allowedGiftCategories << "luckyBag" << "TreasureChest" << "Event" << "army" 
                         << "Birthday" << "Texture" << "LevelUp" << "Extravagant" 
                         << "Audio" << "Heavenly" << "default";
    
    // Create tab widget for gift categories
    giftTabWidget = new QTabWidget(this);
    // giftTabWidget->setFixedHeight(180);
    giftTabWidget->setFixedSize(430, 250);
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
    
    // connect(giftTabWidget, &QTabWidget::currentChanged, this, &OneSevenLiveCustomEventDialog::onGiftTabChanged);
    
    // Load gift tabs data
    loadGiftTabs();
    
    // Create form layout for gifts section
    QFormLayout* giftsFormLayout = new QFormLayout();
    giftsFormLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    giftsFormLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy
    giftsFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    
    giftsFormLayout->addRow(giftsLabel, selectedGiftsEdit);
    
    mainLayout->addLayout(giftsFormLayout);
    mainLayout->addWidget(giftTabWidget);
}

void OneSevenLiveCustomEventDialog::setupEventTargetsSection() {
    // Daily target
    dailyTargetLabel = new QLabel();
    dailyTargetLabel->setText(
        QString("<span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.DailyGoal")));
    dailyTargetLabel->setStyleSheet("font-weight: 600;");
    
    dailyTargetEdit = new QLineEdit(this);
    // dailyTargetEdit->setValidator(new QIntValidator(1, 999999999, this));
    dailyTargetEdit->setPlaceholderText(obs_module_text("CustomEvent.DailyGoal.Placeholder"));
    // dailyTargetEdit->setStyleSheet(
    //     "QLineEdit {"
    //     "    background-color: #3a3a3a;"
    //     "    border: 1px solid #555555;"
    //     "    color: #ffffff;"
    //     "}");
    
    // Create form layout for daily target
    QFormLayout* dailyTargetFormLayout = new QFormLayout();
    dailyTargetFormLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    dailyTargetFormLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy
    dailyTargetFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    dailyTargetFormLayout->addRow(dailyTargetLabel, dailyTargetEdit);
    
    // Total target
    totalTargetLabel = new QLabel();
    totalTargetLabel->setText(
        QString("<span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.TotalGoal")));
    totalTargetLabel->setStyleSheet("font-weight: 600;");
    
    totalTargetEdit = new QLineEdit(this);
    // totalTargetEdit->setValidator(new QIntValidator(1, 999999999, this));
    totalTargetEdit->setPlaceholderText(obs_module_text("CustomEvent.TotalGoal.Placeholder"));
    // totalTargetEdit->setStyleSheet(
    //     "QLineEdit {"
    //     "    background-color: #3a3a3a;"
    //     "    border: 1px solid #555555;"
    //     "    color: #ffffff;"
    //     "}");
    
    // Create form layout for total target
    QFormLayout* totalTargetFormLayout = new QFormLayout();
    totalTargetFormLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    totalTargetFormLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy
    totalTargetFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    totalTargetFormLayout->addRow(totalTargetLabel, totalTargetEdit);
    
    mainLayout->addLayout(dailyTargetFormLayout);
    mainLayout->addLayout(totalTargetFormLayout);
}

void OneSevenLiveCustomEventDialog::setupEventDescriptionSection() {
    descriptionLabel = new QLabel();
    descriptionLabel->setText(
        QString("<span style='color:red;'>*</span><span style='color:white;'>%1</span>")
            .arg(obs_module_text("CustomEvent.Description")));
    descriptionLabel->setStyleSheet("font-weight: 600;");
    
    descriptionEdit = new QTextEdit(this);
    descriptionEdit->setPlaceholderText(obs_module_text("CustomEvent.Description.Placeholder"));
    // descriptionEdit->setStyleSheet(
    //     "QTextEdit {"
    //     "    background-color: #3a3a3a;"
    //     "    border: 1px solid #555555;"
    //     "    color: #ffffff;"
    //     "}");
     
    // Connect text change to update character count
    // connect(descriptionEdit, &QTextEdit::textChanged, this, [this]() {
    //     int length = descriptionEdit->toPlainText().length();
    //     characterCountLabel->setText(QString("%1 / %2").arg(length).arg(MAX_DESCRIPTION_LENGTH));
        
    //     if (length > MAX_DESCRIPTION_LENGTH) {
    //         characterCountLabel->setStyleSheet("color: #ff4757; font-size: 12px;");
    //     } else {
    //         characterCountLabel->setStyleSheet("color: #888888; font-size: 12px;");
    //     }
    // });
    
    // Create form layout for description section
    QFormLayout* descriptionFormLayout = new QFormLayout();
    descriptionFormLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    descriptionFormLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy
    descriptionFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    descriptionFormLayout->addRow(descriptionLabel, descriptionEdit);
    
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
    
    // Determine which button to display based on customEventData status
    if (!customEventData || customEventData->eventID.isEmpty()) {
        // When customEvent doesn't exist or eventID is empty, display Create button
        createButton = new QPushButton(obs_module_text("CustomEvent.Create"), this);
        createButton->setObjectName("createButton");
        // set button color #FF0001
        createButton->setStyleSheet("QPushButton { background-color: #FF0001; color: white; }");
        createButton->setFixedHeight(40);
        createButton->setMinimumWidth(80);
        buttonLayout->addWidget(createButton);
        
        // Connect Create button signal
        connect(createButton, &QPushButton::clicked, this, &OneSevenLiveCustomEventDialog::handleCreateEvent);
    } else if (customEventData->status == 1) {
        // When customEvent exists and status=1, display Stop button
        createButton = new QPushButton(obs_module_text("CustomEvent.Stop"), this);
        // button color #007AFF
        createButton->setStyleSheet("QPushButton {background-color: #007AFF; color: white;}");
        createButton->setObjectName("stopButton");
        createButton->setFixedHeight(40);
        createButton->setMinimumWidth(80);
        buttonLayout->addWidget(createButton);
        
        
        // Connect Stop button signal
        connect(createButton, &QPushButton::clicked, this, &OneSevenLiveCustomEventDialog::handleStopEvent);
    } else if (customEventData->status == 2) {
        // When customEvent exists and status=2, display Close button
        createButton = new QPushButton(obs_module_text("CustomEvent.Close"), this);
        // button color #007AFF
        createButton->setStyleSheet("QPushButton {background-color: #007AFF; color: white;}");
        createButton->setObjectName("closeButton");
        createButton->setFixedHeight(40);
        createButton->setMinimumWidth(80);
        buttonLayout->addWidget(createButton);
        
        
        // Connect Close button signal
        connect(createButton, &QPushButton::clicked, this, &OneSevenLiveCustomEventDialog::handleCloseEvent);
    }
    
    // Add button container to parent layout
    parentLayout->addWidget(buttonContainer);
}

void OneSevenLiveCustomEventDialog::onDateChanged() {
    QDate selectedDate = calendar->selectedDate();
    dateEdit->setDate(selectedDate);
}

void OneSevenLiveCustomEventDialog::onGiftSelected(QPushButton* giftButton, OneSevenLiveGift gift) {
    // Check if the gift is already selected
    QList<QString> selectedGiftIndices;
    for (auto selectedGift : selectedGifts) {
        selectedGiftIndices.append(selectedGift.giftID);
    }

    int indexInSelected = selectedGiftIndices.indexOf(gift.giftID);
    
    if (indexInSelected != -1) {
        // If already selected, deselect it
        selectedGifts.removeAt(indexInSelected);
        giftButton->setChecked(false);
    } else {
        // If not selected, check if maximum selection count is reached
        if (selectedGifts.size() >= MAX_SELECTED_GIFTS) {
            // Maximum selection count reached, show warning and cancel current operation
            QMessageBox::warning(this, obs_module_text("CustomEvent.Error"), 
                                QString(obs_module_text("CustomEvent.Error.MaxGifts")).arg(MAX_SELECTED_GIFTS));
            giftButton->setChecked(false);
            return;
        }
        
        // Add to selected list
        selectedGifts.append(gift);
        giftButton->setChecked(true);
    }

    // get selected gifts' name
    QList<QString> selectedGiftsName;
    for (auto selectedGift : selectedGifts) {
        selectedGiftsName.append(selectedGift.name);
    }

    selectedGiftsEdit->setText(selectedGiftsName.join(" / "));
}

void OneSevenLiveCustomEventDialog::handleCreateEvent() {
    // Validate required fields
    if (eventTitleEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, obs_module_text("CustomEvent.Error"), 
                            obs_module_text("CustomEvent.Error.EmptyTitle"));
        return;
    }
    
    // Validate event end date (already limited to max 30 days in UI)
    
    // Validate gift selection
    if (selectedGifts.isEmpty()) {
        QMessageBox::warning(this, obs_module_text("CustomEvent.Error"), 
                            obs_module_text("CustomEvent.Error.NoGifts"));
        return;
    }
    
    if (selectedGifts.size() > MAX_SELECTED_GIFTS) {
        QMessageBox::warning(this, obs_module_text("CustomEvent.Error"),
                            QString(obs_module_text("CustomEvent.Error.MaxGifts")).arg(MAX_SELECTED_GIFTS));
        return;
    }
    
    // Validate event description
    if (descriptionEdit->toPlainText().trimmed().isEmpty()) {
        QMessageBox::warning(this, obs_module_text("CustomEvent.Error"), 
                            obs_module_text("CustomEvent.Error.EmptyDescription"));
        return;
    }
    
    if (descriptionEdit->toPlainText().length() > MAX_DESCRIPTION_LENGTH) {
        QMessageBox::warning(this, obs_module_text("CustomEvent.Error"), 
                            obs_module_text("CustomEvent.Error.DescriptionTooLong"));
        return;
    }
    
    // Create event data
    OneSevenLiveCustomEvent eventRequest;
    eventRequest.eventName = eventTitleEdit->text().trimmed();
    eventRequest.endTime = QDateTime(dateEdit->date(), QTime(23, 59, 59)).toSecsSinceEpoch();
    eventRequest.status = 1; // Event status - Active
    eventRequest.description = descriptionEdit->toPlainText().trimmed();
    eventRequest.dailyGoalPoints = dailyTargetEdit->text().toInt();
    eventRequest.goalPoints = totalTargetEdit->text().toInt();
    
    // Add selected gift IDs
    for (auto gift : selectedGifts) {
        eventRequest.giftIDs.append(gift.giftID);
    }

    OneSevenLiveCustomEvent eventResponse;

    if (!apiWrapper->CreateCustomEvent(eventRequest, eventResponse)) {
        QMessageBox::warning(this, obs_module_text("CustomEvent.Error"),
                            obs_module_text("CustomEvent.Error.CreateFailed"));
        return;
    }

    customEventData = &eventResponse;
    
    // Send event created signal
    emit eventCreated(eventResponse);
    
    // Show success message
    QMessageBox::information(this, obs_module_text("CustomEvent.Success"), 
                           obs_module_text("CustomEvent.Success.Created"));
    
    // Close dialog
    accept();
}

void OneSevenLiveCustomEventDialog::handleStopEvent() {
    // Confirm whether to stop the event
    QMessageBox msgBox(QMessageBox::Question,
                      obs_module_text("CustomEvent.Confirm.Stop.Title"),
                      obs_module_text("CustomEvent.Confirm.StopEvent"),
                      QMessageBox::Yes|QMessageBox::No,
                      this);
    QAbstractButton *yesButton = msgBox.button(QMessageBox::Yes);
    if (yesButton) {
        yesButton->setText(obs_module_text("CustomEvent.Confirm.Stop.Yes"));
    }
    QMessageBox::StandardButton reply = (QMessageBox::StandardButton)msgBox.exec();
    if (reply == QMessageBox::Yes) {
        OneSevenLiveCustomEventStatusRequest request;
        request.status = 2;
        request.userID = customEventData->userID;

        obs_log(LOG_INFO, "Stopping custom event... userID: %s, eventID: %s", 
                customEventData->userID.toStdString().c_str(), 
                customEventData->eventID.toStdString().c_str());

        if (!apiWrapper->ChangeCustomEventStatus(customEventData->eventID.toStdString(), request)) {
            obs_log(LOG_ERROR, "Failed to change custom event status");
            QMessageBox::warning(this, obs_module_text("CustomEvent.Error"), 
                                obs_module_text("CustomEvent.Error.StopFailed"));
            return;
        }
        
        // Send event update signal
        emit eventUpdated(*customEventData);
        
        // Show success message
        QMessageBox::information(this, obs_module_text("CustomEvent.Success"), 
                               obs_module_text("CustomEvent.Success.Stopped"));
        
        // Close dialog
        accept();
    }
}

void OneSevenLiveCustomEventDialog::handleCloseEvent() {
    QMessageBox msgBox(QMessageBox::Question,
                    obs_module_text("CustomEvent.Confirm.Close.Title"),
                    obs_module_text("CustomEvent.Confirm.CloseEvent"),
                      QMessageBox::Yes|QMessageBox::No,
                      this);
    QAbstractButton *yesButton = msgBox.button(QMessageBox::Yes);
    if (yesButton) {
        yesButton->setText(obs_module_text("CustomEvent.Confirm.Close.Yes"));
    }
    QMessageBox::StandardButton reply = (QMessageBox::StandardButton)msgBox.exec();
    if (reply == QMessageBox::Yes) {
       OneSevenLiveCustomEventStatusRequest request;
        request.status = 3;
        request.userID = customEventData->userID;

        if (!apiWrapper->ChangeCustomEventStatus(customEventData->eventID.toStdString(), request)) {
            obs_log(LOG_ERROR, "Failed to change custom event status");
            QMessageBox::warning(this, obs_module_text("CustomEvent.Error"), 
                                obs_module_text("CustomEvent.Error.CloseFailed"));
            return;
        }
        
        // Send event update signal
        emit eventUpdated(*customEventData);
    }

    // Directly close the dialog
    accept();
}

void OneSevenLiveCustomEventDialog::loadGiftTabs() {
    if (!apiWrapper) {
        obs_log(LOG_WARNING, "API wrapper not available, using placeholder gift tabs");
        setupGiftTabsUI();
        return;
    }
    
    // TODO: Get live stream ID from current session
    std::string roomID;
    std::string region;

    if (!configManager->getConfigValue("RoomID", roomID)) {
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
    if (apiWrapper->GetGiftTabs(roomID, region, giftTabsJson)) {
        
        Json giftsJson;
        OneSevenLiveGiftsResponse giftsResponse;
        configManager->loadGifts(giftsJson);

        JsonToOneSevenLiveGiftsResponse(giftsJson, giftsResponse);
        QList<OneSevenLiveGift> gifts = giftsResponse.gifts;

        if (JsonToOneSevenLiveGiftTabsResponse(giftTabsJson, giftTabsData)) {
            // Filter tabs based on allowed categories
            filteredGiftTabs.clear();
            for (auto& tab : giftTabsData.tabs) {
                // 确保使用QString类型进行比较，避免QString和std::string的比较
                if (allowedGiftCategories.contains(tab.id)) {
                    // Filter gifts based on rules
                    QList<OneSevenLiveGift> filteredGifts;
                    for (const auto& tabGift : tab.gifts) {
                        // Find gift data from gifts by gift.id
                        OneSevenLiveGift gift;
                        for (const auto& giftItem : gifts) {
                            // 确保使用QString类型进行比较，避免QString和std::string的比较
                            if (giftItem.giftID == tabGift.giftID) {
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
                                // 将std::string转换为QString，避免类型不匹配
                                shouldShow = gift.regions.contains(QString::fromStdString(region));
                                break;
                            case 3:
                                // regionMode = 3: Don't show if streamer region is in gift regions
                                // 将std::string转换为QString，避免类型不匹配
                                shouldShow = !gift.regions.contains(QString::fromStdString(region));
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
                        tab.gifts.clear();
                        for (const auto& gift : filteredGifts) {
                            tab.gifts.append(gift);
                        }
                        
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
        
        scrollArea->setWidget(giftsContainer);
        tabLayout->addWidget(scrollArea);
        
        giftTabWidget->addTab(tabWidget, giftTab.name);
        
        // Store the gifts layout for later use
        tabWidget->setProperty("giftsLayout", QVariant::fromValue(static_cast<void*>(giftsLayout)));
        tabWidget->setProperty("giftsContainer", QVariant::fromValue(static_cast<void*>(giftsContainer)));
        tabWidget->setProperty("tabIndex", i);

        populateGiftTab(giftTab, i);
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
    int giftsToShow = giftTab.gifts.size();

    for (int i = 0; i < giftsToShow; ++i) {
        const auto& gift = giftTab.gifts[i];
        
        // Create a container widget to hold image and text
        QWidget* giftWidget = new QWidget();
        giftWidget->setFixedSize(80, 140);
        // giftWidget->setStyleSheet(
        //     "QWidget {"
        //     "    background-color: #404040;"
        //     "    border: 2px solid #555555;"
        //     "    border-radius: 8px;"
        //     "}"
        //     "QWidget:hover {"
        //     "    background-color: #505050;"
        //     "    border-color: #666666;"
        //     "}");
        
        // Create vertical layout
        QVBoxLayout* layout = new QVBoxLayout(giftWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(1);
        
        // Create image label
        QLabel* imageLabel = new QLabel();
        imageLabel->setStyleSheet("QLabel {"
                                 "    border-radius: 2px;"
                                 "}");
        imageLabel->setFixedSize(80, 80);
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setScaledContents(true);
        
        // Set placeholder, in actual project should use QNetworkAccessManager to load network images asynchronously
        imageLabel->setText(gift.name.left(1));
        imageLabel->setStyleSheet("background-color: #555555; color: white; font-size: 16px; font-weight: bold; border-radius: 5px;");
        
        // Store image URL as property for future async loading
        if (!gift.leaderboardIcon.isEmpty()) {
            QString iconUrl = "https://cdn.17app.co/" + gift.leaderboardIcon;
            imageLabel->setProperty("iconUrl", iconUrl);

            RemoteTextThread *thread = new RemoteTextThread(iconUrl.toStdString(), "image/png", "", 0, true);
        
            connect(thread, &RemoteTextThread::ImageResult, this, [this, imageLabel](const QByteArray &imageData, const QString &error) {
                if (error.isEmpty() && !imageData.isEmpty()) {
                    QPixmap pix;
                    if (pix.loadFromData(imageData)) {
                        imageLabel->setPixmap(pix.scaled(imageLabel->size(),
                                                        Qt::KeepAspectRatio,
                                                        Qt::SmoothTransformation));
                        imageLabel->setText("");
                    }
                }
            });
            
            connect(thread, &QThread::finished, thread, &QObject::deleteLater);
            
            thread->start();
        }
        
        // Create name label
        QLabel* nameLabel = new QLabel(gift.name);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("color: white; font-size: 14px;");
        nameLabel->setWordWrap(true);
        
        // Create price label
        QLabel* pointLabel = new QLabel(QString::number(gift.point));
        pointLabel->setAlignment(Qt::AlignCenter);
        pointLabel->setStyleSheet("color: white; font-size: 14px;");
        pointLabel->setWordWrap(true);
        
        // Add to layout
        layout->addWidget(imageLabel);
        layout->addWidget(nameLabel);
        layout->addWidget(pointLabel);
        layout->addStretch();
        
        // Set layout stretch factors
        // layout->setStretch(0, 5); // Image
        // layout->setStretch(1, 2); // Name
        // layout->setStretch(2, 1); // Point
        
        // Create a transparent button covering the entire widget to handle click events
        QPushButton* giftButton = new QPushButton(giftWidget);
        giftButton->setFixedSize(80, 140);
        giftButton->setFlat(true);
        giftButton->setStyleSheet(
            "QPushButton {"
            "    background-color: transparent;"
            "    border: none;"
            "}"
            "QPushButton:checked {"
            "    background-color: rgba(0, 122, 204, 150);"
            "    border-radius: 2px;"
            "}");
        
        giftButton->setCheckable(true);
        giftButton->setToolTip(gift.name);
        
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

        QString giftID = gift.giftID;
        QString giftName = gift.name;
        
        connect(giftButton, &QPushButton::clicked, this, [this, i, tabIndex, giftButton, gift]() {
            onGiftSelected(giftButton, gift);
        });
    }
    
    // Update the gifts container
    QScrollArea* scrollArea = tabWidget->findChild<QScrollArea*>();
    if (scrollArea) {
        scrollArea->setWidget(giftsContainer);
    }
}

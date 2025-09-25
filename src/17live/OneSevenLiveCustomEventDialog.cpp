// Include project header first to ensure proper dependency resolution
#include "OneSevenLiveCustomEventDialog.hpp"

// OBS includes
#include <obs-module.h>
#include <plugin-support.h>

// Qt includes
#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QStyle>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QThread>
#include <QToolTip>
#include <QVBoxLayout>
#include <QFontMetrics>
#include <QTextLayout>
#include <QTextOption>
#include <QVector>
#include <QTextEdit>
#include <QAbstractTextDocumentLayout>
#include <QTextFrame>
#include <QCalendarWidget>

// Project includes
#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "utility/Common.hpp"
#include "utility/RemoteTextThread.hpp"
#include "utility/CustomCalendarWidget.hpp"

// Static helper: insert zero-width spaces into CJK or other no-space text to enable line breaks with WrapAnywhere
static QString insertZeroWidthSpaces(const QString& s) {
    QString out;
    out.reserve(s.size() * 2);
    for (int i = 0; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        out.append(ch);
        // Avoid inserting zero-width spaces after whitespace, and do not insert after the last character
        if (i < s.size() - 1 && !ch.isSpace()) {
            out.append(QChar(0x200B)); // ZERO WIDTH SPACE
        }
    }
    return out;
}

// Static helper: limit text to at most two lines (single wrap); overflow is elided at the end of the second line
static QString elideTextToTwoLines(const QString& text, const QFont& font, int widthPx) {
    if (text.isEmpty() || widthPx <= 0)
        return text;

    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAnywhere);

    QTextLayout layout(text, font);
    layout.setTextOption(opt);

    layout.beginLayout();
    int firstEnd = 0;      // End index of the first line (length from start to end)
    int secondStart = 0;   // Start index of the second line
    int processedChars = 0;
    int linesCount = 0;
    qreal y = 0.0;

    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(widthPx);
        line.setPosition(QPointF(0, y));
        y += line.height();

        ++linesCount;
        const int start = line.textStart();
        const int len = line.textLength();

        if (linesCount == 1) {
            firstEnd = start + len;
        } else if (linesCount == 2) {
            secondStart = start;
        }

        processedChars = start + len;
        if (linesCount >= 3) {
            // A third line has been produced; we need to elide within the second line
            break;
        }
    }
    layout.endLayout();

    // If the text overall does not exceed two lines, return the original text
    if (linesCount <= 2 && processedChars >= text.size()) {
        return text;
    }

    // Assemble: keep the first line as-is, elide the second line on the right
    QFontMetrics fm(font);
    const QString firstLine = text.left(firstEnd);
    const QString secondContent = text.mid(secondStart);
    const QString secondElided = fm.elidedText(secondContent, Qt::ElideRight, widthPx);
    return firstLine + QStringLiteral("\n") + secondElided;
}

OneSevenLiveCustomEventDialog::OneSevenLiveCustomEventDialog(
    QWidget* parent, OneSevenLiveApiWrappers* apiWrapper_,
    OneSevenLiveConfigManager* configManager_)
    : QDialog(parent), apiWrapper(apiWrapper_), configManager(configManager_) {
    setupUi();
    setWindowTitle(obs_module_text("CustomEvent.Dialog.Title"));
    setMinimumSize(450, 600);
    setMaximumWidth(600);
    resize(450, 600);
    // setModal(false);  // Set to non-modal

    // Ensure the dialog is properly initialized and visible
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);

    fetchCustomEventAsync();
    loadGiftTabsAsync();

    // Ensure all widgets are properly initialized
    update();
}

OneSevenLiveCustomEventDialog::~OneSevenLiveCustomEventDialog() = default;

void OneSevenLiveCustomEventDialog::setupUi() {
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

    setupBottomButtons(contentLayout);

    // Set content widget to scroll area
    scrollArea->setWidget(contentWidget);

    // Add scroll area to dialog layout
    dialogLayout->addWidget(scrollArea);
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
    // Set field growth policy to allow controls to extend with dialog width
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
    QDate today = QDate::currentDate();
    QDate maxDate = QDate::currentDate().addDays(30);

    dateEdit->setDate(today);
    dateEdit->setMinimumDate(today);
    dateEdit->setMaximumDate(maxDate);
    dateEdit->setDisplayFormat("yyyy/MM/dd");
    dateEdit->setCalendarPopup(true);

    CustomCalendarWidget* calendar = new CustomCalendarWidget(today, maxDate, dateEdit);
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar->setSelectionMode(QCalendarWidget::SingleSelection);
    calendar->setGridVisible(true);
    calendar->setSelectedDate(today);
    dateEdit->setCalendarWidget(calendar);
    
    // Create form layout for date section
    QFormLayout* dateFormLayout = new QFormLayout();
    dateFormLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    dateFormLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy to allow controls to extend with dialog width
    dateFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    dateFormLayout->addRow(dateLabel, dateEdit);

    mainLayout->addSpacing(20);
    mainLayout->addLayout(dateFormLayout);
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
    selectedGiftsEdit->setReadOnly(true);
    // Initialize allowed gift categories
    allowedGiftCategories << "luckyBag" << "TreasureChest" << "Event" << "army"
                          << "Birthday" << "Texture" << "LevelUp" << "Extravagant"
                          << "Audio" << "Heavenly" << "default";

    // Create tab widget for gift categories
    giftTabWidget = new QTabWidget(this);

    giftTabWidget->setMinimumSize(430, 250);
    giftTabWidget->setMaximumWidth(580);
    giftTabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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

#ifdef Q_OS_MACOS
    // Enable tab scrolling arrows on macOS
    giftTabWidget->setUsesScrollButtons(true);
    giftTabWidget->tabBar()->setExpanding(false);
    giftTabWidget->tabBar()->setStyle(QStyleFactory::create("Fusion"));
#endif

    // Load gift tabs data (async to avoid blocking UI)
    // Show placeholder immediately
    setupGiftTabsUI();
    loadGiftTabsAsync();

    // Create form layout for gifts section
    QFormLayout* giftsFormLayout = new QFormLayout();
    giftsFormLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    giftsFormLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy to allow controls to extend with dialog width
    giftsFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    giftsFormLayout->addRow(giftsLabel, selectedGiftsEdit);

    mainLayout->addSpacing(20);
    mainLayout->addLayout(giftsFormLayout);
    mainLayout->addWidget(giftTabWidget);
}

void OneSevenLiveCustomEventDialog::setupEventTargetsSection() {
    // Daily target
    dailyTargetLabel = new QLabel();
    dailyTargetLabel->setText(QString("<span style='color:white;'>%1</span>")
                                  .arg(obs_module_text("CustomEvent.DailyGoal")));
    dailyTargetLabel->setStyleSheet("font-weight: 600;");

    dailyTargetEdit = new QLineEdit(this);
    dailyTargetEdit->setValidator(new QIntValidator(1, 999999999, this));
    dailyTargetEdit->setPlaceholderText(obs_module_text("CustomEvent.DailyGoal.Placeholder"));

    // Create form layout for daily target
    QFormLayout* dailyTargetFormLayout = new QFormLayout();
    dailyTargetFormLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    dailyTargetFormLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy to allow controls to extend with dialog width
    dailyTargetFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    dailyTargetFormLayout->addRow(dailyTargetLabel, dailyTargetEdit);

    // Total target
    totalTargetLabel = new QLabel();
    totalTargetLabel->setText(QString("<span style='color:white;'>%1</span>")
                                  .arg(obs_module_text("CustomEvent.TotalGoal")));
    totalTargetLabel->setStyleSheet("font-weight: 600;");

    totalTargetEdit = new QLineEdit(this);
    totalTargetEdit->setValidator(new QIntValidator(1, 999999999, this));
    totalTargetEdit->setPlaceholderText(obs_module_text("CustomEvent.TotalGoal.Placeholder"));

    // Create form layout for total target
    QFormLayout* totalTargetFormLayout = new QFormLayout();
    totalTargetFormLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    totalTargetFormLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy to allow controls to extend with dialog width
    totalTargetFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    totalTargetFormLayout->addRow(totalTargetLabel, totalTargetEdit);

    mainLayout->addSpacing(20);
    mainLayout->addLayout(dailyTargetFormLayout);
    mainLayout->addSpacing(20);
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

    // Create form layout for description section
    QFormLayout* descriptionFormLayout = new QFormLayout();
    descriptionFormLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // Set label left alignment
    descriptionFormLayout->setLabelAlignment(Qt::AlignLeft);
    // Set field growth policy to allow controls to extend with dialog width
    descriptionFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    descriptionFormLayout->addRow(descriptionLabel, descriptionEdit);

    mainLayout->addSpacing(20);
    mainLayout->addLayout(descriptionFormLayout);
}

void OneSevenLiveCustomEventDialog::setupBottomButtons(QVBoxLayout* parentLayout) {
    // Create bottom buttons container
    QWidget* buttonContainer = new QWidget(this);
    buttonContainer->setStyleSheet(
        "QWidget { background-color: #1a1a1a; border-top: 1px solid #404040; }");

    buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setSpacing(10);
    buttonLayout->setContentsMargins(20, 15, 20, 15);

    // Add spacer to push buttons to right
    buttonLayout->addStretch();

    // Determine which button to display based on customEventData status
    if (customEvent.eventID.isEmpty()) {
        // When customEvent doesn't exist or eventID is empty, display Create button
        createButton = new QPushButton(obs_module_text("CustomEvent.Create"), this);
        createButton->setObjectName("createButton");
        // set button color #FF0001
        createButton->setStyleSheet("QPushButton { background-color: #FF0001; color: white; }");
        createButton->setFixedHeight(40);
        createButton->setMinimumWidth(80);
        buttonLayout->addWidget(createButton);

        // Connect Create button signal
        connect(createButton, &QPushButton::clicked, this,
                &OneSevenLiveCustomEventDialog::handleCreateEvent);
    } else if (customEvent.status == 1) {
        // When customEvent exists and status=1, display Stop button
        createButton = new QPushButton(obs_module_text("CustomEvent.Stop"), this);
        // button color #007AFF
        createButton->setStyleSheet("QPushButton {background-color: #007AFF; color: white;}");
        createButton->setObjectName("stopButton");
        createButton->setFixedHeight(40);
        createButton->setMinimumWidth(80);
        buttonLayout->addWidget(createButton);

        // Connect Stop button signal
        connect(createButton, &QPushButton::clicked, this,
                &OneSevenLiveCustomEventDialog::handleStopEvent);
    } else if (customEvent.status == 2) {
        // When customEvent exists and status=2, display Close button
        createButton = new QPushButton(obs_module_text("CustomEvent.Close"), this);
        // button color #007AFF
        createButton->setStyleSheet("QPushButton {background-color: #007AFF; color: white;}");
        createButton->setObjectName("closeButton");
        createButton->setFixedHeight(40);
        createButton->setMinimumWidth(80);
        buttonLayout->addWidget(createButton);

        // Connect Close button signal
        connect(createButton, &QPushButton::clicked, this,
                &OneSevenLiveCustomEventDialog::handleCloseEvent);
    }

    // Add button container to parent layout
    parentLayout->addWidget(buttonContainer);
}

void OneSevenLiveCustomEventDialog::loadGiftTabsAsync() {
    QThread* thread = new QThread(this);
    QObject* worker = new QObject();
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [this, worker, thread]() {
        QList<OneSevenLiveGiftTab> localFilteredGiftTabs;
        QList<OneSevenLiveGift> localSelectedGifts;
        OneSevenLiveGiftTabsResponse localGiftTabsData;

        if (!apiWrapper || !configManager) {
            QMetaObject::invokeMethod(
                this,
                [this, thread]() {
                    setupGiftTabsUI();
                    thread->quit();
                },
                Qt::QueuedConnection);
            return;
        }

        std::string roomID;
        std::string region;
        if (!configManager->getConfigValue("RoomID", roomID) ||
            !configManager->getConfigValue("Region", region)) {
            QMetaObject::invokeMethod(
                this,
                [this, thread]() {
                    setupGiftTabsUI();
                    thread->quit();
                },
                Qt::QueuedConnection);
            return;
        }

        std::string language = GetCurrentLanguage();

        Json giftTabsJson;
        if (!apiWrapper->GetGiftTabs(roomID, language, giftTabsJson)) {
            QMetaObject::invokeMethod(
                this,
                [this, thread]() {
                    setupGiftTabsUI();
                    thread->quit();
                },
                Qt::QueuedConnection);
            return;
        }

        Json giftsJson;
        OneSevenLiveGiftsResponse giftsResponse;
        configManager->loadGifts(giftsJson);
        JsonToOneSevenLiveGiftsResponse(giftsJson, giftsResponse);
        QList<OneSevenLiveGift> gifts = giftsResponse.gifts;

        if (JsonToOneSevenLiveGiftTabsResponse(giftTabsJson, localGiftTabsData)) {
            for (auto& tab : localGiftTabsData.tabs) {
                if (allowedGiftCategories.contains(tab.id)) {
                    QList<OneSevenLiveGift> filteredGifts;
                    for (const auto& tabGift : tab.gifts) {
                        OneSevenLiveGift gift;
                        for (const auto& giftItem : gifts) {
                            if (giftItem.giftID == tabGift.giftID) {
                                gift = giftItem;
                                break;
                            }
                        }
                        if (gift.isHidden == 1) {
                            continue;
                        }
                        bool shouldShow = false;
                        switch (gift.regionMode) {
                        case 1:
                            shouldShow = true;
                            break;
                        case 2:
                            shouldShow = gift.regions.contains(QString::fromStdString(region));
                            break;
                        case 3:
                            shouldShow = !gift.regions.contains(QString::fromStdString(region));
                            break;
                        default:
                            shouldShow = false;
                            break;
                        }
                        if (shouldShow) {
                            filteredGifts.append(gift);
                            if (customEvent.giftIDs.size() > 0 &&
                                customEvent.giftIDs.contains(gift.giftID)) {
                                localSelectedGifts.append(gift);
                            }
                        }
                    }
                    if (!filteredGifts.isEmpty()) {
                        tab.gifts = filteredGifts;
                        localFilteredGiftTabs.append(tab);
                    }
                }
            }
        }

        QMetaObject::invokeMethod(
            this,
            [this, thread, localFilteredGiftTabs, localSelectedGifts]() {
                filteredGiftTabs = localFilteredGiftTabs;
                selectedGifts = localSelectedGifts;
                setupGiftTabsUI();

                // update selected gifts' name
                QList<QString> selectedGiftsName;
                for (auto selectedGift : selectedGifts) {
                    selectedGiftsName.append(selectedGift.name);
                }
                if (selectedGiftsEdit)
                    selectedGiftsEdit->setText(selectedGiftsName.join(" / "));

                // Disable gift buttons if event exists
                if (!customEvent.eventID.isEmpty() && giftTabWidget) {
                    for (int i = 0; i < giftTabWidget->count(); ++i) {
                        QWidget* tab = giftTabWidget->widget(i);
                        if (!tab)
                            continue;
                        const auto buttons = tab->findChildren<QPushButton*>();
                        for (auto* btn : buttons) {
                            if (btn->property("giftID").isValid())
                                btn->setEnabled(false);
                        }
                    }
                }

                // Reflect selection
                updateGiftSelectionUIFromCustomEvent();

                thread->quit();
            },
            Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void OneSevenLiveCustomEventDialog::fetchCustomEventAsync() {
    // Create a new thread and lightweight worker, following RockZoneDock pattern
    QThread* thread = new QThread(this);
    QObject* worker = new QObject();
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [this, worker, thread]() {
        std::string userID;
        if (configManager) {
            configManager->getConfigValue("UserID", userID);
        }

        bool ok = false;
        if (apiWrapper) {
            ok = apiWrapper->GetCustomEvent(userID, customEvent);
        }

        QMetaObject::invokeMethod(
            this,
            [this, ok, thread]() {
                // Update UI on main thread
                if (!ok) {
                    obs_log(LOG_ERROR, "Failed to get custom event");
                } else {
                    obs_log(LOG_INFO, "id=%s, customEvent.status = %d",
                            customEvent.eventID.toStdString().c_str(), customEvent.status);

                    // Populate fields
                    eventTitleEdit->setText(customEvent.eventName);
                    if (customEvent.endTime > 0) {
                        QDateTime endDateTime = QDateTime::fromSecsSinceEpoch(customEvent.endTime);
                        dateEdit->setDate(endDateTime.date());
                    }
                    descriptionEdit->setText(customEvent.description);
                    dailyTargetEdit->setText(QString::number(customEvent.dailyGoalPoints));
                    totalTargetEdit->setText(QString::number(customEvent.goalPoints));

                    // If event exists, lock down inputs and switch bottom button accordingly
                    if (!customEvent.eventID.isEmpty()) {
                        eventTitleEdit->setEnabled(false);
                        dateEdit->setEnabled(false);
                        dailyTargetEdit->setEnabled(false);
                        totalTargetEdit->setEnabled(false);
                        descriptionEdit->setEnabled(false);
                        // Disable gift selection buttons
                        if (giftTabWidget) {
                            for (int i = 0; i < giftTabWidget->count(); ++i) {
                                QWidget* tab = giftTabWidget->widget(i);
                                if (!tab)
                                    continue;
                                const auto buttons = tab->findChildren<QPushButton*>();
                                for (auto* btn : buttons) {
                                    if (btn->property("giftID").isValid())
                                        btn->setEnabled(false);
                                }
                            }
                        }

                        // Update bottom button state
                        if (createButton) {
                            // Disconnect previous connections to avoid duplicates
                            createButton->disconnect();
                            if (customEvent.status == 1) {
                                createButton->setText(obs_module_text("CustomEvent.Stop"));
                                createButton->setStyleSheet(
                                    "QPushButton {background-color: #007AFF; color: white;}");
                                createButton->setObjectName("stopButton");
                                connect(createButton, &QPushButton::clicked, this,
                                        &OneSevenLiveCustomEventDialog::handleStopEvent);
                            } else if (customEvent.status == 2) {
                                createButton->setText(obs_module_text("CustomEvent.Close"));
                                createButton->setStyleSheet(
                                    "QPushButton {background-color: #007AFF; color: white;}");
                                createButton->setObjectName("closeButton");
                                connect(createButton, &QPushButton::clicked, this,
                                        &OneSevenLiveCustomEventDialog::handleCloseEvent);
                            } else {
                                // Fallback to Create
                                createButton->setText(obs_module_text("CustomEvent.Create"));
                                createButton->setStyleSheet(
                                    "QPushButton { background-color: #FF0001; color: white; }");
                                createButton->setObjectName("createButton");
                                connect(createButton, &QPushButton::clicked, this,
                                        &OneSevenLiveCustomEventDialog::handleCreateEvent);
                            }
                        }
                    }

                    // Synchronously update gift selection state (regardless of order)
                    updateGiftSelectionUIFromCustomEvent();
                }

                // Stop thread after UI update
                thread->quit();
            },
            Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void OneSevenLiveCustomEventDialog::updateGiftSelectionUIFromCustomEvent() {
    if (!giftTabWidget)
        return;

    QList<QString> targetGiftIDs;
    if (customEvent.giftIDs.isEmpty()) {
        targetGiftIDs = customEvent.giftIDs;
    } else {
        for (const auto& g : selectedGifts)
            targetGiftIDs.append(g.giftID);
    }

    for (int i = 0; i < giftTabWidget->count(); ++i) {
        QWidget* tab = giftTabWidget->widget(i);
        if (!tab)
            continue;
        const auto buttons = tab->findChildren<QPushButton*>();
        for (auto* btn : buttons) {
            QVariant giftIdVar = btn->property("giftID");
            if (!giftIdVar.isValid())
                continue;
            const QString giftID = giftIdVar.toString();
            QSignalBlocker blocker(btn);  // prevent toggled signal loop
            btn->setChecked(targetGiftIDs.contains(giftID));
        }
    }
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
            QMessageBox::warning(
                this, obs_module_text("CustomEvent.Error"),
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
        QMessageBox::warning(
            this, obs_module_text("CustomEvent.Error"),
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
    eventRequest.status = 1;  // Event status - Active
    eventRequest.description = descriptionEdit->toPlainText().trimmed();
    eventRequest.dailyGoalPoints = dailyTargetEdit->text().toInt();
    eventRequest.goalPoints = totalTargetEdit->text().toInt();

    // Add selected gift IDs
    for (auto gift : selectedGifts) {
        eventRequest.giftIDs.append(gift.giftID);
    }

    if (!apiWrapper->CreateCustomEvent(eventRequest, customEvent)) {
        QMessageBox::warning(this, obs_module_text("CustomEvent.Error"),
                             obs_module_text("CustomEvent.Error.CreateFailed"));
        return;
    }

    // Send event created signal
    emit eventCreated(customEvent);

    disconnect(createButton, &QPushButton::clicked, this,
               &OneSevenLiveCustomEventDialog::handleCreateEvent);
    createButton->setText(obs_module_text("CustomEvent.Stop"));
    createButton->setStyleSheet("QPushButton {background-color: #007AFF; color: white;}");
    connect(createButton, &QPushButton::clicked, this,
            &OneSevenLiveCustomEventDialog::handleStopEvent);

    eventTitleEdit->setEnabled(false);
    dateEdit->setEnabled(false);
    dailyTargetEdit->setEnabled(false);
    totalTargetEdit->setEnabled(false);
    descriptionEdit->setEnabled(false);
    // set buttons in giftTabWidget to disabled
    for (int i = 0; i < giftTabWidget->count(); i++) {
        QWidget* tab = giftTabWidget->widget(i);
        if (tab) {
            QPushButton* giftButton = tab->findChild<QPushButton*>();
            if (giftButton) {
                giftButton->setEnabled(false);
            }
        }
    }
}

void OneSevenLiveCustomEventDialog::handleStopEvent() {
    // Confirm whether to stop the event
    QMessageBox msgBox(QMessageBox::Question, obs_module_text("CustomEvent.Confirm.Stop.Title"),
                       obs_module_text("CustomEvent.Confirm.StopEvent"),
                       QMessageBox::Yes | QMessageBox::No, this);
    QAbstractButton* yesButton = msgBox.button(QMessageBox::Yes);
    if (yesButton) {
        yesButton->setText(obs_module_text("CustomEvent.Confirm.Stop.Yes"));
    }
    QMessageBox::StandardButton reply = (QMessageBox::StandardButton) msgBox.exec();
    if (reply == QMessageBox::Yes) {
        OneSevenLiveCustomEventStatusRequest request;
        request.status = 2;
        request.userID = customEvent.userID;

        obs_log(LOG_INFO, "Stopping custom event... userID: %s, eventID: %s",
                customEvent.userID.toStdString().c_str(),
                customEvent.eventID.toStdString().c_str());

        if (!apiWrapper->ChangeCustomEventStatus(customEvent.eventID.toStdString(), request)) {
            obs_log(LOG_ERROR, "Failed to change custom event status");
            QMessageBox::warning(this, obs_module_text("CustomEvent.Error"),
                                 obs_module_text("CustomEvent.Error.StopFailed"));
            return;
        }

        customEvent.status = 2;

        // Send event update signal
        emit eventUpdated(customEvent);

        disconnect(createButton, &QPushButton::clicked, this,
                   &OneSevenLiveCustomEventDialog::handleStopEvent);
        createButton->setText(obs_module_text("CustomEvent.Close"));
        // Connect Close button signal
        connect(createButton, &QPushButton::clicked, this,
                &OneSevenLiveCustomEventDialog::handleCloseEvent);
    }
}

void OneSevenLiveCustomEventDialog::handleCloseEvent() {
    QMessageBox msgBox(QMessageBox::Question, obs_module_text("CustomEvent.Confirm.Close.Title"),
                       obs_module_text("CustomEvent.Confirm.CloseEvent"),
                       QMessageBox::Yes | QMessageBox::No, this);
    QAbstractButton* yesButton = msgBox.button(QMessageBox::Yes);
    if (yesButton) {
        yesButton->setText(obs_module_text("CustomEvent.Confirm.Close.Yes"));
    }
    QMessageBox::StandardButton reply = (QMessageBox::StandardButton) msgBox.exec();
    if (reply == QMessageBox::Yes) {
        OneSevenLiveCustomEventStatusRequest request;
        request.status = 3;
        request.userID = customEvent.userID;

        if (!apiWrapper->ChangeCustomEventStatus(customEvent.eventID.toStdString(), request)) {
            obs_log(LOG_ERROR, "Failed to change custom event status");
            QMessageBox::warning(this, obs_module_text("CustomEvent.Error"),
                                 obs_module_text("CustomEvent.Error.CloseFailed"));
            return;
        }

        // Send event update signal
        customEvent.status = 3;
        emit eventUpdated(customEvent);
    } else {
        return;
    }

    // Directly close the dialog
    accept();
}

void OneSevenLiveCustomEventDialog::setupGiftTabsUI() {
    // Clear existing tabs
    giftTabWidget->clear();

    if (filteredGiftTabs.isEmpty()) {
        // Create placeholder tab if no data available
        QWidget* placeholderTab = new QWidget();
        QVBoxLayout* placeholderLayout = new QVBoxLayout(placeholderTab);

        QLabel* placeholderLabel = new QLabel(obs_module_text("CustomEvent.LoadingGifts"));
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
        tabWidget->setProperty("giftsContainer",
                               QVariant::fromValue(static_cast<void*>(giftsContainer)));
        tabWidget->setProperty("tabIndex", i);

        populateGiftTab(giftTab, i);
    }
}

void OneSevenLiveCustomEventDialog::populateGiftTab(const OneSevenLiveGiftTab& giftTab,
                                                    int tabIndex) {
    QWidget* tabWidget = giftTabWidget->widget(tabIndex);
    if (!tabWidget)
        return;

    QGridLayout* giftsLayout =
        static_cast<QGridLayout*>(tabWidget->property("giftsLayout").value<void*>());
    QWidget* giftsContainer =
        static_cast<QWidget*>(tabWidget->property("giftsContainer").value<void*>());

    if (!giftsLayout || !giftsContainer)
        return;

    QList<QString> selectedGiftIds;
    for (const auto& gift : selectedGifts) {
        selectedGiftIds.append(gift.giftID);
    }

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

        // Create vertical layout
        QVBoxLayout* layout = new QVBoxLayout(giftWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);

        // Create image label
        QLabel* imageLabel = new QLabel();
        imageLabel->setStyleSheet(
            "QLabel {"
            "    border-radius: 2px;"
            "}");
        imageLabel->setFixedSize(80, 80);
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setScaledContents(true);

        // Set placeholder, in actual project should use QNetworkAccessManager to load network
        // images asynchronously
        imageLabel->setText(gift.name.left(1));
        imageLabel->setStyleSheet(
            "background-color: #555555; color: white; font-size: 16px; font-weight: bold; "
            "border-radius: 5px;");

        // Store image URL as property for future async loading
        if (!gift.leaderboardIcon.isEmpty()) {
            QString iconUrl = "https://cdn.17app.co/" + gift.leaderboardIcon;
            imageLabel->setProperty("iconUrl", iconUrl);

            RemoteTextThread* thread =
                new RemoteTextThread(iconUrl.toStdString(), "image/png", "", 0, true);

            QPointer<QLabel> safeImageLabel = imageLabel;
            connect(thread, &RemoteTextThread::ImageResult, this,
                    [this, safeImageLabel](const QByteArray& imageData, const QString& error) {
                        if (error.isEmpty() && !imageData.isEmpty()) {
                            QPixmap pix;
                            if (pix.loadFromData(imageData) && safeImageLabel) {
                                safeImageLabel->setPixmap(pix.scaled(safeImageLabel->size(),
                                                                     Qt::KeepAspectRatio,
                                                                     Qt::SmoothTransformation));
                                safeImageLabel->setText("");
                            }
                        }
                    });

            connect(thread, &QThread::finished, thread, &QObject::deleteLater);

            thread->start();
        }

        // Create name edit
        QTextEdit* nameEdit = new QTextEdit(giftWidget);
        nameEdit->setReadOnly(true);
        nameEdit->setFrameStyle(QFrame::NoFrame);
        nameEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        nameEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        nameEdit->setStyleSheet("color: white; font-size: 12px; background: transparent; padding: 2px 0 0 0; margin: 0; border: none;");
        nameEdit->setContentsMargins(0, 0, 0, 0);
        nameEdit->setFixedWidth(80);
        // Height will be set dynamically below based on document height
        {
            QTextOption opt;
            opt.setWrapMode(QTextOption::WrapAnywhere);
            opt.setAlignment(Qt::AlignCenter);
            nameEdit->document()->setDefaultTextOption(opt);
            nameEdit->document()->setDocumentMargin(0);
            nameEdit->setWordWrapMode(QTextOption::WrapAnywhere);
            nameEdit->setAcceptRichText(false);
            const QString processedName = insertZeroWidthSpaces(gift.name);
            const QString clamped = elideTextToTwoLines(processedName, nameEdit->font(), 80);
            nameEdit->setText(clamped);
            // Constrain document width to widget width and compute doc height
            nameEdit->document()->setTextWidth(80);
            // Compute dynamic height from document layout (max two lines from elide), then add 5px
            qreal docHeight = nameEdit->document()->documentLayout()->documentSize().height();
            int lineH = QFontMetrics(nameEdit->font()).lineSpacing();
            int minH = lineH;            // at least 1 line
            int maxH = lineH * 2;        // at most 2 lines
            int h = qRound(docHeight);
            if (h < minH) h = minH;
            if (h > maxH) h = maxH;
            nameEdit->setFixedHeight(h + 5);
         }

        // Create price label
        QLabel* pointLabel = new QLabel(QString::number(gift.point));
        pointLabel->setAlignment(Qt::AlignCenter);
        pointLabel->setStyleSheet("color: white; font-size: 12px;");
        pointLabel->setMaximumWidth(80);
        pointLabel->setWordWrap(true);
        pointLabel->setFixedHeight(20);

        // Add to layout
        layout->addWidget(imageLabel);
        layout->addWidget(nameEdit);
        layout->addWidget(pointLabel);

        // Create a transparent button covering the entire widget to handle click events
        QPushButton* giftButton = new QPushButton(giftWidget);
        giftButton->setFixedSize(80, 140);
        giftButton->setFlat(true);
        giftButton->setStyleSheet(
            "QPushButton {"
            "    background-color: transparent;"
            "    border: none;"
            "    padding: 0px;"
            "}"
            "QPushButton:checked {"
            "    background-color: rgba(128, 128, 128, 0.3);"
            "    border: 1px solid rgba(128, 128, 128, 0.5);"
            "    padding: 0px;"
            "}");

        giftButton->setCheckable(true);
        giftButton->setToolTip(gift.name);
        // Initial state: no icon when unchecked
        giftButton->setIcon(QIcon());
        giftButton->setIconSize(QSize(40, 40));
        // Only show icon when checked (hide when unchecked)
        connect(giftButton, &QPushButton::toggled, this, [giftButton](bool checked) {
            if (checked) {
                giftButton->setIcon(QIcon(":/resources/circle-check.svg"));
            } else {
                giftButton->setIcon(QIcon());
            }
        });

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

        if (selectedGiftIds.contains(gift.giftID)) {
            giftButton->setChecked(true);
        }

        if (!customEvent.eventID.isEmpty()) {
            giftButton->setEnabled(false);
        }

        QString giftID = gift.giftID;
        QString giftName = gift.name;

        connect(giftButton, &QPushButton::clicked, this,
                [this, i, tabIndex, giftButton, gift]() { onGiftSelected(giftButton, gift); });
    }

    // Update the gifts container
    QScrollArea* scrollArea = tabWidget->findChild<QScrollArea*>();
    if (scrollArea) {
        scrollArea->setWidget(giftsContainer);
    }
}

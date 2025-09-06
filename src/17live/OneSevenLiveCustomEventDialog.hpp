#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QDateEdit>
#include <QCalendarWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QFrame>
#include <QSpinBox>
#include <QComboBox>
#include <QTabWidget>

struct OneSevenLiveCustomEvent;
struct OneSevenLiveGiftTab;
struct OneSevenLiveGiftTabsResponse;
class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;

class OneSevenLiveCustomEventDialog : public QDialog {
    Q_OBJECT

public:
    explicit OneSevenLiveCustomEventDialog(QWidget* parent = nullptr,
                                          OneSevenLiveApiWrappers* apiWrapper_ = nullptr,
                                          OneSevenLiveConfigManager* configManager_ = nullptr,
                                          const OneSevenLiveCustomEvent* customEvent = nullptr);
    ~OneSevenLiveCustomEventDialog();

private:
    void setupUi();
    void setupEventTitleSection();
    void setupEventDateSection();
    void setupEventGiftsSection();
    void setupEventTargetsSection();
    void setupEventDescriptionSection();
    void setupBottomButtons(QVBoxLayout* parentLayout);
    
    void handleCreateEvent();
    void handleStopEvent();
    void handleCloseEvent();

    void onDateChanged();
    void onGiftSelected(int giftIndex);
    void onGiftTabChanged(int tabIndex);
    void loadGiftTabs();
    void setupGiftTabsUI();
    void populateGiftTab(const OneSevenLiveGiftTab& giftTab, int tabIndex);
    
signals:
    /**
     * @brief Custom event created successfully
     * @param eventData Event information
     */
    void eventCreated(const OneSevenLiveCustomEvent& eventData);
    
    /**
     * @brief Custom event updated successfully
     * @param eventData Updated event information
     */
    void eventUpdated(const OneSevenLiveCustomEvent& eventData);

private:
    // UI Components
    QVBoxLayout* mainLayout;
    
    // Event Title Section
    QLabel* titleLabel;
    QLineEdit* eventTitleEdit;
    
    // Event Date Section
    QLabel* dateLabel;
    QDateEdit* dateEdit;
    QCalendarWidget* calendar;
    QFrame* calendarFrame;
    
    // Event Gifts Section
    QLabel* giftsLabel;
    QTabWidget* giftTabWidget;
    QScrollArea* giftsScrollArea;
    QWidget* giftsContainer;
    QGridLayout* giftsLayout;
    QList<QPushButton*> giftButtons;
    QList<int> selectedGiftIndices; // 支持多选礼物
    static const int MAX_SELECTED_GIFTS = 4; // 最多选择4个礼物
    
    // Gift Tab Data
    OneSevenLiveGiftTabsResponse giftTabsData;
    QStringList allowedGiftCategories;
    QList<OneSevenLiveGiftTab> filteredGiftTabs;
    
    // Event Targets Section
    QLabel* dailyTargetLabel;
    QSpinBox* dailyTargetSpinBox;
    QLabel* totalTargetLabel;
    QSpinBox* totalTargetSpinBox;
    
    // Event Description Section
    QLabel* descriptionLabel;
    QTextEdit* descriptionEdit;
    QLabel* characterCountLabel;
    
    // Bottom Buttons
    QHBoxLayout* buttonLayout;
    QPushButton* createButton;
    
    // API Wrapper
    OneSevenLiveApiWrappers* apiWrapper;
    // Config manager
    OneSevenLiveConfigManager* configManager;
    
    // Custom event data
    const OneSevenLiveCustomEvent* customEventData;
    
    // Constants
    static const int MAX_DESCRIPTION_LENGTH = 200;
    static constexpr int GIFT_GRID_ROWS = 3;
    static constexpr int GIFT_GRID_COLUMNS = 4;
    static constexpr int MAX_GIFTS = GIFT_GRID_ROWS * GIFT_GRID_COLUMNS;
};

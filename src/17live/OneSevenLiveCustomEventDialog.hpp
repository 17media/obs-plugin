#pragma once

// C++ Standard Library includes
#include <type_traits>
#include <mutex>
#include <string>
#include <limits>
#include <chrono>

// Qt includes
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

#include "api/OneSevenLiveModels.hpp"

class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;

class OneSevenLiveCustomEventDialog : public QDialog {
    Q_OBJECT

public:
    explicit OneSevenLiveCustomEventDialog(QWidget* parent = nullptr,
                                          OneSevenLiveApiWrappers* apiWrapper_ = nullptr,
                                          OneSevenLiveConfigManager* configManager_ = nullptr);
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
    void onGiftSelected(QPushButton* giftButton, OneSevenLiveGift gift);
    // void onGiftTabChanged(int tabIndex);


    // Async
    // 异步获取自定义活动数据
    void fetchCustomEventAsync();
    // 异步加载礼物标签，避免阻塞主线程
    void loadGiftTabsAsync();
    // 基于 customEvent 数据刷新礼物选择与UI勾选状态
    void updateGiftSelectionUIFromCustomEvent();
    
    // Gift UI setup functions
    void setupGiftTabsUI();
    void populateGiftTab(const OneSevenLiveGiftTab& giftTab, int tabIndex);
    
    OneSevenLiveCustomEvent customEvent;
    
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
    QLineEdit* selectedGiftsEdit;
    QTabWidget* giftTabWidget;
    QScrollArea* giftsScrollArea;
    QWidget* giftsContainer;
    QGridLayout* giftsLayout;
    QList<OneSevenLiveGift> selectedGifts; // Support multiple gift selection
    static const int MAX_SELECTED_GIFTS = 4; // Maximum 4 gifts can be selected
    
    // Gift Tab Data
    OneSevenLiveGiftTabsResponse giftTabsData;
    QStringList allowedGiftCategories;
    QList<OneSevenLiveGiftTab> filteredGiftTabs;
    
    // Event Targets Section
    QLabel* dailyTargetLabel;
    QLineEdit* dailyTargetEdit;
    QLabel* totalTargetLabel;
    QLineEdit* totalTargetEdit;
    
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
    
    // Constants
    static const int MAX_TITLE_LENGTH = 20;
    static const int MAX_DESCRIPTION_LENGTH = 200;
    static constexpr int GIFT_GRID_COLUMNS = 4;
};

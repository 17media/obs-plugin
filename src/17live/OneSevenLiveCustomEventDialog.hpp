#pragma once

// Qt includes
#include <QCalendarWidget>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

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
    // Asynchronously fetch custom event data
    void fetchCustomEventAsync();
    // Asynchronously load gift tabs to avoid blocking the main thread
    void loadGiftTabsAsync();
    // Refresh gift selection and UI checked state based on customEvent data
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
    QVBoxLayout* mainLayout = nullptr;

    // Event Title Section
    QLabel* titleLabel = nullptr;
    QLineEdit* eventTitleEdit = nullptr;

    // Event Date Section
    QLabel* dateLabel = nullptr;
    QDateEdit* dateEdit = nullptr;
    QCalendarWidget* calendar = nullptr;
    QFrame* calendarFrame = nullptr;

    // Event Gifts Section
    QLabel* giftsLabel = nullptr;
    QLineEdit* selectedGiftsEdit = nullptr;
    QTabWidget* giftTabWidget = nullptr;
    QScrollArea* giftsScrollArea = nullptr;
    QWidget* giftsContainer = nullptr;
    QGridLayout* giftsLayout = nullptr;
    QList<OneSevenLiveGift> selectedGifts;    // Support multiple gift selection
    static const int MAX_SELECTED_GIFTS = 4;  // Maximum 4 gifts can be selected

    // Gift Tab Data
    OneSevenLiveGiftTabsResponse giftTabsData;
    QStringList allowedGiftCategories;
    QList<OneSevenLiveGiftTab> filteredGiftTabs;

    // Event Targets Section
    QLabel* dailyTargetLabel = nullptr;
    QLineEdit* dailyTargetEdit = nullptr;
    QLabel* totalTargetLabel = nullptr;
    QLineEdit* totalTargetEdit = nullptr;

    // Event Description Section
    QLabel* descriptionLabel = nullptr;
    QTextEdit* descriptionEdit = nullptr;
    QLabel* characterCountLabel = nullptr;

    // Bottom Buttons
    QHBoxLayout* buttonLayout = nullptr;
    QPushButton* createButton = nullptr;

    // API Wrapper
    OneSevenLiveApiWrappers* apiWrapper = nullptr;
    // Config manager
    OneSevenLiveConfigManager* configManager = nullptr;

    // Constants
    static const int MAX_TITLE_LENGTH = 20;
    static const int MAX_DESCRIPTION_LENGTH = 200;
    static constexpr int GIFT_GRID_COLUMNS = 4;
};

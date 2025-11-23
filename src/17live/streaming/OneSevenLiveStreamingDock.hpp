#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMutex>
#include <QMutexLocker>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <atomic>

#include "OneSevenLiveLoadRoomInfoWorker.hpp"
#include "api/OneSevenLiveModels.hpp"

class OneSevenLiveCustomEventDialog;
class OneSevenLiveStreamManager;
class OneSevenLiveApiWrappers;

class OneSevenLiveStreamingDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveStreamingDock(QWidget *parent = nullptr,
                                       OneSevenLiveStreamManager *streamManager = nullptr,
                                       OneSevenLiveApiWrappers *apiWrappers = nullptr);
    ~OneSevenLiveStreamingDock();

    void updateLiveStatus(OneSevenLiveStreamingStatus status);
    void createLiveWithRequest(const OneSevenLiveRtmpRequest &request);
    void editLiveWithInfo(const OneSevenLiveStreamInfo &info);
    void loadRoomInfo();

   private:
    void setupUi();
    void createConnections();
    void updateUIWithRoomInfo();
    void updateRequiredArmyRankSelections();
    void updateUIValues();
    void handleLoadingCompleted(const OneSevenLiveLoadRoomInfoWorker::LoadResult &result);

    /**
     * @brief Change event during streaming
     * @param eventID New event ID
     * @return bool True if event change was successful
     */
    bool changeEvent(qint64 eventID);

    /**
     * @brief Start event cooldown timer
     * @param duration Cooldown duration in seconds (default 300 = 5 minutes)
     */
    void startEventCooldown(int duration = 300);

    /**
     * @brief Check if event change is in cooldown
     * @return bool True if cooldown is active
     */
    bool isEventInCooldown() const;

    /**
     * @brief Get remaining cooldown time
     * @return int Remaining cooldown time in seconds
     */
    int getEventCooldownRemaining() const;

    // Member variables
    OneSevenLiveApiWrappers *apiWrapper;

   private:
    // UI elements
    QLineEdit *titleEdit;
    QComboBox *categoryCombo;

    // Tag area
    QLineEdit *tagEdit;
    QPushButton *addTagButton;
    QWidget *tagsContainer;   // Container for displaying tags
    QHBoxLayout *tagsLayout;  // Layout for tag container
    QList<QString> tagsList;  // Store current tag list

    // Streaming format
    QRadioButton *landscapeStreamRadio;
    QRadioButton *portraitStreamRadio;

    // Live mode - army-only viewing
    QLabel *broadcastModeLabel;
    QWidget *armyOnlyHeader;
    QHBoxLayout *armyOnlyHeaderLayout;
    QLabel *armyOnlyLabel;
    QPushButton *armyOnlyToggleButton;
    QWidget *armyOnlyContainer;
    QVBoxLayout *armyOnlyContainerLayout;
    QCheckBox *armyOnlyCheck;
    QComboBox *requiredArmyRankCombo;
    QCheckBox *showInHotPageCheck;
    QCheckBox *liveNotificationCheck;
    bool armyOnlyExpanded;

    QComboBox *eventCombo;
    QLabel *hintLabel;  // Event hint label

    // Custom Event
    QWidget *customEventHeader;
    QHBoxLayout *customEventHeaderLayout;
    QLabel *customEventLabel;
    QPushButton *customEventToggleButton;
    OneSevenLiveCustomEventDialog *customEventDialog = nullptr;

    // Party Live
    QWidget *GroupCallContainer;
    QHBoxLayout *GroupCallContainerLayout;
    QLabel *GroupCallLabel;
    QPushButton *GroupCallHelpButton;
    QCheckBox *GroupCallCheck;

    // Switches
    QCheckBox *archiveStreamCheck;
    QCheckBox *autoPreviewCheck;

    QComboBox *clipIdentityCombo;
    QCheckBox *virtualStreamerCheck;

    // Bottom buttons
    QPushButton *saveConfigButton;
    QPushButton *createLiveButton;

    // Loading state UI
    QWidget *loadingOverlay;
    QProgressBar *loadingProgress;
    QLabel *loadingLabel;

    OneSevenLiveRoomInfo roomInfo;
    OneSevenLiveConfigStreamer configStreamer;
    OneSevenLiveUserInfo userInfo;
    OneSevenLiveArmySubscriptionLevels levels;

   signals:
    void streamInfoSaved();
    void streamStatusUpdated(OneSevenLiveStreamingStatus status);
    void eventCooldownUpdated(int remainingTime);

   private slots:
    void onAddTagClicked();
    void onTagEnterPressed();
    void onRemoveTagClicked();
    void onCreateLiveClicked();
    void onDeleteLiveClicked();
    void onSaveConfigClicked();
    void onArmyOnlyToggleClicked();          // New collapse/expand button click event
    void onArmyOnlyCheckChanged(int state);  // Triggered when armyOnlyCheck state changes
    void onCustomEventToggleClicked();       // Custom event toggle button click event
    void onGroupCallHelpClicked();           // Party live help button click event
    void onEventChanged(int index);          // Event change event handler
    void onEventCooldownTimeout();           // Event cooldown timer timeout handler

    bool gatherRtmpRequest(OneSevenLiveRtmpRequest &request);
    void populateRtmpRequest(const OneSevenLiveRtmpRequest &request);
    void updateLiveButton(bool isLive);

    void createLive(const OneSevenLiveRtmpRequest &request);
    void startLive(bool startStream = true);

    // Tag-related functions
    void addTag(const QString &tag);
    void updateTagsFromList();

   private:
    int hashtagSelectLimit = 2;  // Maximum number of tags that can be added

    OneSevenLiveStreamManager *streamManager = nullptr;

    QString currentInfoUuid = "";
    // Loading state now controlled by OneSevenLiveStreamManager
    OneSevenLiveStreamingStatus currentLiveStatus = OneSevenLiveStreamingStatus::NotStarted;

    // Category change cooldown timer
    QTimer *eventCooldownTimer = nullptr;
    int eventCooldownRemaining = 0;     // Remaining cooldown time in seconds
    QString originalCategoryText = "";  // Original category text before cooldown
    int previousEventIndex = -1;        // Store previous event index for confirmation dialog
    static constexpr int DEFAULT_COOLDOWN_DURATION = 300;  // 5 minutes

   protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
};

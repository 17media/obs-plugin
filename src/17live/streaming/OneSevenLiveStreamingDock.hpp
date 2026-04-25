#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
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
class OneSevenLiveConfigManager;

class OneSevenLiveStreamingDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveStreamingDock(QWidget *parent = nullptr,
                                       OneSevenLiveStreamManager *streamManager = nullptr,
                                       OneSevenLiveApiWrappers *apiWrappers = nullptr,
                                       OneSevenLiveConfigManager *configManager = nullptr);
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
    void maybePromptObsAutoAdjust(bool allowSilentApply);

    /**
     * @brief Change event during streaming
     * @param eventID New event ID
     */
    void changeEvent(qint64 eventID);

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
    OneSevenLiveApiWrappers *apiWrapper = nullptr;

   private:
    // UI elements
    QLineEdit *titleEdit = nullptr;
    QComboBox *categoryCombo = nullptr;

    // Tag area
    QLineEdit *tagEdit = nullptr;
    QPushButton *addTagButton = nullptr;
    QWidget *tagsContainer = nullptr;   // Container for displaying tags
    QHBoxLayout *tagsLayout = nullptr;  // Layout for tag container
    QList<QString> tagsList;            // Store current tag list

    // Streaming format
    QRadioButton *landscapeStreamRadio = nullptr;
    QRadioButton *portraitStreamRadio = nullptr;

    // Live mode - army-only viewing
    QLabel *broadcastModeLabel = nullptr;
    QWidget *armyOnlyHeader = nullptr;
    QHBoxLayout *armyOnlyHeaderLayout = nullptr;
    QLabel *armyOnlyLabel = nullptr;
    QPushButton *armyOnlyToggleButton = nullptr;
    QWidget *armyOnlyContainer = nullptr;
    QVBoxLayout *armyOnlyContainerLayout = nullptr;
    QCheckBox *armyOnlyCheck = nullptr;
    QComboBox *requiredArmyRankCombo = nullptr;
    QCheckBox *showInHotPageCheck = nullptr;
    QCheckBox *liveNotificationCheck = nullptr;
    bool armyOnlyExpanded = false;

    QComboBox *eventCombo = nullptr;
    QLabel *hintLabel = nullptr;  // Event hint label

    // Custom Event
    QWidget *customEventHeader = nullptr;
    QHBoxLayout *customEventHeaderLayout = nullptr;
    QLabel *customEventLabel = nullptr;
    QPushButton *customEventToggleButton = nullptr;
    QPointer<OneSevenLiveCustomEventDialog> customEventDialog = nullptr;

    // Party Live
    QWidget *GroupCallContainer = nullptr;
    QHBoxLayout *GroupCallContainerLayout = nullptr;
    QLabel *GroupCallLabel = nullptr;
    QPushButton *GroupCallHelpButton = nullptr;
    QCheckBox *GroupCallCheck = nullptr;

    // Switches
    QCheckBox *archiveStreamCheck = nullptr;
    QCheckBox *autoPreviewCheck = nullptr;

    QComboBox *clipIdentityCombo = nullptr;
    QCheckBox *virtualStreamerCheck = nullptr;

    // Bottom buttons
    QPushButton *saveConfigButton = nullptr;
    QPushButton *createLiveButton = nullptr;

    // Loading state UI
    QWidget *loadingOverlay = nullptr;
    QProgressBar *loadingProgress = nullptr;
    QLabel *loadingLabel = nullptr;

    OneSevenLiveRoomInfo roomInfo;
    OneSevenLiveConfigStreamer configStreamer;
    OneSevenLiveUserInfo userInfo;
    OneSevenLiveArmySubscriptionLevels levels;

   signals:
    void streamInfoSaved();
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
    void startCreateLiveSequence(const OneSevenLiveRtmpRequest &request);
    void handleCreateLiveChecks(const OneSevenLiveLoginData &loginData,
                                const nlohmann::json &configJson, bool success,
                                const QString &error, const OneSevenLiveRtmpRequest &request_);

    // Tag-related functions
    void addTag(const QString &tag);
    void updateTagsFromList();

   private:
    int hashtagSelectLimit = 2;  // Maximum number of tags that can be added

    QPointer<OneSevenLiveStreamManager> streamManager = nullptr;
    OneSevenLiveConfigManager *configManager = nullptr;

    QString currentInfoUuid = "";
    // Loading state now controlled by OneSevenLiveStreamManager
    OneSevenLiveStreamingStatus currentLiveStatus = OneSevenLiveStreamingStatus::NotStarted;

    // Category change cooldown timer
    QTimer *eventCooldownTimer = nullptr;
    int eventCooldownRemaining = 0;     // Remaining cooldown time in seconds
    QString originalCategoryText = "";  // Original category text before cooldown
    int previousEventIndex = -1;        // Store previous event index for confirmation dialog
    static constexpr int DEFAULT_COOLDOWN_DURATION = 300;  // 5 minutes

    bool obsAutoAdjustPromptShown = false;

   protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
};

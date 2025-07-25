#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QWidget>

#include "api/OneSevenLiveModels.hpp"

class OneSevenLiveApiWrappers;

class OneSevenLiveConfigManager;

class OneSevenLiveStreamingDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveStreamingDock(QWidget *parent = nullptr,
                                       OneSevenLiveApiWrappers *apiWrapper = nullptr,
                                       OneSevenLiveConfigManager *configManager = nullptr);
    ~OneSevenLiveStreamingDock();

    void updateLiveStatus(OneSevenLiveStreamingStatus status);
    void createLiveWithRequest(const OneSevenLiveRtmpRequest &request);
    void editLiveWithInfo(const OneSevenLiveStreamInfo &info);
    void loadRoomInfo(qint64 roomID);

    void closeLive(const std::string &currUserID, const std::string &currLiveStreamID);

   private:
    void setupUi();
    void createConnections();
    void updateUIWithRoomInfo();
    void updateRequiredArmyRankSelections();
    void updateUIValues();

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

    QComboBox *activityCombo;
    QComboBox *customActivityCombo;
    QComboBox *viewerLimitCombo;

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

   private slots:
    void onAddTagClicked();
    void onTagEnterPressed();
    void onRemoveTagClicked();
    void onCreateLiveClicked();
    void onDeleteLiveClicked();
    void onSaveConfigClicked();
    void onArmyOnlyToggleClicked();          // New collapse/expand button click event
    void onArmyOnlyCheckChanged(int state);  // Triggered when armyOnlyCheck state changes

   private:
    bool gatherRtmpRequest(OneSevenLiveRtmpRequest &request);
    void populateRtmpRequest(const OneSevenLiveRtmpRequest &request);
    void updateLiveButton(bool isLive);

    void saveStreamingSettings(const std::string &liveStreamID, const std::string &streamUrl,
                               const std::string &streamKey);
    void stopStreaming();

    void createLive(const OneSevenLiveRtmpRequest &request);
    void startLive(const std::string userID, const OneSevenLiveRtmpResponse &response,
                   bool autoRecording, bool skip = false);

    void syncWithWeb(OneSevenLiveStreamingStatus status);

    // Tag-related functions
    void addTag(const QString &tag);
    void updateTagsFromList();
    int hashtagSelectLimit = 2;  // Maximum number of tags that can be added

    OneSevenLiveApiWrappers *apiWrapper = nullptr;
    OneSevenLiveConfigManager *configManager = nullptr;

    QString currentInfoUuid = "";
    bool isLoading = false;  // Indicates whether loading is in progress
    OneSevenLiveStreamingStatus currentLiveStatus = OneSevenLiveStreamingStatus::NotStarted;

   protected:
    void resizeEvent(QResizeEvent *event) override;
};

#pragma once

#include <QDockWidget>
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QProgressBar>

#include "api/OneSevenLiveModels.hpp"

class OneSevenLiveApiWrappers;

class OneSevenLiveConfigManager;

class OneSevenLiveStreamingDock : public QDockWidget {
    Q_OBJECT

public:
    explicit OneSevenLiveStreamingDock(QWidget *parent = nullptr, OneSevenLiveApiWrappers *apiWrapper = nullptr, OneSevenLiveConfigManager *configManager = nullptr);
    ~OneSevenLiveStreamingDock();

    void updateLiveStatus(OneSevenLiveStreamingStatus status);
    void createLiveWithRequest(const OneSevenLiveRtmpRequest &request);
    void editLiveWithInfo(const OneSevenLiveStreamInfo &info);
    void loadRoomInfo(qint64 roomID);

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
    
    // 标签区域
    QLineEdit *tagEdit;
    QPushButton *addTagButton;
    QWidget *tagsContainer; // 用于显示标签的容器
    QHBoxLayout *tagsLayout; // 标签容器的布局
    QList<QString> tagsList; // 存储当前的标签列表
    
    // 开播格式
    QRadioButton *normalStreamRadio;
    QRadioButton *verticalStreamRadio;

    // 直播模式 - 战队限定观看
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
    
    // 开关
    QCheckBox *archiveStreamCheck;
    QCheckBox *autoPreviewCheck;
    
    QComboBox *clipIdentityCombo;
    QCheckBox *virtualStreamerCheck;
    
    // 底部按钮
    QPushButton *saveConfigButton;
    QPushButton *createLiveButton;

    // 加载状态UI
    QWidget *loadingOverlay;
    QProgressBar *loadingProgress;
    QLabel *loadingLabel;

    OneSevenLiveRoomInfo roomInfo;
    OneSevenLiveConfigStreamer configStreamer;
    OneSevenLiveUserInfo userInfo;
    OneSevenLiveArmySubscriptionLevels levels;

signals:
    void streamInfoSaved();

private slots:
    void onAddTagClicked();
    void onTagEnterPressed();
    void onRemoveTagClicked();
    void onCreateLiveClicked();
    void onDeleteLiveClicked();
    void onSaveConfigClicked();
    void onArmyOnlyToggleClicked(); // 新增折叠/展开按钮点击事件
    void onArmyOnlyCheckChanged(int state); // armyOnlyCheck 状态改变时触发

private:
    bool gatherRtmpRequest(OneSevenLiveRtmpRequest &request);
    void populateRtmpRequest(const OneSevenLiveRtmpRequest &request);
    void updateLiveButton(bool isLive);

    void saveStreamingSettings(const std::string &liveStreamID, const std::string &streamUrl, const std::string &streamKey);
    void stopStreaming();

    void createLive(const OneSevenLiveRtmpRequest& request);
    void startLive(const std::string userID, const OneSevenLiveRtmpResponse &response, bool autoRecording, bool skip = false);
    void closeLive();
    void syncWithWeb(OneSevenLiveStreamingStatus status);
    
    // 标签相关函数
    void addTag(const QString &tag);
    void updateTagsFromList();
    int hashtagSelectLimit = 2; // 最多可以添加的标签数量

    OneSevenLiveApiWrappers *apiWrapper = nullptr;
    OneSevenLiveConfigManager *configManager = nullptr;

    QString currentInfoUuid = "";
    bool isLoading = false; // 标识是否正在加载中
    OneSevenLiveStreamingStatus currentLiveStatus = OneSevenLiveStreamingStatus::NotStarted;

protected:
    void resizeEvent(QResizeEvent *event) override;
};

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

#include "api/SeventeenLiveModels.hpp"

class SeventeenLiveApiWrappers;

class SeventeenLiveConfigManager;

class SeventeenLiveStreamingDock : public QDockWidget {
    Q_OBJECT

public:
    explicit SeventeenLiveStreamingDock(QWidget *parent = nullptr, SeventeenLiveApiWrappers *apiWrapper = nullptr, SeventeenLiveConfigManager *configManager = nullptr);
    ~SeventeenLiveStreamingDock();

    void updateLiveStatus(SeventeenLiveStreamingStatus status);
    void createLiveWithRequest(const SeventeenLiveRtmpRequest &request);
    void editLiveWithInfo(const SeventeenLiveStreamInfo &info);
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

    SeventeenLiveRoomInfo roomInfo;
    SeventeenLiveConfigStreamer configStreamer;
    SeventeenLiveUserInfo userInfo;
    SeventeenLiveArmySubscriptionLevels levels;

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
    bool gatherRtmpRequest(SeventeenLiveRtmpRequest &request);
    void populateRtmpRequest(const SeventeenLiveRtmpRequest &request);
    void updateLiveButton(bool isLive);

    void saveStreamingSettings(const std::string &liveStreamID, const std::string &streamUrl, const std::string &streamKey);
    void stopStreaming();

    void startStreaming(const SeventeenLiveRtmpRequest& request);
    
    // 标签相关函数
    void addTag(const QString &tag);
    void updateTagsFromList();
    int hashtagSelectLimit = 2; // 最多可以添加的标签数量

    SeventeenLiveApiWrappers *apiWrapper = nullptr;
    SeventeenLiveConfigManager *configManager = nullptr;

    QString currentInfoUuid = "";
    bool isLoading = false; // 标识是否正在加载中
    SeventeenLiveStreamingStatus currentLiveStatus = SeventeenLiveStreamingStatus::NotStarted;

protected:
    void resizeEvent(QResizeEvent *event) override;
};

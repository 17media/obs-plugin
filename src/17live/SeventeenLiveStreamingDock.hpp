#pragma once

#include <QDockWidget>
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QLabel>

#include "api/SeventeenLiveModels.hpp"

namespace seventeenlive {

class SeventeenLiveApiWrappers;

class SeventeenLiveConfigManager;

class SeventeenLiveStreamingDock : public QDockWidget {
    Q_OBJECT

public:
    explicit SeventeenLiveStreamingDock(QWidget *parent = nullptr, const SeventeenLiveRoomInfo &roomInfo = SeventeenLiveRoomInfo(), SeventeenLiveApiWrappers *apiWrapper = nullptr, SeventeenLiveConfigManager *configManager = nullptr);
    ~SeventeenLiveStreamingDock();

    void updateLiveStatus(SeventeenLiveStreamingStatus status);

private:
    void setupUi();
    void createConnections();

private:
    // UI elements
    QLineEdit *titleEdit;
    QComboBox *categoryCombo;
    
    // 标签区域
    QLineEdit *tagEdit;
    QPushButton *addTagButton;
    
    // 开播格式
    QRadioButton *normalStreamRadio;
    QRadioButton *verticalStreamRadio;
    
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

    SeventeenLiveRoomInfo roomInfo;

// signals:
//     void createStreamClicked(const SeventeenLiveRtmpRequest &request);
//     void createLiveClicked(const SeventeenLiveRtmpRequest &request);
//     void stopStreamingClicked();
//     void stopPushStreamingClicked();

private slots:
    void onAddTagClicked();
    void onCreateLiveClicked();
    void onDeleteLiveClicked();
    void onSaveConfigClicked();
    // void onStopPushStreamingClicked();

private:
    void gatherRtmpRequest(SeventeenLiveRtmpRequest &request);
    void updateLiveButton(bool isLive);

    void saveStreamingSettings(const std::string &liveStreamID, const std::string &streamUrl, const std::string &streamKey);
    void stopStreaming();

    SeventeenLiveApiWrappers *apiWrapper = nullptr;
    SeventeenLiveConfigManager *configManager = nullptr;
};

} // namespace seventeenlive

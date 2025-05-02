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

class SeventeenLiveStreamingDock : public QDockWidget {
    Q_OBJECT

public:
    explicit SeventeenLiveStreamingDock(QWidget *parent = nullptr, const SeventeenLiveRoomInfo &roomInfo = SeventeenLiveRoomInfo());
    ~SeventeenLiveStreamingDock();

    void updateStreamingStatus(SeventeenLiveStreamingStatus status);

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
    QPushButton *createStreamButton;
    QPushButton *createAndStartButton;

    SeventeenLiveRoomInfo roomInfo;

signals:
    void createStreamClicked(const SeventeenLiveRtmpRequest &request);
    void createAndStartStreamClicked(const SeventeenLiveRtmpRequest &request);
    void stopStreamingClicked();
    void stopPushStreamingClicked();

private slots:
    void onAddTagClicked();
    void onCreateStreamClicked();
    void onCreateAndStartStreamClicked();
    void onStopStreamingClicked();
    void onStopPushStreamingClicked();

private:
    void gatherRtmpRequest(SeventeenLiveRtmpRequest &request);
    void updateLiveButton(bool isLive);
    void updateStreamingButton(bool isStreaming);
};

} // namespace seventeenlive

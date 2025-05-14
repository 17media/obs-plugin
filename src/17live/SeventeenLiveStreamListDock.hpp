#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QPushButton>
#include <QDateTime>

#include "api/SeventeenLiveApiWrappers.hpp"
#include "SeventeenLiveConfigManager.hpp"

namespace seventeenlive {

struct SeventeenLiveRtmpRequest;

class SeventeenLiveStreamListDock : public QDockWidget {
    Q_OBJECT

public:
    struct StreamInfo {
        QString title;
        QString category;
        QDateTime startTime;
        QString streamId;
        SeventeenLiveRtmpRequest request;
    };

    SeventeenLiveStreamListDock(QWidget *parent, SeventeenLiveConfigManager *configManager_);
    ~SeventeenLiveStreamListDock();

    void refreshStreamList();

signals:
    void startLiveClicked(const SeventeenLiveRtmpRequest& request);

private slots:
    void onEditStreamClicked(QListWidgetItem* item, const StreamInfo& info);
    void onDeleteStreamClicked(QListWidgetItem* item, const StreamInfo& info);
    void onStartLiveClicked();
    
private:
    void setupUi();
    void createConnections();
    void updateStreamItem(QListWidgetItem* item, const StreamInfo& info);

    QListWidget *streamList;
    QPushButton *startLiveButton;
    SeventeenLiveApiWrappers *apiWrapper;
    SeventeenLiveConfigManager *configManager;
};

} // namespace seventeenlive

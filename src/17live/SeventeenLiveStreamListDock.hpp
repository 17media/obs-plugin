#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QPushButton>
#include <QDateTime>

#include "api/SeventeenLiveApiWrappers.hpp"
#include "SeventeenLiveConfigManager.hpp"

struct SeventeenLiveRtmpRequest;

struct SeventeenLiveStreamInfo;

class SeventeenLiveStreamListDock : public QDockWidget {
    Q_OBJECT

public:

    SeventeenLiveStreamListDock(QWidget *parent, SeventeenLiveConfigManager *configManager_);
    ~SeventeenLiveStreamListDock();

    void refreshStreamList();

signals:
    void startLiveClicked(const SeventeenLiveRtmpRequest& request);
    void editLiveClicked(const SeventeenLiveStreamInfo& info);

private slots:
    void onEditStreamClicked(QListWidgetItem* item, const SeventeenLiveStreamInfo& info);
    void onDeleteStreamClicked(QListWidgetItem* item, const SeventeenLiveStreamInfo& info);
    void onStartLiveClicked();
    
private:
    void setupUi();
    void createConnections();
    void updateStreamItem(QListWidgetItem* item, const SeventeenLiveStreamInfo& info);
    void showEmptyListMessage();

    QListWidget *streamList;
    QPushButton *startLiveButton;
    QWidget *emptyContainer = nullptr;
    SeventeenLiveApiWrappers *apiWrapper;
    SeventeenLiveConfigManager *configManager;
};

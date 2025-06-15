#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QPushButton>
#include <QDateTime>

#include "api/OneSevenLiveApiWrappers.hpp"
#include "OneSevenLiveConfigManager.hpp"

struct OneSevenLiveRtmpRequest;

struct OneSevenLiveStreamInfo;

class OneSevenLiveStreamListDock : public QDockWidget {
    Q_OBJECT

public:

    OneSevenLiveStreamListDock(QWidget *parent, OneSevenLiveConfigManager *configManager_);
    ~OneSevenLiveStreamListDock();

    void refreshStreamList();

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void startLiveClicked(const OneSevenLiveRtmpRequest& request);
    void editLiveClicked(const OneSevenLiveStreamInfo& info);

private slots:
    void onEditStreamClicked(QListWidgetItem* item, const OneSevenLiveStreamInfo& info);
    void onDeleteStreamClicked(QListWidgetItem* item, const OneSevenLiveStreamInfo& info);
    void onStartLiveClicked();
    
private:
    void setupUi();
    void createConnections();
    void updateStreamItem(QListWidgetItem* item, const OneSevenLiveStreamInfo& info);
    void showEmptyListMessage();

    QListWidget *streamList;
    QPushButton *startLiveButton;
    QWidget *emptyContainer = nullptr;
    OneSevenLiveApiWrappers *apiWrapper;
    OneSevenLiveConfigManager *configManager;
};

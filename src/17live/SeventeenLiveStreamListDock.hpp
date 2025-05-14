#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QPushButton>
#include <QDateTime>

#include "api/SeventeenLiveApiWrappers.hpp"
#include "SeventeenLiveConfigManager.hpp"

namespace seventeenlive {

class SeventeenLiveStreamListDock : public QDockWidget {
    Q_OBJECT

public:
    struct StreamInfo {
        QString title;
        QString category;
        QDateTime startTime;
        QString streamId;
    };

    SeventeenLiveStreamListDock(QWidget *parent, SeventeenLiveConfigManager *configManager_);
    ~SeventeenLiveStreamListDock();

private slots:
    void onEditStreamClicked();
    void onDeleteStreamClicked();
    void onStartLiveClicked();
    void refreshStreamList();

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

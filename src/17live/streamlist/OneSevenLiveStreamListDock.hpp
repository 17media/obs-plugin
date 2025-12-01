#pragma once

// Qt includes (keep minimal in header to reduce transitive dependencies)
#include <QDockWidget>

#include "api/OneSevenLiveModels.hpp"

// Forward declarations to avoid heavy includes in header
class QListWidget;
class QListWidgetItem;
class QPushButton;

class OneSevenLiveConfigManager;

class OneSevenLiveStreamListDock : public QDockWidget {
    Q_OBJECT

   public:
    OneSevenLiveStreamListDock(
        QWidget* parent, OneSevenLiveConfigManager* configManager_,
        OneSevenLiveStreamingStatus status_ = OneSevenLiveStreamingStatus::NotStarted);
    ~OneSevenLiveStreamListDock();

    void refreshStreamList();

    void setStatus(OneSevenLiveStreamingStatus status_);

   protected:
    void resizeEvent(QResizeEvent* event) override;

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

        QListWidget* streamList = nullptr;
        QPushButton* startLiveButton = nullptr;
        QWidget* emptyContainer = nullptr;
        OneSevenLiveConfigManager* configManager = nullptr;

        QPushButton* goToStreamingButton = nullptr;

        OneSevenLiveStreamingStatus status;

   private slots:
    void handleTopLevelChanged(bool topLevel);
};

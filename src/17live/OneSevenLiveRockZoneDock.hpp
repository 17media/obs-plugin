#pragma once

#include <QDockWidget>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>

#include "OneSevenLiveUserDialog.hpp"
#include "api/OneSevenLiveModels.hpp"

class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;

class OneSevenLiveRockZoneDock : public QDockWidget {
    Q_OBJECT

   public:
    OneSevenLiveRockZoneDock(QWidget* parent = nullptr,
                             OneSevenLiveApiWrappers* apiWrapper = nullptr,
                             OneSevenLiveConfigManager* configManager = nullptr);
    ~OneSevenLiveRockZoneDock();

    void refreshUserList();

   protected:
    void resizeEvent(QResizeEvent* event) override;

   signals:
    void viewAllFriendsClicked();

   private slots:
    void onPokeAllClicked();
    void handleTopLevelChanged(bool topLevel);
    void onUserItemClicked(QListWidgetItem* item);

   private:
    void setupUi();
    void createConnections();
    void updateUserItem(QListWidgetItem* item, const OneSevenLiveRockZoneViewer& user,
                        const OneSevenLiveArmyNameResponse& armyNameResponse);
    void showEmptyListMessage();

    QListWidget* userList;
    QPushButton* pokeAllButton;
    QWidget* emptyContainer = nullptr;

    OneSevenLiveApiWrappers* apiWrapper = nullptr;
    OneSevenLiveConfigManager* configManager = nullptr;

    QList<OneSevenLiveRockZoneViewer> viewersList;

    // User information dialog
    OneSevenLiveUserDialog* userDialog = nullptr;

    // Loading status UI
    QWidget* loadingOverlay = nullptr;
    QProgressBar* loadingProgress = nullptr;
    QLabel* loadingLabel = nullptr;
    bool isLoading = false;  // Indicates whether loading is in progress
};

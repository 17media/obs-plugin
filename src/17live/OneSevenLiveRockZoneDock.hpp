#pragma once

#include <QDockWidget>
#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>

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
    void clearArmyNameCache();

   protected:
    void resizeEvent(QResizeEvent* event) override;

   signals:
    void viewAllFriendsClicked();

   private slots:
    void onPokeAllClicked();
    void handleTopLevelChanged(bool topLevel);
    void onUserItemClicked(QListWidgetItem* item);
    void onCooldownTimerTimeout();

   private:
    void setupUi();
    void createConnections();
    void updateUserItem(QListWidgetItem* item, const OneSevenLiveRockZoneViewer& user,
                        const OneSevenLiveArmyNameResponse& armyNameResponse);

    QListWidget* userList;
    QPushButton* pokeAllButton;

    OneSevenLiveApiWrappers* apiWrapper = nullptr;
    OneSevenLiveConfigManager* configManager = nullptr;

    QList<OneSevenLiveRockZoneViewer> viewersList;
    QHash<QString, QListWidgetItem*> userItemMap;

    // Cached army name response to avoid repeated API calls
    OneSevenLiveArmyNameResponse cachedArmyNameResponse;
    bool armyNameCached = false;

    // User information dialog
    OneSevenLiveUserDialog* userDialog = nullptr;

    // Auto refresh timer
    QTimer* refreshTimer = nullptr;

    // Cooldown timer for poke all button
    QTimer* cooldownTimer = nullptr;
    int cooldownSeconds = 0;
    QString originalButtonText;
};

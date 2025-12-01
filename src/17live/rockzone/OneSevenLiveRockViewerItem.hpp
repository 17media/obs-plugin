#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSharedPointer>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include "api/OneSevenLiveModels.hpp"

// Forward declarations
class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;
class RemoteTextThread;

// Rock Zone viewer list item widget
class OneSevenLiveRockViewerItem : public QWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveRockViewerItem(const OneSevenLiveRockZoneViewer &user,
                                        OneSevenLiveApiWrappers *apiWrapper = nullptr,
                                        OneSevenLiveConfigManager *configManager = nullptr,
                                        const OneSevenLiveArmyNameResponse &armyNameResponse = {},
                                        QWidget *parent = nullptr);

    QSize sizeHint() const override;
    void updateData(const OneSevenLiveRockZoneViewer &user,
                    const OneSevenLiveArmyNameResponse &armyNameResponse);

   signals:
    void clicked(const OneSevenLiveRockZoneViewer &user);

   protected:
    void mousePressEvent(QMouseEvent *event) override;

   private:
    QLabel *usernameLabel = nullptr;
    QLabel *avatarLabel = nullptr;
    QLabel *pointsLabel = nullptr;
    QHBoxLayout *badgeRowLayout = nullptr;
    QHBoxLayout *nameRowLayout = nullptr;
    QVBoxLayout *rightLayout = nullptr;

    OneSevenLiveRockZoneViewer user;
    OneSevenLiveApiWrappers *apiWrapper = nullptr;
    OneSevenLiveConfigManager *configManager = nullptr;
    OneSevenLiveArmyNameResponse armyNameResponse;

    static QString buildUrl(const QString &path);
    void setupUi();
    void setupAvatar();
    QHBoxLayout *setupNameRow();
    QHBoxLayout *setupBadgeRow();
    QHBoxLayout *setupPointsRow();
    void reloadAvatar();
    void updateBadges();
    void updateNameRow();
};

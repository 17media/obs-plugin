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
    QLabel *usernameLabel;

    OneSevenLiveRockZoneViewer user;
    OneSevenLiveApiWrappers *apiWrapper;
    OneSevenLiveConfigManager *configManager;
    OneSevenLiveArmyNameResponse armyNameResponse;

    static QString buildUrl(const QString &path);
    void setupUi();
    QLabel *setupAvatar();
    QHBoxLayout *setupNameRow();
    QHBoxLayout *setupBadgeRow();
};

#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSharedPointer>
#include <QThread>

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
                                       QWidget *parent = nullptr);

    QSize sizeHint() const override;

signals:
    void clicked(const OneSevenLiveRockZoneViewer &user);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    OneSevenLiveRockZoneViewer user;
    OneSevenLiveApiWrappers *apiWrapper;
    OneSevenLiveConfigManager *configManager;

    static QString buildUrl(const QString &path);
    void setupUi();
};

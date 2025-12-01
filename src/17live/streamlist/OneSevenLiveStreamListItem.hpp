#pragma once

#include <QLabel>
#include <QPushButton>
#include <QWidget>

class OneSevenLiveStreamListItem : public QWidget {
    Q_OBJECT

   public:
        QLabel* titleLabel = nullptr;
        QLabel* contentLabel = nullptr;
        QLabel* timestampLabel = nullptr;
        QPushButton* editButton = nullptr;
        QPushButton* deleteButton = nullptr;

        OneSevenLiveStreamListItem(const QString& title, const QString& content,
                               const QString& timestamp, QWidget* parent = nullptr);

   signals:
    void editClicked();
    void deleteClicked();
};

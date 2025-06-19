#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>

class OneSevenLiveStreamListItem : public QWidget
{
    Q_OBJECT
    
public:
    QLabel* titleLabel;
    QLabel* contentLabel;
    QLabel* timestampLabel;
    QPushButton* editButton;
    QPushButton* deleteButton;

    OneSevenLiveStreamListItem(const QString& title, const QString& content, const QString& timestamp, QWidget* parent = nullptr);

signals:
    void editClicked();
    void deleteClicked();
};

#pragma once
#include <obs-module.h>
#include <QDockWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

class SeventeenLiveChatDock : public QDockWidget {
    Q_OBJECT

public:
    SeventeenLiveChatDock(QWidget *parent = nullptr);
    ~SeventeenLiveChatDock() = default;

    static void Register();

private slots:
    void onMessageReceived(const QString &username, const QString &message);
    void onClearChat();

private:
    void setupUi();
    void connectChatSignals();
    void disconnectChatSignals();

    QTextEdit *chatDisplay;
    QWidget *container;
    bool isConnected;
    QString currentStreamId;
};
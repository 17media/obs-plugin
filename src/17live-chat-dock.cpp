#include "17live-chat-dock.hpp"
#include <QDateTime>
#include <QScrollBar>
#include <obs-frontend-api.h>
#include "17live-api.hpp"

SeventeenLiveChatDock::SeventeenLiveChatDock(QWidget *parent)
    : QDockWidget(parent)
    , isConnected(false)
{
    setWindowTitle("17LIVE Chat");
    setupUi();
}

void SeventeenLiveChatDock::Register()
{
    auto mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!mainWindow)
        return;

    auto dock = new SeventeenLiveChatDock(mainWindow);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, dock);
}

void SeventeenLiveChatDock::setupUi()
{
    container = new QWidget(this);
    auto layout = new QVBoxLayout(container);

    chatDisplay = new QTextEdit(container);
    chatDisplay->setReadOnly(true);
    chatDisplay->setStyleSheet("QTextEdit { background-color: #2b2b2b; color: #ffffff; }");

    layout->addWidget(chatDisplay);
    layout->setContentsMargins(0, 0, 0, 0);

    setWidget(container);
    setMinimumWidth(300);
    setMinimumHeight(400);
}

void SeventeenLiveChatDock::onMessageReceived(const QString &username, const QString &message)
{
    if (!isConnected)
        return;

    QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss]");
    QString formattedMessage = QString("%1 %2: %3").arg(timestamp, username, message);

    chatDisplay->append(formattedMessage);

    // Auto-scroll to bottom
    QScrollBar *scrollBar = chatDisplay->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void SeventeenLiveChatDock::onClearChat()
{
    chatDisplay->clear();
}

void SeventeenLiveChatDock::connectChatSignals()
{
    if (isConnected)
        return;

    // TODO: Implement actual chat connection using 17live-api
    isConnected = true;
    chatDisplay->append("Connected to chat...");
}

void SeventeenLiveChatDock::disconnectChatSignals()
{
    if (!isConnected)
        return;

    // TODO: Implement actual chat disconnection using 17live-api
    isConnected = false;
    chatDisplay->append("Disconnected from chat...");
}
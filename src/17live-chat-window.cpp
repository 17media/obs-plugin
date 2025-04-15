#include "17live-chat-window.hpp"
#include <QVBoxLayout>
#include <QUrl>

SeventeenLiveChatWindow::SeventeenLiveChatWindow(const QString &chatUrl, QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("17LIVE Chat Room");
    setMinimumSize(400, 600);

    auto layout = new QVBoxLayout(this);
    webView = new QWebEngineView(this);
    webView->setUrl(QUrl(chatUrl));
    layout->addWidget(webView);

    setAttribute(Qt::WA_DeleteOnClose);
}
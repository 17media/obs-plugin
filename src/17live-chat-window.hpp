#pragma once

#include <QWidget>
#include <QWebEngineView>

class SeventeenLiveChatWindow : public QWidget {
    Q_OBJECT

public:
    explicit SeventeenLiveChatWindow(const QString &chatUrl, QWidget *parent = nullptr);

private:
    QWebEngineView *webView;
};
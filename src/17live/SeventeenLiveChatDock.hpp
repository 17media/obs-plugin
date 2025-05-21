#pragma once

#include <QDockWidget>
#include <QScopedPointer>

class QCefWidget;

class SeventeenLiveChatDock : public QDockWidget {
    Q_OBJECT

public:
    explicit SeventeenLiveChatDock(QWidget* parent = nullptr, int port = 0);
    ~SeventeenLiveChatDock() override;

private:
    void initializeWebEngine(const QString &htmlPath);

private:
    QScopedPointer<QCefWidget> webView;
    int port = 0;
};

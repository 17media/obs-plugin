#pragma once

#include <QDockWidget>
#include <QVBoxLayout>
#include <memory>

class QWidget;

class QCefWidget;

class SeventeenLiveChatDock : public QDockWidget {
    Q_OBJECT

public:
    explicit SeventeenLiveChatDock(QWidget* parent = nullptr);
    ~SeventeenLiveChatDock();

private:
    void setupUi();
    void initializeWebEngine();

private:
    QWidget* containerWidget = nullptr;
    QVBoxLayout* layout = nullptr;
    std::unique_ptr<QCefWidget> webView;
};

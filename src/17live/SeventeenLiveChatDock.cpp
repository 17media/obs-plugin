#include "SeventeenLiveChatDock.hpp"
#include "browser/QCefWidget.hpp"
#include <obs-module.h>

SeventeenLiveChatDock::SeventeenLiveChatDock(QWidget* parent, int port_)
    : QDockWidget(obs_module_text("ChatRoom.Title"), parent), port(port_)
{
    setAttribute(Qt::WA_NativeWindow);  // 有利于嵌入 CEF 子窗口
    webView.reset(new QCefWidget(this));
    setWidget(webView.data());  // 设置为 dock 的主控件
    initializeWebEngine(QString("http://localhost:%1/chat").arg(QString::number(port)));
}

SeventeenLiveChatDock::~SeventeenLiveChatDock() = default;

void SeventeenLiveChatDock::initializeWebEngine(const QString &htmlPath)
{
    if (webView)
        webView->loadUrl(htmlPath);
}

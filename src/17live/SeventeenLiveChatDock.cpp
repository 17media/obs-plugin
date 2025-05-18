#include "SeventeenLiveChatDock.hpp"
#include <QUrl>
#include "plugin-support.h"
#include "qt-wrappers.hpp"
#include "browser/QCefWidget.hpp"
#include <obs-module.h>
SeventeenLiveChatDock::SeventeenLiveChatDock(QWidget* parent)
    : QDockWidget(tr("留言"), parent)
{
    setupUi();
}

SeventeenLiveChatDock::~SeventeenLiveChatDock() = default;

void SeventeenLiveChatDock::setupUi()
{
    containerWidget = new QWidget(this);
    layout = new QVBoxLayout(containerWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    setWidget(containerWidget);
    
    initializeWebEngine();
}

void SeventeenLiveChatDock::initializeWebEngine()
{
    // 创建CEF视图
    webView = std::make_unique<QCefWidget>(containerWidget);
    
    // 获取html文件的路径
    // QString htmlPath = QString("file:///%1/html/chat/index.html").arg(obs_get_module_data_path(obs_current_module()));
    QString htmlPath = QString("http://localhost:3000");
    // obs_log(LOG_INFO, "htmlPath: %s", htmlPath.toStdString().c_str());
    
    // 加载html文件
    webView->loadUrl(htmlPath);

    // 设置CEF视图的大小策略为可扩展，以确保它能填满布局
    webView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 将CEF视图添加到布局中
    layout->addWidget(webView.get());
}

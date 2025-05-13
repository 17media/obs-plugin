#include "SeventeenLiveChatDock.hpp"
#include <QUrl>
#include "plugin-support.h"
#include "qt-wrappers.hpp"
#include "browser/QCefWidget.hpp"
#include <obs-module.h>

namespace seventeenlive {

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
    QString htmlPath = QString("file:///%1/17live/html/chat/index.html").arg(obs_get_module_data_path(obs_current_module()));
    
    // 加载html文件
    webView->loadUrl(htmlPath);
    
    // 将CEF视图添加到布局中
    layout->addWidget(webView.get());
}

} // namespace seventeenlive

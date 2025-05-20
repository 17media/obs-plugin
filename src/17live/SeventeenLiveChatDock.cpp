#include "SeventeenLiveChatDock.hpp"
#include <QUrl>
#include "plugin-support.h"
#include "qt-wrappers.hpp"
#include "browser/QCefWidget.hpp"
#include <obs-module.h>
SeventeenLiveChatDock::SeventeenLiveChatDock(QWidget* parent, int port_)
    : QDockWidget(tr("留言"), parent), port(port_)
{
    setupUi();
}

SeventeenLiveChatDock::~SeventeenLiveChatDock() = default;

void SeventeenLiveChatDock::setupUi()
{
    // No need for a containerWidget or layout if webView will be the central widget.
    initializeWebEngine("http://localhost:" + QString::number(port));
}

void SeventeenLiveChatDock::initializeWebEngine(const QString &htmlPath)
{
    if (!webView) {
        webView = new QCefWidget(this);
        webView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setWidget(webView); // Set webView as the central widget of the QDockWidget
    }
    webView->loadUrl(htmlPath);

}

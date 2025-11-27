#include "OneSevenLiveChatRelayWidget.hpp"

#include <obs-module.h>

#include <QUrl>

#include "plugin-support.h"
#include "utility/QCefView.hpp"

OneSevenLiveChatRelayWidget::OneSevenLiveChatRelayWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("OneSevenLiveChatRelayWidget");
    setAttribute(Qt::WA_NativeWindow, true);
    cefView_ = new QCefView(this);
    cefView_->setVisible(false);
    setVisible(false);
    resize(1, 1);
}

OneSevenLiveChatRelayWidget::~OneSevenLiveChatRelayWidget() {}

void OneSevenLiveChatRelayWidget::startRelay(const QString& roomID, int httpPort, int wsPort) {
    if (roomID.isEmpty() || httpPort <= 0 || wsPort <= 0)
        return;
    const QString wsRaw = QString("ws://127.0.0.1:%1").arg(wsPort);
    const QString wsParam = QString::fromUtf8(QUrl::toPercentEncoding(wsRaw));
    const QString url = QString("http://localhost:%1/?roomID=%2&ws=%3")
                            .arg(QString::number(httpPort))
                            .arg(roomID)
                            .arg(wsParam);
    obs_log(LOG_INFO, "Starting chat relay: %s", url.toStdString().c_str());
    cefView_->loadUrl(url);
}

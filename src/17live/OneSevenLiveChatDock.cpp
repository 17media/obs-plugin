#include "OneSevenLiveChatDock.hpp"

#include <obs-module.h>

#include <QVBoxLayout>

#include "utility/QCefView.hpp"
#include "moc_OneSevenLiveChatDock.cpp"

OneSevenLiveChatDock::OneSevenLiveChatDock(QWidget* parent, const QString& chatUrl)
    : QDockWidget(obs_module_text("ChatRoom.Title"), parent), chatUrl_(chatUrl) {
    cefView_ = new QCefView(this);
    setWidget(cefView_);
    if (!chatUrl_.isEmpty()) {
        cefView_->loadUrl(chatUrl_);
    }
}

OneSevenLiveChatDock::~OneSevenLiveChatDock() {}

void OneSevenLiveChatDock::setUrl(const QString& url) {
    chatUrl_ = url;
    if (cefView_) {
        cefView_->loadUrl(chatUrl_);
    }
}

void OneSevenLiveChatDock::reload() {
    if (cefView_) {
        cefView_->reload();
    }
}
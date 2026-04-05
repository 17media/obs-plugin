#include "OneSevenLiveAuthDialog.hpp"

#include <obs-module.h>

#include <QDialog>
#include <QString>

#include "../../plugin-support.h"
#include "cef/CefWidgetHost.hpp"
#include "moc_OneSevenLiveAuthDialog.cpp"

OneSevenLiveAuthDialog::OneSevenLiveAuthDialog(QWidget* parent)
    : QDialog(parent) {
    setupUi();
}

OneSevenLiveAuthDialog::OneSevenLiveAuthDialog(const QString& url, QWidget* parent)
    : QDialog(parent) {
    setupUi();
    setUrl(url);
}

OneSevenLiveAuthDialog::~OneSevenLiveAuthDialog() {
    obs_log(LOG_INFO, "OneSevenLiveAuthDialog: destructor");
    cefHost_.reset();
}

void OneSevenLiveAuthDialog::setupUi() {
    setWindowTitle("Authorization");
    setModal(true);
    resize(800, 600);

    cefHost_ = std::make_unique<CefWidgetHost>();
    cefHost_->setCookieStorage("onesevenlive-auth", false);
    if (cefHost_->ensureCreated(this, "about:blank")) {
        if (auto* w = cefHost_->widget()) {
            connect(w, SIGNAL(urlChanged(const QString&)), this, SIGNAL(urlChanged(const QString&)));
            w->show();
        }
    } else {
        obs_log(LOG_ERROR, "OneSevenLiveAuthDialog: Failed to create browser widget");
    }
}

void OneSevenLiveAuthDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    if (cefHost_ && cefHost_->widget()) {
        cefHost_->widget()->setGeometry(rect());
    }
}

void OneSevenLiveAuthDialog::setUrl(const QString& url) {
    if (cefHost_) {
        cefHost_->setUrl(url);
    }
}

void OneSevenLiveAuthDialog::accept() {
    if (cefHost_) {
        cefHost_->release(true);
    }
    QDialog::accept();
}

void OneSevenLiveAuthDialog::reject() {
    if (cefHost_) {
        cefHost_->release(true);
    }
    QDialog::reject();
}

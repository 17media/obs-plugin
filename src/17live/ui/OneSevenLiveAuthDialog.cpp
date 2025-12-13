// OneSevenLiveAuthDialog: embed QCefView to show external URL and re-emit URL changes
#include "OneSevenLiveAuthDialog.hpp"

#include <obs-module.h>

#include <QDialog>
#include <QString>
// #include <QVBoxLayout>

#include "../../plugin-support.h"
#include "../chat/cef_panel.hpp"
#include "moc_OneSevenLiveAuthDialog.cpp"

OneSevenLiveAuthDialog::OneSevenLiveAuthDialog(QWidget* parent)
    : QDialog(parent), cef_(nullptr), cefWidget_(nullptr) {
    setupUi();
}

OneSevenLiveAuthDialog::OneSevenLiveAuthDialog(const QString& url, QWidget* parent)
    : QDialog(parent), cef_(nullptr), cefWidget_(nullptr) {
    setupUi();
    setUrl(url);
}

OneSevenLiveAuthDialog::~OneSevenLiveAuthDialog() {
    obs_log(LOG_INFO, "OneSevenLiveAuthDialog: destructor");
}

void OneSevenLiveAuthDialog::setupUi() {
    setWindowTitle("Authorization");
    setModal(true);
    resize(800, 600);

    // QDialog is typically already a native window.
    // Explicitly setting WA_NativeWindow might be redundant or cause issues with child native
    // widgets. setAttribute(Qt::WA_NativeWindow);

    cef_ = obs_browser_init_panel();
    if (cef_) {
        cef_->init_browser();
        // Initialize with about:blank; real URL set via setUrl()
        cefWidget_ = cef_->create_widget(this, "about:blank");
        if (cefWidget_) {
            cefWidget_->show();

            // Connect urlChanged signal dynamically since QCefWidget interface doesn't expose it
            // but the underlying implementation (obs-browser panel) does.
            connect(cefWidget_, SIGNAL(urlChanged(const QString&)), this,
                    SIGNAL(urlChanged(const QString&)));
        }
    } else {
        obs_log(LOG_ERROR, "OneSevenLiveAuthDialog: Failed to initialize obs-browser panel");
    }
}

void OneSevenLiveAuthDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    if (cefWidget_) {
        cefWidget_->setGeometry(rect());
    }
}

void OneSevenLiveAuthDialog::setUrl(const QString& url) {
    if (cefWidget_) {
        cefWidget_->setURL(url.toStdString());
    }
}

void OneSevenLiveAuthDialog::accept() {
    if (cefWidget_) {
        delete cefWidget_;
        cefWidget_ = nullptr;
    }
    QDialog::accept();
}

void OneSevenLiveAuthDialog::reject() {
    if (cefWidget_) {
        delete cefWidget_;
        cefWidget_ = nullptr;
    }
    QDialog::reject();
}

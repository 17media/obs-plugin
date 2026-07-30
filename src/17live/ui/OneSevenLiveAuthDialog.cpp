#include "OneSevenLiveAuthDialog.hpp"

#include <obs-module.h>

#include <QAbstractButton>
#include <QDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include "../../plugin-support.h"
#include "../chat/cef_panel.hpp"
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
    cleanupBrowser();

    if (cookieManager_) {
        cookieManager_->FlushStore();
        delete cookieManager_;
        cookieManager_ = nullptr;
    }
}

void OneSevenLiveAuthDialog::setupUi() {
    setWindowTitle("Authorization");
    setModal(true);
    setMinimumSize(400, 400);
    resize(700, 700);

    Qt::WindowFlags flags = windowFlags();
    Qt::WindowFlags helpFlag = Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags & (~helpFlag));

    cef_ = obs_browser_init_panel();
    if (!cef_) {
        obs_log(LOG_ERROR, "OneSevenLiveAuthDialog: obs-browser panel is not available");
        return;
    }

    if (!cef_->initialized()) {
        cef_->init_browser();
        cef_->wait_for_browser_init();
    }

    cookieManager_ = cef_->create_cookie_manager("onesevenlive-auth", false);
    QCefWidget* widget = cef_->create_widget(nullptr, "about:blank", cookieManager_);
    if (!widget) {
        obs_log(LOG_ERROR, "OneSevenLiveAuthDialog: Failed to create browser widget");
        return;
    }

    cefWidget_.reset(widget);

    connect(cefWidget_.data(), SIGNAL(urlChanged(const QString&)), this,
            SIGNAL(urlChanged(const QString&)));

    QPushButton* close = new QPushButton(tr("Cancel"));
    connect(close, &QAbstractButton::clicked, this, &QDialog::reject);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    bottomLayout->addWidget(close);
    bottomLayout->addStretch();

    QVBoxLayout* topLayout = new QVBoxLayout(this);
    topLayout->addWidget(cefWidget_.data());
    topLayout->addLayout(bottomLayout);
}

void OneSevenLiveAuthDialog::setUrl(const QString& url) {
    if (cefWidget_) {
        cefWidget_->setURL(url.toStdString());
    }
}

void OneSevenLiveAuthDialog::accept() {
    cleanupBrowser();
    QDialog::accept();
}

void OneSevenLiveAuthDialog::reject() {
    cleanupBrowser();
    QDialog::reject();
}

void OneSevenLiveAuthDialog::cleanupBrowser() {
    if (cefWidget_) {
        cefWidget_.reset(nullptr);
    }
}

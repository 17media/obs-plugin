// OneSevenLiveAuthDialog: embed QCefView to show external URL and re-emit URL changes
#include "OneSevenLiveAuthDialog.hpp"

#include <obs-module.h>

#include <QDialog>
#include <QString>
#include <QVBoxLayout>

#include "../../plugin-support.h"
#include "../utility/QCefView.hpp"
#include "moc_OneSevenLiveAuthDialog.cpp"

OneSevenLiveAuthDialog::OneSevenLiveAuthDialog(QWidget* parent)
    : QDialog(parent), cefView_(nullptr) {
    setupUi();
}

OneSevenLiveAuthDialog::OneSevenLiveAuthDialog(const QString& url, QWidget* parent)
    : QDialog(parent), cefView_(nullptr) {
    setupUi();
    setUrl(url);
}

OneSevenLiveAuthDialog::~OneSevenLiveAuthDialog() {}

void OneSevenLiveAuthDialog::setupUi() {
    setWindowTitle("Authorization");
    setModal(true);
    resize(800, 600);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    cefView_ = new QCefView(this);
    layout->addWidget(cefView_);

    // Forward URL changes to external listeners
    connect(cefView_, &QCefView::urlChanged, this, &OneSevenLiveAuthDialog::urlChanged);
}

void OneSevenLiveAuthDialog::setUrl(const QString& url) {
    if (!cefView_) {
        setupUi();
    }
    cefView_->loadUrl(url);
}

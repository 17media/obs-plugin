#include "OneSevenLiveChatDock.hpp"

#include <obs-module.h>

#include <QVBoxLayout>
#include <QMessageBox>

#include "utility/QCefView.hpp"
#include "moc_OneSevenLiveChatDock.cpp"

OneSevenLiveChatDock::OneSevenLiveChatDock(QWidget* parent, const QString& chatUrl)
    : QDockWidget(obs_module_text("ChatRoom.Title"), parent), chatUrl_(chatUrl) {
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                QDockWidget::DockWidgetClosable);
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

void OneSevenLiveChatDock::closeEvent(QCloseEvent* event) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(obs_module_text("Live.Common.Notice"));
    msgBox.setText(obs_module_text("ChatDock.Close.Warning"));
    msgBox.addButton(obs_module_text("Live.Settings.Yes"), QMessageBox::AcceptRole);
    QPushButton* cancelButton = msgBox.addButton(obs_module_text("Live.Settings.No"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(cancelButton);
    msgBox.exec();
    if (msgBox.buttonRole(msgBox.clickedButton()) != QMessageBox::AcceptRole) {
        event->ignore();
        return;
    }
    QDockWidget::closeEvent(event);
}

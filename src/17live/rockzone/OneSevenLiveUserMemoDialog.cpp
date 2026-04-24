#include "OneSevenLiveUserMemoDialog.hpp"

#include <obs-module.h>

#include <QHBoxLayout>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QScrollBar>
#include <QVBoxLayout>

#include "api/OneSevenLiveApiWrappers.hpp"
#include "plugin-support.h"

namespace {
    constexpr int kMaxMemoChars = 500;
}

OneSevenLiveUserMemoDialog::OneSevenLiveUserMemoDialog(QWidget* parent,
                                                       OneSevenLiveApiWrappers* apiWrapper_,
                                                       const QString& userID_)
    : QDialog(parent), apiWrapper(apiWrapper_), userID(userID_) {
    setWindowTitle(QString());
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedSize(340, 380);
    setModal(true);

    setupUi();
    loadUserNoteAsync();
}

OneSevenLiveUserMemoDialog::~OneSevenLiveUserMemoDialog() = default;

void OneSevenLiveUserMemoDialog::setupUi() {
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QWidget* card = new QWidget(this);
    card->setObjectName("card");
    card->setStyleSheet("#card { background-color: #272A33; border-radius: 8px; }");

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    cardLayout->setSpacing(12);

    titleLabel = new QLabel(obs_module_text("RockZone.UserMemo.Title"), card);
    titleLabel->setStyleSheet(
        "QLabel { color: white; font-weight: 700; font-size: 20px; line-height: 28px; }");
    cardLayout->addWidget(titleLabel, 0, Qt::AlignLeft);

    descLabel = new QLabel(obs_module_text("RockZone.UserMemo.Description"), card);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("QLabel { color: #B9BCC3; font-size: 14px; line-height: 20px; }");
    cardLayout->addWidget(descLabel);

    memoEdit = new QTextEdit(card);
    memoEdit->setAcceptRichText(false);
    memoEdit->setStyleSheet(
        "QTextEdit {"
        "  background: transparent;"
        "  color: #E6E6E6;"
        "  border: 0.5px solid #757575;"
        "  font-size: 14px;"
        "  line-height: 20px;"
        "}"
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 10px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: rgba(255,255,255,0.18);"
        "  border-radius: 5px;"
        "  min-height: 24px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: transparent;"
        "}");
    memoEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    memoEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    memoEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    cardLayout->addWidget(memoEdit, 1);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 8, 0, 0);
    buttonLayout->setSpacing(12);

    cancelButton = new QPushButton(obs_module_text("RockZone.UserMemo.Cancel"), card);
    cancelButton->setFixedHeight(44);
    cancelButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #5A5E6A;"
        "  color: white;"
        "  border-radius: 6px;"
        "  font-weight: 600;"
        "  font-size: 16px;"
        "}"
        "QPushButton:hover { background-color: #4F535E; }"
        "QPushButton:pressed { background-color: #444751; }");

    saveButton = new QPushButton(obs_module_text("RockZone.UserMemo.Save"), card);
    saveButton->setFixedHeight(44);
    saveButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #FF0001;"
        "  color: white;"
        "  border-radius: 6px;"
        "  font-weight: 600;"
        "  font-size: 16px;"
        "}"
        "QPushButton:hover { background-color: #D10001; }"
        "QPushButton:pressed { background-color: #B00001; }");

    buttonLayout->addWidget(cancelButton, 1);
    buttonLayout->addWidget(saveButton, 1);
    cardLayout->addLayout(buttonLayout);

    rootLayout->addWidget(card);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveButton, &QPushButton::clicked, this,
            [this]() { saveUserNoteAsync(memoEdit ? memoEdit->toPlainText() : QString()); });
    connect(memoEdit, &QTextEdit::textChanged, this, [this]() { enforceTextLimit(); });
}

void OneSevenLiveUserMemoDialog::loadUserNoteAsync() {
    if (!apiWrapper || userID.isEmpty()) {
        return;
    }

    saveButton->setEnabled(false);

    QThread* workerThread = new QThread();
    QPointer<OneSevenLiveUserMemoDialog> safeThis = this;
    const QString targetUserID = userID;

    connect(workerThread, &QThread::started, [=]() {
        OneSevenLiveUserNote note;
        const bool success = apiWrapper->GetUserNote(targetUserID.toStdString(), note);
        const QString errorMessage = apiWrapper->getLastErrorMessage();

        QMetaObject::invokeMethod(
            safeThis,
            [=]() {
                if (!safeThis) {
                    return;
                }

                safeThis->saveButton->setEnabled(true);

                if (success) {
                    safeThis->suppressTextChanged = true;
                    safeThis->memoEdit->setPlainText(note.content);
                    safeThis->suppressTextChanged = false;
                    safeThis->enforceTextLimit();
                } else if (!errorMessage.isEmpty()) {
                    QMessageBox::warning(safeThis, obs_module_text("RockZone.UserMemo.Error.Title"),
                                         obs_module_text("RockZone.UserMemo.LoadFailed") +
                                             QString("\n%1").arg(errorMessage));
                }
            },
            Qt::QueuedConnection);

        workerThread->quit();
    });

    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    workerThread->start();
}

void OneSevenLiveUserMemoDialog::saveUserNoteAsync(const QString& content) {
    if (!apiWrapper || userID.isEmpty()) {
        return;
    }

    if (content.size() > kMaxMemoChars) {
        QMessageBox::warning(this, obs_module_text("RockZone.UserMemo.Error.Title"),
                             obs_module_text("RockZone.UserMemo.CharLimitExceeded"));
        return;
    }

    cancelButton->setEnabled(false);
    saveButton->setEnabled(false);

    QThread* workerThread = new QThread();
    QPointer<OneSevenLiveUserMemoDialog> safeThis = this;
    const QString targetUserID = userID;
    const QString payload = content;

    connect(workerThread, &QThread::started, [=]() {
        const bool success = apiWrapper->SetUserNote(targetUserID.toStdString(), payload);
        const QString errorMessage = apiWrapper->getLastErrorMessage();

        QMetaObject::invokeMethod(
            safeThis,
            [=]() {
                if (!safeThis) {
                    return;
                }

                safeThis->cancelButton->setEnabled(true);
                safeThis->saveButton->setEnabled(true);

                if (success) {
                    safeThis->accept();
                    return;
                }

                QString message = obs_module_text("RockZone.UserMemo.SaveFailed");
                if (!errorMessage.isEmpty()) {
                    message += QString("\n%1").arg(errorMessage);
                }
                QMessageBox::warning(safeThis, obs_module_text("RockZone.UserMemo.Error.Title"),
                                     message);
            },
            Qt::QueuedConnection);

        workerThread->quit();
    });

    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    workerThread->start();
}

void OneSevenLiveUserMemoDialog::enforceTextLimit() {
    if (suppressTextChanged || !memoEdit) {
        return;
    }

    const QString text = memoEdit->toPlainText();
    if (text.size() <= kMaxMemoChars) {
        return;
    }

    suppressTextChanged = true;
    QTextCursor cursor = memoEdit->textCursor();
    const int cursorPos = cursor.position();

    memoEdit->setPlainText(text.left(kMaxMemoChars));

    QTextCursor newCursor = memoEdit->textCursor();
    newCursor.setPosition(qMin(cursorPos, kMaxMemoChars));
    memoEdit->setTextCursor(newCursor);
    suppressTextChanged = false;
}

void OneSevenLiveUserMemoDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        dragStartPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void OneSevenLiveUserMemoDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - dragStartPosition);
        event->accept();
    }
}

void OneSevenLiveUserMemoDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        event->accept();
    }
}

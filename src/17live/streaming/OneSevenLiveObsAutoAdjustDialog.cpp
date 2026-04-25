#include "OneSevenLiveObsAutoAdjustDialog.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "plugin-support.h"

OneSevenLiveObsAutoAdjustDialog::PromptResult OneSevenLiveObsAutoAdjustDialog::ShowPrompt(
    QWidget* parent, const QString& message, bool dontRemindDefault) {
    OneSevenLiveObsAutoAdjustDialog dialog(parent, Mode::Prompt);
    dialog.setupUiPrompt(message, dontRemindDefault);
    dialog.exec();
    PromptResult result;
    result.confirmed = dialog.result() == QDialog::Accepted;
    result.dontRemind = dialog.dontRemindCheck ? dialog.dontRemindCheck->isChecked() : false;
    return result;
}

void OneSevenLiveObsAutoAdjustDialog::ShowError(QWidget* parent, const QString& message) {
    OneSevenLiveObsAutoAdjustDialog dialog(parent, Mode::Error);
    dialog.setupUiError(message);
    dialog.exec();
}

OneSevenLiveObsAutoAdjustDialog::OneSevenLiveObsAutoAdjustDialog(QWidget* parent, Mode)
    : QDialog(parent) {
    setWindowTitle(QString());
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setModal(true);
}

void OneSevenLiveObsAutoAdjustDialog::setupUiPrompt(const QString& message, bool dontRemindDefault) {
    setFixedSize(380, 320);

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QWidget* card = new QWidget(this);
    card->setObjectName("card");
    card->setStyleSheet("#card { background-color: #272A33; border-radius: 8px; }");

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 20, 24, 16);
    cardLayout->setSpacing(14);

    messageLabel = new QLabel(message, card);
    messageLabel->setWordWrap(true);
    messageLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    messageLabel->setStyleSheet("QLabel { color: white; font-size: 16px; line-height: 22px; }");
    cardLayout->addWidget(messageLabel, 1);

    QHBoxLayout* checkRow = new QHBoxLayout();
    checkRow->setContentsMargins(0, 0, 0, 0);
    checkRow->addStretch();

    dontRemindCheck = new QCheckBox(obs_module_text("Live.Settings.AutoAdjust.DontRemind"), card);
    dontRemindCheck->setChecked(dontRemindDefault);
    dontRemindCheck->setStyleSheet(
        "QCheckBox { color: white; font-size: 14px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; }"
        "QCheckBox::indicator:unchecked { background-color: transparent; border: 2px solid #6B6F7B; border-radius: 4px; }"
        "QCheckBox::indicator:checked { background-color: #1877F2; border: 2px solid #1877F2; border-radius: 4px; }");
    checkRow->addWidget(dontRemindCheck);
    checkRow->addStretch();
    cardLayout->addLayout(checkRow);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 6, 0, 0);
    buttonLayout->setSpacing(14);

    cancelButton = new QPushButton(obs_module_text("Live.EventChange.Confirm.Cancel"), card);
    cancelButton->setFixedHeight(44);
    cancelButton->setStyleSheet(
        "QPushButton { background-color: #5A5E6A; color: white; border-radius: 6px; font-weight: 600; font-size: 16px; }"
        "QPushButton:hover { background-color: #4F535E; }"
        "QPushButton:pressed { background-color: #444751; }");

    confirmButton = new QPushButton(obs_module_text("Live.EventChange.Confirm.Confirm"), card);
    confirmButton->setFixedHeight(44);
    confirmButton->setStyleSheet(
        "QPushButton { background-color: #FF0001; color: white; border-radius: 6px; font-weight: 600; font-size: 16px; }"
        "QPushButton:hover { background-color: #D10001; }"
        "QPushButton:pressed { background-color: #B00001; }");

    buttonLayout->addWidget(cancelButton, 1);
    buttonLayout->addWidget(confirmButton, 1);
    cardLayout->addLayout(buttonLayout);

    rootLayout->addWidget(card);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirmButton, &QPushButton::clicked, this, &QDialog::accept);
}

void OneSevenLiveObsAutoAdjustDialog::setupUiError(const QString& message) {
    setFixedSize(380, 220);

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QWidget* card = new QWidget(this);
    card->setObjectName("card");
    card->setStyleSheet("#card { background-color: #272A33; border-radius: 8px; }");

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 28, 24, 22);
    cardLayout->setSpacing(22);

    messageLabel = new QLabel(message, card);
    messageLabel->setWordWrap(true);
    messageLabel->setAlignment(Qt::AlignCenter);
    messageLabel->setStyleSheet("QLabel { color: white; font-size: 16px; line-height: 22px; }");
    cardLayout->addWidget(messageLabel, 1);

    confirmButton = new QPushButton(obs_module_text("Live.EventChange.Confirm.Confirm"), card);
    confirmButton->setFixedSize(140, 44);
    confirmButton->setStyleSheet(
        "QPushButton { background-color: #1877F2; color: white; border-radius: 6px; font-weight: 600; font-size: 16px; }"
        "QPushButton:hover { background-color: #1668D7; }"
        "QPushButton:pressed { background-color: #135BC0; }");

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(confirmButton);
    btnRow->addStretch();
    cardLayout->addLayout(btnRow);

    rootLayout->addWidget(card);

    connect(confirmButton, &QPushButton::clicked, this, &QDialog::accept);
}

void OneSevenLiveObsAutoAdjustDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        dragStartPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void OneSevenLiveObsAutoAdjustDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - dragStartPosition);
        event->accept();
    }
}

void OneSevenLiveObsAutoAdjustDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        event->accept();
    }
}


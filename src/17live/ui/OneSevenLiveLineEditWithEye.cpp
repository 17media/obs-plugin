#include "OneSevenLiveLineEditWithEye.hpp"

#include <QHBoxLayout>
#include <QPointer>

#include "moc_OneSevenLiveLineEditWithEye.cpp"

OneSevenLiveLineEditWithEye::OneSevenLiveLineEditWithEye(QWidget* parent) : QWidget(parent) {
    QHBoxLayout* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Password input field
    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setEchoMode(QLineEdit::Password);
    m_lineEdit->setFixedHeight(40);
    m_lineEdit->setStyleSheet(
        "QLineEdit {"
        "    border: none;"
        "    border-radius: 2px 0 0 2px;"
        "    padding: 0 15px;"
        "}");

    // Show/hide password button
    m_eyeButton = new QPushButton(this);

    // Set initial icon to show password icon
    QIcon showIcon(":/resources/show-password.svg");
    m_eyeButton->setIcon(showIcon);
    m_eyeButton->setIconSize(QSize(20, 20));
    m_eyeButton->setFixedSize(40, 40);
    m_eyeButton->setStyleSheet(
        "QPushButton {"
        "    border: none;"
        "    border-radius: 0 2px 2px 0;"
        "    margin: 0;"
        "    padding: 0;"
        "}");

    rootLayout->addWidget(m_lineEdit);
    rootLayout->addWidget(m_eyeButton);
    rootLayout->setAlignment(m_lineEdit, Qt::AlignVCenter);
    rootLayout->setAlignment(m_eyeButton, Qt::AlignVCenter);

    // Connect button click event
    QPointer<QPushButton> safeButton(m_eyeButton);
    QPointer<QLineEdit> safeLineEdit(m_lineEdit);
    QPointer<OneSevenLiveLineEditWithEye> safeThis(this);
    connect(safeButton, &QPushButton::clicked, safeThis, [safeLineEdit, safeButton, safeThis]() {
        if (safeLineEdit->echoMode() == QLineEdit::Password) {
            safeLineEdit->setEchoMode(QLineEdit::Normal);
            // Switch to hide password icon
            QIcon hideIcon(":/resources/hide-password.svg");
            safeButton->setIcon(hideIcon);
        } else {
            safeLineEdit->setEchoMode(QLineEdit::Password);
            // Switch to show password icon
            QIcon showIcon(":/resources/show-password.svg");
            safeButton->setIcon(showIcon);
        }
    });
}

OneSevenLiveLineEditWithEye::~OneSevenLiveLineEditWithEye() {
    if (m_eyeButton) {
        disconnect(m_eyeButton, nullptr, this, nullptr);
    }
    if (m_lineEdit) {
        disconnect(m_lineEdit, nullptr, this, nullptr);
    }
    m_eyeButton = nullptr;
    m_lineEdit = nullptr;
}

QString OneSevenLiveLineEditWithEye::text() const {
    return m_lineEdit->text();
}

void OneSevenLiveLineEditWithEye::setText(const QString& text) {
    m_lineEdit->setText(text);
}

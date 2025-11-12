#include "OneSevenLiveLineEditWithEye.hpp"

#include <QHBoxLayout>

#include "moc_OneSevenLiveLineEditWithEye.cpp"

OneSevenLiveLineEditWithEye::OneSevenLiveLineEditWithEye(QWidget* parent)
    : QWidget(parent)
{
  // Password input field container
  QWidget* passwordContainer = new QWidget(this);
  QHBoxLayout* passwordLayout = new QHBoxLayout(passwordContainer);
  passwordLayout->setContentsMargins(0, 0, 0, 0);
  passwordLayout->setSpacing(0);

  // Password input field
  m_lineEdit = new QLineEdit(passwordContainer);
  m_lineEdit->setEchoMode(QLineEdit::Password);
  m_lineEdit->setFixedHeight(40);
  m_lineEdit->setStyleSheet(
      "QLineEdit {"
      "    border: none;"
      "    border-radius: 2px 0 0 2px;"
      "    padding: 0 15px;"
      "}");

  // Show/hide password button
  m_eyeButton = new QPushButton(passwordContainer);

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

  passwordLayout->addWidget(m_lineEdit);
  passwordLayout->addWidget(m_eyeButton);
  passwordLayout->setAlignment(m_lineEdit, Qt::AlignVCenter);
  passwordLayout->setAlignment(m_eyeButton, Qt::AlignVCenter);

  // Connect button click event
  connect(m_eyeButton, &QPushButton::clicked, this, [this]() {
    if (m_lineEdit->echoMode() == QLineEdit::Password) {
      m_lineEdit->setEchoMode(QLineEdit::Normal);
      // Switch to hide password icon
      QIcon hideIcon(":/resources/hide-password.svg");
      m_eyeButton->setIcon(hideIcon);
    } else {
      m_lineEdit->setEchoMode(QLineEdit::Password);
      // Switch to show password icon
      QIcon showIcon(":/resources/show-password.svg");
      m_eyeButton->setIcon(showIcon);
    }
  });
}

OneSevenLiveLineEditWithEye::~OneSevenLiveLineEditWithEye()
{
}

QString OneSevenLiveLineEditWithEye::text() const
{
  return m_lineEdit->text();
}

void OneSevenLiveLineEditWithEye::setText(const QString &text)
{
  m_lineEdit->setText(text);
}

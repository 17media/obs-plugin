#include "OneSevenLiveStreamListItem.hpp"

#include <QHBoxLayout>
#include <QHBoxLayout>

#include "moc_OneSevenLiveStreamListItem.cpp"

OneSevenLiveStreamListItem::OneSevenLiveStreamListItem(const QString& title, const QString& content, const QString& timestamp, QWidget *parent_) : QWidget(parent_) { 
  // 左边内容布局
  titleLabel = new QLabel(title);
  contentLabel = new QLabel(content);
  timestampLabel = new QLabel(timestamp);

  contentLabel->setWordWrap(true);

  titleLabel->setStyleSheet("font-family: Inter; font-weight: bold; font-size: 14px;");
  contentLabel->setStyleSheet("font-family: Inter; color: gray; font-size: 14px;");
  timestampLabel->setStyleSheet("font-family: Inter; color: gray; font-size: 14px;");

  titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  contentLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  timestampLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  QVBoxLayout* leftLayout = new QVBoxLayout;
  leftLayout->addWidget(titleLabel);
  leftLayout->addWidget(contentLabel);
  leftLayout->addWidget(timestampLabel);
  leftLayout->setSpacing(5);

  
  editButton = new QPushButton();
  editButton->setFixedSize(24, 24);
  editButton->setIcon(QIcon(":/resources/edit.svg"));
  editButton->setIconSize(QSize(16, 16));
  editButton->setStyleSheet("background: transparent; border: none;");

  deleteButton = new QPushButton();
  deleteButton->setFixedSize(24, 24);
  deleteButton->setIcon(QIcon(":/resources/delete.svg"));
  deleteButton->setIconSize(QSize(16, 16));
  deleteButton->setStyleSheet("background: transparent; border: none;");

  connect(editButton, &QPushButton::clicked, this, &OneSevenLiveStreamListItem::editClicked);
  connect(deleteButton, &QPushButton::clicked, this, &OneSevenLiveStreamListItem::deleteClicked);

  QHBoxLayout* rightLayout = new QHBoxLayout;
  rightLayout->addWidget(editButton);
  rightLayout->addWidget(deleteButton);

  QHBoxLayout* mainLayout = new QHBoxLayout(this);
  mainLayout->addLayout(leftLayout);
  mainLayout->addStretch();
  mainLayout->addLayout(rightLayout);
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->setSpacing(0);
}

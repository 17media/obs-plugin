#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

class OneSevenLiveLineEditWithEye : public QWidget {
    Q_OBJECT

   public:
    OneSevenLiveLineEditWithEye(QWidget *parent = nullptr);
    ~OneSevenLiveLineEditWithEye();

    void setText(const QString &text);
    QString text() const;

   private:
    QLineEdit *m_lineEdit = nullptr;
    QPushButton *m_eyeButton = nullptr;
};

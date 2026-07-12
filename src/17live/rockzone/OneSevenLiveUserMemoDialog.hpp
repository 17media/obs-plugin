#pragma once

#include <QDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QPoint>
#include <QPushButton>
#include <QTextEdit>
#include <QThread>

class OneSevenLiveApiWrappers;

class OneSevenLiveUserMemoDialog : public QDialog {
   public:
    OneSevenLiveUserMemoDialog(QWidget* parent, OneSevenLiveApiWrappers* apiWrapper,
                               const QString& userID);
    ~OneSevenLiveUserMemoDialog() override;

   protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

   private:
    void setupUi();
    void loadUserNoteAsync();
    void saveUserNoteAsync(const QString& content);
    void enforceTextLimit();
    void updateCharacterCount();

    QLabel* titleLabel = nullptr;
    QLabel* descLabel = nullptr;
    QLabel* counterLabel = nullptr;
    QTextEdit* memoEdit = nullptr;
    QPushButton* cancelButton = nullptr;
    QPushButton* saveButton = nullptr;

    OneSevenLiveApiWrappers* apiWrapper = nullptr;
    QString userID;

    bool dragging = false;
    QPoint dragStartPosition;

    bool suppressTextChanged = false;
};

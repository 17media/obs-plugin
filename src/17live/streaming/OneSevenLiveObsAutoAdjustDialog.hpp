#pragma once

#include <QDialog>
#include <QMouseEvent>
#include <QPoint>

class QCheckBox;
class QLabel;
class QPushButton;

class OneSevenLiveObsAutoAdjustDialog : public QDialog {
    Q_OBJECT

   public:
    enum class Mode {
        Prompt,
        Error,
    };

    struct PromptResult {
        bool confirmed = false;
        bool dontRemind = false;
    };

    static PromptResult ShowPrompt(QWidget* parent, const QString& message, bool dontRemindDefault);
    static void ShowError(QWidget* parent, const QString& message);

   protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

   private:
    explicit OneSevenLiveObsAutoAdjustDialog(QWidget* parent, Mode mode);
    void setupUiPrompt(const QString& message, bool dontRemindDefault);
    void setupUiError(const QString& message);

    QCheckBox* dontRemindCheck = nullptr;
    QLabel* messageLabel = nullptr;
    QPushButton* cancelButton = nullptr;
    QPushButton* confirmButton = nullptr;

    bool dragging = false;
    QPoint dragStartPosition;
};


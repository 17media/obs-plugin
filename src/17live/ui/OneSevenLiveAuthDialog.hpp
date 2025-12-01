#pragma once

#include <QDialog>
#include <QString>

class QCefView;

/**
 * Authorization dialog using embedded CEF view.
 * Loads a given URL and forwards URL changes to external listeners.
 */
class OneSevenLiveAuthDialog : public QDialog {
    Q_OBJECT

   public:
    explicit OneSevenLiveAuthDialog(QWidget* parent = nullptr);
    explicit OneSevenLiveAuthDialog(const QString& url, QWidget* parent = nullptr);
    ~OneSevenLiveAuthDialog();

    // Set or update the URL in the embedded browser
    void setUrl(const QString& url);

   signals:
    // Emitted when the embedded browser URL changes
    void urlChanged(const QString& url);

   private:
    void setupUi();
    QCefView* cefView_ = nullptr;
};

#pragma once

#include <QDialog>
#include <QString>
#include <memory>

class QCefView;
struct QCef;
class QCefWidget;
struct QCefCookieManager;
class CefWidgetHost;

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

   public slots:
    void accept() override;
    void reject() override;

   signals:
    // Emitted when the embedded browser URL changes
    void urlChanged(const QString& url);

   protected:
    void resizeEvent(QResizeEvent* event) override;

   private:
    void setupUi();
    std::unique_ptr<CefWidgetHost> cefHost_;
};

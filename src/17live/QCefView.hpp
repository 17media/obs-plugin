#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4996)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <include/cef_app.h>
#include <include/cef_browser.h>
#include <include/cef_client.h>
#include <include/cef_render_handler.h>
#include <include/wrapper/cef_helpers.h>

#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

class SimpleCefClient;

class QCefView : public QWidget {
    Q_OBJECT

   public:
    explicit QCefView(QWidget *parent = nullptr);
    ~QCefView();

    // Load URL
    void loadUrl(const QString &url);
    // Get current URL
    QString currentUrl() const;
    // Reload current page
    void reload();

   protected:
    virtual void resizeEvent(QResizeEvent *event) override;

   private:
    CefRefPtr<SimpleCefClient> m_client;

    QWindow *m_window;
    QWidget *m_container;
    QVBoxLayout *m_layout;
    QString m_currentUrl;
};

#pragma once

#include <QDockWidget>
#include <QScopedPointer>
#include <QString>

class QCloseEvent;
class QHideEvent;
class QLabel;
class QMoveEvent;
class QResizeEvent;
class QShowEvent;
class QTimer;
class QWidget;

#include "cef_panel.hpp"

class OneSevenLiveChatDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveChatDock(const QString& title, const QString& chatUrl,
                                  QWidget* parent = nullptr);
    ~OneSevenLiveChatDock();

    void setUrl(const QString& url);
    void reload();
    void prepareForDelete();

   protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

   private slots:
    void onGiftsLoaded();

   private:
    QCef* getOrCreateSharedCef() const;
    int qcefVersion() const;
    void createBrowser(const QString& url);
    void syncBrowserGeometry();
    void ensureFloatingGeometry();
    void schedulePersistDockState();
    void updateOverlayGeometry();
    void shutdownBrowser();

    QString title_;
    QString chatUrl_;
    QWidget* loadingOverlay_ = nullptr;
    QLabel* loadingLabel_ = nullptr;
    QLabel* errorLabel_ = nullptr;
    QScopedPointer<QCefWidget> cefWidget_;
    QTimer* persistDockStateTimer_ = nullptr;
    bool deleting_{false};
};

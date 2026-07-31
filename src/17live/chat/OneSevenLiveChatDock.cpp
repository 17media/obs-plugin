#include "OneSevenLiveChatDock.hpp"

#include <obs-module.h>

#include <QCloseEvent>
#include <QEvent>
#include <QHideEvent>
#include <QMoveEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QLabel>
#include <QResizeEvent>
#include <QScreen>
#include <QSizePolicy>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QMainWindow>

#include "../OneSevenLiveCoreManager.hpp"
#include "../OneSevenLiveConfigManager.hpp"
#include "../../plugin-support.h"
#include "moc_OneSevenLiveChatDock.cpp"

namespace {
constexpr int kDefaultFloatingWidth = 520;
constexpr int kDefaultFloatingHeight = 780;
constexpr int kMinimumFloatingHeight = 300;
constexpr int kPersistDockStateDelayMs = 200;
}

OneSevenLiveChatDock::OneSevenLiveChatDock(const QString& title, const QString& chatUrl,
                                           QWidget* parent)
    : QDockWidget(title, parent), title_(title), chatUrl_(chatUrl) {
    setAttribute(Qt::WA_NativeWindow);
    setMinimumSize(300, kMinimumFloatingHeight);
    resize(kDefaultFloatingWidth, kDefaultFloatingHeight);

    persistDockStateTimer_ = new QTimer(this);
    persistDockStateTimer_->setSingleShot(true);
    persistDockStateTimer_->setInterval(kPersistDockStateDelayMs);
    connect(persistDockStateTimer_, &QTimer::timeout, this, [this]() {
        auto* core = OneSevenLiveCoreManager::peekInstance();
        if (!core || core->isShuttingDown() || deleting_) {
            return;
        }
        auto* mainWindow = core->getMainWindow();
        auto* configManager = core->getConfigManager();
        if (!mainWindow || !configManager) {
            return;
        }
        configManager->setDockState(mainWindow->saveState());
    });

    createBrowser(chatUrl_);

    loadingOverlay_ = new QWidget(this);
    loadingOverlay_->setStyleSheet("background-color: rgba(0, 0, 0, 180);");

    QVBoxLayout* overlayLayout = new QVBoxLayout(loadingOverlay_);
    overlayLayout->setAlignment(Qt::AlignCenter);

    loadingLabel_ = new QLabel(obs_module_text("ChatRoom.LoadingGifts"), loadingOverlay_);
    loadingLabel_->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    overlayLayout->addWidget(loadingLabel_);

    loadingOverlay_->raise();
    updateOverlayGeometry();

    auto& core = OneSevenLiveCoreManager::getInstance();
    connect(&core, &OneSevenLiveCoreManager::giftsLoaded, this, &OneSevenLiveChatDock::onGiftsLoaded);

    if (core.isGiftsLoaded()) {
        loadingOverlay_->hide();
    } else {
        loadingOverlay_->show();
    }

    connect(this, &QDockWidget::topLevelChanged, this, [this](bool floating) {
        auto syncDockedLayout = [this]() {
            syncBrowserGeometry();
            updateOverlayGeometry();
        };

        if (floating) {
            QTimer::singleShot(0, this, [this]() { ensureFloatingGeometry(); });
            QTimer::singleShot(50, this, [this]() { ensureFloatingGeometry(); });
            schedulePersistDockState();
            return;
        }

        QTimer::singleShot(0, this, syncDockedLayout);
        QTimer::singleShot(50, this, syncDockedLayout);
        schedulePersistDockState();
    });
}

OneSevenLiveChatDock::~OneSevenLiveChatDock() = default;

QCef* OneSevenLiveChatDock::getOrCreateSharedCef() const {
    static QCef* shared = nullptr;
    if (shared) {
        return shared;
    }

    shared = obs_browser_init_panel();
    if (!shared) {
        return nullptr;
    }

    if (!shared->initialized()) {
        shared->init_browser();
        shared->wait_for_browser_init();
    }

    return shared;
}

int OneSevenLiveChatDock::qcefVersion() const {
    return obs_browser_qcef_version();
}

void OneSevenLiveChatDock::createBrowser(const QString& url) {
    chatUrl_ = url;

    if (cefWidget_) {
        cefWidget_->setURL(url.toStdString());
        return;
    }

    QCef* cef = getOrCreateSharedCef();
    if (!cef) {
        obs_log(LOG_ERROR, "OneSevenLiveChatDock: obs-browser panel is not available");
        return;
    }

    QCefWidget* widget = cef->create_widget(this, url.toStdString(), nullptr);
    if (!widget) {
        if (!errorLabel_) {
            errorLabel_ = new QLabel("Browser source not available", this);
            errorLabel_->setAlignment(Qt::AlignCenter);
            errorLabel_->setStyleSheet("background-color: #202020; color: white;");
        }
        obs_log(LOG_ERROR, "OneSevenLiveChatDock: Failed to create QCefWidget");
        updateOverlayGeometry();
        return;
    }

    cefWidget_.reset(widget);
    setWidget(cefWidget_.data());
    syncBrowserGeometry();

    if (qcefVersion() >= 1) {
        cefWidget_->allowAllPopups(true);
    }

    if (errorLabel_) {
        errorLabel_->hide();
    }
    updateOverlayGeometry();
    QTimer::singleShot(0, this, [this]() { syncBrowserGeometry(); });
}

void OneSevenLiveChatDock::syncBrowserGeometry() {
    QWidget* content = widget();
    if (!content) {
        return;
    }

    content->setMinimumSize(0, 0);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    const QRect area = contentsRect();
    if (area.isValid() && area.size() != content->size()) {
        content->setGeometry(area);
        content->resize(area.size());
    }
    content->updateGeometry();
}

void OneSevenLiveChatDock::ensureFloatingGeometry() {
    if (!isFloating()) {
        return;
    }

    int targetWidth = width();
    int targetHeight = height();
    if (targetWidth <= minimumWidth()) {
        targetWidth = std::max(minimumWidth(), kDefaultFloatingWidth);
    }
    if (targetHeight < minimumHeight()) {
        targetHeight = std::max(minimumHeight(), kDefaultFloatingHeight);
    }
    if (targetWidth != width() || targetHeight != height()) {
        resize(targetWidth, targetHeight);
    }

    QRect frame = frameGeometry();
    bool onScreen = false;
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen && screen->availableGeometry().intersects(frame)) {
            onScreen = true;
            break;
        }
    }

    if (!onScreen) {
        QRect anchorRect;
        if (QWidget* parent = parentWidget()) {
            anchorRect = parent->frameGeometry();
        } else if (!screens.isEmpty() && screens.first()) {
            anchorRect = screens.first()->availableGeometry();
        }

        if (!anchorRect.isNull()) {
            const int x = anchorRect.x() + (anchorRect.width() - width()) / 2;
            const int y = anchorRect.y() + (anchorRect.height() - height()) / 2;
            move(x, y);
        }
    }

    show();
    raise();
    activateWindow();
    syncBrowserGeometry();
}

void OneSevenLiveChatDock::schedulePersistDockState() {
    if (!persistDockStateTimer_) {
        return;
    }

    auto* core = OneSevenLiveCoreManager::peekInstance();
    if (!core || core->isShuttingDown() || deleting_) {
        return;
    }

    persistDockStateTimer_->start();
}

void OneSevenLiveChatDock::updateOverlayGeometry() {
    const QRect area = contentsRect();
    if (loadingOverlay_) {
        loadingOverlay_->setGeometry(area);
    }
    if (errorLabel_) {
        errorLabel_->setGeometry(area);
    }
}

void OneSevenLiveChatDock::shutdownBrowser() {
    if (!cefWidget_) {
        return;
    }

    // Match OBS browser-panel guidance: decouple the browser widget from the
    // dock before closing, so CEF does not treat the root window as the host.
    QWidget* browserWidget = cefWidget_.data();
    if (widget() == browserWidget) {
        setWidget(nullptr);
    }
    browserWidget->hide();
    browserWidget->setParent(nullptr);

    if (qcefVersion() >= 2) {
        cefWidget_->closeBrowser();
    }
}

void OneSevenLiveChatDock::setUrl(const QString& url) {
    chatUrl_ = url;

    if (!cefWidget_) {
        createBrowser(url);
        return;
    }

    cefWidget_->setURL(url.toStdString());
}

void OneSevenLiveChatDock::reload() {
    if (cefWidget_) {
        cefWidget_->reloadPage();
    }
}

void OneSevenLiveChatDock::prepareForDelete() {
    if (deleting_) {
        return;
    }

    deleting_ = true;
}

void OneSevenLiveChatDock::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);
    setWindowTitle(title_);
    if (!cefWidget_ && !deleting_) {
        createBrowser(chatUrl_);
    }
    if (cefWidget_) {
        cefWidget_->setVisible(true);
        syncBrowserGeometry();
    }
    if (loadingOverlay_ && loadingOverlay_->isVisible()) {
        loadingOverlay_->raise();
    }
    if (errorLabel_ && errorLabel_->isVisible()) {
        errorLabel_->raise();
    }
    updateOverlayGeometry();
    if (isFloating()) {
        ensureFloatingGeometry();
    }
    QTimer::singleShot(0, this, [this]() { syncBrowserGeometry(); });
}

void OneSevenLiveChatDock::hideEvent(QHideEvent* event) {
    QDockWidget::hideEvent(event);
    if (cefWidget_) {
        cefWidget_->setVisible(false);
    }
}

void OneSevenLiveChatDock::closeEvent(QCloseEvent* event) {
    QDockWidget::closeEvent(event);

    if (event && event->isAccepted() && widget()) {
        QEvent widgetEvent(QEvent::Type(QEvent::User + QEvent::Close));
        qApp->sendEvent(widget(), &widgetEvent);
    }

    if (event && event->isAccepted() && cefWidget_) {
        shutdownBrowser();

        if (!deleting_) {
            // Manual close keeps the dock instance around, so recreate the browser next time it opens.
            cefWidget_.reset(nullptr);
        }
    }
}

void OneSevenLiveChatDock::moveEvent(QMoveEvent* event) {
    QDockWidget::moveEvent(event);
    if (isFloating()) {
        schedulePersistDockState();
    }
}

void OneSevenLiveChatDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);
    syncBrowserGeometry();
    updateOverlayGeometry();
    if (isFloating()) {
        schedulePersistDockState();
    }
}

void OneSevenLiveChatDock::onGiftsLoaded() {
    if (loadingOverlay_) {
        loadingOverlay_->hide();
    }
}

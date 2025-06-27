#include "CefDockWidget.hpp"

#include <QResizeEvent>
#include <QTimer>

#include "cef-view.hpp"

#include "moc_CefDockWidget.cpp"

CefDockWidget::CefDockWidget(QWidget *parent) : QDockWidget(parent)
{
}

void CefDockWidget::resizeEvent(QResizeEvent *event) {
    QDockWidget::resizeEvent(event);
    if (event->size() != event->oldSize()) {
        // Delay the resize to ensure the widget is properly resized
        QTimer::singleShot(10, [this, event]() {
            cef_view_resize_browser(event->size().width(), event->size().height());
        });
    }
}

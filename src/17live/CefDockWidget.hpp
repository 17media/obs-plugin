#pragma once

#include <QDockWidget>
#include <QResizeEvent>

// Custom QDockWidget class to handle resize events
class CefDockWidget : public QDockWidget {
Q_OBJECT
public:
    CefDockWidget(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
};

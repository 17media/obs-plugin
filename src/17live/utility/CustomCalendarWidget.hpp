#pragma once

#include <QCalendarWidget>
#include <QDate>
#include <QPainter>
#include <QRect>
#include <QWidget>

class CustomCalendarWidget : public QCalendarWidget {
    Q_OBJECT
   public:
    CustomCalendarWidget(const QDate& minDate, const QDate& maxDate, QWidget* parent = nullptr);

   protected:
    virtual void paintCell(QPainter* painter, const QRect& rect, QDate date) const override;
};

#include "CustomCalendarWidget.hpp"

#include <obs-module.h>

#include "plugin-support.h"
#include "moc_CustomCalendarWidget.cpp"

CustomCalendarWidget::CustomCalendarWidget(const QDate& minDate, const QDate& maxDate, QWidget* parent)
    : QCalendarWidget(parent)
{
    setMinimumDate(minDate);
    setMaximumDate(maxDate);
    connect(this, &QCalendarWidget::currentPageChanged, this, [this]() { updateCells(); });
}

void CustomCalendarWidget::paintCell(QPainter* painter, const QRect& rect, QDate date) const
{
    if (date < minimumDate() || date > maximumDate()) {
        painter->save();
        // painter->fillRect(rect, QColor(150, 150, 150));
        painter->setPen(Qt::gray);
        painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));
        painter->restore();
    } else {
        QCalendarWidget::paintCell(painter, rect, date);
    }
}

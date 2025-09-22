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
    obs_log(LOG_INFO, "paintCell: %s", qPrintable(date.toString()));

    if (date < minimumDate() || date > maximumDate()) {
        obs_log(LOG_INFO, "date %s is out of range", date.toString().toStdString().c_str());
        painter->save();
        painter->fillRect(rect, QColor(200, 200, 200)); // 灰色背景
        painter->setPen(Qt::gray);
        painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));
        painter->restore();
    } else {
        obs_log(LOG_INFO, "date %s is in range", date.toString().toStdString().c_str());
        QCalendarWidget::paintCell(painter, rect, date);
    }
}

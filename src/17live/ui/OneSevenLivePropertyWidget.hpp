#pragma once

#include <obs.h>

#include <QLayout>
#include <QString>
#include <QWidget>

struct OneSevenLivePropertyRefreshHandler;

struct obs_property;
struct obs_data;

class QLabel;

class OneSevenLivePropertyWidget : public QWidget {
    Q_OBJECT

   public:
    OneSevenLivePropertyWidget(QWidget *parent = nullptr,
                               OneSevenLivePropertyRefreshHandler *refreshHandler = nullptr,
                               obs_property *property = nullptr);
    ~OneSevenLivePropertyWidget();

    void ReloadProperty(obs_property *property);
    void LoadData(obs_data *settings);
    void SaveData(obs_data *settings);

    QLabel *label = nullptr;
    QWidget *ctrl = nullptr;
    QWidget *container = nullptr;
    std::string name;

   private:
    OneSevenLivePropertyRefreshHandler *m_refreshHandler = nullptr;
    obs_property *m_property = nullptr;
    obs_property_type m_propertyType;
    obs_combo_format m_comboFormat;
};

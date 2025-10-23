#pragma once

#include <obs.h>

#include <QWidget>
#include <QString>

struct OneSevenLivePropertyRefreshHandler;

struct obs_property;
struct obs_data;

class QLabel;

class OneSevenLivePropertyWidget : public QWidget {
  Q_OBJECT

public:
  OneSevenLivePropertyWidget(QWidget *parent = nullptr, OneSevenLivePropertyRefreshHandler *refreshHandler = nullptr, obs_property *property = nullptr);
  ~OneSevenLivePropertyWidget();

  void ReloadProperty(obs_property *property);
  void LoadData(obs_data *settings);
  void SaveData(obs_data *settings);

  QLabel *label = nullptr;
  QWidget *ctrl = nullptr;
  std::string name;

private:
  OneSevenLivePropertyRefreshHandler *m_RefreshHandler = nullptr;
  obs_property *m_Property = nullptr;
  obs_property_type m_PropertyType;
  obs_combo_format m_ComboFormat;
};

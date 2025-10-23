#pragma once

#include <obs.h>

#include <QWidget>
#include <QLabel>

#include "OneSevenLivePropertyRefreshHandler.hpp"

class OneSevenLivePropertyWidget;

class OneSevenLivePropertiesWidget : public QWidget, public OneSevenLivePropertyRefreshHandler {
  Q_OBJECT

private:
  obs_data_t *m_OrigSettings;
  obs_data_t *m_Settings;
  obs_properties_t *m_Props;

public:
  OneSevenLivePropertiesWidget(QWidget *parent = nullptr, obs_data_t *settings = nullptr, obs_properties_t *props = nullptr);
  ~OneSevenLivePropertiesWidget();

  void RefreshUI() override;

private:
  std::unordered_map<std::string, std::shared_ptr<OneSevenLivePropertyWidget>> m_PropertyWidgets;

  bool isRefreshing = false;

  void loadProperties();
};

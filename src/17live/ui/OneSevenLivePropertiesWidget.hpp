#pragma once

#include <obs.h>

#include <QFormLayout>
#include <QLabel>
#include <QWidget>

#include "OneSevenLivePropertyRefreshHandler.hpp"
#include "nlohmann/json.hpp"

class OneSevenLivePropertyWidget;

class OneSevenLivePropertiesWidget : public QWidget, public OneSevenLivePropertyRefreshHandler {
    Q_OBJECT

   private:
    obs_data_t *m_origSettings;
    obs_data_t *m_settings;
    obs_properties_t *m_props;

   public:
    OneSevenLivePropertiesWidget(QWidget *parent = nullptr, obs_data_t *settings = nullptr,
                                 obs_properties_t *props = nullptr);
    ~OneSevenLivePropertiesWidget();

    void UpdateProperties(obs_data_t *settings, obs_properties_t *props);
    void RefreshUI() override;
    nlohmann::json SaveData();

   private:
    QFormLayout *m_formLayout;

    std::unordered_map<std::string, std::shared_ptr<OneSevenLivePropertyWidget>> m_propertyWidgets;

    bool isRefreshing = false;

    void loadProperties();
};

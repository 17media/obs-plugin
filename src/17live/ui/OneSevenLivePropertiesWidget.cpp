#include "OneSevenLivePropertiesWidget.hpp"

#include <QFormLayout>
#include <unordered_map>
#include <memory>
#include <utility>

#include "OneSevenLivePropertyWidget.hpp"

OneSevenLivePropertiesWidget::OneSevenLivePropertiesWidget(QWidget *parent, obs_data_t *settings, obs_properties_t *props)
    : QWidget(parent), m_OrigSettings(settings), m_Props(props) {

      obs_data_release(m_Settings);

      m_Settings = obs_data_create();
      obs_data_release(m_Settings);

      auto defaultSettings = obs_data_get_defaults(m_OrigSettings);
      obs_data_apply(m_Settings, defaultSettings);
      obs_data_release(defaultSettings);

      obs_data_apply(m_Settings, m_OrigSettings);

      obs_properties_apply_settings(m_Props, m_Settings);

      RefreshUI();
}

OneSevenLivePropertiesWidget::~OneSevenLivePropertiesWidget() {
  if (m_Props)
    obs_properties_destroy(m_Props);

  if (m_Settings)
    obs_data_release(m_Settings);

  if (m_OrigSettings)
    obs_data_release(m_OrigSettings);
}

void OneSevenLivePropertiesWidget::RefreshUI() {
    if (isRefreshing)
        return;
    isRefreshing = true;

    for(auto& x: m_PropertyWidgets)
    {
        x.second->SaveData(m_Settings);
    }

    obs_properties_apply_settings(m_Props, m_Settings);
    loadProperties();

    for(auto& x: m_PropertyWidgets)
    {
        x.second->LoadData(m_Settings);
    }

    isRefreshing = false;
}

void OneSevenLivePropertiesWidget::loadProperties() {
    std::unordered_map<std::string, std::shared_ptr<OneSevenLivePropertyWidget>> origPropWidgets;
    origPropWidgets.swap(m_PropertyWidgets);

    auto oldLayout = layout();
    if (oldLayout) {
        for (auto& x : origPropWidgets) {
            if (x.second->label)
                oldLayout->removeWidget(x.second->label);
            if (x.second->ctrl)
                oldLayout->removeWidget(x.second->ctrl);
        }
    }

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->setContentsMargins(0, 0, 0, 0);
    obs_property_t *prop = obs_properties_first(m_Props);
    do { 
        if (obs_property_visible(prop) == false)
            continue;

        auto name = obs_property_name(prop);
        auto it = origPropWidgets.find(name);
        if (it == origPropWidgets.end()) {
            auto newWidget = std::make_shared<OneSevenLivePropertyWidget>(this, this, prop);
            newWidget->LoadData(m_Settings);
            m_PropertyWidgets.insert(std::make_pair(newWidget->name, newWidget));
            formLayout->addRow(newWidget->label, newWidget->ctrl);
        } else {
            it->second->ReloadProperty(prop);
            it->second->LoadData(m_Settings);
            m_PropertyWidgets.insert(std::make_pair(it->first, it->second));
            formLayout->addRow(it->second->label, it->second->ctrl);
        }
        
    } while (obs_property_next(&prop));

    if (oldLayout)
        delete oldLayout;
    setLayout(formLayout);
}

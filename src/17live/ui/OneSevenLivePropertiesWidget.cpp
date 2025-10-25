#include "OneSevenLivePropertiesWidget.hpp"

#include <QFormLayout>
#include <unordered_map>
#include <memory>
#include <utility>

#include "OneSevenLivePropertyWidget.hpp"

OneSevenLivePropertiesWidget::OneSevenLivePropertiesWidget(QWidget *parent, obs_data_t *settings, obs_properties_t *props)
    : QWidget(parent), m_origSettings(settings), m_settings(nullptr), m_props(props) {

  // Initialize internal settings store
  m_settings = obs_data_create();

  // If original settings provided, seed with defaults then apply originals
  if (m_origSettings) {
    obs_data_t *defaultSettings = obs_data_get_defaults(m_origSettings);
    if (defaultSettings) {
      obs_data_apply(m_settings, defaultSettings);
      obs_data_release(defaultSettings);
    }
    obs_data_apply(m_settings, m_origSettings);
  }

  // Minimal UI: start with an empty form layout so the widget renders blank
  QFormLayout *formLayout = new QFormLayout();
  formLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
  formLayout->setLabelAlignment(Qt::AlignLeft);
  formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  formLayout->setContentsMargins(0, 0, 0, 0);
  setLayout(formLayout);

  // If properties are provided, apply and build the property controls now
  if (m_props) {
    obs_properties_apply_settings(m_props, m_settings);
    RefreshUI();
  }
}

OneSevenLivePropertiesWidget::~OneSevenLivePropertiesWidget() {
  if (m_props)
    obs_properties_destroy(m_props);

  if (m_settings)
    obs_data_release(m_settings);

  if (m_origSettings)
    obs_data_release(m_origSettings);
}

void OneSevenLivePropertiesWidget::RefreshUI() {
    if (isRefreshing)
        return;
    isRefreshing = true;

    for(auto& x: m_propertyWidgets)
    {
        x.second->SaveData(m_settings);
    }

    obs_properties_apply_settings(m_props, m_settings);
    loadProperties();

    for(auto& x: m_propertyWidgets)
    {
        x.second->LoadData(m_settings);
    }

    isRefreshing = false;
}

void OneSevenLivePropertiesWidget::loadProperties() {
    std::unordered_map<std::string, std::shared_ptr<OneSevenLivePropertyWidget>> origPropWidgets;
    origPropWidgets.swap(m_propertyWidgets);

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
    obs_property_t *prop = obs_properties_first(m_props);
    do { 
        if (obs_property_visible(prop) == false)
            continue;

        auto name = obs_property_name(prop);
        auto it = origPropWidgets.find(name);
        if (it == origPropWidgets.end()) {
            auto newWidget = std::make_shared<OneSevenLivePropertyWidget>(this, this, prop);
            newWidget->LoadData(m_settings);
            m_propertyWidgets.insert(std::make_pair(newWidget->name, newWidget));
            formLayout->addRow(newWidget->label, newWidget->ctrl);
        } else {
            it->second->ReloadProperty(prop);
            it->second->LoadData(m_settings);
            m_propertyWidgets.insert(std::make_pair(it->first, it->second));
            formLayout->addRow(it->second->label, it->second->ctrl);
        }
        
    } while (obs_property_next(&prop));

    if (oldLayout)
        delete oldLayout;
    setLayout(formLayout);
}

void OneSevenLivePropertiesWidget::UpdateProperties(obs_data_t *settings, obs_properties_t *props)
{
  // Remove existing controls and layout
  QLayout *oldLayout = layout();
  if (oldLayout) {
    for (auto &kv : m_propertyWidgets) {
      if (kv.second) {
        if (kv.second->label) oldLayout->removeWidget(kv.second->label);
        if (kv.second->ctrl) oldLayout->removeWidget(kv.second->ctrl);
        if (kv.second->label) kv.second->label->deleteLater();
        if (kv.second->ctrl) kv.second->ctrl->deleteLater();
      }
    }
    delete oldLayout;
  }
  m_propertyWidgets.clear();

  // Release previous OBS objects
  if (m_props) { obs_properties_destroy(m_props); m_props = nullptr; }
  if (m_settings) { obs_data_release(m_settings); m_settings = nullptr; }
  if (m_origSettings) { obs_data_release(m_origSettings); m_origSettings = nullptr; }

  // Take ownership of new OBS structures
  m_origSettings = settings;
  m_props = props;

  // Initialize settings storage from defaults and provided originals
  m_settings = obs_data_create();
  if (m_origSettings) {
    obs_data_t *defaultSettings = obs_data_get_defaults(m_origSettings);
    if (defaultSettings) {
      obs_data_apply(m_settings, defaultSettings);
      obs_data_release(defaultSettings);
    }
    obs_data_apply(m_settings, m_origSettings);
  }

  // Create a fresh blank form layout
  QFormLayout *formLayout = new QFormLayout();
  formLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
  formLayout->setLabelAlignment(Qt::AlignLeft);
  formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  formLayout->setContentsMargins(0, 0, 0, 0);
  setLayout(formLayout);

  // Build property controls if properties provided
  if (m_props) {
    obs_properties_apply_settings(m_props, m_settings);
    loadProperties();
    for (auto &kv : m_propertyWidgets) {
      if (kv.second) kv.second->LoadData(m_settings);
    }
  }
}

nlohmann::json OneSevenLivePropertiesWidget::SaveData() {
    obs_data_apply(m_origSettings, m_settings);

    auto jsonstr = obs_data_get_json(m_settings);
    if (!jsonstr)
        return {};
    try {
        return nlohmann::json::parse(jsonstr);
    } catch (const nlohmann::json::parse_error& e) {
        return {};
    }
}

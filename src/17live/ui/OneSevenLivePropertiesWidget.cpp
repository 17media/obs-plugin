#include "OneSevenLivePropertiesWidget.hpp"

#include <QFormLayout>
#include <unordered_map>
#include <memory>
#include <utility>

#include <obs-module.h>

#include "OneSevenLivePropertyWidget.hpp"

#include "plugin-support.h"

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
    obs_log(LOG_DEBUG, "[loadProperties] Starting property loading");
    
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
    
    // 安全检查：确保m_props不为空
    if (!m_props) {
        obs_log(LOG_WARNING, "[loadProperties] m_props is null, skipping property loading");
        // setLayout()会自动删除oldLayout，不需要手动删除
        setLayout(formLayout);
        return;
    }
    
    obs_property_t *prop = obs_properties_first(m_props);
    if (!prop) {
        obs_log(LOG_INFO, "[loadProperties] No properties found");
        // setLayout()会自动删除oldLayout，不需要手动删除
        setLayout(formLayout);
        return;
    }
    
    int propertyCount = 0;
    // 修复：使用标准的while循环而不是do-while
    while (prop) {
        propertyCount++;
        obs_log(LOG_DEBUG, "[loadProperties] Processing property %d", propertyCount);
        
        // 安全检查：验证prop指针
        if (!obs_property_visible(prop)) {
            obs_log(LOG_DEBUG, "[loadProperties] Property %d is not visible, skipping", propertyCount);
            if (!obs_property_next(&prop)) break;
            continue;
        }

        const char* name_cstr = obs_property_name(prop);
        if (!name_cstr) {
            obs_log(LOG_ERROR, "[loadProperties] Property %d has null name, skipping", propertyCount);
            if (!obs_property_next(&prop)) break;
            continue;
        }
        
        std::string name(name_cstr);
        obs_log(LOG_DEBUG, "[loadProperties] Processing property: %s", name.c_str());
        
        auto it = origPropWidgets.find(name);
        if (it == origPropWidgets.end()) {
            obs_log(LOG_DEBUG, "[loadProperties] Creating new widget for property: %s", name.c_str());
            try {
                auto newWidget = std::make_shared<OneSevenLivePropertyWidget>(this, this, prop);
                if (!newWidget || !newWidget->label || !newWidget->ctrl) {
                    obs_log(LOG_ERROR, "[loadProperties] Failed to create widget for property: %s", name.c_str());
                    if (!obs_property_next(&prop)) break;
                    continue;
                }
                newWidget->LoadData(m_settings);
                m_propertyWidgets.insert(std::make_pair(newWidget->name, newWidget));
                formLayout->addRow(newWidget->label, newWidget->ctrl);
                obs_log(LOG_DEBUG, "[loadProperties] Successfully created widget for property: %s", name.c_str());
            } catch (const std::exception& e) {
                obs_log(LOG_ERROR, "[loadProperties] Exception creating widget for property %s: %s", name.c_str(), e.what());
                if (!obs_property_next(&prop)) break;
                continue;
            }
        } else {
            obs_log(LOG_DEBUG, "[loadProperties] Reusing existing widget for property: %s", name.c_str());
            try {
                it->second->ReloadProperty(prop);
                it->second->LoadData(m_settings);
                m_propertyWidgets.insert(std::make_pair(it->first, it->second));
                formLayout->addRow(it->second->label, it->second->ctrl);
                obs_log(LOG_DEBUG, "[loadProperties] Successfully reused widget for property: %s", name.c_str());
            } catch (const std::exception& e) {
                obs_log(LOG_ERROR, "[loadProperties] Exception reusing widget for property %s: %s", name.c_str(), e.what());
                if (!obs_property_next(&prop)) break;
                continue;
            }
        }
        
        // 安全地移动到下一个属性
        if (!obs_property_next(&prop)) {
            obs_log(LOG_DEBUG, "[loadProperties] Reached end of properties");
            break;
        }
        
        // 防止无限循环的安全检查
        if (propertyCount > 1000) {
            obs_log(LOG_ERROR, "[loadProperties] Too many properties (%d), breaking to prevent infinite loop", propertyCount);
            break;
        }
    }

    obs_log(LOG_INFO, "[loadProperties] Processed %d properties successfully", propertyCount);
    
    // setLayout()会自动删除oldLayout，不需要手动删除
    setLayout(formLayout);

    obs_log(LOG_DEBUG, "[loadProperties] Finished property loading");
}

void OneSevenLivePropertiesWidget::UpdateProperties(obs_data_t *settings, obs_properties_t *props)
{
  obs_log(LOG_INFO, "[UpdateProperties] Starting with settings: %p, props: %p", (void*)settings, (void*)props);

  // Validate input parameters
  if (!settings) {
    obs_log(LOG_ERROR, "[UpdateProperties] settings parameter is null");
    return;
  }
  
  if (!props) {
    obs_log(LOG_ERROR, "[UpdateProperties] props parameter is null");
    if (settings) {
      obs_data_release(settings);
    }
    return;
  }

  obs_log(LOG_DEBUG, "[UpdateProperties] Removing existing controls and layout");
  
  // Remove existing controls and layout
  QLayout *oldLayout = layout();
  if (oldLayout) {
    obs_log(LOG_DEBUG, "[UpdateProperties] Cleaning up %zu existing property widgets", m_propertyWidgets.size());
    for (auto &kv : m_propertyWidgets) {
      if (kv.second) {
        if (kv.second->label) {
          oldLayout->removeWidget(kv.second->label);
          kv.second->label->deleteLater();
        }
        if (kv.second->ctrl) {
          oldLayout->removeWidget(kv.second->ctrl);
          kv.second->ctrl->deleteLater();
        }
      }
    }
    // 不要手动删除oldLayout，setLayout()会自动处理
    obs_log(LOG_DEBUG, "[UpdateProperties] Old layout widgets removed, layout will be auto-deleted by setLayout()");
  }
  m_propertyWidgets.clear();

  obs_log(LOG_DEBUG, "[UpdateProperties] Releasing previous OBS objects");
  
  // Release previous OBS objects
  if (m_props) { 
    obs_log(LOG_DEBUG, "[UpdateProperties] Destroying previous props: %p", (void*)m_props);
    obs_properties_destroy(m_props); 
    m_props = nullptr; 
  }
  if (m_settings) { 
    obs_log(LOG_DEBUG, "[UpdateProperties] Releasing previous settings: %p", (void*)m_settings);
    obs_data_release(m_settings); 
    m_settings = nullptr; 
  }
  if (m_origSettings) { 
    obs_log(LOG_DEBUG, "[UpdateProperties] Releasing previous origSettings: %p", (void*)m_origSettings);
    obs_data_release(m_origSettings); 
    m_origSettings = nullptr; 
  }

  obs_log(LOG_DEBUG, "[UpdateProperties] Taking ownership of new OBS structures");
  
  // Take ownership of new OBS structures
  m_origSettings = settings;
  m_props = props;

  obs_log(LOG_DEBUG, "[UpdateProperties] Initializing settings storage");
  
  // Initialize settings storage from defaults and provided originals
  m_settings = obs_data_create();
  if (!m_settings) {
    obs_log(LOG_ERROR, "[UpdateProperties] Failed to create settings data");
    return;
  }
  
  if (m_origSettings) {
    obs_log(LOG_DEBUG, "[UpdateProperties] Applying default and original settings");
    obs_data_t *defaultSettings = obs_data_get_defaults(m_origSettings);
    if (defaultSettings) {
      obs_data_apply(m_settings, defaultSettings);
      obs_data_release(defaultSettings);
      obs_log(LOG_DEBUG, "[UpdateProperties] Applied default settings");
    }
    obs_data_apply(m_settings, m_origSettings);
    obs_log(LOG_DEBUG, "[UpdateProperties] Applied original settings");
  }

  obs_log(LOG_DEBUG, "[UpdateProperties] Creating fresh form layout");
  
  // Create a fresh blank form layout
  QFormLayout *formLayout = new QFormLayout();
  if (!formLayout) {
    obs_log(LOG_ERROR, "[UpdateProperties] Failed to create form layout");
    return;
  }
  
  formLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
  formLayout->setLabelAlignment(Qt::AlignLeft);
  formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  formLayout->setContentsMargins(0, 0, 0, 0);
  setLayout(formLayout);

  obs_log(LOG_DEBUG, "[UpdateProperties] Building property controls");
  
  // Build property controls if properties provided
  if (m_props) {
    try {
      obs_log(LOG_DEBUG, "[UpdateProperties] Applying settings to properties");
      obs_properties_apply_settings(m_props, m_settings);
      
      obs_log(LOG_DEBUG, "[UpdateProperties] Loading properties");
      loadProperties();
      
      obs_log(LOG_DEBUG, "[UpdateProperties] Loading data for %zu property widgets", m_propertyWidgets.size());
      for (auto &kv : m_propertyWidgets) {
        if (kv.second) {
          kv.second->LoadData(m_settings);
        }
      }
      obs_log(LOG_DEBUG, "[UpdateProperties] All property widgets loaded successfully");
    } catch (const std::exception& e) {
      obs_log(LOG_ERROR, "[UpdateProperties] Exception during property loading: %s", e.what());
    } catch (...) {
      obs_log(LOG_ERROR, "[UpdateProperties] Unknown exception during property loading");
    }
  } else {
    obs_log(LOG_WARNING, "[UpdateProperties] No properties provided, skipping property loading");
  }
  
  obs_log(LOG_INFO, "[UpdateProperties] Completed successfully");
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

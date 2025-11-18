#include "OneSevenLivePropertiesWidget.hpp"

#include <obs-module.h>

#include <QFormLayout>
#include <memory>
#include <unordered_map>
#include <utility>

#include "OneSevenLivePropertyWidget.hpp"
#include "plugin-support.h"

OneSevenLivePropertiesWidget::OneSevenLivePropertiesWidget(QWidget *parent, obs_data_t *settings,
                                                           obs_properties_t *props)
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
    m_formLayout = new QFormLayout();
    m_formLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_formLayout->setLabelAlignment(Qt::AlignLeft);
    m_formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_formLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_formLayout);

    // If properties are provided, apply and build the property controls now
    if (m_props) {
        obs_properties_apply_settings(m_props, m_settings);
        RefreshUI();
    }
}

OneSevenLivePropertiesWidget::~OneSevenLivePropertiesWidget() {
    if (m_props)
        obs_properties_destroy(m_props);
    m_props = nullptr;

    if (m_settings)
        obs_data_release(m_settings);
    m_settings = nullptr;

    if (m_origSettings)
        obs_data_release(m_origSettings);
    m_origSettings = nullptr;
}

void OneSevenLivePropertiesWidget::RefreshUI() {
    if (isRefreshing)
        return;
    isRefreshing = true;

    for (auto &x : m_propertyWidgets) {
        x.second->SaveData(m_settings);
    }

    {
        obs_log(LOG_INFO, "[RefreshUI] Dumping settings");
        // iterate all m_settings and display property name, type and description
        obs_data_item_t *item = obs_data_first(m_settings);
        while (item) {
            const char *name_cstr = obs_data_item_get_name(item);
            if (name_cstr) {
                std::string name(name_cstr);
                obs_log(LOG_INFO, "[RefreshUI] Setting %s, type %d", name.c_str(),
                        static_cast<int>(obs_data_item_gettype(item)));
            }
            obs_data_item_next(&item);
        }
    }

    {
        obs_log(LOG_INFO, "[RefreshUI] Dumping properties");
        // iterate all m_props and display property name, type and description
        obs_property_t *prop = obs_properties_first(m_props);
        while (prop) {
            const char *name_cstr = obs_property_name(prop);
            if (name_cstr) {
                std::string name(name_cstr);
                obs_log(LOG_INFO, "[RefreshUI] Property %s, description %s", name.c_str(),
                        obs_property_description(prop));
            }
            obs_property_next(&prop);
        }
    }

    obs_properties_apply_settings(m_props, m_settings);
    loadProperties();

    for (auto &x : m_propertyWidgets) {
        x.second->LoadData(m_settings);
    }

    isRefreshing = false;
}

void OneSevenLivePropertiesWidget::loadProperties() {
    obs_log(LOG_DEBUG, "[loadProperties] Starting property loading");

    std::unordered_map<std::string, std::shared_ptr<OneSevenLivePropertyWidget>> origPropWidgets;
    origPropWidgets.swap(m_propertyWidgets);

    for (auto &x : origPropWidgets) {
        if (x.second->label)
            m_formLayout->removeWidget(x.second->label);
        if (x.second->ctrl)
            m_formLayout->removeWidget(x.second->ctrl);
        if (x.second->container)
            m_formLayout->removeWidget(x.second->container);
    }

    if (!m_props) {
        obs_log(LOG_WARNING, "[loadProperties] m_props is null, skipping property loading");
        return;
    }

    obs_property_t *prop = obs_properties_first(m_props);
    if (!prop) {
        obs_log(LOG_INFO, "[loadProperties] No properties found");
        return;
    }

    int propertyCount = 0;
    while (prop) {
        propertyCount++;
        obs_log(LOG_DEBUG, "[loadProperties] Processing property %d", propertyCount);

        if (!obs_property_visible(prop)) {
            obs_log(LOG_DEBUG, "[loadProperties] Property %d is not visible, skipping",
                    propertyCount);
            if (!obs_property_next(&prop))
                break;
            continue;
        }

        const char *name_cstr = obs_property_name(prop);
        if (!name_cstr) {
            obs_log(LOG_ERROR, "[loadProperties] Property %d has null name, skipping",
                    propertyCount);
            if (!obs_property_next(&prop))
                break;
            continue;
        }

        std::string name(name_cstr);
        if (name == "service" || name == "show_all") {
            if (!obs_property_next(&prop))
                break;
            continue;
        }
        obs_log(LOG_DEBUG, "[loadProperties] Processing property: %s", name.c_str());

        auto it = origPropWidgets.find(name);
        if (it == origPropWidgets.end()) {
            obs_log(LOG_DEBUG, "[loadProperties] Creating new widget for property: %s",
                    name.c_str());
            try {
                auto newWidget = std::make_shared<OneSevenLivePropertyWidget>(this, this, prop);
                if (!newWidget || !newWidget->label || !newWidget->ctrl) {
                    obs_log(LOG_ERROR, "[loadProperties] Failed to create widget for property: %s",
                            name.c_str());
                    if (!obs_property_next(&prop))
                        break;
                    continue;
                }
                newWidget->LoadData(m_settings);
                m_propertyWidgets.insert(std::make_pair(newWidget->name, newWidget));
                if (newWidget->container)
                    m_formLayout->addWidget(newWidget->container);
                else
                    m_formLayout->addRow(newWidget->label, newWidget->ctrl);
                obs_log(LOG_DEBUG, "[loadProperties] Successfully created widget for property: %s",
                        name.c_str());
            } catch (const std::exception &e) {
                obs_log(LOG_ERROR, "[loadProperties] Exception creating widget for property %s: %s",
                        name.c_str(), e.what());
                if (!obs_property_next(&prop))
                    break;
                continue;
            }
        } else {
            obs_log(LOG_DEBUG, "[loadProperties] Reusing existing widget for property: %s",
                    name.c_str());
            try {
                it->second->ReloadProperty(prop);
                it->second->LoadData(m_settings);
                m_propertyWidgets.insert(std::make_pair(it->first, it->second));
                if (it->second->container)
                    m_formLayout->addWidget(it->second->container);
                else
                    m_formLayout->addRow(it->second->label, it->second->ctrl);
                obs_log(LOG_DEBUG, "[loadProperties] Successfully reused widget for property: %s",
                        name.c_str());
            } catch (const std::exception &e) {
                obs_log(LOG_ERROR, "[loadProperties] Exception reusing widget for property %s: %s",
                        name.c_str(), e.what());
                if (!obs_property_next(&prop))
                    break;
                continue;
            }
        }

        if (!obs_property_next(&prop)) {
            obs_log(LOG_DEBUG, "[loadProperties] Reached end of properties");
            break;
        }

        if (propertyCount > 100) {
            obs_log(LOG_ERROR,
                    "[loadProperties] Too many properties (%d), breaking to prevent infinite loop",
                    propertyCount);
            break;
        }
    }

    obs_log(LOG_INFO, "[loadProperties] Processed %d properties successfully", propertyCount);
}

void OneSevenLivePropertiesWidget::UpdateProperties(obs_data_t *settings, obs_properties_t *props) {
    obs_log(LOG_INFO, "[UpdateProperties] Starting with settings: %p, props: %p", (void *) settings,
            (void *) props);

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

    obs_log(LOG_DEBUG, "[UpdateProperties] Cleaning up %zu existing property widgets",
            m_propertyWidgets.size());
    for (auto &kv : m_propertyWidgets) {
        if (kv.second) {
            if (kv.second->label) {
                m_formLayout->removeWidget(kv.second->label);
                kv.second->label->deleteLater();
            }
            if (kv.second->ctrl) {
                m_formLayout->removeWidget(kv.second->ctrl);
                kv.second->ctrl->deleteLater();
            }
            if (kv.second->container) {
                m_formLayout->removeWidget(kv.second->container);
                kv.second->container->deleteLater();
            }
        }
    }

    m_propertyWidgets.clear();

    obs_log(LOG_DEBUG, "[UpdateProperties] Releasing previous OBS objects");

    // Release previous OBS objects
    if (m_props) {
        obs_log(LOG_DEBUG, "[UpdateProperties] Destroying previous props: %p", (void *) m_props);
        obs_properties_destroy(m_props);
        m_props = nullptr;
    }
    if (m_settings) {
        obs_log(LOG_DEBUG, "[UpdateProperties] Releasing previous settings: %p",
                (void *) m_settings);
        obs_data_release(m_settings);
        m_settings = nullptr;
    }
    if (m_origSettings) {
        obs_log(LOG_DEBUG, "[UpdateProperties] Releasing previous origSettings: %p",
                (void *) m_origSettings);
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

    obs_log(LOG_DEBUG, "[UpdateProperties] Building property controls");

    // Build property controls if properties provided
    if (m_props) {
        try {
            obs_log(LOG_DEBUG, "[UpdateProperties] Applying settings to properties");
            obs_properties_apply_settings(m_props, m_settings);

            obs_log(LOG_DEBUG, "[UpdateProperties] Loading properties");
            loadProperties();

            obs_log(LOG_DEBUG, "[UpdateProperties] Loading data for %zu property widgets",
                    m_propertyWidgets.size());
            for (auto &kv : m_propertyWidgets) {
                if (kv.second) {
                    kv.second->LoadData(m_settings);
                }
            }
            obs_log(LOG_DEBUG, "[UpdateProperties] All property widgets loaded successfully");
        } catch (const std::exception &e) {
            obs_log(LOG_ERROR, "[UpdateProperties] Exception during property loading: %s",
                    e.what());
        } catch (...) {
            obs_log(LOG_ERROR, "[UpdateProperties] Unknown exception during property loading");
        }
    } else {
        obs_log(LOG_WARNING,
                "[UpdateProperties] No properties provided, skipping property loading");
    }

    obs_log(LOG_INFO, "[UpdateProperties] Completed successfully");
}

nlohmann::json OneSevenLivePropertiesWidget::SaveData() {
    for (auto &kv : m_propertyWidgets) {
        if (kv.second) {
            kv.second->SaveData(m_settings);
        }
    }

    auto jsonstr = obs_data_get_json(m_settings);
    obs_log(LOG_DEBUG, "[SaveData] Saving data to JSON: %s", jsonstr);
    if (!jsonstr)
        return {};
    try {
        return nlohmann::json::parse(jsonstr);
    } catch (const nlohmann::json::parse_error &e) {
        obs_log(LOG_ERROR, "[SaveData] JSON parse error: %s", e.what());
        return {};
    }
}

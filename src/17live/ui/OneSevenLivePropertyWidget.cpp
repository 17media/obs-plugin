#include "OneSevenLivePropertyWidget.hpp"

#include <obs.h>

#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QIntValidator>
#include <QDoubleValidator>

#include "OneSevenLivePropertyRefreshHandler.hpp"

#include "plugin-support.h"

OneSevenLivePropertyWidget::OneSevenLivePropertyWidget(QWidget *parent, OneSevenLivePropertyRefreshHandler *refreshHandler, obs_property *property)
  : QWidget(parent), m_refreshHandler(refreshHandler), m_property(property)
{
  name = obs_property_name(m_property);

  const char *desc = obs_property_description(m_property);
  if (!desc)
    desc = obs_property_name(m_property);

  label = new QLabel(desc);

  m_propertyType = obs_property_get_type(property);

  switch (m_propertyType) {
    case OBS_PROPERTY_BOOL: {
        auto cb = new QCheckBox(parent);
        QObject::connect(cb, &QCheckBox::stateChanged, [this]() {
            if (m_refreshHandler)
                m_refreshHandler->RefreshUI();
        });
        ctrl = cb;
        break;
    }
    case OBS_PROPERTY_INT: {
        auto le = new QLineEdit(parent);
        le->setValidator(new QIntValidator(le));
        ctrl = le;
        break;
    }
    case OBS_PROPERTY_FLOAT: {
        auto le = new QLineEdit(parent);
        le->setValidator(new QDoubleValidator(le));
        ctrl = le;
        break;
    }
    case OBS_PROPERTY_TEXT: {
        auto le = new QLineEdit(parent);
        if (obs_property_text_type(property) == OBS_TEXT_PASSWORD)
            le->setEchoMode(QLineEdit::Password);
        ctrl = le;
        break;
    }
    case OBS_PROPERTY_LIST: {
        auto cb = new QComboBox(parent);
        QObject::connect(cb, &QComboBox::currentIndexChanged, [this]() {
            if (m_refreshHandler)
                m_refreshHandler->RefreshUI();
        });
        ctrl = cb;
        break;
    }
    default:
        ctrl = new QLabel("Unsupported", parent);
        break;
  }

  QObject::connect(label, &QObject::destroyed, [this]() {
      label = nullptr;
  });
  QObject::connect(ctrl, &QObject::destroyed, [this]() {
      ctrl = nullptr;
  });

  ReloadProperty(property);
}

OneSevenLivePropertyWidget::~OneSevenLivePropertyWidget()
{
  // No need to manually delete label and ctrl, as they have a parent and Qt will manage them automatically.
  // Manual deletion could lead to double-free crashes.
  obs_log(LOG_DEBUG, "[~OneSevenLivePropertyWidget] Destructor called for property: %s", name.c_str());
}

void OneSevenLivePropertyWidget::ReloadProperty(obs_property *property)
{
  if (!property)
    return;
  m_property = property;
  if (obs_property_get_type(property) == m_propertyType) {
      switch (m_propertyType) {
          case OBS_PROPERTY_LIST: {
              auto cb = static_cast<QComboBox*>(ctrl);
              if (!cb) break;
              for (int i = cb->count() - 1; i >= 0; --i)
                  cb->removeItem(i);
              m_comboFormat = obs_property_list_format(property);
              const size_t cnt = obs_property_list_item_count(property);
              for (size_t i = 0; i < cnt; ++i) {
                  const char *itemname = obs_property_list_item_name(property, i);
                  QVariant data;
                  if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_INT)
                      data = obs_property_list_item_int(property, i);
                  else if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_FLOAT)
                      data = obs_property_list_item_float(property, i);
                  else if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_STRING)
                      data = QString(obs_property_list_item_string(property, i));
                  cb->addItem(itemname ? itemname : "", data);
              }
              break;
          }
          default:
            //   obs_log(LOG_WARNING, "ReloadProperty did not handle property of type %d", (int)m_propertyType);
              break;
      }
  }
}

void OneSevenLivePropertyWidget::LoadData(obs_data_t *settings)
{
  if (!settings || !ctrl)
    return;
  switch (m_propertyType) {
    case OBS_PROPERTY_BOOL: {
        auto cb = static_cast<QCheckBox*>(ctrl);
        bool v = obs_data_get_bool(settings, name.c_str());
        cb->setChecked(v);
        break;
    }
    case OBS_PROPERTY_INT: {
        auto le = static_cast<QLineEdit*>(ctrl);
        int v = (int)obs_data_get_int(settings, name.c_str());
        le->setText(QString::number(v));
        break;
    }
    case OBS_PROPERTY_FLOAT: {
        auto le = static_cast<QLineEdit*>(ctrl);
        double v = obs_data_get_double(settings, name.c_str());
        le->setText(QString::number(v));
        break;
    }
    case OBS_PROPERTY_TEXT: {
        auto le = static_cast<QLineEdit*>(ctrl);
        const char *str = obs_data_get_string(settings, name.c_str());
        le->setText(QString(str ? str : ""));
        break;
    }
    case OBS_PROPERTY_LIST: {
        auto cb = static_cast<QComboBox*>(ctrl);
        if (!cb) break;
        int indexToSelect = -1;
        if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_INT) {
            int target = (int)obs_data_get_int(settings, name.c_str());
            for (int i = 0; i < cb->count(); ++i) {
                if (cb->itemData(i).toInt() == target) { indexToSelect = i; break; }
            }
        } else if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_FLOAT) {
            double target = obs_data_get_double(settings, name.c_str());
            for (int i = 0; i < cb->count(); ++i) {
                if (cb->itemData(i).toDouble() == target) { indexToSelect = i; break; }
            }
        } else if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_STRING) {
            const char *target = obs_data_get_string(settings, name.c_str());
            for (int i = 0; i < cb->count(); ++i) {
                if (cb->itemData(i).toString() == QString(target ? target : "")) { indexToSelect = i; break; }
            }
        }
        if (indexToSelect >= 0) cb->setCurrentIndex(indexToSelect);
        break;
    }
    default:
        break;
  }
}

void OneSevenLivePropertyWidget::SaveData(obs_data_t *settings)
{
  if (!settings || !ctrl)
    return;
  switch (m_propertyType) {
    case OBS_PROPERTY_BOOL: {
        auto cb = static_cast<QCheckBox*>(ctrl);
        obs_data_set_bool(settings, name.c_str(), cb->isChecked());
        break;
    }
    case OBS_PROPERTY_INT: {
        auto le = static_cast<QLineEdit*>(ctrl);
        bool ok = false;
        int v = le->text().toInt(&ok);
        if (ok) obs_data_set_int(settings, name.c_str(), v);
        break;
    }
    case OBS_PROPERTY_FLOAT: {
        auto le = static_cast<QLineEdit*>(ctrl);
        bool ok = false;
        double v = le->text().toDouble(&ok);
        if (ok) obs_data_set_double(settings, name.c_str(), v);
        break;
    }
    case OBS_PROPERTY_TEXT: {
        auto le = static_cast<QLineEdit*>(ctrl);
        obs_data_set_string(settings, name.c_str(), le->text().toUtf8().constData());
        break;
    }
    case OBS_PROPERTY_LIST: {
        auto cb = static_cast<QComboBox*>(ctrl);
        if (!cb) break;
        QVariant data = cb->currentData();
        if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_INT) {
            obs_data_set_int(settings, name.c_str(), data.toInt());
        } else if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_FLOAT) {
            obs_data_set_double(settings, name.c_str(), data.toDouble());
        } else if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_STRING) {
            QString s = data.toString();
            obs_data_set_string(settings, name.c_str(), s.toUtf8().constData());
        }
        break;
    }
    default:
        break;
  }
}

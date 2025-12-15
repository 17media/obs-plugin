#include "OneSevenLivePropertyWidget.hpp"

#include <obs.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleValidator>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QTimer>

#include "OneSevenLiveLineEditWithEye.hpp"
#include "OneSevenLivePropertyRefreshHandler.hpp"
#include "plugin-support.h"

OneSevenLivePropertyWidget::OneSevenLivePropertyWidget(
    QWidget *parent, OneSevenLivePropertyRefreshHandler *refreshHandler, obs_property *property)
    : QWidget(parent), m_refreshHandler(refreshHandler), m_property(property) {
    name = obs_property_name(m_property);

    const char *desc = obs_property_description(m_property);
    if (!desc)
        desc = obs_property_name(m_property);

    obs_log(LOG_DEBUG, "[OneSevenLivePropertyWidget] Constructor called for property: %s [%s]",
            name.c_str(), desc);

    label = new QLabel(desc, this);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_propertyType = obs_property_get_type(property);

    switch (m_propertyType) {
    case OBS_PROPERTY_BOOL: {
        auto cb = new QCheckBox(this);
        // Defer signal connection to avoid triggering recursive RefreshUI calls during construction
        QPointer<QCheckBox> safeCb(cb);
        QPointer<OneSevenLivePropertyWidget> safeThis(this);
        QTimer::singleShot(0, [this, safeCb, safeThis]() {
            if (!safeCb || !safeThis)
                return;

            if (safeThis && safeThis->m_refreshHandler) {
                QObject::connect(safeCb, &QCheckBox::stateChanged, [safeThis]() {
                    if (safeThis && safeThis->m_refreshHandler)
                        safeThis->m_refreshHandler->RefreshUI();
                });
            }
        });
        ctrl = cb;

        container = new QWidget(this);
        QHBoxLayout *hl = new QHBoxLayout(container);
        hl->addWidget(label);
        hl->addStretch();
        hl->addWidget(ctrl);

        QVBoxLayout *vl = new QVBoxLayout(this);
        vl->addWidget(container);

        break;
    }
    case OBS_PROPERTY_INT: {
        auto le = new QLineEdit(this);
        le->setValidator(new QIntValidator(le));
        le->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        ctrl = le;
        break;
    }
    case OBS_PROPERTY_FLOAT: {
        auto le = new QLineEdit(this);
        le->setValidator(new QDoubleValidator(le));
        le->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        ctrl = le;
        break;
    }
    case OBS_PROPERTY_TEXT: {
        if (obs_property_text_type(property) == OBS_TEXT_PASSWORD) {
            auto le = new OneSevenLiveLineEditWithEye(this);
            ctrl = static_cast<QWidget *>(le);
            m_isPassword = true;
        } else {
            auto le = new QLineEdit(this);
            ctrl = le;
        }
        break;
    }
    case OBS_PROPERTY_LIST: {
        auto cb = new QComboBox(this);
        cb->setEditable(false);
        cb->setInsertPolicy(QComboBox::NoInsert);

        QPointer<QComboBox> safeCb(cb);
        QPointer<OneSevenLivePropertyWidget> safeThis(this);
        QTimer::singleShot(0, [safeCb, safeThis]() {
            if (!safeCb || !safeThis)
                return;

            if (safeThis && safeThis->m_refreshHandler) {
                QObject::connect(safeCb, &QComboBox::currentIndexChanged, [safeThis]() {
                    if (safeThis && safeThis->m_refreshHandler)
                        safeThis->m_refreshHandler->RefreshUI();
                });
            }
        });
        ctrl = cb;
        break;
    }
    default:
        ctrl = new QLabel("Unsupported", this);
        break;
    }

    QObject::connect(label, &QObject::destroyed, [this]() { label = nullptr; });
    QObject::connect(ctrl, &QObject::destroyed, [this]() { ctrl = nullptr; });

    ReloadProperty(property);
}

OneSevenLivePropertyWidget::~OneSevenLivePropertyWidget() {
    obs_log(LOG_DEBUG, "[~OneSevenLivePropertyWidget] Destructor called for property: %s",
            name.c_str());

    // Disconnect all signals to prevent crashes during destruction
    disconnect(this);

    if (ctrl) {
        disconnect(ctrl, nullptr, nullptr, nullptr);
    }

    if (label) {
        disconnect(label, nullptr, nullptr, nullptr);
    }

    ctrl = nullptr;
    label = nullptr;
    container = nullptr;
}

void OneSevenLivePropertyWidget::ReloadProperty(obs_property *property) {
    if (!property)
        return;
    m_property = property;
    if (obs_property_get_type(property) == m_propertyType) {
        switch (m_propertyType) {
        case OBS_PROPERTY_LIST: {
            auto cb = qobject_cast<QComboBox *>(ctrl);
            if (!cb)
                break;
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
            //   obs_log(LOG_WARNING, "ReloadProperty did not handle property of type %d",
            //   (int)m_propertyType);
            break;
        }
    }
}

void OneSevenLivePropertyWidget::LoadData(obs_data_t *settings) {
    if (!settings || !ctrl)
        return;
    switch (m_propertyType) {
    case OBS_PROPERTY_BOOL: {
        auto cb = qobject_cast<QCheckBox *>(ctrl);
        if (cb) {
            bool v = obs_data_get_bool(settings, name.c_str());
            cb->setChecked(v);
        }
        break;
    }
    case OBS_PROPERTY_INT: {
        auto le = qobject_cast<QLineEdit *>(ctrl);
        if (le) {
            int v = (int) obs_data_get_int(settings, name.c_str());
            le->setText(QString::number(v));
        }
        break;
    }
    case OBS_PROPERTY_FLOAT: {
        auto le = qobject_cast<QLineEdit *>(ctrl);
        if (le) {
            double v = obs_data_get_double(settings, name.c_str());
            le->setText(QString::number(v));
        }
        break;
    }
    case OBS_PROPERTY_TEXT: {
        if (m_isPassword) {
            auto le = qobject_cast<OneSevenLiveLineEditWithEye *>(ctrl);
            if (le) {
                const char *str = obs_data_get_string(settings, name.c_str());
                le->setText(QString(str ? str : ""));
            }
        } else {
            auto le = qobject_cast<QLineEdit *>(ctrl);
            if (le) {
                const char *str = obs_data_get_string(settings, name.c_str());
                le->setText(QString(str ? str : ""));
            }
        }
        break;
    }
    case OBS_PROPERTY_LIST: {
        auto cb = qobject_cast<QComboBox *>(ctrl);
        if (!cb)
            break;
        int indexToSelect = -1;
        if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_INT) {
            int target = (int) obs_data_get_int(settings, name.c_str());
            for (int i = 0; i < cb->count(); ++i) {
                if (cb->itemData(i).toInt() == target) {
                    indexToSelect = i;
                    break;
                }
            }
        } else if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_FLOAT) {
            double target = obs_data_get_double(settings, name.c_str());
            for (int i = 0; i < cb->count(); ++i) {
                if (cb->itemData(i).toDouble() == target) {
                    indexToSelect = i;
                    break;
                }
            }
        } else if (m_comboFormat == obs_combo_format::OBS_COMBO_FORMAT_STRING) {
            const char *target = obs_data_get_string(settings, name.c_str());
            for (int i = 0; i < cb->count(); ++i) {
                if (cb->itemData(i).toString() == QString(target ? target : "")) {
                    indexToSelect = i;
                    break;
                }
            }
        }
        if (indexToSelect >= 0)
            cb->setCurrentIndex(indexToSelect);
        break;
    }
    default:
        break;
    }
}

void OneSevenLivePropertyWidget::SaveData(obs_data_t *settings) {
    obs_log(LOG_DEBUG, "Saving property %s", name.c_str());
    if (!settings || !ctrl)
        return;
    obs_log(LOG_DEBUG, "Saving property %s as %d", name.c_str(), (int) m_propertyType);
    switch (m_propertyType) {
    case OBS_PROPERTY_BOOL: {
        auto cb = qobject_cast<QCheckBox *>(ctrl);
        if (cb)
            obs_data_set_bool(settings, name.c_str(), cb->isChecked());
        break;
    }
    case OBS_PROPERTY_INT: {
        auto le = qobject_cast<QLineEdit *>(ctrl);
        if (le) {
            bool ok = false;
            int v = le->text().toInt(&ok);
            if (ok)
                obs_data_set_int(settings, name.c_str(), v);
        }
        break;
    }
    case OBS_PROPERTY_FLOAT: {
        auto le = qobject_cast<QLineEdit *>(ctrl);
        if (le) {
            bool ok = false;
            double v = le->text().toDouble(&ok);
            if (ok)
                obs_data_set_double(settings, name.c_str(), v);
        }
        break;
    }
    case OBS_PROPERTY_TEXT: {
        obs_log(LOG_DEBUG, "Saving property %s as string", name.c_str());
        if (m_isPassword) {
            auto le = qobject_cast<OneSevenLiveLineEditWithEye *>(ctrl);
            if (le)
                obs_data_set_string(settings, name.c_str(), le->text().toUtf8().constData());
        } else {
            auto le = qobject_cast<QLineEdit *>(ctrl);
            if (le)
                obs_data_set_string(settings, name.c_str(), le->text().toUtf8().constData());
        }
        break;
    }
    case OBS_PROPERTY_LIST: {
        auto cb = qobject_cast<QComboBox *>(ctrl);
        if (!cb)
            break;
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

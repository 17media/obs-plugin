#include "Meta.hpp"

#include <QFile>
#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include "Common.hpp"
#include "json11.hpp"
#include "obs-module.h"

using namespace json11;

using namespace std;

// Global meta data, loaded at program startup
OneSevenLiveMetaData metaData;

bool JsonToOneSevenLiveMetaData(const Json& json, OneSevenLiveMetaData& metaData) {
    if (!json.is_object()) {
        return false;
    }

    // Iterate through all key-value pairs of JSON object
    for (const auto& pair : json.object_items()) {
        const std::string& key = pair.first;
        const Json& value = pair.second;

        if (value.is_array()) {
            // Handle array type
            QVariantList variantList;
            for (const auto& item : value.array_items()) {
                if (item.is_object()) {
                    // Handle object array
                    QVariantMap variantMap;
                    for (const auto& objPair : item.object_items()) {
                        variantMap[QString::fromStdString(objPair.first)] =
                            QString::fromStdString(objPair.second.string_value());
                    }
                    variantList.append(variantMap);
                } else if (item.is_string()) {
                    // Handle string array
                    variantList.append(QString::fromStdString(item.string_value()));
                } else if (item.is_number()) {
                    // Handle number array
                    variantList.append(item.number_value());
                } else if (item.is_bool()) {
                    // Handle boolean array
                    variantList.append(item.bool_value());
                }
            }
            metaData.data[QString::fromStdString(key)] = variantList;
        } else if (value.is_object()) {
            // Handle object type
            QVariantMap variantMap;
            for (const auto& objPair : value.object_items()) {
                variantMap[QString::fromStdString(objPair.first)] =
                    QString::fromStdString(objPair.second.string_value());
            }
            metaData.data[QString::fromStdString(key)] = variantMap;
        } else if (value.is_string()) {
            // Handle string type
            metaData.data[QString::fromStdString(key)] =
                QString::fromStdString(value.string_value());
        } else if (value.is_number()) {
            // Handle number type
            metaData.data[QString::fromStdString(key)] = value.number_value();
        } else if (value.is_bool()) {
            // Handle boolean type
            metaData.data[QString::fromStdString(key)] = value.bool_value();
        }
    }

    return true;
}

Json OneSevenLiveMetaDataToJson(const OneSevenLiveMetaData& metaData) {
    Json::object json;

    for (auto it = metaData.data.constBegin(); it != metaData.data.constEnd(); ++it) {
        const QString& key = it.key();
        const QVariant& value = it.value();
        int typeId = value.metaType().id();

        if (typeId == QMetaType::QVariantList) {
            QVariantList list = value.toList();
            std::vector<Json> jsonArray;

            for (const QVariant& item : list) {
                int itemTypeId = item.metaType().id();

                if (itemTypeId == QMetaType::QVariantMap) {
                    QVariantMap map = item.toMap();
                    Json::object jsonObj;

                    for (auto mapIt = map.constBegin(); mapIt != map.constEnd(); ++mapIt) {
                        jsonObj[mapIt.key().toStdString()] = mapIt.value().toString().toStdString();
                    }

                    jsonArray.push_back(Json(jsonObj));
                } else if (itemTypeId == QMetaType::QString) {
                    jsonArray.push_back(Json(item.toString().toStdString()));
                } else if (item.canConvert<double>()) {
                    jsonArray.push_back(Json(item.toDouble()));
                } else if (itemTypeId == QMetaType::Bool) {
                    jsonArray.push_back(Json(item.toBool()));
                }
            }

            json[key.toStdString()] = Json(jsonArray);
        } else if (typeId == QMetaType::QVariantMap) {
            QVariantMap map = value.toMap();
            Json::object jsonObj;

            for (auto mapIt = map.constBegin(); mapIt != map.constEnd(); ++mapIt) {
                jsonObj[mapIt.key().toStdString()] = Json(mapIt.value().toString().toStdString());
            }

            json[key.toStdString()] = jsonObj;
        } else if (typeId == QMetaType::QString) {
            json[key.toStdString()] = value.toString().toStdString();
        } else if (value.canConvert<double>()) {
            json[key.toStdString()] = value.toDouble();
        } else if (typeId == QMetaType::Bool) {
            json[key.toStdString()] = value.toBool();
        }
    }

    return Json(json);
}

bool LoadMetaData() {
    string dataPath = obs_get_module_data_path(obs_current_module());
    string metaDataPath = dataPath + "/meta_" + GetCurrentLanguage() + ".json";
    QFile file(QString::fromStdString(metaDataPath));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    string error;
    Json json = Json::parse(content.toStdString(), error);
    if (!error.empty()) {
        return false;
    }
    if (!JsonToOneSevenLiveMetaData(json, metaData)) {
        return false;
    }
    return true;
}

bool SaveMetaData() {
    string dataPath = obs_get_module_data_path(obs_current_module());
    string metaDataPath = dataPath + "/meta_" + GetCurrentLanguage() + ".json";

    QString metaFile = QString::fromStdString(metaDataPath);
    QFile file(metaFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    Json jsonObj = OneSevenLiveMetaDataToJson(metaData);
    QTextStream out(&file);
    out << QString::fromStdString(jsonObj.dump());
    file.close();
    return true;
}

bool getMetaValueLabelList(const QString& key, QList<OneSevenLiveMetaValueLabel>& result) {
    QVariant value = metaData.data[key];
    if (value.metaType().id() == QMetaType::QVariantList) {
        result = metaData.getMetaValueLabel(key);
        return true;
    }

    return false;
}

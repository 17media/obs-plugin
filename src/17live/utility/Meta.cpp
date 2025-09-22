#include "Meta.hpp"

#include <QFile>
#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <nlohmann/json.hpp>

#include "Common.hpp"
#include "obs-module.h"

using Json = nlohmann::json;

using namespace std;

// Global meta data, loaded at program startup
OneSevenLiveMetaData metaData;

bool JsonToOneSevenLiveMetaData(const Json& json, OneSevenLiveMetaData& metaData) {
    try {
        if (!json.is_object()) {
            return false;
        }

        // Reserve space for better performance
        const auto& jsonObj = json.items();

        // Iterate through all key-value pairs of JSON object
        for (const auto& pair : jsonObj) {
            const std::string& key = pair.key();
            const Json& value = pair.value();

            // Convert key once and reuse
            const QString qKey = QString::fromStdString(key);

            if (value.is_array()) {
                // Handle array type
                const auto& arrayItems = value;
                QVariantList variantList;
                variantList.reserve(arrayItems.size());  // Reserve space

                for (const auto& item : arrayItems) {
                    if (item.is_object()) {
                        // Handle object array
                        const auto& objItems = item.items();
                        QVariantMap variantMap;

                        for (const auto& objPair : objItems) {
                            const QString objKey = QString::fromStdString(objPair.key());
                            const QString objValue =
                                QString::fromStdString(objPair.value().get<std::string>());
                            variantMap[objKey] = objValue;
                        }
                        variantList.append(variantMap);
                    } else if (item.is_string()) {
                        // Handle string array
                        variantList.append(QString::fromStdString(item.get<std::string>()));
                    } else if (item.is_number()) {
                        // Handle number array
                        variantList.append(item.get<double>());
                    } else if (item.is_boolean()) {
                        // Handle boolean array
                        variantList.append(item.get<bool>());
                    }
                }
                metaData.data[qKey] = variantList;
            } else if (value.is_object()) {
                // Handle object type
                const auto& objItems = value.items();
                QVariantMap variantMap;

                for (const auto& objPair : objItems) {
                    const QString objKey = QString::fromStdString(objPair.key());
                    const QString objValue =
                        QString::fromStdString(objPair.value().get<std::string>());
                    variantMap[objKey] = objValue;
                }
                metaData.data[qKey] = variantMap;
            } else if (value.is_string()) {
                // Handle string type
                metaData.data[qKey] = QString::fromStdString(value.get<std::string>());
            } else if (value.is_number()) {
                // Handle number type
                metaData.data[qKey] = value.get<double>();
            } else if (value.is_boolean()) {
                // Handle boolean type
                metaData.data[qKey] = value.get<bool>();
            }
        }

        return true;
    } catch (const std::exception& e) {
        blog(LOG_ERROR, "Exception in JsonToOneSevenLiveMetaData: %s", e.what());
        return false;
    } catch (...) {
        blog(LOG_ERROR, "Unknown exception in JsonToOneSevenLiveMetaData");
        return false;
    }
}

Json OneSevenLiveMetaDataToJson(const OneSevenLiveMetaData& metaData) {
    try {
        Json json = Json::object();

        for (auto it = metaData.data.constBegin(); it != metaData.data.constEnd(); ++it) {
            const QString& key = it.key();
            const QVariant& value = it.value();
            int typeId = value.metaType().id();

            // Convert key once and reuse
            const std::string stdKey = key.toStdString();

            if (typeId == QMetaType::QVariantList) {
                const QVariantList list = value.toList();
                std::vector<Json> jsonArray;
                jsonArray.reserve(list.size());  // Reserve space for better performance

                for (const QVariant& item : list) {
                    int itemTypeId = item.metaType().id();

                    if (itemTypeId == QMetaType::QVariantMap) {
                        const QVariantMap map = item.toMap();
                        Json jsonObj = Json::object();

                        for (auto mapIt = map.constBegin(); mapIt != map.constEnd(); ++mapIt) {
                            const std::string mapKey = mapIt.key().toStdString();
                            const std::string mapValue = mapIt.value().toString().toStdString();
                            jsonObj[mapKey] = mapValue;
                        }

                        jsonArray.push_back(jsonObj);
                    } else if (itemTypeId == QMetaType::QString) {
                        jsonArray.push_back(item.toString().toStdString());
                    } else if (item.canConvert<double>()) {
                        jsonArray.push_back(item.toDouble());
                    } else if (itemTypeId == QMetaType::Bool) {
                        jsonArray.push_back(item.toBool());
                    }
                }

                json[stdKey] = jsonArray;
            } else if (typeId == QMetaType::QVariantMap) {
                const QVariantMap map = value.toMap();
                Json jsonObj = Json::object();

                for (auto mapIt = map.constBegin(); mapIt != map.constEnd(); ++mapIt) {
                    const std::string mapKey = mapIt.key().toStdString();
                    const std::string mapValue = mapIt.value().toString().toStdString();
                    jsonObj[mapKey] = mapValue;
                }

                json[stdKey] = jsonObj;
            } else if (typeId == QMetaType::QString) {
                json[stdKey] = value.toString().toStdString();
            } else if (value.canConvert<double>()) {
                json[stdKey] = value.toDouble();
            } else if (typeId == QMetaType::Bool) {
                json[stdKey] = value.toBool();
            }
        }

        return json;
    } catch (const std::exception& e) {
        blog(LOG_ERROR, "Exception in OneSevenLiveMetaDataToJson: %s", e.what());
        return Json();
    } catch (...) {
        blog(LOG_ERROR, "Unknown exception in OneSevenLiveMetaDataToJson");
        return Json();
    }
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
    try {
        Json json = Json::parse(content.toStdString());
        if (!JsonToOneSevenLiveMetaData(json, metaData)) {
            return false;
        }
        return true;
    } catch (const Json::parse_error& e) {
        blog(LOG_ERROR, "JSON parse error in LoadMetaData: %s", e.what());
        return false;
    }
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

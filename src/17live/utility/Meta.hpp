#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "json11.hpp"

using namespace json11;

// Clip permission item structure
struct OneSevenLiveMetaValueLabel {
    QString value;
    QString label;
};

// Metadata structure - using dictionary structure
struct OneSevenLiveMetaData {
    // Use QMap as dictionary structure, key is string, value is QVariant to support different types
    QMap<QString, QVariant> data;

    // Helper method: get value-label list
    QList<OneSevenLiveMetaValueLabel> getMetaValueLabel(const QString &key) const {
        QList<OneSevenLiveMetaValueLabel> result;
        if (data.contains(key)) {
            QVariantList valueList = data[key].toList();
            for (const QVariant &item : valueList) {
                QVariantMap map = item.toMap();
                OneSevenLiveMetaValueLabel dataItem;
                dataItem.value = map["value"].toString();
                dataItem.label = map["label"].toString();
                result.append(dataItem);
            }
        }
        return result;
    }

    // Helper method: set clip permission list
    void setMetaValueLabel(const QString &key, const QList<OneSevenLiveMetaValueLabel> &values) {
        QVariantList valueList;
        for (const OneSevenLiveMetaValueLabel &item : values) {
            QVariantMap map;
            map["value"] = item.value;
            map["label"] = item.label;
            valueList.append(map);
        }
        data[key] = valueList;
    }
};

bool LoadMetaData();
bool SaveMetaData();

// Function declaration to parse JSON to OneSevenLiveMetaData structure
bool JsonToOneSevenLiveMetaData(const Json &json, OneSevenLiveMetaData &metaData);
Json OneSevenLiveMetaDataToJson(const OneSevenLiveMetaData &metaData);

bool getMetaValueLabelList(const QString &key, QList<OneSevenLiveMetaValueLabel> &result);

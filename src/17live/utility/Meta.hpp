#pragma once

#include <QString>
#include <QVariantMap>
#include <QList>
#include <QStringList>

#include "json11.hpp"

using namespace json11;

// 剪辑权限项结构体
struct OneSevenLiveMetaValueLabel {
  QString value;
  QString label;
};

// 元数据结构体 - 使用字典结构
struct OneSevenLiveMetaData {
  // 使用 QMap 作为字典结构，键为字符串，值为 QVariant 以支持不同类型
  QMap<QString, QVariant> data;
  
  // 辅助方法：获取value-label列表
  QList<OneSevenLiveMetaValueLabel> getMetaValueLabel(const QString &key) const {
      QList<OneSevenLiveMetaValueLabel> result;
      if (data.contains(key)) {
          QVariantList valueList = data[key].toList();
          for (const QVariant& item : valueList) {
              QVariantMap map = item.toMap();
              OneSevenLiveMetaValueLabel dataItem;
              dataItem.value = map["value"].toString();
              dataItem.label = map["label"].toString();
              result.append(dataItem);
          }
      }
      return result;
  }
  
  // 辅助方法：设置剪辑权限列表
  void setMetaValueLabel(const QString &key, const QList<OneSevenLiveMetaValueLabel>& values) {
      QVariantList valueList;
      for (const OneSevenLiveMetaValueLabel& item : values) {
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

// 解析JSON到OneSevenLiveMetaData结构体的函数声明
bool JsonToOneSevenLiveMetaData(const Json &json, OneSevenLiveMetaData &metaData);
Json OneSevenLiveMetaDataToJson(const OneSevenLiveMetaData &metaData);

bool getMetaValueLabelList(const QString &key, QList<OneSevenLiveMetaValueLabel> &result);

#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>

class OneSevenLivePreviewConfigLoader : public QObject {
    Q_OBJECT

public:
    struct PreviewConfig {
        QString sourceType;
        QString url;
        QString style;
        int width = 1920;
        int height = 1080;
        int fps = 30;
        bool isValid = false;
    };

    explicit OneSevenLivePreviewConfigLoader(QObject* parent = nullptr);
    ~OneSevenLivePreviewConfigLoader();

    bool loadConfiguration(const QString& configPath);
    PreviewConfig getConfiguration() const;
    bool isConfigurationValid() const;

private:
    PreviewConfig config;
    bool parseJsonConfig(const QJsonObject& jsonObj);
};

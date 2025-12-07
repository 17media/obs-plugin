#include "OneSevenLivePreviewConfigLoader.hpp"

#include <obs-module.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "../../plugin-support.h"
#include "moc_OneSevenLivePreviewConfigLoader.cpp"

OneSevenLivePreviewConfigLoader::OneSevenLivePreviewConfigLoader(QObject* parent)
    : QObject(parent) {}

OneSevenLivePreviewConfigLoader::~OneSevenLivePreviewConfigLoader() {}

bool OneSevenLivePreviewConfigLoader::loadConfiguration(const QString& configPath) {
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        obs_log(LOG_WARNING, "Failed to open preview config file: %s",
                configPath.toUtf8().constData());
        return false;
    }

    QByteArray configData = configFile.readAll();
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(configData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        obs_log(LOG_ERROR, "Failed to parse preview config JSON: %s",
                parseError.errorString().toUtf8().constData());
        return false;
    }

    return parseJsonConfig(jsonDoc.object());
}

bool OneSevenLivePreviewConfigLoader::parseJsonConfig(const QJsonObject& jsonObj) {
    config = PreviewConfig();  // Reset config

    if (!jsonObj.contains("browser_source")) {
        obs_log(LOG_ERROR, "Preview config missing browser_source section");
        return false;
    }

    config.sourceType = "browser_source";
    QJsonObject browserObj = jsonObj["browser_source"].toObject();

    config.url = browserObj["url"].toString();
    config.style = browserObj["css"].toString();
    config.width = browserObj["width"].toInt(640);
    config.height = browserObj["height"].toInt(480);
    config.fps = browserObj["fps"].toInt(30);
    config.isValid = true;

    obs_log(LOG_INFO, "Preview config loaded successfully");
    return true;
}

OneSevenLivePreviewConfigLoader::PreviewConfig OneSevenLivePreviewConfigLoader::getConfiguration()
    const {
    return config;
}

bool OneSevenLivePreviewConfigLoader::isConfigurationValid() const {
    return config.isValid;
}

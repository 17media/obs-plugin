#include <obs-module.h>
#include <QDateTime>
#include "17live-api.hpp"

SeventeenLiveAPI& SeventeenLiveAPI::getInstance()
{
    static SeventeenLiveAPI instance;
    return instance;
}

SeventeenLiveAPI::SeventeenLiveAPI()
    : authenticated(false)
{
    blog(LOG_INFO, "Initializing 17LIVE API");
}

bool SeventeenLiveAPI::authenticate(const QString &username, const QString &password)
{
    blog(LOG_INFO, "Authenticating user: %s (%s)", username.toUtf8().constData(), password.toUtf8().constData());
    // TODO: Implement actual authentication
    authenticated = true;
    authToken = QString("dummy_token_%1").arg(username);
    return true;
}

void SeventeenLiveAPI::logout()
{
    blog(LOG_INFO, "Logging out user");
    authenticated = false;
    authToken.clear();
    if (!currentStreamId.isEmpty()) {
        disconnectFromChat();
    }
}

bool SeventeenLiveAPI::isAuthenticated() const
{
    return authenticated;
}

bool SeventeenLiveAPI::updateStreamSettings(const QString &title, const QString &hashtags)
{
    if (!authenticated) {
        blog(LOG_WARNING, "Cannot update stream settings: Not authenticated");
        return false;
    }

    blog(LOG_INFO, "Updating stream settings - Title: %s, Hashtags: %s",
         title.toUtf8().constData(),
         hashtags.toUtf8().constData());
    return true;
}

QString SeventeenLiveAPI::generateStreamKey()
{
    if (!authenticated) {
        blog(LOG_WARNING, "Cannot generate stream key: Not authenticated");
        return QString();
    }

    QString streamKey = QString("dummy_stream_key_%1").arg(QDateTime::currentSecsSinceEpoch());
    blog(LOG_INFO, "Generated stream key: %s", streamKey.toUtf8().constData());
    emit streamKeyGenerated(streamKey);
    return streamKey;
}

bool SeventeenLiveAPI::connectToChat(const QString &streamId)
{
    if (!authenticated) {
        blog(LOG_WARNING, "Cannot connect to chat: Not authenticated");
        return false;
    }

    currentStreamId = streamId;
    blog(LOG_INFO, "Connecting to chat for stream: %s", streamId.toUtf8().constData());
    emit chatConnectionStateChanged(true);
    return true;
}

void SeventeenLiveAPI::disconnectFromChat()
{
    if (!currentStreamId.isEmpty()) {
        blog(LOG_INFO, "Disconnecting from chat");
        currentStreamId.clear();
        emit chatConnectionStateChanged(false);
    }
}

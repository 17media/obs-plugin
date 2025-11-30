#include "OneSevenLiveTwitchChatClient.hpp"

#include <obs-module.h>

#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QTimer>
#include <nlohmann/json.hpp>
#include <vector>

#include "OneSevenLiveCoreManager.hpp"
#include "multi-rtmp/OneSevenLiveMultiRtmpManager.hpp"
#include "plugin-support.h"
#include "websocket/OneSevenLiveWebsocketClient.hpp"
#include "websocket/OneSevenLiveWebsocketServer.hpp"
#include "websocket/WebsocketUtils.hpp"
#include "websocket/WsMessage.hpp"

const QString OneSevenLiveTwitchChatClient::TWITCH_IRC_SERVER = "wss://irc-ws.chat.twitch.tv:443";
const int OneSevenLiveTwitchChatClient::DEFAULT_PING_INTERVAL = 60;   // 1 minute
const int OneSevenLiveTwitchChatClient::DEFAULT_RECONNECT_DELAY = 5;  // 5 seconds
const int OneSevenLiveTwitchChatClient::MAX_RECONNECT_ATTEMPTS = 5;
const int OneSevenLiveTwitchChatClient::STATUS_BROADCAST_INTERVAL = 10;
const int OneSevenLiveTwitchChatClient::LONG_RETRY_DELAY = 600;

// Helper to convert TwitchMessageType to string
static const char* toString(TwitchMessageType type) {
    switch (type) {
    case TwitchMessageType::Chat:
        return "chat";
    case TwitchMessageType::Notice:
        return "notice";
    default:
        return "chat";
    }
}

// Helper to convert TwitchChatMessage to JSON
static nlohmann::json toJson(const TwitchChatMessage& msg) {
    return {{"channel", msg.channel.toStdString()},
            {"username", msg.username.toStdString()},
            {"message", msg.message.toStdString()},
            {"timestamp", msg.timestamp.toString(Qt::ISODate).toStdString()},
            {"type", toString(msg.type)}};
}

OneSevenLiveTwitchChatClient::OneSevenLiveTwitchChatClient(QObject* parent)
    : QObject(parent),
      m_connected(false),
      m_autoReconnect(true),
      m_reconnectDelay(DEFAULT_RECONNECT_DELAY),
      m_pingInterval(DEFAULT_PING_INTERVAL),
      m_reconnectAttempts(0),
      m_maxReconnectAttempts(MAX_RECONNECT_ATTEMPTS),
      m_pingTimer(nullptr),
      m_reconnectTimer(nullptr) {
    // Set up ping timer
    m_pingTimer = new QTimer(this);
    m_pingTimer->setSingleShot(false);
    connect(m_pingTimer, &QTimer::timeout, this, &OneSevenLiveTwitchChatClient::onPingTimeout);

    // Set up reconnect timer
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this,
            &OneSevenLiveTwitchChatClient::attemptReconnect);

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(false);
    m_statusTimer->setInterval(STATUS_BROADCAST_INTERVAL * 1000);
    connect(m_statusTimer, &QTimer::timeout, this, &OneSevenLiveTwitchChatClient::onStatusTimer);
    m_statusTimer->start();
}

OneSevenLiveTwitchChatClient::~OneSevenLiveTwitchChatClient() {
    disconnectFromChat();
    stopPingTimer();

    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
        m_reconnectTimer->deleteLater();
    }
}

void OneSevenLiveTwitchChatClient::connectToChat(const QString& username,
                                                 const QString& oauthToken) {
    if (m_connected) {
        obs_log(LOG_INFO, "Already connected to Twitch chat");
        return;
    }

    m_username = username;
    m_oauthToken = oauthToken;
    m_reconnectAttempts = 0;
    const bool isLive = OneSevenLiveMultiRtmpManager::getInstance()->isPlatformStreaming("Twitch");
    obs_log(LOG_INFO, "Twitch chat connect requested: isLive=%d username=%s", isLive ? 1 : 0,
            username.toUtf8().constData());
    if (!isLive) {
        obs_log(LOG_INFO, "Twitch stream is not live; deferring chat connection");
        return;
    }
    obs_log(LOG_INFO, "Connecting to Twitch chat server: %s",
            TWITCH_IRC_SERVER.toUtf8().constData());
    connectWebSocket();
}

void OneSevenLiveTwitchChatClient::disconnectFromChat() {
    if (!m_connected) {
        return;
    }

    obs_log(LOG_INFO, "Disconnecting from Twitch chat");
    m_autoReconnect = false;  // Prevent auto-reconnect on manual disconnect

    disconnectWebSocket();

    stopPingTimer();
    m_joinedChannels.clear();
}

bool OneSevenLiveTwitchChatClient::isConnected() const {
    return m_connected;
}

void OneSevenLiveTwitchChatClient::joinChannel(const QString& channel) {
    if (!m_connected) {
        obs_log(LOG_WARNING, "Cannot join channel: not connected to chat");
        return;
    }

    QString normalizedChannel = normalizeChannelName(channel);
    if (isChannelJoined(normalizedChannel)) {
        obs_log(LOG_INFO, "Already joined channel: %s", normalizedChannel.toUtf8().constData());
        return;
    }

    sendIRCCommand("JOIN", "#" + normalizedChannel);
    m_joinedChannels.append(normalizedChannel);

    obs_log(LOG_INFO, "Joining channel: %s", normalizedChannel.toUtf8().constData());
}

void OneSevenLiveTwitchChatClient::leaveChannel(const QString& channel) {
    if (!m_connected) {
        obs_log(LOG_WARNING, "Cannot leave channel: not connected to chat");
        return;
    }

    QString normalizedChannel = normalizeChannelName(channel);
    if (!isChannelJoined(normalizedChannel)) {
        obs_log(LOG_INFO, "Not in channel: %s", normalizedChannel.toUtf8().constData());
        return;
    }

    sendIRCCommand("PART", "#" + normalizedChannel);
    m_joinedChannels.removeOne(normalizedChannel);

    obs_log(LOG_INFO, "Leaving channel: %s", normalizedChannel.toUtf8().constData());
    emit channelLeft(normalizedChannel);
}

void OneSevenLiveTwitchChatClient::leaveAllChannels() {
    for (const QString& channel : m_joinedChannels) {
        leaveChannel(channel);
    }
}

QVector<QString> OneSevenLiveTwitchChatClient::getJoinedChannels() const {
    return m_joinedChannels;
}

void OneSevenLiveTwitchChatClient::sendMessage(const QString& channel, const QString& message) {
    if (!m_connected) {
        obs_log(LOG_WARNING, "Cannot send message: not connected to chat");
        return;
    }

    QString normalizedChannel = normalizeChannelName(channel);
    if (!isChannelJoined(normalizedChannel)) {
        obs_log(LOG_WARNING, "Cannot send message: not in channel %s",
                normalizedChannel.toUtf8().constData());
        return;
    }

    sendIRCCommand("PRIVMSG", "#" + normalizedChannel + " :" + message);
    obs_log(LOG_INFO, "Sending message to %s: %s", normalizedChannel.toUtf8().constData(),
            message.toUtf8().constData());
}

void OneSevenLiveTwitchChatClient::sendWhisper(const QString& username, const QString& message) {
    if (!m_connected) {
        obs_log(LOG_WARNING, "Cannot send whisper: not connected to chat");
        return;
    }

    // Note: Whisper functionality requires special permissions and may not work with all tokens
    sendIRCCommand("PRIVMSG", "#jtv :/w " + username + " " + message);
    obs_log(LOG_INFO, "Sending whisper to %s: %s", username.toUtf8().constData(),
            message.toUtf8().constData());
}

void OneSevenLiveTwitchChatClient::setAutoReconnect(bool enabled) {
    m_autoReconnect = enabled;
}

void OneSevenLiveTwitchChatClient::setReconnectDelay(int seconds) {
    m_reconnectDelay = seconds;
}

void OneSevenLiveTwitchChatClient::setPingInterval(int seconds) {
    m_pingInterval = seconds;
}

// WebSocket event handlers
void OneSevenLiveTwitchChatClient::onWebSocketMessage(const std::string& message) {
    QString qMessage = QString::fromStdString(message);
    parseIRCMessage(qMessage);

    if (qMessage.contains(" PONG ") || qMessage.startsWith(":tmi.twitch.tv PONG")) {
        obs_log(LOG_DEBUG, "Received PONG from Twitch chat");
    } else {
        obs_log(LOG_DEBUG, "Received message from Twitch chat: %s", qMessage.toUtf8().constData());
    }
    OneSevenLiveCoreManager::getInstance().enqueueOrBroadcastChatEvent(
        QString::fromUtf8(ws::EventTwitchChatMessage),
        nlohmann::json{{"raw", qMessage.toStdString()}});
}

void OneSevenLiveTwitchChatClient::onWebSocketOpen() {
    obs_log(LOG_INFO, "Connected to Twitch chat server");
    m_connected = true;
    m_reconnectAttempts = 0;
    m_lastPongTs = QDateTime::currentDateTime();

    // Request capabilities
    requestCapabilities();

    // Authenticate
    authenticate();

    // Start ping timer
    startPingTimer();

    emit connected();
    auto& core = OneSevenLiveCoreManager::getInstance();
    const bool isLive = OneSevenLiveMultiRtmpManager::getInstance()->isPlatformStreaming("Twitch");
    const char* st = (m_connected && isLive) ? "connected" : "break";
    obs_log(LOG_INFO, "Broadcast EventTwitchChatConnected on open: username=%s status=%s isLive=%d m_connected=%d",
            m_username.toUtf8().constData(), st, isLive ? 1 : 0, m_connected ? 1 : 0);
    core.enqueueOrBroadcastChatEvent(
        QString::fromUtf8(ws::EventTwitchChatConnected),
        nlohmann::json{{"username", m_username.toStdString()}, {"status", st}});

    QString channelToJoin = m_targetChannel.isEmpty() ? m_username : m_targetChannel;
    if (!channelToJoin.isEmpty()) {
        joinChannel(channelToJoin);
    }
}

void OneSevenLiveTwitchChatClient::onWebSocketClose() {
    obs_log(LOG_INFO, "Disconnected from Twitch chat server");
    m_connected = false;
    stopPingTimer();

    emit disconnected();
    auto& core = OneSevenLiveCoreManager::getInstance();
    const bool isLive = OneSevenLiveMultiRtmpManager::getInstance()->isPlatformStreaming("Twitch");
    const char* st = (m_connected && isLive) ? "connected" : "break";
    obs_log(LOG_INFO, "Broadcast EventTwitchChatConnected on close: username=%s status=%s isLive=%d m_connected=%d",
            m_username.toUtf8().constData(), st, isLive ? 1 : 0, m_connected ? 1 : 0);
    core.enqueueOrBroadcastChatEvent(
        QString::fromUtf8(ws::EventTwitchChatConnected),
        nlohmann::json{{"username", m_username.toStdString()}, {"status", st}});

    if (m_autoReconnect) {
        scheduleReconnect();
    }
}

void OneSevenLiveTwitchChatClient::onWebSocketError(const std::string& error) {
    QString errorMsg = QString::fromStdString(error);
    obs_log(LOG_WARNING, "WebSocket error: %s", errorMsg.toUtf8().constData());
    emit connectionError(errorMsg);

    if (m_autoReconnect) {
        scheduleReconnect();
    }
}

void OneSevenLiveTwitchChatClient::onPingTimeout() {
    if (m_connected) {
        sendRawMessage("PING :tmi.twitch.tv");
    }
}

void OneSevenLiveTwitchChatClient::attemptReconnect() {
    const bool isLive = OneSevenLiveMultiRtmpManager::getInstance()->isPlatformStreaming("Twitch");
    if (m_connected || !m_autoReconnect || !isLive) {
        return;
    }

    m_reconnectAttempts++;
    obs_log(LOG_INFO, "Attempting reconnection %d of %d", m_reconnectAttempts,
            m_maxReconnectAttempts);

    emit reconnecting(m_reconnectAttempts);

    // Reconnect with saved credentials
    if (!m_username.isEmpty() && !m_oauthToken.isEmpty()) {
        connectToChat(m_username, m_oauthToken);
    }
}

// IRC protocol implementation
void OneSevenLiveTwitchChatClient::sendRawMessage(const QString& message) {
    if (m_connected && m_client && m_client->isConnected()) {
        sendWebSocketMessage(message.toStdString());
        int level = message.startsWith("PING ") ? LOG_DEBUG : LOG_INFO;
        obs_log(level, "IRC ->: %s", message.toUtf8().constData());
    }
}

void OneSevenLiveTwitchChatClient::sendIRCCommand(const QString& command,
                                                  const QString& parameters) {
    sendRawMessage(command + " " + parameters);
}

void OneSevenLiveTwitchChatClient::authenticate() {
    if (!m_oauthToken.isEmpty() && !m_username.isEmpty()) {
        // Send OAuth authentication
        if (m_oauthToken.startsWith("oauth:")) {
            sendRawMessage("PASS " + m_oauthToken);
        } else {
            sendRawMessage("PASS oauth:" + m_oauthToken);
        }
        sendRawMessage("NICK " + m_username);
        obs_log(LOG_INFO, "Authenticating as %s", m_username.toUtf8().constData());
    }
}

void OneSevenLiveTwitchChatClient::requestCapabilities() {
    // Request Twitch-specific capabilities
    sendRawMessage("CAP REQ :twitch.tv/tags");
    sendRawMessage("CAP REQ :twitch.tv/commands");
    sendRawMessage("CAP REQ :twitch.tv/membership");
    obs_log(LOG_INFO, "Requesting Twitch capabilities");
}

void OneSevenLiveTwitchChatClient::startPingTimer() {
    if (m_pingTimer) {
        m_pingTimer->start(m_pingInterval * 1000);
    }
}

void OneSevenLiveTwitchChatClient::stopPingTimer() {
    if (m_pingTimer) {
        m_pingTimer->stop();
    }
}

void OneSevenLiveTwitchChatClient::parseIRCMessage(const QString& rawMessage) {
    obs_log(LOG_DEBUG, "IRC <-: %s", rawMessage.toUtf8().constData());

    QString message = rawMessage;
    QString prefix;
    QString command;
    QString parameters;

    if (message.startsWith('@')) {
        int tagEnd = message.indexOf(' ');
        if (tagEnd != -1) {
            message = message.mid(tagEnd + 1);
        }
    }

    if (message.startsWith(':')) {
        int prefixEnd = message.indexOf(' ');
        if (prefixEnd != -1) {
            prefix = message.mid(1, prefixEnd - 1);
            message = message.mid(prefixEnd + 1);
        }
    }

    int commandEnd = message.indexOf(' ');
    if (commandEnd != -1) {
        command = message.left(commandEnd);
        parameters = message.mid(commandEnd + 1);
    } else {
        command = message;
    }

    if (command == "PING") {
        sendRawMessage("PONG " + parameters);
        return;
    }
    if (command == "PONG") {
        m_lastPongTs = QDateTime::currentDateTime();
        obs_log(LOG_DEBUG, "IRC PONG acknowledged");
        return;
    }
    if (command == "001") {
        emit reconnected();
        return;
    }
    if (command == "JOIN") {
        QString channel = parameters.mid(1);
        QString username = extractUsernameFromPrefix(prefix);
        if (username == m_username) {
            emit channelJoined(channel);
        } else {
            TwitchChatUser user;
            user.username = username;
            emit userJoined(channel, user);
        }
        return;
    }
    if (command == "PART") {
        QString channel = parameters.mid(1);
        QString username = extractUsernameFromPrefix(prefix);
        if (username == m_username) {
            m_joinedChannels.removeOne(channel);
            emit channelLeft(channel);
        } else {
            emit userLeft(channel, username);
        }
        return;
    }
    if (command == "PRIVMSG") {
        TwitchChatMessage chatMessage = parseChatMessage(rawMessage, prefix);
        chatMessage.type = TwitchMessageType::Chat;
        emit messageReceived(chatMessage);
        return;
    }
    if (command == "NOTICE") {
        QString channel = parameters.section(' ', 0, 0).mid(1);
        QString noticeMsg = parameters.section(' ', 1);
        if (noticeMsg.startsWith(':')) {
            noticeMsg = noticeMsg.mid(1);
        }
        emit noticeReceived(channel, noticeMsg);
        return;
    }
}

TwitchChatMessage OneSevenLiveTwitchChatClient::parseChatMessage(const QString& rawMessage,
                                                                 const QString& prefix) {
    TwitchChatMessage message;
    message.timestamp = QDateTime::currentDateTime();

    message.username = extractUsernameFromPrefix(prefix);
    message.displayName = message.username;

    QString params = rawMessage;
    int channelStart = params.indexOf('#');
    int channelEnd = params.indexOf(" :");
    if (channelStart != -1 && channelEnd != -1 && channelEnd > channelStart) {
        message.channel = params.mid(channelStart + 1, channelEnd - channelStart - 1);
        message.message = params.mid(channelEnd + 2);
    }

    return message;
}

QString OneSevenLiveTwitchChatClient::extractUsernameFromPrefix(const QString& prefix) {
    if (prefix.isEmpty()) {
        return QString();
    }

    int exclamation = prefix.indexOf('!');
    if (exclamation != -1) {
        return prefix.left(exclamation);
    }

    return prefix;
}

bool OneSevenLiveTwitchChatClient::isChannelJoined(const QString& channel) const {
    QString normalizedChannel = normalizeChannelName(channel);
    return m_joinedChannels.contains(normalizedChannel);
}

QString OneSevenLiveTwitchChatClient::normalizeChannelName(const QString& channel) const {
    QString normalized = channel.toLower();
    if (normalized.startsWith('#')) {
        normalized = normalized.mid(1);
    }
    return normalized;
}

void OneSevenLiveTwitchChatClient::scheduleReconnect() {
    const bool isLive = OneSevenLiveMultiRtmpManager::getInstance()->isPlatformStreaming("Twitch");
    if (!isLive)
        return;
    if (m_reconnectTimer && !m_reconnectTimer->isActive()) {
        int delay = m_reconnectDelay;
        if (m_reconnectAttempts >= m_maxReconnectAttempts) {
            delay = LONG_RETRY_DELAY;
            m_reconnectAttempts = 0;
        }
        obs_log(LOG_INFO, "Scheduling reconnection in %d seconds", delay);
        m_reconnectTimer->start(delay * 1000);
    }
}

void OneSevenLiveTwitchChatClient::resetReconnectAttempts() {
    m_reconnectAttempts = 0;
}

void OneSevenLiveTwitchChatClient::onStatusTimer() {
    obs_log(LOG_INFO, "StatusTimer broadcast: username=%s status=%s",
            m_username.toUtf8().constData(), m_connected ? "connected" : "break");
    OneSevenLiveCoreManager::getInstance().enqueueOrBroadcastChatEvent(
        QString::fromUtf8(ws::EventTwitchChatConnected),
        nlohmann::json{{"username", m_username.toStdString()},
                       {"status", m_connected ? "connected" : "break"}});

    const bool isLive = OneSevenLiveMultiRtmpManager::getInstance()->isPlatformStreaming("Twitch");
    if (!isLive && m_connected) {
        disconnectWebSocket();
    } else if (isLive && !m_connected && !m_username.isEmpty() && !m_oauthToken.isEmpty()) {
        obs_log(LOG_INFO, "StatusTimer: attempting connect as live=%d username=%s",
                isLive ? 1 : 0, m_username.toUtf8().constData());
        connectToChat(m_username, m_oauthToken);
    }

    if (m_connected) {
        QDateTime now = QDateTime::currentDateTime();
        if (m_lastPongTs.isValid()) {
            int seconds = m_lastPongTs.secsTo(now);
            if (seconds > 2 * m_pingInterval) {
                obs_log(LOG_INFO, "No PONG within %d seconds, reconnecting", seconds);
                disconnectWebSocket();
                if (m_autoReconnect) {
                    scheduleReconnect();
                }
            }
        }
    }
}

void OneSevenLiveTwitchChatClient::connectWebSocket() {
    obs_log(LOG_INFO, "Connecting to Twitch chat server: %s",
            TWITCH_IRC_SERVER.toUtf8().constData());
    m_client = std::make_unique<OneSevenLiveWebsocketClient>(this);
    m_client->setOpenCallback([this]() { onWebSocketOpen(); });
    m_client->setMessageCallback([this](const std::string& m) { onWebSocketMessage(m); });
    m_client->setCloseCallback([this]() { onWebSocketClose(); });
    m_client->setErrorCallback([this](const std::string& e) { onWebSocketError(e); });
    m_client->connectUrl(TWITCH_IRC_SERVER);
}

void OneSevenLiveTwitchChatClient::disconnectWebSocket() {
    obs_log(LOG_INFO, "Disconnecting WebSocket client from Twitch");
    if (m_client) {
        m_client->disconnectAsync();
    }
}

void OneSevenLiveTwitchChatClient::sendWebSocketMessage(const std::string& message) {
    if (m_client && m_client->isConnected()) {
        m_client->sendText(QString::fromStdString(message));
    } else {
        obs_log(LOG_WARNING, "Cannot send WebSocket message: not connected");
    }
}

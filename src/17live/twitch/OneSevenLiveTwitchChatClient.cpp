#include "OneSevenLiveTwitchChatClient.hpp"
#include "deps/IXWebSocket/ixwebsocket/IXWebSocket.h"
#include "../OneSevenLiveCoreManager.hpp"
#include "../OneSevenLiveWebsocketServer.hpp"
#include <QTimer>
#include <QDebug>
#include "plugin-support.h"
#include <obs-module.h>
#include <QRegularExpression>
#include <QDateTime>
#include <nlohmann/json.hpp>

const QString OneSevenLiveTwitchChatClient::TWITCH_IRC_SERVER = "wss://irc-ws.chat.twitch.tv:443";
const int OneSevenLiveTwitchChatClient::DEFAULT_PING_INTERVAL = 60; // 1 minute
const int OneSevenLiveTwitchChatClient::DEFAULT_RECONNECT_DELAY = 5; // 5 seconds
const int OneSevenLiveTwitchChatClient::MAX_RECONNECT_ATTEMPTS = 5;

// Helper to convert TwitchMessageType to string
static const char* toString(TwitchMessageType type) {
    switch (type) {
        case TwitchMessageType::Chat: return "chat";
        case TwitchMessageType::Subscription: return "subscription";
        case TwitchMessageType::Resubscription: return "resubscription";
        case TwitchMessageType::GiftSubscription: return "gift_subscription";
        case TwitchMessageType::Raid: return "raid";
        case TwitchMessageType::Host: return "host";
        case TwitchMessageType::Whisper: return "whisper";
        case TwitchMessageType::Notice: return "notice";
        case TwitchMessageType::UserNotice: return "user_notice";
        case TwitchMessageType::RoomState: return "room_state";
        case TwitchMessageType::UserState: return "user_state";
        case TwitchMessageType::GlobalUserState: return "global_user_state";
        case TwitchMessageType::Unknown: default: return "unknown";
    }
}

// Helper to convert TwitchChatMessage to JSON
static nlohmann::json toJson(const TwitchChatMessage& msg) {
    return {
        {"id", msg.id.toStdString()},
        {"channel", msg.channel.toStdString()},
        {"username", msg.username.toStdString()},
        {"displayName", msg.displayName.toStdString()},
        {"message", msg.message.toStdString()},
        {"userId", msg.userId.toStdString()},
        {"color", msg.color.toStdString()},
        {"timestamp", msg.timestamp.toString(Qt::ISODate).toStdString()},
        {"isModerator", msg.isModerator},
        {"isSubscriber", msg.isSubscriber},
        {"isTurbo", msg.isTurbo},
        {"isFirstMessage", msg.isFirstMessage},
        {"isReturningChatter", msg.isReturningChatter},
        {"bits", msg.bits},
        {"emotes", msg.emotes.toStdString()},
        {"badges", msg.badges.toStdString()},
        {"type", toString(msg.type)}
    };
}

OneSevenLiveTwitchChatClient::OneSevenLiveTwitchChatClient(QObject* parent)
    : QObject(parent)
    , m_connected(false)
    , m_autoReconnect(true)
    , m_reconnectDelay(DEFAULT_RECONNECT_DELAY)
    , m_pingInterval(DEFAULT_PING_INTERVAL)
    , m_reconnectAttempts(0)
    , m_maxReconnectAttempts(MAX_RECONNECT_ATTEMPTS)
    , m_pingTimer(nullptr)
    , m_reconnectTimer(nullptr)
{
    m_webSocket = std::make_unique<ix::WebSocket>();
    
    // Set up WebSocket event handlers
    m_webSocket->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        switch (msg->type) {
            case ix::WebSocketMessageType::Message: {
                const std::string text = msg->str;
                QMetaObject::invokeMethod(this, [this, text]() {
                    onWebSocketMessage(text);
                });
                break;
            }
            case ix::WebSocketMessageType::Open:
                QMetaObject::invokeMethod(this, [this]() {
                    onWebSocketOpen();
                });
                break;
            case ix::WebSocketMessageType::Close: {
                const auto code = msg->closeInfo.code;
                const auto reason = msg->closeInfo.reason;
                QMetaObject::invokeMethod(this, [this, code, reason]() {
                    obs_log(LOG_WARNING, "Twitch chat closed. code=%d reason=%s", code, reason.c_str());
                    onWebSocketClose();
                });
                break;
            }
                break;
            case ix::WebSocketMessageType::Error: {
                const std::string reason = msg->errorInfo.reason;
                QMetaObject::invokeMethod(this, [this, reason]() {
                    onWebSocketError(reason);
                });
                break;
            }
            default:
                break;
        }
    });
    
    // Set up ping timer
    m_pingTimer = new QTimer(this);
    m_pingTimer->setSingleShot(false);
    connect(m_pingTimer, &QTimer::timeout, this, &OneSevenLiveTwitchChatClient::onPingTimeout);
    
    // Set up reconnect timer
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &OneSevenLiveTwitchChatClient::attemptReconnect);
}

OneSevenLiveTwitchChatClient::~OneSevenLiveTwitchChatClient()
{
    disconnectFromChat();
    stopPingTimer();
    
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
        m_reconnectTimer->deleteLater();
    }
}

void OneSevenLiveTwitchChatClient::connectToChat(const QString& username, const QString& oauthToken)
{
    if (m_connected) {
        obs_log(LOG_INFO, "Already connected to Twitch chat");
        return;
    }
    
    m_username = username;
    m_oauthToken = oauthToken;
    m_reconnectAttempts = 0;
    
    // Configure WebSocket URL
    m_webSocket->setUrl(TWITCH_IRC_SERVER.toStdString());
    
    // Set connection timeout
    m_webSocket->setPingInterval(m_pingInterval);
    
    obs_log(LOG_INFO, "Connecting to Twitch chat server: %s", TWITCH_IRC_SERVER.toUtf8().constData());
    m_webSocket->start();
}

void OneSevenLiveTwitchChatClient::disconnectFromChat()
{
    if (!m_connected) {
        return;
    }
    
    obs_log(LOG_INFO, "Disconnecting from Twitch chat");
    m_autoReconnect = false; // Prevent auto-reconnect on manual disconnect
    
    if (m_webSocket) {
        m_webSocket->close();
    }
    
    stopPingTimer();
    m_joinedChannels.clear();
}

bool OneSevenLiveTwitchChatClient::isConnected() const
{
    return m_connected;
}

void OneSevenLiveTwitchChatClient::joinChannel(const QString& channel)
{
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

void OneSevenLiveTwitchChatClient::leaveChannel(const QString& channel)
{
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

void OneSevenLiveTwitchChatClient::leaveAllChannels()
{
    for (const QString& channel : m_joinedChannels) {
        leaveChannel(channel);
    }
}

QVector<QString> OneSevenLiveTwitchChatClient::getJoinedChannels() const
{
    return m_joinedChannels;
}

void OneSevenLiveTwitchChatClient::sendMessage(const QString& channel, const QString& message)
{
    if (!m_connected) {
        obs_log(LOG_WARNING, "Cannot send message: not connected to chat");
        return;
    }
    
    QString normalizedChannel = normalizeChannelName(channel);
    if (!isChannelJoined(normalizedChannel)) {
        obs_log(LOG_WARNING, "Cannot send message: not in channel %s", normalizedChannel.toUtf8().constData());
        return;
    }
    
    sendIRCCommand("PRIVMSG", "#" + normalizedChannel + " :" + message);
    obs_log(LOG_INFO, "Sending message to %s: %s", normalizedChannel.toUtf8().constData(), message.toUtf8().constData());
}

void OneSevenLiveTwitchChatClient::sendWhisper(const QString& username, const QString& message)
{
    if (!m_connected) {
        obs_log(LOG_WARNING, "Cannot send whisper: not connected to chat");
        return;
    }
    
    // Note: Whisper functionality requires special permissions and may not work with all tokens
    sendIRCCommand("PRIVMSG", "#jtv :/w " + username + " " + message);
    obs_log(LOG_INFO, "Sending whisper to %s: %s", username.toUtf8().constData(), message.toUtf8().constData());
}

void OneSevenLiveTwitchChatClient::setAutoReconnect(bool enabled)
{
    m_autoReconnect = enabled;
}

void OneSevenLiveTwitchChatClient::setReconnectDelay(int seconds)
{
    m_reconnectDelay = seconds;
}

void OneSevenLiveTwitchChatClient::setPingInterval(int seconds)
{
    m_pingInterval = seconds;
    if (m_webSocket) {
        m_webSocket->setPingInterval(seconds);
    }
}

// WebSocket event handlers
void OneSevenLiveTwitchChatClient::onWebSocketMessage(const std::string& message)
{
    QString qMessage = QString::fromStdString(message);
    parseIRCMessage(qMessage);
}

void OneSevenLiveTwitchChatClient::onWebSocketOpen()
{
    obs_log(LOG_INFO, "Connected to Twitch chat server");
    m_connected = true;
    m_reconnectAttempts = 0;
    
    // Request capabilities
    requestCapabilities();
    
    // Authenticate
    authenticate();
    
    // Start ping timer
    startPingTimer();
    
    emit connected();
}

void OneSevenLiveTwitchChatClient::onWebSocketClose()
{
    obs_log(LOG_INFO, "Disconnected from Twitch chat server");
    m_connected = false;
    stopPingTimer();
    
    emit disconnected();
    
    // Schedule reconnection if auto-reconnect is enabled
    if (m_autoReconnect && m_reconnectAttempts < m_maxReconnectAttempts) {
        scheduleReconnect();
    }
}

void OneSevenLiveTwitchChatClient::onWebSocketError(const std::string& error)
{
    QString errorMsg = QString::fromStdString(error);
    obs_log(LOG_WARNING, "WebSocket error: %s", errorMsg.toUtf8().constData());
    emit connectionError(errorMsg);
    
    // Schedule reconnection if auto-reconnect is enabled
    if (m_autoReconnect && m_reconnectAttempts < m_maxReconnectAttempts) {
        scheduleReconnect();
    }
}

void OneSevenLiveTwitchChatClient::onPingTimeout()
{
    if (m_connected) {
        sendRawMessage("PING :tmi.twitch.tv");
    }
}

void OneSevenLiveTwitchChatClient::attemptReconnect()
{
    if (m_connected || !m_autoReconnect) {
        return;
    }
    
    m_reconnectAttempts++;
    obs_log(LOG_INFO, "Attempting reconnection %d of %d", m_reconnectAttempts, m_maxReconnectAttempts);
    
    emit reconnecting(m_reconnectAttempts);
    
    // Reconnect with saved credentials
    if (!m_username.isEmpty() && !m_oauthToken.isEmpty()) {
        connectToChat(m_username, m_oauthToken);
    }
}

// IRC protocol implementation
void OneSevenLiveTwitchChatClient::sendRawMessage(const QString& message)
{
    if (m_connected && m_webSocket) {
        m_webSocket->send(message.toStdString());
        obs_log(LOG_INFO, "IRC ->: %s", message.toUtf8().constData());
    }
}

void OneSevenLiveTwitchChatClient::sendIRCCommand(const QString& command, const QString& parameters)
{
    sendRawMessage(command + " " + parameters);
}

void OneSevenLiveTwitchChatClient::authenticate()
{
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

void OneSevenLiveTwitchChatClient::requestCapabilities()
{
    // Request Twitch-specific capabilities
    sendRawMessage("CAP REQ :twitch.tv/tags");
    sendRawMessage("CAP REQ :twitch.tv/commands");
    sendRawMessage("CAP REQ :twitch.tv/membership");
    obs_log(LOG_INFO, "Requesting Twitch capabilities");
}

void OneSevenLiveTwitchChatClient::startPingTimer()
{
    if (m_pingTimer) {
        m_pingTimer->start(m_pingInterval * 1000);
    }
}

void OneSevenLiveTwitchChatClient::stopPingTimer()
{
    if (m_pingTimer) {
        m_pingTimer->stop();
    }
}

void OneSevenLiveTwitchChatClient::parseIRCMessage(const QString& rawMessage)
{
    obs_log(LOG_INFO, "IRC <-: %s", rawMessage.toUtf8().constData());
    
    // IRC message format: [:prefix] command [params...]
    QString message = rawMessage;
    QString prefix;
    QString command;
    QString parameters;
    QString tags;
    
    // Parse tags (if present)
    if (message.startsWith('@')) {
        int tagEnd = message.indexOf(' ');
        if (tagEnd != -1) {
            tags = message.mid(1, tagEnd - 1);
            message = message.mid(tagEnd + 1);
        }
    }
    
    // Parse prefix (if present)
    if (message.startsWith(':')) {
        int prefixEnd = message.indexOf(' ');
        if (prefixEnd != -1) {
            prefix = message.mid(1, prefixEnd - 1);
            message = message.mid(prefixEnd + 1);
        }
    }
    
    // Parse command and parameters
    int commandEnd = message.indexOf(' ');
    if (commandEnd != -1) {
        command = message.left(commandEnd);
        parameters = message.mid(commandEnd + 1);
    } else {
        command = message;
    }
    
    // Handle different IRC commands
    if (command == "PING") {
        sendRawMessage("PONG " + parameters);
    } else if (command == "PONG") {
        // PONG received, connection is alive
    } else if (command == "001") {
        // Welcome message, connection successful
        obs_log(LOG_INFO, "Successfully connected to Twitch IRC");
        emit reconnected();
    } else if (command == "JOIN") {
        QString channel = parameters.mid(1); // Remove #
        QString username = extractUsernameFromPrefix(prefix);
        if (username == m_username) {
            emit channelJoined(channel);
        } else {
            TwitchChatUser user;
            user.username = username;
            emit userJoined(channel, user);
        }
    } else if (command == "PART") {
        QString channel = parameters.mid(1); // Remove #
        QString username = extractUsernameFromPrefix(prefix);
        if (username == m_username) {
            m_joinedChannels.removeOne(channel);
            emit channelLeft(channel);
        } else {
            emit userLeft(channel, username);
        }
    } else if (command == "PRIVMSG") {
        TwitchChatMessage chatMessage = parseChatMessage(rawMessage, command, tags, prefix);
        emit messageReceived(chatMessage);

        // Broadcast to websocket clients: { type: "twitch-chat", payload: message }
        OneSevenLiveCoreManager* core = qobject_cast<OneSevenLiveCoreManager*>(parent());
        if (core) {
            OneSevenLiveWebsocketServer* ws = core->getWebsocketServer();
            if (ws && ws->is_running()) {
                try {
                    nlohmann::json payload = {
                        {"type", "twitch-chat"},
                        {"payload", toJson(chatMessage)}
                    };
                    ws->broadcastMessage(payload.dump());
                } catch (const std::exception& e) {
                    obs_log(LOG_WARNING, "Failed to serialize/broadcast Twitch chat message: %s", e.what());
                }
            }
        }
        
        if (chatMessage.type == TwitchMessageType::Subscription) {
            emit subscriptionReceived(chatMessage);
        }
    } else if (command == "USERNOTICE") {
        // Handle subscription notices, raids, etc.
        QMap<QString, QString> tagMap = parseTags(tags);
        QString msgId = tagMap["msg-id"];
        
        if (msgId == "raid") {
            QString channel = parameters.section(' ', 0, 0).mid(1); // Remove #
            QString raider = tagMap["login"];
            int viewerCount = tagMap["msg-param-viewerCount"].toInt();
            emit raidReceived(channel, raider, viewerCount);
        }
    } else if (command == "ROOMSTATE") {
        // Room state changes
    } else if (command == "NOTICE") {
        QString channel = parameters.section(' ', 0, 0).mid(1); // Remove #
        QString noticeMsg = parameters.section(' ', 1);
        if (noticeMsg.startsWith(':')) {
            noticeMsg = noticeMsg.mid(1);
        }
        emit noticeReceived(channel, noticeMsg);
    } else if (command == "CAP") {
        // Capability acknowledgment
    obs_log(LOG_INFO, "Capability acknowledged: %s", parameters.toUtf8().constData());
    }
}

TwitchChatMessage OneSevenLiveTwitchChatClient::parseChatMessage(const QString& rawMessage, const QString& command, const QString& tags, const QString& prefix)
{
    TwitchChatMessage message;
    message.timestamp = QDateTime::currentDateTime();
    message.type = determineMessageType(command, tags);
    
    // Parse tags
    QMap<QString, QString> tagMap = parseTags(tags);
    
    message.id = tagMap["id"];
    message.userId = tagMap["user-id"];
    message.color = tagMap["color"];
    message.displayName = tagMap["display-name"];
    message.emotes = tagMap["emotes"];
    message.badges = tagMap["badges"];
    message.bits = tagMap["bits"].toInt();
    message.isModerator = (tagMap["mod"] == "1");
    message.isSubscriber = (tagMap["subscriber"] == "1");
    message.isTurbo = (tagMap["turbo"] == "1");
    message.isFirstMessage = (tagMap["first-msg"] == "1");
    message.isReturningChatter = (tagMap["returning-chatter"] == "1");
    
    // Parse prefix for username
    message.username = extractUsernameFromPrefix(prefix);
    if (message.displayName.isEmpty()) {
        message.displayName = message.username;
    }
    
    // Parse channel and message content
    QString params = rawMessage;
    int channelEnd = params.indexOf(" :");
    if (channelEnd != -1) {
        message.channel = params.mid(params.indexOf('#'), channelEnd - params.indexOf('#')).mid(1);
        message.message = params.mid(channelEnd + 2);
    }
    
    return message;
}

TwitchMessageType OneSevenLiveTwitchChatClient::determineMessageType(const QString& command, const QString& tags)
{
    if (command == "USERNOTICE") {
        QMap<QString, QString> tagMap = parseTags(tags);
        QString msgId = tagMap["msg-id"];
        
        if (msgId == "sub") return TwitchMessageType::Subscription;
        if (msgId == "resub") return TwitchMessageType::Resubscription;
        if (msgId == "subgift") return TwitchMessageType::GiftSubscription;
        if (msgId == "raid") return TwitchMessageType::Raid;
    }
    
    if (command == "NOTICE") return TwitchMessageType::Notice;
    if (command == "WHISPER") return TwitchMessageType::Whisper;
    
    return TwitchMessageType::Chat;
}

QMap<QString, QString> OneSevenLiveTwitchChatClient::parseTags(const QString& tags)
{
    QMap<QString, QString> result;
    
    if (tags.isEmpty()) {
        return result;
    }
    
    QStringList tagPairs = tags.split(';');
    for (const QString& pair : tagPairs) {
        int separator = pair.indexOf('=');
        if (separator != -1) {
            QString key = pair.left(separator);
            QString value = pair.mid(separator + 1);
            result[key] = value;
        }
    }
    
    return result;
}

QString OneSevenLiveTwitchChatClient::extractUsernameFromPrefix(const QString& prefix)
{
    if (prefix.isEmpty()) {
        return QString();
    }
    
    int exclamation = prefix.indexOf('!');
    if (exclamation != -1) {
        return prefix.left(exclamation);
    }
    
    return prefix;
}

bool OneSevenLiveTwitchChatClient::isChannelJoined(const QString& channel) const
{
    QString normalizedChannel = normalizeChannelName(channel);
    return m_joinedChannels.contains(normalizedChannel);
}

QString OneSevenLiveTwitchChatClient::normalizeChannelName(const QString& channel) const
{
    QString normalized = channel.toLower();
    if (normalized.startsWith('#')) {
        normalized = normalized.mid(1);
    }
    return normalized;
}

void OneSevenLiveTwitchChatClient::scheduleReconnect()
{
    if (m_reconnectTimer && !m_reconnectTimer->isActive()) {
    obs_log(LOG_INFO, "Scheduling reconnection in %d seconds", m_reconnectDelay);
        m_reconnectTimer->start(m_reconnectDelay * 1000);
    }
}

void OneSevenLiveTwitchChatClient::resetReconnectAttempts()
{
    m_reconnectAttempts = 0;
}

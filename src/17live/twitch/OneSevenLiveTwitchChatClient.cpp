#include "OneSevenLiveTwitchChatClient.hpp"
#include "../OneSevenLiveCoreManager.hpp"
#include "../OneSevenLiveWebsocketServer.hpp"
#include <QTimer>
#include <QDebug>
#include "plugin-support.h"
#include <obs-module.h>
#include <QRegularExpression>
#include <QDateTime>
#include <nlohmann/json.hpp>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/error.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>

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
    , m_webSocketConnected(false)
    , m_webSocketThreadRunning(false)
    , m_connected(false)
    , m_autoReconnect(true)
    , m_reconnectDelay(DEFAULT_RECONNECT_DELAY)
    , m_pingInterval(DEFAULT_PING_INTERVAL)
    , m_reconnectAttempts(0)
    , m_maxReconnectAttempts(MAX_RECONNECT_ATTEMPTS)
    , m_pingTimer(nullptr)
    , m_reconnectTimer(nullptr)
{
    // Initialize mbedtls contexts
    m_ssl = std::make_unique<mbedtls_ssl_context>();
    m_server_fd = std::make_unique<mbedtls_net_context>();
    m_conf = std::make_unique<mbedtls_ssl_config>();
    m_ctr_drbg = std::make_unique<mbedtls_ctr_drbg_context>();
    m_entropy = std::make_unique<mbedtls_entropy_context>();
    m_cacert = std::make_unique<mbedtls_x509_crt>();
    
    mbedtls_ssl_init(m_ssl.get());
    mbedtls_net_init(m_server_fd.get());
    mbedtls_ssl_config_init(m_conf.get());
    mbedtls_ctr_drbg_init(m_ctr_drbg.get());
    mbedtls_entropy_init(m_entropy.get());
    mbedtls_x509_crt_init(m_cacert.get());
    
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
    
    // Cleanup mbedtls contexts
    cleanupTLSContext();
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
    
    obs_log(LOG_INFO, "Connecting to Twitch chat server: %s", TWITCH_IRC_SERVER.toUtf8().constData());
    connectWebSocket();
}

void OneSevenLiveTwitchChatClient::disconnectFromChat()
{
    if (!m_connected) {
        return;
    }
    
    obs_log(LOG_INFO, "Disconnecting from Twitch chat");
    m_autoReconnect = false; // Prevent auto-reconnect on manual disconnect
    
    disconnectWebSocket();
    
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
    if (m_connected && m_webSocketConnected) {
        sendWebSocketMessage(message.toStdString());
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

void OneSevenLiveTwitchChatClient::connectWebSocket()
{
    if (m_webSocketThreadRunning) {
        obs_log(LOG_WARNING, "WebSocket thread already running");
        return;
    }
    
    obs_log(LOG_INFO, "Connecting to Twitch chat server: %s", TWITCH_IRC_SERVER.toUtf8().constData());
    
    // Start WebSocket thread
    m_webSocketThreadRunning = true;
    m_webSocketThread = std::thread(&OneSevenLiveTwitchChatClient::webSocketThreadFunc, this);
}

void OneSevenLiveTwitchChatClient::disconnectWebSocket()
{
    obs_log(LOG_INFO, "Disconnecting WebSocket client from Twitch");
    
    m_webSocketThreadRunning = false;
    
    if (m_webSocketThread.joinable()) {
        m_webSocketThread.join();
    }
    
    cleanupTLSContext();
    
    if (m_webSocketConnected) {
        m_webSocketConnected = false;
        onWebSocketClose();
    }
}

void OneSevenLiveTwitchChatClient::sendWebSocketMessage(const std::string& message)
{
    if (m_webSocketConnected) {
        obs_log(LOG_DEBUG, "Sending WebSocket message: %s", message.c_str());
        
        // Construct WebSocket frame (client to server must be masked)
        std::string wsFrame;
        wsFrame.push_back(static_cast<char>(0x81)); // FIN = 1, opcode = 1 (text)
        
        // Generate random masking key
        unsigned char maskingKey[4];
        mbedtls_ctr_drbg_random(m_ctr_drbg.get(), maskingKey, 4);
        
        if (message.length() <= 125) {
            wsFrame.push_back(static_cast<char>(0x80 | static_cast<unsigned char>(message.length()))); // MASK = 1, length
        } else if (message.length() <= 65535) {
            wsFrame.push_back(static_cast<char>(0x80 | 126)); // MASK = 1, 16-bit length
            wsFrame.push_back(static_cast<char>((message.length() >> 8) & 0xFF));
            wsFrame.push_back(static_cast<char>(message.length() & 0xFF));
        } else {
            obs_log(LOG_ERROR, "Message too long for WebSocket frame");
            return;
        }
        
        // Add masking key
        wsFrame.append((char*)maskingKey, 4);
        
        // Add masked payload
        for (size_t i = 0; i < message.length(); i++) {
            wsFrame.push_back(message[i] ^ maskingKey[i % 4]);
        }
        
        if (!sendTLSData(wsFrame)) {
            obs_log(LOG_ERROR, "Failed to send WebSocket message");
        }
    } else {
        obs_log(LOG_WARNING, "Cannot send WebSocket message: not connected");
    }
}

// Helper function to generate WebSocket key
static std::string generateWebSocketKey() {
    unsigned char randomBytes[16];
    // For now, use a fixed key - in production, this should be truly random
    for (int i = 0; i < 16; i++) {
        randomBytes[i] = rand() & 0xFF;
    }
    
    // Base64 encode
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    for (int i = 0; i < 16; i += 3) {
        int val = (randomBytes[i] << 16) | ((i+1 < 16 ? randomBytes[i+1] : 0) << 8) | (i+2 < 16 ? randomBytes[i+2] : 0);
        encoded.push_back(base64_chars[(val >> 18) & 0x3F]);
        encoded.push_back(base64_chars[(val >> 12) & 0x3F]);
        encoded.push_back(i+1 < 16 ? base64_chars[(val >> 6) & 0x3F] : '=');
        encoded.push_back(i+2 < 16 ? base64_chars[val & 0x3F] : '=');
    }
    
    return encoded;
}

// TLS WebSocket implementation
void OneSevenLiveTwitchChatClient::webSocketThreadFunc()
{
    obs_log(LOG_INFO, "WebSocket thread started");
    
    // Parse server URL (wss://irc-ws.chat.twitch.tv:443)
    QString host = "irc-ws.chat.twitch.tv";
    QString port = "443";
    QString path = "/";
    
    try {
        // Initialize TLS context
        const char* pers = "twitch_chat_client";
        int ret = mbedtls_ctr_drbg_seed(m_ctr_drbg.get(), mbedtls_entropy_func, m_entropy.get(),
                                       (const unsigned char*)pers, strlen(pers));
        if (ret != 0) {
            obs_log(LOG_ERROR, "Failed to seed RNG: %d", ret);
            QMetaObject::invokeMethod(this, "onWebSocketError", Qt::QueuedConnection,
                                    Q_ARG(std::string, "Failed to initialize TLS RNG"));
            return;
        }
        
        // Load CA certificates - skip for now, use system defaults
        obs_log(LOG_INFO, "Using system CA certificates for TLS verification");
        
        // Setup SSL configuration
        ret = mbedtls_ssl_config_defaults(m_conf.get(), MBEDTLS_SSL_IS_CLIENT,
                                         MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        if (ret != 0) {
            obs_log(LOG_ERROR, "Failed to set SSL config defaults: %d", ret);
            QMetaObject::invokeMethod(this, "onWebSocketError", Qt::QueuedConnection,
                                    Q_ARG(std::string, "Failed to configure TLS"));
            return;
        }
        
        mbedtls_ssl_conf_authmode(m_conf.get(), MBEDTLS_SSL_VERIFY_NONE); // Allow connections without strict cert verification for now
        mbedtls_ssl_conf_ca_chain(m_conf.get(), nullptr, nullptr); // Skip CA chain for now
        mbedtls_ssl_conf_rng(m_conf.get(), mbedtls_ctr_drbg_random, m_ctr_drbg.get());
        
        // Connect to server
        obs_log(LOG_INFO, "Connecting to %s:%s", host.toUtf8().constData(), port.toUtf8().constData());
        ret = mbedtls_net_connect(m_server_fd.get(), host.toUtf8().constData(),
                                 port.toUtf8().constData(), MBEDTLS_NET_PROTO_TCP);
        if (ret != 0) {
            obs_log(LOG_ERROR, "Failed to connect to server: %d", ret);
            QMetaObject::invokeMethod(this, "onWebSocketError", Qt::QueuedConnection,
                                    Q_ARG(std::string, "Failed to connect to server"));
            return;
        }
        
        // Setup SSL context
        ret = mbedtls_ssl_setup(m_ssl.get(), m_conf.get());
        if (ret != 0) {
            obs_log(LOG_ERROR, "Failed to setup SSL: %d", ret);
            QMetaObject::invokeMethod(this, "onWebSocketError", Qt::QueuedConnection,
                                    Q_ARG(std::string, "Failed to setup TLS"));
            return;
        }
        
        mbedtls_ssl_set_bio(m_ssl.get(), m_server_fd.get(), mbedtls_net_send, mbedtls_net_recv, nullptr);
        
        // Perform handshake
        obs_log(LOG_INFO, "Performing TLS handshake...");
        while ((ret = mbedtls_ssl_handshake(m_ssl.get())) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                obs_log(LOG_ERROR, "TLS handshake failed: %d", ret);
                QMetaObject::invokeMethod(this, "onWebSocketError", Qt::QueuedConnection,
                                        Q_ARG(std::string, "TLS handshake failed"));
                return;
            }
        }
        
        obs_log(LOG_INFO, "TLS handshake successful");
        
        // Send WebSocket upgrade request with proper headers for Twitch IRC
        std::string wsKey = generateWebSocketKey();
        std::string wsRequest = "GET " + path.toStdString() + " HTTP/1.1\r\n"
                               "Host: " + host.toStdString() + "\r\n"
                               "Upgrade: websocket\r\n"
                               "Connection: Upgrade\r\n"
                               "Sec-WebSocket-Key: " + wsKey + "\r\n"
                               "Sec-WebSocket-Version: 13\r\n"
                               "User-Agent: obs-17live/1.0\r\n"
                               "\r\n";
        
        if (!sendTLSData(wsRequest)) {
            obs_log(LOG_ERROR, "Failed to send WebSocket upgrade request");
            return;
        }
        
        // Read response
        std::string response;
        char buffer[1024];
        int totalRead = 0;
        int maxResponseSize = 4096;
        
        // Read HTTP response headers
        while (totalRead < maxResponseSize) {
            int ret = mbedtls_ssl_read(m_ssl.get(), (unsigned char*)buffer, sizeof(buffer) - 1);
            if (ret > 0) {
                buffer[ret] = '\0';
                response.append(buffer, ret);
                totalRead += ret;
                
                // Check if we've received the complete HTTP headers
                if (response.find("\r\n\r\n") != std::string::npos) {
                    break;
                }
            } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                continue;
            } else {
                obs_log(LOG_ERROR, "Failed to read WebSocket upgrade response: %d", ret);
                QMetaObject::invokeMethod(this, "onWebSocketError", Qt::QueuedConnection,
                                        Q_ARG(std::string, "Failed to read WebSocket upgrade response"));
                return;
            }
        }
        
        if (response.find("101 Switching Protocols") == std::string::npos) {
            obs_log(LOG_ERROR, "WebSocket upgrade failed: %s", response.substr(0, 200).c_str());
            QMetaObject::invokeMethod(this, "onWebSocketError", Qt::QueuedConnection,
                                    Q_ARG(std::string, "WebSocket upgrade failed"));
            return;
        }
        
        obs_log(LOG_INFO, "WebSocket upgrade successful");
        
        obs_log(LOG_INFO, "WebSocket connection established");
        m_webSocketConnected = true;
        QMetaObject::invokeMethod(this, "onWebSocketOpen", Qt::QueuedConnection);
        
        // Main message loop
        while (m_webSocketThreadRunning) {
            // Read WebSocket frame header (2 bytes minimum)
            unsigned char frameHeader[2];
            int ret = mbedtls_ssl_read(m_ssl.get(), frameHeader, 2);
            
            if (ret == 2) {
                unsigned char opcode = frameHeader[0] & 0x0F;
                bool masked = (frameHeader[1] & 0x80) != 0;
                uint64_t payloadLen = frameHeader[1] & 0x7F;
                
                // Handle extended payload length
                if (payloadLen == 126) {
                    unsigned char extLen[2];
                    ret = mbedtls_ssl_read(m_ssl.get(), extLen, 2);
                    if (ret != 2) {
                        obs_log(LOG_ERROR, "Failed to read extended payload length");
                        break;
                    }
                    payloadLen = (extLen[0] << 8) | extLen[1];
                } else if (payloadLen == 127) {
                    unsigned char extLen[8];
                    ret = mbedtls_ssl_read(m_ssl.get(), extLen, 8);
                    if (ret != 8) {
                        obs_log(LOG_ERROR, "Failed to read extended payload length");
                        break;
                    }
                    payloadLen = 0;
                    for (int i = 0; i < 8; i++) {
                        payloadLen = (payloadLen << 8) | extLen[i];
                    }
                }
                
                // Read masking key if present
                unsigned char maskingKey[4] = {0};
                if (masked) {
                    ret = mbedtls_ssl_read(m_ssl.get(), maskingKey, 4);
                    if (ret != 4) {
                        obs_log(LOG_ERROR, "Failed to read masking key");
                        break;
                    }
                }
                
                // Read payload
                if (payloadLen > 0 && payloadLen < 65536) { // Reasonable limit
                    std::vector<unsigned char> payload(payloadLen);
                    size_t bytesRead = 0;
                    
                    while (bytesRead < payloadLen) {
                        ret = mbedtls_ssl_read(m_ssl.get(), payload.data() + bytesRead, payloadLen - bytesRead);
                        if (ret > 0) {
                            bytesRead += ret;
                        } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                            continue;
                        } else {
                            obs_log(LOG_ERROR, "Failed to read payload: %d", ret);
                            break;
                        }
                    }
                    
                    if (bytesRead == payloadLen) {
                        // Unmask payload if needed
                        if (masked) {
                            for (size_t i = 0; i < payloadLen; i++) {
                                payload[i] ^= maskingKey[i % 4];
                            }
                        }
                        
                        // Handle different opcodes
                        if (opcode == 0x1) { // Text frame
                            std::string text(payload.begin(), payload.end());
                            QMetaObject::invokeMethod(this, "onWebSocketMessage", Qt::QueuedConnection,
                                                    Q_ARG(std::string, text));
                        } else if (opcode == 0x8) { // Close frame
                            obs_log(LOG_INFO, "WebSocket close frame received");
                            break;
                        } else if (opcode == 0x9) { // Ping frame
                            // Send pong
                            std::string pongFrame;
                            pongFrame.push_back(static_cast<char>(0x8A)); // FIN = 1, opcode = 10 (pong)
                            pongFrame.push_back(static_cast<char>(payloadLen));
                            pongFrame.append(payload.begin(), payload.end());
                            sendTLSData(pongFrame);
                        } else if (opcode == 0xA) { // Pong frame
                            // Ignore pong
                        }
                    }
                }
            } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                obs_log(LOG_INFO, "TLS connection closed by peer");
                break;
            } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                // No data available, continue
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            } else if (ret < 0) {
                obs_log(LOG_ERROR, "TLS read error: %d", ret);
                break;
            } else {
                // Connection closed
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "WebSocket thread exception: %s", e.what());
        QMetaObject::invokeMethod(this, "onWebSocketError", Qt::QueuedConnection,
                                Q_ARG(std::string, std::string("WebSocket error: ") + e.what()));
    }
    
    obs_log(LOG_INFO, "WebSocket thread stopped");
}

bool OneSevenLiveTwitchChatClient::performTLSHandshake()
{
    obs_log(LOG_INFO, "Performing TLS handshake...");
    
    int ret;
    while ((ret = mbedtls_ssl_handshake(m_ssl.get())) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            obs_log(LOG_ERROR, "TLS handshake failed: %d", ret);
            return false;
        }
    }
    
    obs_log(LOG_INFO, "TLS handshake successful");
    return true;
}

bool OneSevenLiveTwitchChatClient::sendTLSData(const std::string& data)
{
    int ret = mbedtls_ssl_write(m_ssl.get(), (const unsigned char*)data.c_str(), data.length());
    if (ret < 0) {
        obs_log(LOG_ERROR, "TLS write failed: %d", ret);
        return false;
    }
    return true;
}

std::string OneSevenLiveTwitchChatClient::receiveTLSData()
{
    unsigned char buffer[4096];
    int ret = mbedtls_ssl_read(m_ssl.get(), buffer, sizeof(buffer) - 1);
    
    if (ret > 0) {
        buffer[ret] = '\0';
        return std::string((char*)buffer);
    } else if (ret == 0) {
        // Connection closed
        obs_log(LOG_INFO, "TLS connection closed normally");
        return "";
    } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        // No data available
        return "";
    } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        obs_log(LOG_INFO, "TLS connection closed by peer");
        return "";
    } else {
        obs_log(LOG_ERROR, "TLS read failed with error: %d", ret);
        return "";
    }
}

void OneSevenLiveTwitchChatClient::cleanupTLSContext()
{
    if (m_ssl) {
        mbedtls_ssl_close_notify(m_ssl.get());
        mbedtls_ssl_free(m_ssl.get());
        m_ssl.reset();
    }
    if (m_server_fd) {
        mbedtls_net_free(m_server_fd.get());
        m_server_fd.reset();
    }
    if (m_conf) {
        mbedtls_ssl_config_free(m_conf.get());
        m_conf.reset();
    }
    if (m_ctr_drbg) {
        mbedtls_ctr_drbg_free(m_ctr_drbg.get());
        m_ctr_drbg.reset();
    }
    if (m_entropy) {
        mbedtls_entropy_free(m_entropy.get());
        m_entropy.reset();
    }
    if (m_cacert) {
        mbedtls_x509_crt_free(m_cacert.get());
        m_cacert.reset();
    }
}

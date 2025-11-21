#include "OneSevenLiveWebsocketClient.hpp"
#include <QMetaObject>
#include <QUrl>
#include <QDateTime>
#include <obs-module.h>
#include "plugin-support.h"
#include "WebsocketUtils.hpp"
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>

OneSevenLiveWebsocketClient::OneSevenLiveWebsocketClient(QObject* parent) : QObject(parent) {}

OneSevenLiveWebsocketClient::~OneSevenLiveWebsocketClient() {
    disconnect();
}

void OneSevenLiveWebsocketClient::setOpenCallback(const std::function<void()>& cb) { onOpen = cb; }
void OneSevenLiveWebsocketClient::setMessageCallback(const std::function<void(const std::string&)>& cb) { onMessage = cb; }
void OneSevenLiveWebsocketClient::setCloseCallback(const std::function<void()>& cb) { onClose = cb; }
void OneSevenLiveWebsocketClient::setErrorCallback(const std::function<void(const std::string&)>& cb) { onError = cb; }

void OneSevenLivewebsocketClient_connectUrl_parse_helper(QUrl& parsed, QString& host, QString& port, QString& path) {
    host = parsed.host();
    QString pth = parsed.path();
    if (pth.isEmpty()) pth = "/";
    if (parsed.hasQuery()) {
        path = pth + "?" + parsed.query();
    } else {
        path = pth;
    }
    int p = parsed.port(443);
    port = QString::number(p);
}

void OneSevenLiveWebsocketClient::connectUrl(const QString& url) {
    QUrl parsed(url.trimmed());
    QString host;
    QString port;
    QString path;
    OneSevenLivewebsocketClient_connectUrl_parse_helper(parsed, host, port, path);
    startThread(host, port, path);
}

void OneSevenLiveWebsocketClient::disconnect() {
    stopThread();
}

bool OneSevenLiveWebsocketClient::isConnected() const { return connected.load(); }

void OneSevenLiveWebsocketClient::startThread(const QString& host, const QString& port, const QString& path) {
    if (running.load()) return;
    running.store(true);
    th = std::thread(&OneSevenLiveWebsocketClient::threadFunc, this, host, port, path);
}

void OneSevenLiveWebsocketClient::stopThread() {
    running.store(false);
    if (th.joinable()) {
        if (std::this_thread::get_id() == th.get_id()) {
            th.detach();
        } else {
            th.join();
        }
    }
    cleanupTLS();
    if (connected.load() && onClose) QMetaObject::invokeMethod(this, [this]() { onClose(); }, Qt::QueuedConnection);
    connected.store(false);
}


void OneSevenLiveWebsocketClient::threadFunc(const QString& host, const QString& port, const QString& path) {
    ssl = new mbedtls_ssl_context;
    server_fd = new mbedtls_net_context;
    conf = new mbedtls_ssl_config;
    ctr_drbg = new mbedtls_ctr_drbg_context;
    entropy = new mbedtls_entropy_context;
    cacert = new mbedtls_x509_crt;
    mbedtls_ssl_init(ssl);
    mbedtls_net_init(server_fd);
    mbedtls_ssl_config_init(conf);
    mbedtls_ctr_drbg_init(ctr_drbg);
    mbedtls_entropy_init(entropy);
    mbedtls_x509_crt_init(cacert);

    const char* pers = "ws_client";
    int ret = mbedtls_ctr_drbg_seed(ctr_drbg, mbedtls_entropy_func, entropy, (const unsigned char*)pers, strlen(pers));
    if (ret != 0) { if (onError) QMetaObject::invokeMethod(this, [this]() { onError("rng_init"); }, Qt::QueuedConnection); stopThread(); return; }

    ret = mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) { if (onError) QMetaObject::invokeMethod(this, [this]() { onError("ssl_cfg"); }, Qt::QueuedConnection); stopThread(); return; }

    mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_ca_chain(conf, nullptr, nullptr);
    mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, ctr_drbg);

    ret = mbedtls_net_connect(server_fd, host.toUtf8().constData(), port.toUtf8().constData(), MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) { if (onError) QMetaObject::invokeMethod(this, [this]() { onError("tcp_connect"); }, Qt::QueuedConnection); stopThread(); return; }

    ret = mbedtls_ssl_setup(ssl, conf);
    if (ret != 0) { if (onError) QMetaObject::invokeMethod(this, [this]() { onError("ssl_setup"); }, Qt::QueuedConnection); stopThread(); return; }
    mbedtls_ssl_set_bio(ssl, server_fd, mbedtls_net_send, mbedtls_net_recv, nullptr);
    mbedtls_ssl_set_hostname(ssl, host.toUtf8().constData());

    while ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) { if (onError) QMetaObject::invokeMethod(this, [this]() { onError("tls_handshake"); }, Qt::QueuedConnection); stopThread(); return; }
    }

    std::string wsKey = generateWebSocketKey();
    std::string req = "GET " + path.toStdString() + " HTTP/1.1\r\n" "Host: " + host.toStdString() + "\r\n" "Upgrade: websocket\r\n" "Connection: Upgrade\r\n" "Sec-WebSocket-Key: " + wsKey + "\r\n" "Sec-WebSocket-Version: 13\r\n" "User-Agent: obs-17live/1.0\r\n\r\n";
    if (!sendTLS(req)) { if (onError) QMetaObject::invokeMethod(this, [this]() { onError("ws_req"); }, Qt::QueuedConnection); stopThread(); return; }

    std::string resp; char buf[1024]; int tr = 0; int maxR = 4096;
    while (tr < maxR) {
        int r = mbedtls_ssl_read(ssl, (unsigned char*)buf, sizeof(buf) - 1);
        if (r > 0) { buf[r] = '\0'; resp.append(buf, r); tr += r; if (resp.find("\r\n\r\n") != std::string::npos) break; }
        else if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) { continue; }
        else { if (onError) QMetaObject::invokeMethod(this, [this]() { onError("ws_resp"); }, Qt::QueuedConnection); stopThread(); return; }
    }
    if (resp.find("101 Switching Protocols") == std::string::npos) { if (onError) QMetaObject::invokeMethod(this, [this]() { onError("ws_101"); }, Qt::QueuedConnection); stopThread(); return; }

    connected.store(true);
    if (onOpen) QMetaObject::invokeMethod(this, [this]() { onOpen(); }, Qt::QueuedConnection);

    while (running.load()) {
        unsigned char h[2];
        int r = mbedtls_ssl_read(ssl, h, 2);
        if (r == 2) {
            unsigned char opcode = h[0] & 0x0F;
            uint64_t len = h[1] & 0x7F;
            if (len == 126) { unsigned char ext[2]; r = mbedtls_ssl_read(ssl, ext, 2); if (r != 2) break; len = (ext[0] << 8) | ext[1]; }
            else if (len == 127) { unsigned char ext[8]; r = mbedtls_ssl_read(ssl, ext, 8); if (r != 8) break; len = 0; for (int i = 0; i < 8; i++) len = (len << 8) | ext[i]; }
            if (len > 0 && len < 65536) {
                std::string payload; payload.resize(len);
                size_t br = 0;
                while (br < len) {
                    r = mbedtls_ssl_read(ssl, (unsigned char*)payload.data() + br, len - br);
                    if (r > 0) br += r; else if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue; else { br = 0; break; }
                }
                if (br == len) {
                    if (opcode == 0x1) {
                        if (onMessage) QMetaObject::invokeMethod(this, [this, payload]() { onMessage(payload); }, Qt::QueuedConnection);
                    } else if (opcode == 0x8) {
                        int code = 1000;
                        std::string reason;
                        if (payload.size() >= 2) {
                            code = ((unsigned char)payload[0] << 8) | (unsigned char)payload[1];
                            if (payload.size() > 2) reason.assign(payload.data() + 2, payload.size() - 2);
                        }
                        obs_log(LOG_INFO, "[17Live WebSocket] Close received: code=%d reason=%s", code, reason.c_str());
                        if (onError) {
                            std::string msg = std::string("ws_close ") + std::to_string(code) + (reason.empty() ? std::string("") : std::string(" ") + reason);
                            QMetaObject::invokeMethod(this, [this, msg]() { onError(msg); }, Qt::QueuedConnection);
                        }
                        break;
                    }
                    else if (opcode == 0x9) {
                        std::string pl = payload;
                        if (pl.size() > 125) pl.clear();
                        unsigned char k[4]; mbedtls_ctr_drbg_random(ctr_drbg, k, 4);
                        std::string f; f.push_back((char)0x8A);
                        f.push_back((char)(0x80 | (unsigned char)pl.size()));
                        f.append((char*)k, 4);
                        for (size_t i = 0; i < pl.size(); i++) f.push_back(pl[i] ^ k[i % 4]);
                        sendTLS(f);
                    }
                }
            }
        } else if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            obs_log(LOG_INFO, "[17Live WebSocket] Peer close notify received");
            if (onError) QMetaObject::invokeMethod(this, [this]() { onError("peer_close_notify"); }, Qt::QueuedConnection);
            break;
        }
        else if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) { continue; }
        else if (r < 0) { if (onError) QMetaObject::invokeMethod(this, [this]() { onError("tls_read"); }, Qt::QueuedConnection); break; }
        else { break; }
    }

    stopThread();
}

bool OneSevenLiveWebsocketClient::sendTLS(const std::string& data) {
    int ret = mbedtls_ssl_write(ssl, (const unsigned char*)data.c_str(), data.length());
    return ret >= 0;
}

void OneSevenLiveWebsocketClient::sendText(const QString& text) {
    if (!connected.load()) return;
    std::string m = text.toStdString();
    std::string f; f.push_back((char)0x81);
    unsigned char k[4]; mbedtls_ctr_drbg_random(ctr_drbg, k, 4);
    if (m.length() <= 125) { f.push_back((char)(0x80 | (unsigned char)m.length())); }
    else if (m.length() <= 65535) { f.push_back((char)(0x80 | 126)); f.push_back((char)((m.length() >> 8) & 0xFF)); f.push_back((char)(m.length() & 0xFF)); }
    else { return; }
    f.append((char*)k, 4);
    for (size_t i = 0; i < m.length(); i++) f.push_back(m[i] ^ k[i % 4]);
    sendTLS(f);
}

void OneSevenLiveWebsocketClient::cleanupTLS() {
    if (ssl) { mbedtls_ssl_close_notify(ssl); mbedtls_ssl_free(ssl); delete ssl; ssl = nullptr; }
    if (server_fd) { mbedtls_net_free(server_fd); delete server_fd; server_fd = nullptr; }
    if (conf) { mbedtls_ssl_config_free(conf); delete conf; conf = nullptr; }
    if (ctr_drbg) { mbedtls_ctr_drbg_free(ctr_drbg); delete ctr_drbg; ctr_drbg = nullptr; }
    if (entropy) { mbedtls_entropy_free(entropy); delete entropy; entropy = nullptr; }
    if (cacert) { mbedtls_x509_crt_free(cacert); delete cacert; cacert = nullptr; }
}

#include "OneSevenLiveWebsocketClient.hpp"

#include <QPointer>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#else
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#endif
#include <obs-module.h>

#include <QDateTime>
#include <QMetaObject>
#include <QUrl>

#include "WebsocketUtils.hpp"
#include "plugin-support.h"

#ifdef _WIN32
struct TLSHandles {
    HINTERNET hSession{nullptr};
    HINTERNET hConnect{nullptr};
    HINTERNET hRequest{nullptr};
    HINTERNET hWebSocket{nullptr};

    ~TLSHandles() {
        if (hWebSocket) {
            WinHttpWebSocketShutdown(hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr,
                                     0);
            WinHttpCloseHandle(hWebSocket);
        }
        if (hRequest)
            WinHttpCloseHandle(hRequest);
        if (hConnect)
            WinHttpCloseHandle(hConnect);
        if (hSession)
            WinHttpCloseHandle(hSession);
    }
};
#else
struct TLSHandles {
    mbedtls_ssl_context ssl;
    mbedtls_net_context server_fd;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_x509_crt cacert;

    TLSHandles() {
        mbedtls_ssl_init(&ssl);
        mbedtls_net_init(&server_fd);
        mbedtls_ssl_config_init(&conf);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        mbedtls_entropy_init(&entropy);
        mbedtls_x509_crt_init(&cacert);
    }

    ~TLSHandles() {
        mbedtls_ssl_close_notify(&ssl);
        mbedtls_ssl_free(&ssl);
        mbedtls_net_free(&server_fd);
        mbedtls_ssl_config_free(&conf);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_x509_crt_free(&cacert);
    }
};
#endif

OneSevenLiveWebsocketClient::OneSevenLiveWebsocketClient(QObject* parent) : QObject(parent) {}

OneSevenLiveWebsocketClient::~OneSevenLiveWebsocketClient() {
    disconnect();
}

void OneSevenLiveWebsocketClient::setOpenCallback(const std::function<void()>& cb) {
    onOpen = cb;
}

void OneSevenLiveWebsocketClient::setMessageCallback(
    const std::function<void(const std::string&)>& cb) {
    onMessage = cb;
}

void OneSevenLiveWebsocketClient::setCloseCallback(const std::function<void()>& cb) {
    onClose = cb;
}

void OneSevenLiveWebsocketClient::setErrorCallback(
    const std::function<void(const std::string&)>& cb) {
    onError = cb;
}

void OneSevenLivewebsocketClient_connectUrl_parse_helper(QUrl& parsed, QString& host, QString& port,
                                                         QString& path) {
    host = parsed.host();
    QString pth = parsed.path();
    if (pth.isEmpty())
        pth = "/";
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

void OneSevenLiveWebsocketClient::disconnectAsync() {
    running.store(false);
    if (tls) {
#ifdef _WIN32
        if (tls->hWebSocket)
            WinHttpWebSocketShutdown(tls->hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                                     nullptr, 0);
#else
        mbedtls_ssl_close_notify(&tls->ssl);
#endif
    }
    // Do not join here to avoid blocking UI; thread will exit and self-clean
}

bool OneSevenLiveWebsocketClient::isConnected() const {
    return connected.load();
}

void OneSevenLiveWebsocketClient::startThread(const QString& host, const QString& port,
                                              const QString& path) {
    // Ensure previous thread is joined and resources are cleaned up
    stopThread();

    if (running.load())
        return;
    running.store(true);
    th = std::thread(&OneSevenLiveWebsocketClient::threadFunc, this, host, port, path);
}

void OneSevenLiveWebsocketClient::stopThread() {
    running.store(false);
    // Proactively signal TLS to close to unblock any pending reads
    if (tls) {
#ifdef _WIN32
        if (tls->hWebSocket)
            WinHttpWebSocketShutdown(tls->hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                                     nullptr, 0);
#else
        mbedtls_ssl_close_notify(&tls->ssl);
        mbedtls_net_free(&tls->server_fd);
#endif
    }
    try {
        if (th.joinable()) {
            th.join();
        }
    } catch (...) {
        // Swallow thread join errors to avoid terminate during shutdown
    }
    cleanupTLS();
    if (connected.load()) {
        // If we are stopping (e.g. destruction or explicit disconnect),
        // we should not invoke async callbacks if the object might be destroyed soon.
        // However, standard disconnect() calls might expect a callback.
        // To be safe in destruction scenarios, we should avoid QueuedConnection if we can't
        // guarantee lifetime, but since we don't know if this is destruction or just stop, we rely
        // on the caller to manage lifetime OR we can use QPointer in the lambda capture if we
        // inherited from QObject (which we do).

        // Better yet: invokeMethod with Qt::DirectConnection if we are in the same thread?
        // No, stopThread can be called from any thread.
        // Let's use QPointer protection pattern here too.

        QPointer<OneSevenLiveWebsocketClient> self(this);
        QMetaObject::invokeMethod(
            this,
            [self]() {
                if (self && self->onClose)
                    self->onClose();
            },
            Qt::QueuedConnection);

        connected.store(false);
    }
}

#ifdef _WIN32
void OneSevenLiveWebsocketClient::threadFunc(const QString& host, const QString& port,
                                             const QString& path) {
    tls = std::make_unique<TLSHandles>();
    std::wstring ua = L"obs-17live/1.0";
    tls->hSession = WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!tls->hSession) {
        if (onError)
            QMetaObject::invokeMethod(
                this, [this]() { onError("winhttp_open"); }, Qt::QueuedConnection);
        stopThread();
        return;
    }
    std::wstring whost = host.toStdWString();
    int p = port.toInt();
    tls->hConnect = WinHttpConnect(tls->hSession, whost.c_str(), (INTERNET_PORT) p, 0);
    if (!tls->hConnect) {
        if (onError)
            QMetaObject::invokeMethod(
                this, [this]() { onError("winhttp_connect"); }, Qt::QueuedConnection);
        stopThread();
        return;
    }
    std::wstring wpath = path.toStdWString();
    tls->hRequest =
        WinHttpOpenRequest(tls->hConnect, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER,
                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!tls->hRequest) {
        DWORD ec = GetLastError();
        if (onError)
            QMetaObject::invokeMethod(
                this,
                [this, ec]() { onError(std::string("winhttp_openreq ") + std::to_string(ec)); },
                Qt::QueuedConnection);
        stopThread();
        return;
    }
    if (!WinHttpSetOption(tls->hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
        DWORD ec = GetLastError();
        if (onError)
            QMetaObject::invokeMethod(
                this,
                [this, ec]() {
                    onError(std::string("winhttp_setopt_upgrade ") + std::to_string(ec));
                },
                Qt::QueuedConnection);
        stopThread();
        return;
    }
#if defined(WINHTTP_OPTION_SECURE_PROTOCOLS) && defined(WINHTTP_PROTOCOL_FLAG_TLS1_2)
    DWORD sp = WINHTTP_PROTOCOL_FLAG_TLS1_2;
    WinHttpSetOption(tls->hRequest, WINHTTP_OPTION_SECURE_PROTOCOLS, &sp, sizeof(sp));
#endif
    std::string wsKey = generateWebSocketKey();
    std::wstring hdr = L"Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: ";
    hdr += QString::fromStdString(wsKey).toStdWString();
    hdr += L"\r\nSec-WebSocket-Version: 13\r\nOrigin: https://www.twitch.tv\r\n";
    if (!WinHttpAddRequestHeaders(tls->hRequest, hdr.c_str(), (DWORD) hdr.size(),
                                  WINHTTP_ADDREQ_FLAG_ADD)) {
        DWORD ec = GetLastError();
        if (onError)
            QMetaObject::invokeMethod(
                this,
                [this, ec]() { onError(std::string("winhttp_addhdr ") + std::to_string(ec)); },
                Qt::QueuedConnection);
        stopThread();
        return;
    }
    if (!WinHttpSendRequest(tls->hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        DWORD ec = GetLastError();
        if (onError)
            QMetaObject::invokeMethod(
                this, [this, ec]() { onError(std::string("winhttp_send ") + std::to_string(ec)); },
                Qt::QueuedConnection);
        stopThread();
        return;
    }
    if (!WinHttpReceiveResponse(tls->hRequest, nullptr)) {
        DWORD ec = GetLastError();
        if (onError)
            QMetaObject::invokeMethod(
                this, [this, ec]() { onError(std::string("winhttp_resp ") + std::to_string(ec)); },
                Qt::QueuedConnection);
        stopThread();
        return;
    }
    DWORD status = 0;
    DWORD len = sizeof(status);
    if (!WinHttpQueryHeaders(tls->hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &len,
                             WINHTTP_NO_HEADER_INDEX)) {
        if (onError)
            QMetaObject::invokeMethod(
                this, [this]() { onError("winhttp_status"); }, Qt::QueuedConnection);
        stopThread();
        return;
    }
    if (status != 101) {
        if (onError)
            QMetaObject::invokeMethod(this, [this]() { onError("ws_101"); }, Qt::QueuedConnection);
        stopThread();
        return;
    }
    tls->hWebSocket = WinHttpWebSocketCompleteUpgrade(tls->hRequest, 0);
    if (!tls->hWebSocket) {
        if (onError)
            QMetaObject::invokeMethod(
                this, [this]() { onError("ws_complete"); }, Qt::QueuedConnection);
        stopThread();
        return;
    }
    connected.store(true);
    if (onOpen)
        QMetaObject::invokeMethod(this, [this]() { onOpen(); }, Qt::QueuedConnection);
    std::vector<char> buf(65536);
    while (running.load()) {
        DWORD rd = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE tp = WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
        DWORD r =
            WinHttpWebSocketReceive(tls->hWebSocket, buf.data(), (DWORD) buf.size(), &rd, &tp);
        if (r == ERROR_SUCCESS) {
            if (tp == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                std::string payload(buf.data(), buf.data() + rd);
                if (onMessage)
                    QMetaObject::invokeMethod(
                        this, [this, payload]() { onMessage(payload); }, Qt::QueuedConnection);
            } else if (tp == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE ||
                       tp == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE ||
                       tp == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE) {
                // Ignore non-text frames or fragments for now
            } else if (tp == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
                break;
            }
        } else {
            if (onError)
                QMetaObject::invokeMethod(
                    this, [this]() { onError("ws_recv"); }, Qt::QueuedConnection);
            break;
        }
    }
    stopThread();
}
#else
#include <sys/select.h>

void OneSevenLiveWebsocketClient::threadFunc(const QString& host, const QString& port,
                                             const QString& path) {
    tls = std::make_unique<TLSHandles>();
    const char* pers = "ws_client";
    int ret = mbedtls_ctr_drbg_seed(&tls->ctr_drbg, mbedtls_entropy_func, &tls->entropy,
                                    (const unsigned char*) pers, strlen(pers));
    if (ret != 0) {
        if (onError)
            QMetaObject::invokeMethod(
                this, [this]() { onError("rng_init"); }, Qt::QueuedConnection);
        running.store(false);
        return;
    }
    ret = mbedtls_ssl_config_defaults(&tls->conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        if (onError)
            QMetaObject::invokeMethod(this, [this]() { onError("ssl_cfg"); }, Qt::QueuedConnection);
        running.store(false);
        return;
    }
    mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_ca_chain(&tls->conf, nullptr, nullptr);
    mbedtls_ssl_conf_rng(&tls->conf, mbedtls_ctr_drbg_random, &tls->ctr_drbg);
    ret = mbedtls_net_connect(&tls->server_fd, host.toUtf8().constData(), port.toUtf8().constData(),
                              MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        if (onError)
            QMetaObject::invokeMethod(
                this, [this]() { onError("tcp_connect"); }, Qt::QueuedConnection);
        running.store(false);
        return;
    }
    ret = mbedtls_ssl_setup(&tls->ssl, &tls->conf);
    if (ret != 0) {
        if (onError)
            QMetaObject::invokeMethod(
                this, [this]() { onError("ssl_setup"); }, Qt::QueuedConnection);
        running.store(false);
        return;
    }
    mbedtls_net_set_nonblock(&tls->server_fd);
    mbedtls_ssl_set_bio(&tls->ssl, &tls->server_fd, mbedtls_net_send, mbedtls_net_recv, nullptr);
    mbedtls_ssl_set_hostname(&tls->ssl, host.toUtf8().constData());
    while (running.load()) {
        ret = mbedtls_ssl_handshake(&tls->ssl);
        if (ret == 0) {
            break;
        }
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            // Wait for socket
            int fd = tls->server_fd.fd;
            if (fd >= 0) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(fd, &fds);
                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 100000;  // 100ms
                select(fd + 1, (ret == MBEDTLS_ERR_SSL_WANT_READ) ? &fds : NULL,
                       (ret == MBEDTLS_ERR_SSL_WANT_WRITE) ? &fds : NULL, NULL, &tv);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            continue;
        }
        if (onError)
            QMetaObject::invokeMethod(
                this, [this]() { onError("tls_handshake"); }, Qt::QueuedConnection);
        running.store(false);
        return;
    }
    if (!running.load()) {
        running.store(false);
        return;
    }
    std::string wsKey = generateWebSocketKey();
    std::string req = "GET " + path.toStdString() +
                      " HTTP/1.1\r\n"
                      "Host: " +
                      host.toStdString() +
                      "\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: " +
                      wsKey +
                      "\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "User-Agent: obs-17live/1.0\r\n\r\n";
    if (!sendTLS(req)) {
        if (onError)
            QMetaObject::invokeMethod(this, [this]() { onError("ws_req"); }, Qt::QueuedConnection);
        running.store(false);
        return;
    }
    std::string resp;
    char buf[1024];
    int tr = 0;
    int maxR = 4096;
    while (tr < maxR) {
        int r = mbedtls_ssl_read(&tls->ssl, (unsigned char*) buf, sizeof(buf) - 1);
        if (r > 0) {
            buf[r] = '\0';
            resp.append(buf, r);
            tr += r;
            if (resp.find("\r\n\r\n") != std::string::npos)
                break;
        } else if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
            int fd = tls->server_fd.fd;
            if (fd >= 0) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(fd, &fds);
                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 100000;  // 100ms
                select(fd + 1, (r == MBEDTLS_ERR_SSL_WANT_READ) ? &fds : NULL,
                       (r == MBEDTLS_ERR_SSL_WANT_WRITE) ? &fds : NULL, NULL, &tv);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            continue;
        } else {
            if (onError)
                QMetaObject::invokeMethod(
                    this, [this]() { onError("ws_resp"); }, Qt::QueuedConnection);
            running.store(false);
            return;
        }
    }
    if (resp.find("101 Switching Protocols") == std::string::npos) {
        if (onError)
            QMetaObject::invokeMethod(this, [this]() { onError("ws_101"); }, Qt::QueuedConnection);
        running.store(false);
        return;
    }
    connected.store(true);
    if (onOpen)
        QMetaObject::invokeMethod(this, [this]() { onOpen(); }, Qt::QueuedConnection);
    while (running.load()) {
        unsigned char h[2];
        int r = mbedtls_ssl_read(&tls->ssl, h, 2);
        if (r == 2) {
            unsigned char opcode = h[0] & 0x0F;
            uint64_t len = h[1] & 0x7F;
            if (len == 126) {
                unsigned char ext[2];
                // Loop to ensure we read 2 bytes
                size_t read_bytes = 0;
                while (read_bytes < 2) {
                    r = mbedtls_ssl_read(&tls->ssl, ext + read_bytes, 2 - read_bytes);
                    if (r > 0)
                        read_bytes += r;
                    else if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
                        int fd = tls->server_fd.fd;
                        if (fd >= 0) {
                            fd_set fds;
                            FD_ZERO(&fds);
                            FD_SET(fd, &fds);
                            struct timeval tv;
                            tv.tv_sec = 0;
                            tv.tv_usec = 100000;  // 100ms
                            select(fd + 1, (r == MBEDTLS_ERR_SSL_WANT_READ) ? &fds : NULL,
                                   (r == MBEDTLS_ERR_SSL_WANT_WRITE) ? &fds : NULL, NULL, &tv);
                        }
                        continue;
                    } else
                        break;
                }
                if (read_bytes != 2)
                    break;
                len = (ext[0] << 8) | ext[1];
            } else if (len == 127) {
                unsigned char ext[8];
                size_t read_bytes = 0;
                while (read_bytes < 8) {
                    r = mbedtls_ssl_read(&tls->ssl, ext + read_bytes, 8 - read_bytes);
                    if (r > 0)
                        read_bytes += r;
                    else if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
                        int fd = tls->server_fd.fd;
                        if (fd >= 0) {
                            fd_set fds;
                            FD_ZERO(&fds);
                            FD_SET(fd, &fds);
                            struct timeval tv;
                            tv.tv_sec = 0;
                            tv.tv_usec = 100000;  // 100ms
                            select(fd + 1, (r == MBEDTLS_ERR_SSL_WANT_READ) ? &fds : NULL,
                                   (r == MBEDTLS_ERR_SSL_WANT_WRITE) ? &fds : NULL, NULL, &tv);
                        }
                        continue;
                    } else
                        break;
                }
                if (read_bytes != 8)
                    break;
                len = 0;
                for (int i = 0; i < 8; i++)
                    len = (len << 8) | ext[i];
            }
            if (len > 0 && len < 65536) {
                std::string payload;
                payload.resize(len);
                size_t br = 0;
                while (br < len) {
                    r = mbedtls_ssl_read(&tls->ssl, (unsigned char*) payload.data() + br, len - br);
                    if (r > 0)
                        br += r;
                    else if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
                        int fd = tls->server_fd.fd;
                        if (fd >= 0) {
                            fd_set fds;
                            FD_ZERO(&fds);
                            FD_SET(fd, &fds);
                            struct timeval tv;
                            tv.tv_sec = 0;
                            tv.tv_usec = 100000;  // 100ms
                            select(fd + 1, (r == MBEDTLS_ERR_SSL_WANT_READ) ? &fds : NULL,
                                   (r == MBEDTLS_ERR_SSL_WANT_WRITE) ? &fds : NULL, NULL, &tv);
                        }
                        continue;
                    } else {
                        br = 0;
                        break;
                    }
                }
                if (br == len) {
                    if (opcode == 0x1) {
                        if (onMessage)
                            QMetaObject::invokeMethod(
                                this, [this, payload]() { onMessage(payload); },
                                Qt::QueuedConnection);
                    } else if (opcode == 0x8) {
                        int code = 1000;
                        std::string reason;
                        if (payload.size() >= 2) {
                            code = ((unsigned char) payload[0] << 8) | (unsigned char) payload[1];
                            if (payload.size() > 2)
                                reason.assign(payload.data() + 2, payload.size() - 2);
                        }
                        if (code != 1000) {
                            obs_log(LOG_INFO,
                                    "[Websocket Client] Close received: code=%d reason=%s", code,
                                    reason.c_str());
                        }
                        if (code != 1000 && onError) {
                            std::string msg =
                                std::string("ws_close ") + std::to_string(code) +
                                (reason.empty() ? std::string("") : std::string(" ") + reason);
                            QMetaObject::invokeMethod(
                                this, [this, msg]() { onError(msg); }, Qt::QueuedConnection);
                        }
                        break;
                    } else if (opcode == 0x9) {
                        std::string pl = payload;
                        if (pl.size() > 125)
                            pl.clear();
                        unsigned char k[4];
                        mbedtls_ctr_drbg_random(&tls->ctr_drbg, k, 4);
                        std::string f;
                        f.push_back((char) 0x8A);
                        f.push_back((char) (0x80 | (unsigned char) pl.size()));
                        f.append((char*) k, 4);
                        for (size_t i = 0; i < pl.size(); i++)
                            f.push_back(pl[i] ^ k[i % 4]);
                        sendTLS(f);
                    }
                }
            }
        } else if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            obs_log(LOG_INFO, "[Websocket Client] Peer close notify received");
            if (onError)
                QMetaObject::invokeMethod(
                    this, [this]() { onError("peer_close_notify"); }, Qt::QueuedConnection);
            break;
        } else if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
            int fd = tls->server_fd.fd;
            if (fd >= 0) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(fd, &fds);
                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 100000;  // 100ms
                select(fd + 1, (r == MBEDTLS_ERR_SSL_WANT_READ) ? &fds : NULL,
                       (r == MBEDTLS_ERR_SSL_WANT_WRITE) ? &fds : NULL, NULL, &tv);
            }
            continue;
        } else if (r < 0) {
            if (onError)
                QMetaObject::invokeMethod(
                    this, [this]() { onError("tls_read"); }, Qt::QueuedConnection);
            break;
        } else {
            break;
        }
    }

    if (connected.load()) {
        if (onClose)
            QMetaObject::invokeMethod(this, [this]() { onClose(); }, Qt::QueuedConnection);
        connected.store(false);
    }
    running.store(false);
}
#endif

bool OneSevenLiveWebsocketClient::sendTLS(const std::string& data) {
#ifdef _WIN32
    return false;
#else
    if (!tls)
        return false;
    int ret = mbedtls_ssl_write(&tls->ssl, (const unsigned char*) data.c_str(), data.length());
    return ret >= 0;
#endif
}

void OneSevenLiveWebsocketClient::sendText(const QString& text) {
    if (!connected.load())
        return;
    std::string m = text.toStdString();
#ifdef _WIN32
    if (!tls || !tls->hWebSocket)
        return;
    WinHttpWebSocketSend(tls->hWebSocket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                         (void*) m.data(), (DWORD) m.size());
#else
    std::string f;
    f.push_back((char) 0x81);
    unsigned char k[4];
    if (!tls)
        return;
    mbedtls_ctr_drbg_random(&tls->ctr_drbg, k, 4);
    if (m.length() <= 125) {
        f.push_back((char) (0x80 | (unsigned char) m.length()));
    } else if (m.length() <= 65535) {
        f.push_back((char) (0x80 | 126));
        f.push_back((char) ((m.length() >> 8) & 0xFF));
        f.push_back((char) (m.length() & 0xFF));
    } else {
        return;
    }
    f.append((char*) k, 4);
    for (size_t i = 0; i < m.length(); i++)
        f.push_back(m[i] ^ k[i % 4]);
    sendTLS(f);
#endif
}

void OneSevenLiveWebsocketClient::cleanupTLS() {
    tls.reset();
}

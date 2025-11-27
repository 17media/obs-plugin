#include "OneSevenLiveWebsocketServer.hpp"

#include <obs-module.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

// System headers for socket operations
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <cstring>

#include "plugin-support.h"

OneSevenLiveWebsocketServer::OneSevenLiveWebsocketServer(const std::string& host, int port)
    : host_(host == "localhost" ? "127.0.0.1" : host), port_(port), running_(false) {}

OneSevenLiveWebsocketServer::~OneSevenLiveWebsocketServer() {
    obs_log(LOG_INFO, "[17Live WebSocket Server] Starting WebSocket server destruction");

    // Ensure server is completely stopped and thread properly terminated
    stop();

    // Additional safety check: ensure thread has completely finished
    if (server_thread_ && server_thread_->joinable()) {
        obs_log(LOG_WARNING,
                "[17Live WebSocket Server] Thread still joinable in destructor, forcing thread "
                "termination wait");
        server_thread_->join();
    }

    obs_log(LOG_INFO, "[17Live WebSocket Server] WebSocket server successfully destroyed");
}

bool OneSevenLiveWebsocketServer::start() {
    if (running_) {
        obs_log(LOG_WARNING, "[17Live WebSocket Server] Server already running.");
        return true;
    }

    try {
        // If port is 0, find an available port first
        int actual_port = port_;
        if (port_ == 0) {
            actual_port = getAvailablePort();
            if (actual_port == 0) {
                obs_log(LOG_ERROR, "[17Live WebSocket Server] Failed to find available port");
                return false;
            }
            port_ = actual_port;
            obs_log(LOG_INFO, "[17Live WebSocket Server] Using auto-assigned port: %d",
                    actual_port);
        }

        // Create WebSocket server instance
        server_ = std::make_unique<websocketpp_server>();

        // Initialize ASIO
        server_->init_asio();
        server_->set_reuse_addr(true);

        // Set up logging
        server_->set_access_channels(websocketpp::log::alevel::none);
        server_->clear_access_channels(websocketpp::log::alevel::all);
        server_->clear_error_channels(websocketpp::log::elevel::all);

        // Register handlers
        server_->set_open_handler([this](websocketpp::connection_hdl hdl) { onConnection(hdl); });

        server_->set_close_handler([this](websocketpp::connection_hdl hdl) { onClose(hdl); });

        server_->set_message_handler(
            [this](websocketpp::connection_hdl hdl, websocketpp_server::message_ptr msg) {
                onMessage(hdl, msg);
            });

        server_->set_fail_handler([this](websocketpp::connection_hdl hdl) { onFail(hdl); });

        // Start server in new thread to avoid blocking main thread
        server_thread_ = std::make_unique<std::thread>([this, actual_port]() {
            try {
                obs_log(LOG_INFO, "[17Live WebSocket Server] Starting server on %s:%d",
                        host_.c_str(), actual_port);

                // Listen on specified port
                server_->listen(actual_port);
                server_->start_accept();

                obs_log(LOG_INFO, "[17Live WebSocket Server] Server started successfully on %s:%d",
                        host_.c_str(), actual_port);

                // Run the IO service
                server_->run();

            } catch (const std::exception& e) {
                obs_log(LOG_ERROR, "[17Live WebSocket Server] Exception during server startup: %s",
                        e.what());
                running_ = false;
            } catch (...) {
                obs_log(LOG_ERROR,
                        "[17Live WebSocket Server] Unknown exception during server startup");
                running_ = false;
            }
        });

        // Wait a bit to see if server can start successfully
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        running_ = true;
        obs_log(LOG_INFO, "[17Live WebSocket Server] Server thread started successfully");

        return true;

    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[17Live WebSocket Server] Failed to create server: %s", e.what());
        return false;
    }
}

void OneSevenLiveWebsocketServer::stop() {
    if (!running_) {
        return;
    }

    obs_log(LOG_INFO, "[17Live WebSocket Server] Stopping WebSocket server");

    running_ = false;

    // Stop the server
    if (server_) {
        server_->stop();
    }

    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& pair : clients_) {
            try {
                server_->close(pair.second, websocketpp::close::status::going_away,
                               "Server shutting down");
            } catch (...) {
                // Ignore errors during shutdown
            }
        }
        clients_.clear();
        client_ips_.clear();
        hdl_to_client_id_.clear();
    }

    // Wait for server thread to finish
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }

    obs_log(LOG_INFO, "[17Live WebSocket Server] WebSocket server stopped");
}

bool OneSevenLiveWebsocketServer::is_running() const {
    return running_;
}

int OneSevenLiveWebsocketServer::getPort() const {
    return port_;
}

void OneSevenLiveWebsocketServer::broadcastMessage(const std::string& message) {
    if (!validate_message_size(message)) {
        obs_log(LOG_WARNING, "[17Live WebSocket Server] Message too large for broadcast: %zu bytes",
                message.size());
        return;
    }

    std::lock_guard<std::mutex> lock(clients_mutex_);

    for (auto it = clients_.begin(); it != clients_.end();) {
        try {
            server_->send(it->second, message, websocketpp::frame::opcode::text);
            ++it;
        } catch (const std::exception& e) {
            obs_log(LOG_WARNING,
                    "[17Live WebSocket Server] Failed to send message to client %s: %s",
                    it->first.c_str(), e.what());

            // Remove failed client
            std::string hdl_str = hdl_to_string(it->second);
            hdl_to_client_id_.erase(hdl_str);
            client_ips_.erase(it->first);
            it = clients_.erase(it);
        }
    }
}

void OneSevenLiveWebsocketServer::sendMessageToClient(const std::string& clientId,
                                                      const std::string& message) {
    if (!validate_message_size(message)) {
        obs_log(LOG_WARNING, "[17Live WebSocket Server] Message too large for client %s: %zu bytes",
                clientId.c_str(), message.size());
        return;
    }

    std::lock_guard<std::mutex> lock(clients_mutex_);

    auto it = clients_.find(clientId);
    if (it != clients_.end()) {
        try {
            server_->send(it->second, message, websocketpp::frame::opcode::text);
        } catch (const std::exception& e) {
            obs_log(LOG_WARNING,
                    "[17Live WebSocket Server] Failed to send message to client %s: %s",
                    clientId.c_str(), e.what());

            // Remove failed client
            std::string hdl_str = hdl_to_string(it->second);
            hdl_to_client_id_.erase(hdl_str);
            client_ips_.erase(it->first);
            clients_.erase(it);
        }
    } else {
        obs_log(LOG_WARNING, "[17Live WebSocket Server] Client %s not found", clientId.c_str());
    }
}

size_t OneSevenLiveWebsocketServer::getConnectedClientsCount() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return clients_.size();
}

std::vector<std::string> OneSevenLiveWebsocketServer::getConnectedClientIds() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    std::vector<std::string> clientIds;
    clientIds.reserve(clients_.size());

    for (const auto& pair : clients_) {
        clientIds.push_back(pair.first);
    }

    return clientIds;
}

void OneSevenLiveWebsocketServer::setMessageCallback(const MessageCallback& callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    message_callback_ = callback;
}

void OneSevenLiveWebsocketServer::setConnectionCallback(const ConnectionCallback& callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callback_ = callback;
}

bool OneSevenLiveWebsocketServer::check_rate_limit(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto now = std::chrono::steady_clock::now();
    auto& timestamps = rate_limit_map_[client_ip];

    // Remove old timestamps
    timestamps.erase(
        std::remove_if(
            timestamps.begin(), timestamps.end(),
            [now](const std::chrono::steady_clock::time_point& timestamp) {
                return std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count() >
                       RATE_LIMIT_WINDOW_SECONDS;
            }),
        timestamps.end());

    // Check if rate limit exceeded
    if (timestamps.size() >= RATE_LIMIT_MESSAGES) {
        return false;
    }

    // Add current timestamp
    timestamps.push_back(now);
    return true;
}

bool OneSevenLiveWebsocketServer::validate_message_size(const std::string& message) const {
    return message.size() <= MAX_MESSAGE_SIZE;
}

std::string OneSevenLiveWebsocketServer::generate_client_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << "client_";
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }

    return ss.str();
}

std::string OneSevenLiveWebsocketServer::get_client_ip(websocketpp::connection_hdl hdl) {
    try {
        auto con = server_->get_con_from_hdl(hdl);
        return con->get_remote_endpoint();
    } catch (const std::exception& e) {
        obs_log(LOG_WARNING, "[17Live WebSocket Server] Failed to get client IP: %s", e.what());
        return "unknown";
    }
}

std::string OneSevenLiveWebsocketServer::hdl_to_string(websocketpp::connection_hdl hdl) {
    // Convert connection_hdl to a unique string identifier
    // Since connection_hdl is std::weak_ptr<void>, we can use the pointer address
    try {
        if (auto locked = hdl.lock()) {
            // Use the raw pointer address as a unique identifier
            std::stringstream ss;
            ss << "hdl_" << locked.get();
            return ss.str();
        }
    } catch (const std::exception& e) {
        obs_log(LOG_WARNING, "[17Live WebSocket Server] Failed to convert hdl to string: %s",
                e.what());
    }
    return "hdl_unknown";
}

int OneSevenLiveWebsocketServer::getAvailablePort() const {
    int available_port = 0;
    // Create a socket to find an available port (platform-specific)
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        obs_log(LOG_ERROR, "[17Live WebSocket Server] WSAStartup failed for port detection");
        return 0;
    }

    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == INVALID_SOCKET) {
        obs_log(LOG_ERROR, "[17Live WebSocket Server] Failed to create socket for port detection");
        WSACleanup();
        return 0;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host_.c_str());
    addr.sin_port = 0;  // Let system choose port

    if (bind(sockfd, (struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) {
        obs_log(LOG_ERROR, "[17Live WebSocket Server] Failed to bind socket for port detection");
        closesocket(sockfd);
        WSACleanup();
        return 0;
    }

    int addr_len = sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr*) &addr, &addr_len) == SOCKET_ERROR) {
        obs_log(LOG_ERROR,
                "[17Live WebSocket Server] Failed to get socket name for port detection");
        closesocket(sockfd);
        WSACleanup();
        return 0;
    }

    available_port = ntohs(addr.sin_port);
    closesocket(sockfd);
    WSACleanup();
#else
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        obs_log(LOG_ERROR, "[17Live WebSocket Server] Failed to create socket for port detection");
        return 0;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host_.c_str());
    addr.sin_port = 0;  // Let system choose port

    if (bind(sockfd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        obs_log(LOG_ERROR, "[17Live WebSocket Server] Failed to bind socket for port detection");
        close(sockfd);
        return 0;
    }

    socklen_t addr_len = sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr*) &addr, &addr_len) < 0) {
        obs_log(LOG_ERROR,
                "[17Live WebSocket Server] Failed to get socket name for port detection");
        close(sockfd);
        return 0;
    }

    available_port = ntohs(addr.sin_port);
    close(sockfd);
#endif

    obs_log(LOG_INFO, "[17Live WebSocket Server] Found available port: %d", available_port);
    return available_port;
}

void OneSevenLiveWebsocketServer::onConnection(websocketpp::connection_hdl hdl) {
    if (!running_) {
        return;
    }

    std::string clientId = generate_client_id();
    std::string clientIp = get_client_ip(hdl);

    obs_log(LOG_DEBUG, "[17Live WebSocket Server] New connection: %s from %s", clientId.c_str(),
            clientIp.c_str());

    // Store client connection
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_[clientId] = hdl;
        client_ips_[clientId] = clientIp;
        std::string hdl_str = hdl_to_string(hdl);
        hdl_to_client_id_[hdl_str] = clientId;
    }

    // Send welcome message to newly connected client
    try {
        std::string welcomeMessage =
            "Hello! Welcome to 17Live WebSocket Server. Connection established successfully.";
        server_->send(hdl, welcomeMessage, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        obs_log(LOG_WARNING,
                "[17Live WebSocket Server] Failed to send welcome message to client %s: %s",
                clientId.c_str(), e.what());
    }

    // Notify connection callback
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (connection_callback_) {
            connection_callback_(clientId, true);
        }
    }
}

void OneSevenLiveWebsocketServer::onClose(websocketpp::connection_hdl hdl) {
    std::string clientId;

    // Find client ID
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::string hdl_str = hdl_to_string(hdl);
        auto it = hdl_to_client_id_.find(hdl_str);
        if (it != hdl_to_client_id_.end()) {
            clientId = it->second;

            obs_log(LOG_DEBUG, "[17Live WebSocket Server] Client %s disconnected",
                    clientId.c_str());

            // Remove client from connections
            clients_.erase(clientId);
            client_ips_.erase(clientId);
            hdl_to_client_id_.erase(it);
        }
    }

    // Notify connection callback
    if (!clientId.empty()) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (connection_callback_) {
            connection_callback_(clientId, false);
        }
    }
}

void OneSevenLiveWebsocketServer::onMessage(websocketpp::connection_hdl hdl,
                                            websocketpp_server::message_ptr msg) {
    if (!running_) {
        return;
    }

    // Find client ID for this connection
    std::string clientId;
    std::string clientIp;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::string hdl_str = hdl_to_string(hdl);
        auto it = hdl_to_client_id_.find(hdl_str);
        if (it != hdl_to_client_id_.end()) {
            clientId = it->second;
            auto ip_it = client_ips_.find(clientId);
            if (ip_it != client_ips_.end()) {
                clientIp = ip_it->second;
            }
        }
    }

    if (clientId.empty()) {
        obs_log(LOG_WARNING, "[17Live WebSocket Server] Received message from unknown client");
        return;
    }

    // Rate limiting check
    if (!check_rate_limit(clientIp)) {
        obs_log(LOG_WARNING, "[17Live WebSocket Server] Rate limit exceeded for client %s",
                clientId.c_str());

        try {
            server_->close(hdl, websocketpp::close::status::policy_violation,
                           "Rate limit exceeded");
        } catch (...) {
            // Ignore errors during forced close
        }
        return;
    }

    // Message size validation
    if (!validate_message_size(msg->get_payload())) {
        obs_log(LOG_WARNING,
                "[17Live WebSocket Server] Message too large from client %s: %zu bytes",
                clientId.c_str(), msg->get_payload().size());
        return;
    }

    // Notify message callback
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (message_callback_) {
            message_callback_(clientId, msg->get_payload());
        }
    }
}

void OneSevenLiveWebsocketServer::onFail(websocketpp::connection_hdl hdl) {
    std::string clientId;

    // Find client ID
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::string hdl_str = hdl_to_string(hdl);
        auto it = hdl_to_client_id_.find(hdl_str);
        if (it != hdl_to_client_id_.end()) {
            clientId = it->second;

            obs_log(LOG_ERROR, "[17Live WebSocket Server] Connection failed for client %s",
                    clientId.c_str());

            // Remove client from connections
            clients_.erase(clientId);
            client_ips_.erase(clientId);
            hdl_to_client_id_.erase(it);
        }
    }

    // Notify connection callback
    if (!clientId.empty()) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (connection_callback_) {
            connection_callback_(clientId, false);
        }
    }
}

#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <atomic>
#include <random>

// Include ASIO first to ensure ASIO_STANDALONE is properly defined
#include <asio.hpp>

// Now include websocketpp - ASIO_STANDALONE should be defined
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::asio> websocketpp_server;

class OneSevenLiveWebsocketServer {
   public:
    using MessageCallback = std::function<void(const std::string& clientId, const std::string& message)>;
    using ConnectionCallback = std::function<void(const std::string& clientId, bool connected)>;

    OneSevenLiveWebsocketServer(const std::string& host = "localhost", int port = 0);
    ~OneSevenLiveWebsocketServer();

    bool start();
    void stop();
    bool is_running() const;
    int getPort() const;

    // Message broadcasting
    void broadcastMessage(const std::string& message);
    void sendMessageToClient(const std::string& clientId, const std::string& message);
    
    // Client management
    size_t getConnectedClientsCount() const;
    std::vector<std::string> getConnectedClientIds() const;

    // Callback setters
    void setMessageCallback(const MessageCallback& callback);
    void setConnectionCallback(const ConnectionCallback& callback);

   private:
    // Security-related methods
    bool check_rate_limit(const std::string& client_ip);
    bool validate_message_size(const std::string& message) const;
    std::string generate_client_id();
    std::string get_client_ip(websocketpp::connection_hdl hdl);
    std::string hdl_to_string(websocketpp::connection_hdl hdl);
    
    // Port management helper
    int getAvailablePort() const;

    // WebSocket event handlers
    void onConnection(websocketpp::connection_hdl hdl);
    void onClose(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, websocketpp_server::message_ptr msg);
    void onFail(websocketpp::connection_hdl hdl);

    // Server instance
    std::unique_ptr<websocketpp_server> server_;
    std::string host_;
    int port_;
    std::unique_ptr<std::thread> server_thread_;
    std::atomic<bool> running_;

    // Client management
    mutable std::mutex clients_mutex_;
    std::unordered_map<std::string, websocketpp::connection_hdl> clients_;
    std::unordered_map<std::string, std::string> client_ips_;
    std::unordered_map<std::string, std::string> hdl_to_client_id_;

    // Security-related member variables
    static constexpr size_t MAX_MESSAGE_SIZE = 64 * 1024;    // 64KB
    static constexpr int RATE_LIMIT_MESSAGES = 50;          // Maximum messages per minute
    static constexpr int RATE_LIMIT_WINDOW_SECONDS = 60;

    mutable std::mutex rate_limit_mutex_;
    std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>>
        rate_limit_map_;

    // Callbacks
    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    mutable std::mutex callback_mutex_;
};

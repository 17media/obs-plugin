#include "OneSevenLiveWebsocketServer.hpp"

#include <obs-module.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "plugin-support.h"

OneSevenLiveWebsocketServer::OneSevenLiveWebsocketServer(const std::string& host, int port)
    : host_(host), port_(port), running_(false) {
    blog(LOG_INFO, "[17Live WebSocket Server] Initializing WebSocket server on %s:%d", 
         host_.c_str(), port_);
}

OneSevenLiveWebsocketServer::~OneSevenLiveWebsocketServer() {
    blog(LOG_INFO, "[17Live WebSocket Server] Starting WebSocket server destruction");
    
    // Ensure server is completely stopped and thread properly terminated
    stop();
    
    // Additional safety check: ensure thread has completely finished
    if (server_thread_ && server_thread_->joinable()) {
        blog(LOG_WARNING,
             "[17Live WebSocket Server] Thread still joinable in destructor, forcing thread termination wait");
        server_thread_->join();
    }
    
    blog(LOG_INFO, "[17Live WebSocket Server] WebSocket server successfully destroyed");
}

bool OneSevenLiveWebsocketServer::start() {
    if (running_) {
        blog(LOG_WARNING, "[17Live WebSocket Server] Server already running.");
        return true;
    }

    try {
        // Create WebSocket server instance
        server_ = std::make_unique<ix::WebSocketServer>(port_, host_);
        
        // Set connection handler
        server_->setOnConnectionCallback(
            [this](std::weak_ptr<ix::WebSocket> webSocket, 
                   std::shared_ptr<ix::ConnectionState> connectionState) {
                onConnection(webSocket, connectionState);
            });

        // Start server in new thread to avoid blocking main thread
        server_thread_ = std::make_unique<std::thread>([this]() {
            try {
                blog(LOG_INFO, "[17Live WebSocket Server] Starting server on %s:%d", 
                     host_.c_str(), port_);
                
                auto result = server_->listen();
                if (!result.first) {
                    blog(LOG_ERROR, "[17Live WebSocket Server] Failed to start server: %s", 
                         result.second.c_str());
                    running_ = false;
                    return;
                }
                
                blog(LOG_INFO, "[17Live WebSocket Server] Server started successfully");
                
                // Start the server
                server_->start();
                
            } catch (const std::exception& e) {
                blog(LOG_ERROR, "[17Live WebSocket Server] Exception during server startup: %s", 
                     e.what());
                running_ = false;
            } catch (...) {
                blog(LOG_ERROR, "[17Live WebSocket Server] Unknown exception during server startup");
                running_ = false;
            }
        });

        // Wait a bit to see if server can start successfully
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        running_ = true;
        blog(LOG_INFO, "[17Live WebSocket Server] Server thread started successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        blog(LOG_ERROR, "[17Live WebSocket Server] Failed to create server: %s", e.what());
        return false;
    }
}

void OneSevenLiveWebsocketServer::stop() {
    if (!running_) {
        return;
    }
    
    blog(LOG_INFO, "[17Live WebSocket Server] Stopping WebSocket server");
    
    running_ = false;
    
    // Stop the server
    if (server_) {
        server_->stop();
    }
    
    // Wait for server thread to finish
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    // Clear client connections
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.clear();
        client_ips_.clear();
    }
    
    blog(LOG_INFO, "[17Live WebSocket Server] WebSocket server stopped");
}

bool OneSevenLiveWebsocketServer::is_running() const {
    return running_;
}

int OneSevenLiveWebsocketServer::getPort() const {
    return port_;
}

void OneSevenLiveWebsocketServer::broadcastMessage(const std::string& message) {
    if (!validate_message_size(message)) {
        blog(LOG_WARNING, "[17Live WebSocket Server] Message too large for broadcast: %zu bytes", 
             message.size());
        return;
    }
    
    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    for (auto it = clients_.begin(); it != clients_.end();) {
        if (auto webSocket = it->second.lock()) {
            try {
                webSocket->send(message);
                ++it;
            } catch (const std::exception& e) {
                blog(LOG_WARNING, "[17Live WebSocket Server] Failed to send message to client %s: %s", 
                     it->first.c_str(), e.what());
                it = clients_.erase(it);
            }
        } else {
            // WebSocket is no longer valid, remove from clients
            it = clients_.erase(it);
        }
    }
}

void OneSevenLiveWebsocketServer::sendMessageToClient(const std::string& clientId, 
                                                     const std::string& message) {
    if (!validate_message_size(message)) {
        blog(LOG_WARNING, "[17Live WebSocket Server] Message too large for client %s: %zu bytes", 
             clientId.c_str(), message.size());
        return;
    }
    
    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    auto it = clients_.find(clientId);
    if (it != clients_.end()) {
        if (auto webSocket = it->second.lock()) {
            try {
                webSocket->send(message);
            } catch (const std::exception& e) {
                blog(LOG_WARNING, "[17Live WebSocket Server] Failed to send message to client %s: %s", 
                     clientId.c_str(), e.what());
                clients_.erase(it);
            }
        } else {
            // WebSocket is no longer valid, remove from clients
            clients_.erase(it);
        }
    } else {
        blog(LOG_WARNING, "[17Live WebSocket Server] Client %s not found", clientId.c_str());
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
        std::remove_if(timestamps.begin(), timestamps.end(),
                      [now](const std::chrono::steady_clock::time_point& timestamp) {
                          return std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count() 
                                 > RATE_LIMIT_WINDOW_SECONDS;
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

std::string OneSevenLiveWebsocketServer::get_client_ip(std::shared_ptr<ix::ConnectionState> connectionState) {
    if (connectionState) {
        return connectionState->getRemoteIp();
    }
    return "unknown";
}

void OneSevenLiveWebsocketServer::onConnection(std::weak_ptr<ix::WebSocket> webSocket, 
                                              std::shared_ptr<ix::ConnectionState> connectionState) {
    auto ws = webSocket.lock();
    if (!ws) {
        return;
    }
    
    std::string clientId = generate_client_id();
    std::string clientIp = get_client_ip(connectionState);
    
    blog(LOG_INFO, "[17Live WebSocket Server] New connection: %s from %s", 
         clientId.c_str(), clientIp.c_str());
    
    // Store client connection
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_[clientId] = webSocket;
        client_ips_[clientId] = clientIp;
    }
    
    // Set message handler for this connection
    ws->setOnMessageCallback([this, clientId, clientIp, ws](const ix::WebSocketMessagePtr& msg) {
        onMessage(nullptr, *ws, msg);
    });
    
    // Notify connection callback
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (connection_callback_) {
            connection_callback_(clientId, true);
        }
    }
}

void OneSevenLiveWebsocketServer::onMessage(std::shared_ptr<ix::ConnectionState> connectionState,
                                           ix::WebSocket& webSocket,
                                           const ix::WebSocketMessagePtr& msg) {
    UNUSED_PARAMETER(connectionState);
    
    if (!msg) {
        return;
    }
    
    // Find client ID for this WebSocket
    std::string clientId;
    std::string clientIp;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& pair : clients_) {
            if (auto ws = pair.second.lock()) {
                if (ws.get() == &webSocket) {
                    clientId = pair.first;
                    auto ip_it = client_ips_.find(clientId);
                    if (ip_it != client_ips_.end()) {
                        clientIp = ip_it->second;
                    }
                    break;
                }
            }
        }
    }
    
    if (clientId.empty()) {
        blog(LOG_WARNING, "[17Live WebSocket Server] Received message from unknown client");
        return;
    }
    
    switch (msg->type) {
        case ix::WebSocketMessageType::Message:
            {
                // Rate limiting check
                if (!check_rate_limit(clientIp)) {
                    blog(LOG_WARNING, "[17Live WebSocket Server] Rate limit exceeded for client %s", 
                         clientId.c_str());
                    webSocket.close();
                    return;
                }
                
                // Message size validation
                if (!validate_message_size(msg->str)) {
                    blog(LOG_WARNING, "[17Live WebSocket Server] Message too large from client %s: %zu bytes", 
                         clientId.c_str(), msg->str.size());
                    return;
                }
                
                blog(LOG_INFO, "[17Live WebSocket Server] Received message from %s: %s", 
                     clientId.c_str(), msg->str.c_str());
                
                // Notify message callback
                {
                    std::lock_guard<std::mutex> lock(callback_mutex_);
                    if (message_callback_) {
                        message_callback_(clientId, msg->str);
                    }
                }
            }
            break;
            
        case ix::WebSocketMessageType::Close:
            {
                blog(LOG_INFO, "[17Live WebSocket Server] Client %s disconnected", clientId.c_str());
                
                // Remove client from connections
                {
                    std::lock_guard<std::mutex> lock(clients_mutex_);
                    clients_.erase(clientId);
                    client_ips_.erase(clientId);
                }
                
                // Notify connection callback
                {
                    std::lock_guard<std::mutex> lock(callback_mutex_);
                    if (connection_callback_) {
                        connection_callback_(clientId, false);
                    }
                }
            }
            break;
            
        case ix::WebSocketMessageType::Error:
            {
                blog(LOG_ERROR, "[17Live WebSocket Server] Error from client %s: %s", 
                     clientId.c_str(), msg->errorInfo.reason.c_str());
            }
            break;
            
        case ix::WebSocketMessageType::Ping:
            {
                blog(LOG_DEBUG, "[17Live WebSocket Server] Ping from client %s", clientId.c_str());
                // IXWebSocket automatically handles pong responses
            }
            break;
            
        case ix::WebSocketMessageType::Pong:
            {
                blog(LOG_DEBUG, "[17Live WebSocket Server] Pong from client %s", clientId.c_str());
            }
            break;
            
        default:
            blog(LOG_DEBUG, "[17Live WebSocket Server] Unknown message type from client %s", 
                 clientId.c_str());
            break;
    }
}

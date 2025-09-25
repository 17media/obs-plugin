#ifndef ONESEVENLIVEHTTPSERVER_HPP
#define ONESEVENLIVEHTTPSERVER_HPP

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "../../deps/cpp-httplib/httplib.h"

class OneSevenLiveHttpServer {
   public:
    OneSevenLiveHttpServer(const std::string& host, int port = 0,
                           const std::string& base_dir_relative_to_module_data = "html");
    ~OneSevenLiveHttpServer();

    bool start();
    void stop();
    bool is_running() const;
    int getPort() const;

   private:
    std::string get_mime_type(const std::string& file_path) const;
    std::string get_file_extension(const std::string& file_path) const;

    // Security-related methods
    bool is_safe_path(const std::string& path) const;
    bool check_rate_limit(const std::string& client_ip);
    bool validate_request_size(const httplib::Request& req) const;
    std::string generate_csrf_token();
    bool validate_csrf_token(const std::string& token) const;

    httplib::Server svr_;
    std::string host_;
    int port_ = 0;  // Default to 0, meaning find an available port
    std::string base_dir_;
    std::unique_ptr<std::thread> server_thread_;
    bool running_ = false;

    // Security-related member variables
    static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;  // 1MB
    static constexpr int RATE_LIMIT_REQUESTS = 100;          // Maximum requests per minute
    static constexpr int RATE_LIMIT_WINDOW_SECONDS = 60;

    mutable std::mutex rate_limit_mutex_;
    std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>>
        rate_limit_map_;
    std::string csrf_token_;
};

#endif  // ONESEVENLIVEHTTPSERVER_HPP

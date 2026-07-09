#ifndef ONESEVENLIVEHTTPSERVER_HPP
#define ONESEVENLIVEHTTPSERVER_HPP

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <QPointer>
#include <QThread>

#include "../../deps/cpp-httplib/httplib.h"

class OneSevenLiveHttpServer {
   public:
    OneSevenLiveHttpServer(const std::string& host, int port = 0,
                           const std::string& base_dir_relative_to_module_data = "html",
                           const std::string& name = "17Live HTTP Server");
    ~OneSevenLiveHttpServer();

    bool start();
    void stop();
    void stopAsync();
    bool is_running() const;
    int getPort() const;

    void addGetHandler(const std::string& pattern,
                       std::function<void(const httplib::Request&, httplib::Response&)> handler);
    void addPostHandler(const std::string& pattern,
                        std::function<void(const httplib::Request&, httplib::Response&)> handler);
    void setEnableDefaultApi(bool enable);

   private:
    std::string get_mime_type(const std::string& file_path) const;
    std::string get_file_extension(const std::string& file_path) const;
    void serve_file(const std::filesystem::path& file_path, httplib::Response& res) const;
    bool handle_enter_animation_cache_request(const httplib::Request& req, httplib::Response& res);
    bool ensure_enter_animation_asset_cached(const std::string& source_url,
                                             std::filesystem::path& cached_file_path,
                                             std::string& error_message);
    std::string get_enter_animation_asset_cache_dir() const;

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
    QPointer<QThread> server_thread_;
    bool running_ = false;
    std::atomic<bool> stopping_{false};

    std::vector<
        std::pair<std::string, std::function<void(const httplib::Request&, httplib::Response&)>>>
        extra_get_;
    std::vector<
        std::pair<std::string, std::function<void(const httplib::Request&, httplib::Response&)>>>
        extra_post_;
    bool enable_default_api_ = true;

    // Security-related member variables
    static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;  // 1MB
    static constexpr int RATE_LIMIT_REQUESTS = 100;          // Maximum requests per minute
    static constexpr int RATE_LIMIT_WINDOW_SECONDS = 60;

    mutable std::mutex rate_limit_mutex_;
    mutable std::mutex asset_cache_mutex_;
    std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>>
        rate_limit_map_;
    std::string csrf_token_;
    std::string name_;
};

#endif  // ONESEVENLIVEHTTPSERVER_HPP

#include "OneSevenLiveHttpServer.hpp"

#include <obs-module.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <vector>

#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveCoreManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "plugin-support.h"

// Helper function to get module data path
std::string get_obs_module_data_path_str() {
    const char* path = obs_get_module_data_path(obs_current_module());
    if (path) {
        return std::string(path);
    }
    return "";  // Or throw exception, or return a default known path
}

std::string OneSevenLiveHttpServer::get_file_extension(const std::string& file_path) const {
    size_t dot_pos = file_path.rfind('.');
    if (dot_pos != std::string::npos) {
        return file_path.substr(dot_pos + 1);
    }
    return "";
}

std::string OneSevenLiveHttpServer::get_mime_type(const std::string& file_path) const {
    std::string ext = get_file_extension(file_path);
    if (ext == "html" || ext == "htm")
        return "text/html; charset=utf-8";
    if (ext == "css")
        return "text/css; charset=utf-8";
    if (ext == "js")
        return "application/javascript; charset=utf-8";
    if (ext == "json")
        return "application/json; charset=utf-8";
    if (ext == "png")
        return "image/png";
    if (ext == "jpg" || ext == "jpeg")
        return "image/jpeg";
    if (ext == "gif")
        return "image/gif";
    if (ext == "svg")
        return "image/svg+xml";
    if (ext == "ico")
        return "image/x-icon";
    if (ext == "woff2")
        return "font/woff2";
    if (ext == "woff")
        return "font/woff";
    if (ext == "ttf")
        return "font/ttf";
    return "application/octet-stream";
}

OneSevenLiveHttpServer::OneSevenLiveHttpServer(const std::string& host, int port,
                                               const std::string& base_dir_relative_to_module_data)
    : host_(host), port_(port), running_(false) {
    std::string module_data_path = get_obs_module_data_path_str();
    if (module_data_path.empty()) {
        blog(LOG_ERROR, "[17Live HTTP Server] Failed to get OBS module data path.");
        // Can choose to set a default base_dir_ or let server startup fail
        base_dir_ = base_dir_relative_to_module_data;  // Fallback or error state
    } else {
        std::filesystem::path full_base_path =
            std::filesystem::path(module_data_path) / base_dir_relative_to_module_data;
        base_dir_ = full_base_path.string();
    }

    blog(LOG_INFO, "[17Live HTTP Server] Base directory set to: %s", base_dir_.c_str());

    // Initialize CSRF token
    csrf_token_ = generate_csrf_token();
}

OneSevenLiveHttpServer::~OneSevenLiveHttpServer() {
    blog(LOG_INFO, "[17Live HTTP Server] Starting HTTP server destruction");

    // Ensure server is completely stopped and thread properly terminated
    stop();

    // Additional safety check: ensure thread has completely finished
    if (server_thread_ && server_thread_->joinable()) {
        blog(LOG_WARNING,
             "[17Live HTTP Server] Thread still joinable in destructor, forcing thread termination "
             "wait");
        server_thread_->join();
    }

    blog(LOG_INFO, "[17Live HTTP Server] HTTP server successfully destroyed");
}

bool OneSevenLiveHttpServer::start() {
    if (running_) {
        blog(LOG_WARNING, "[17Live HTTP Server] Server already running.");
        return true;
    }

    // Ensure base_dir_ exists
    if (!std::filesystem::exists(base_dir_) || !std::filesystem::is_directory(base_dir_)) {
        blog(LOG_ERROR,
             "[17Live HTTP Server] Base directory '%s' does not exist or is not a directory.",
             base_dir_.c_str());
        return false;
    }

    // Set up static file service
    // The second parameter of httplib's set_mount_point should be a path relative to current
    // working directory, or absolute path. We have already calculated base_dir_ as absolute path.
    if (!svr_.set_mount_point("/", base_dir_.c_str())) {
        blog(LOG_ERROR, "[17Live HTTP Server] Failed to set mount point '/' to '%s'",
             base_dir_.c_str());
        return false;
    }
    blog(LOG_INFO, "[17Live HTTP Server] Mounting '/' to serve files from '%s'", base_dir_.c_str());

    // Override the default handler for static files to add security checks
    svr_.Get("/.*", [this](const httplib::Request& req, httplib::Response& res) {
        // Security check: get client IP
        std::string client_ip = req.get_header_value("X-Forwarded-For");
        if (client_ip.empty()) {
            client_ip = req.get_header_value("X-Real-IP");
        }
        if (client_ip.empty()) {
            client_ip = "127.0.0.1";  // fallback
        }

        // Security check: rate limiting
        if (!check_rate_limit(client_ip)) {
            res.status = 429;
            res.set_content("Too Many Requests", "text/plain");
            return;
        }

        std::string path = req.path;
        if (path == "/") {
            path = "/index.html";
        }

        // Security check: path validation
        if (!is_safe_path(path)) {
            res.status = 403;
            res.set_content("Forbidden", "text/plain");
            return;
        }

        // Serve the file
        std::filesystem::path file_path = std::filesystem::path(base_dir_) / path.substr(1);
        std::string file_path_str = file_path.string();

        try {
            if (std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path)) {
                std::ifstream ifs(file_path_str, std::ios::in | std::ios::binary);
                if (ifs.is_open() && ifs.good()) {
                    std::string content((std::istreambuf_iterator<char>(ifs)),
                                        (std::istreambuf_iterator<char>()));
                    if (ifs.bad()) {
                        blog(LOG_ERROR, "[17Live HTTP Server] Error reading file: %s",
                             file_path_str.c_str());
                        res.status = 500;
                        res.set_content("Internal Server Error", "text/plain");
                    } else {
                        res.set_content(content, get_mime_type(file_path_str).c_str());
                    }
                } else {
                    blog(LOG_ERROR, "[17Live HTTP Server] Failed to open file: %s",
                         file_path_str.c_str());
                    res.status = 500;
                    res.set_content("Internal Server Error", "text/plain");
                }
            } else {
                res.status = 404;
                res.set_content("Not Found", "text/plain");
            }
        } catch (const std::filesystem::filesystem_error& e) {
            blog(LOG_ERROR, "[17Live HTTP Server] Filesystem error for %s: %s",
                 file_path_str.c_str(), e.what());
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
        } catch (const std::exception& e) {
            blog(LOG_ERROR, "[17Live HTTP Server] Exception serving file %s: %s",
                 file_path_str.c_str(), e.what());
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
        }
    });

    // Provide index.html by default
    svr_.Get("/", [this](const httplib::Request& req, httplib::Response& res) {
        // Security check: rate limiting
        std::string client_ip = req.get_header_value("X-Forwarded-For");
        if (client_ip.empty()) {
            client_ip = req.get_header_value("X-Real-IP");
        }
        if (client_ip.empty()) {
            client_ip = "127.0.0.1";  // local request
        }

        if (!check_rate_limit(client_ip)) {
            res.status = 429;  // Too Many Requests
            res.set_content("Rate limit exceeded", "text/plain");
            return;
        }

        obs_log(LOG_INFO, "[17Live HTTP Server] Handling request for %s from %s", req.path.c_str(),
                client_ip.c_str());

        // Security check: path validation
        if (!is_safe_path(req.path)) {
            blog(LOG_WARNING, "[17Live HTTP Server] Unsafe path detected: %s", req.path.c_str());
            res.status = 403;
            res.set_content("Forbidden", "text/plain");
            return;
        }

        std::filesystem::path path_obj = std::filesystem::path(base_dir_) / "index.html";
        std::string path_str = path_obj.string();

        if (!std::filesystem::exists(path_obj)) {
            path_obj = std::filesystem::path(base_dir_) / "index.html";
            path_str = path_obj.string();
        }

        try {
            std::ifstream ifs(path_str, std::ios::in | std::ios::binary);
            if (ifs.is_open() && ifs.good()) {
                std::string content((std::istreambuf_iterator<char>(ifs)),
                                    (std::istreambuf_iterator<char>()));
                if (ifs.bad()) {
                    blog(LOG_ERROR, "[17Live HTTP Server] Error reading index.html: %s",
                         path_str.c_str());
                    res.status = 500;
                    res.set_content("Internal Server Error", "text/plain");
                } else {
                    res.set_content(content, get_mime_type(path_str).c_str());
                }
            } else {
                blog(LOG_WARNING, "[17Live HTTP Server] File not found for /: %s",
                     path_str.c_str());
                res.status = 404;
                res.set_content("File not found", "text/plain");  // Don't expose internal paths
            }
        } catch (const std::filesystem::filesystem_error& e) {
            blog(LOG_ERROR, "[17Live HTTP Server] Filesystem error for index.html %s: %s",
                 path_str.c_str(), e.what());
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
        } catch (const std::exception& e) {
            blog(LOG_ERROR, "[17Live HTTP Server] Exception serving index.html %s: %s",
                 path_str.c_str(), e.what());
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
        }
    });

    svr_.Get("/ping", [this](const httplib::Request& req, httplib::Response& res) {
        // Security check: rate limiting
        std::string client_ip = req.get_header_value("X-Forwarded-For");
        if (client_ip.empty()) {
            client_ip = req.get_header_value("X-Real-IP");
        }
        if (client_ip.empty()) {
            client_ip = "127.0.0.1";
        }

        if (!check_rate_limit(client_ip)) {
            res.status = 429;
            res.set_content("Rate limit exceeded", "text/plain");
            return;
        }

        res.set_content("PONG", "text/plain");
    });

    // Add CSRF token endpoint
    svr_.Get("/csrf-token", [this](const httplib::Request& req, httplib::Response& res) {
        // Security check: rate limiting
        std::string client_ip = req.get_header_value("X-Forwarded-For");
        if (client_ip.empty()) {
            client_ip = req.get_header_value("X-Real-IP");
        }
        if (client_ip.empty()) {
            client_ip = "127.0.0.1";
        }

        if (!check_rate_limit(client_ip)) {
            res.status = 429;
            res.set_header("Content-Type", "application/json");
            const nlohmann::json errorResponse = {{"success", false},
                                                  {"error", "Rate limit exceeded"}};
            const std::string responseStr = errorResponse.dump();
            res.set_content(responseStr, "application/json");
            return;
        }

        res.set_header("Content-Type", "application/json");
        const nlohmann::json response = {{"success", true}, {"csrf_token", csrf_token_}};
        const std::string responseStr = response.dump();
        res.set_content(responseStr, "application/json");
    });

    // Add /lapi route to handle API requests
    svr_.Post("/lapi", [this](const httplib::Request& req, httplib::Response& res) {
        // Security check: get client IP
        std::string client_ip = req.get_header_value("X-Forwarded-For");
        if (client_ip.empty()) {
            client_ip = req.get_header_value("X-Real-IP");
        }
        if (client_ip.empty()) {
            client_ip = "127.0.0.1";  // local request
        }

        // Security check: rate limiting
        if (!check_rate_limit(client_ip)) {
            res.status = 429;
            res.set_header("Content-Type", "application/json");
            const nlohmann::json errorResponse = {{"success", false},
                                                  {"error", "Rate limit exceeded"}};
            const std::string responseStr = errorResponse.dump();
            res.set_content(responseStr, "application/json");
            return;
        }

        // Security check: request size validation
        if (!validate_request_size(req)) {
            res.status = 413;  // Payload Too Large
            res.set_header("Content-Type", "application/json");
            const nlohmann::json errorResponse = {{"success", false},
                                                  {"error", "Request too large"}};
            const std::string responseStr = errorResponse.dump();
            res.set_content(responseStr, "application/json");
            return;
        }

        // obs_log(LOG_INFO, "[17Live HTTP Server] Handling API request to /lapi from %s",
        // client_ip.c_str());

        // Set response headers
        res.set_header("Content-Type", "application/json");
        res.set_header("X-Content-Type-Options", "nosniff");
        res.set_header("X-Frame-Options", "DENY");
        res.set_header("X-XSS-Protection", "1; mode=block");

        // Get OneSevenLiveCoreManager instance
        auto& coreManager = OneSevenLiveCoreManager::getInstance();

        // Parse JSON data from request body
        nlohmann::json requestJson;
        try {
            requestJson = nlohmann::json::parse(req.body);
        } catch (const nlohmann::json::parse_error& e) {
            // JSON parsing error - pre-build error message to avoid repeated string operations
            const std::string errorMsg = "Invalid JSON: " + std::string(e.what());
            const nlohmann::json errorResponse = {{"success", false}, {"error", errorMsg}};
            const std::string responseStr = errorResponse.dump();
            res.set_content(responseStr, "application/json");
            return;
        }

        // Get requested action
        if (!requestJson.contains("action") || !requestJson["action"].is_string()) {
            // Missing action parameter
            const nlohmann::json errorResponse = {{"success", false},
                                                  {"error", "Missing 'action' parameter"}};
            const std::string responseStr = errorResponse.dump();
            res.set_content(responseStr, "application/json");
            return;
        }

        const std::string action = requestJson["action"].get<std::string>();

        // Call API and return result
        nlohmann::json apiResult;
        bool success = false;

        try {
            // Get apiWrapper instance
            auto apiWrapper = coreManager.getApiWrapper();
            auto configManager = coreManager.getConfigManager();

            if (!apiWrapper) {
                // API Wrapper not initialized
                const nlohmann::json errorResponse = {{"success", false},
                                                      {"error", "API not initialized"}};
                const std::string responseStr = errorResponse.dump();
                res.set_content(responseStr, "application/json");
                return;
            }

            // Call corresponding API function based on action
            if (action == ACTION_GETABLYTOKEN) {
                std::string roomID;
                configManager->getConfigValue("RoomID", roomID);
                success = apiWrapper->GetAblyToken(roomID, apiResult);
            } else if (action == ACTION_GETGIFTS) {
                if (!configManager->loadGifts(apiResult)) {
                    std::string language;
                    configManager->getConfigValue("Region", language);
                    success = apiWrapper->GetGifts(language, apiResult);
                    configManager->saveGifts(apiResult);
                } else {
                    success = true;
                }
            } else if (action == ACTION_GETROOMINFO) {
                OneSevenLiveLoginData loginData;
                configManager->getLoginData(loginData);

                OneSevenLiveRoomInfo roomInfo;
                success = apiWrapper->GetRoomInfo(loginData.userInfo.roomID, roomInfo);
                if (success) {
                    OneSevenLiveRoomInfoToJson(roomInfo, apiResult);
                }
            } else {
                // Unsupported action - pre-build error message
                const std::string errorMsg = "Unsupported action: " + action;
                const nlohmann::json errorResponse = {{"success", false}, {"error", errorMsg}};
                const std::string responseStr = errorResponse.dump();
                res.set_content(responseStr, "application/json");
                return;
            }

            if (!success) {
                // API call failed - pre-convert error message
                const std::string errorMsg = apiWrapper->getLastErrorMessage().toStdString();
                const nlohmann::json errorResponse = {{"success", false}, {"error", errorMsg}};
                const std::string responseStr = errorResponse.dump();
                res.set_content(responseStr, "application/json");
                return;
            }

            // Build response - cache dump result
            const nlohmann::json response = apiResult;
            const std::string responseStr = response.dump();
            res.set_content(responseStr, "application/json");
        } catch (const std::exception& e) {
            // Handle exceptions - pre-build error message
            const std::string errorMsg = std::string("Exception: ") + e.what();
            const nlohmann::json errorResponse = {{"success", false}, {"error", errorMsg}};
            const std::string responseStr = errorResponse.dump();
            res.set_content(responseStr, "application/json");
        }
    });

    // Start server in new thread to avoid blocking main thread
    server_thread_ = std::make_unique<std::thread>([this]() {
        try {
            if (port_ == 0) {
                // Bind to any available port if port_ is 0
                port_ = svr_.bind_to_any_port(host_.c_str());
                if (port_ < 0) {  // bind_to_any_port returns -1 on failure
                    blog(LOG_ERROR, "[17Live HTTP Server] Failed to bind to any port on %s: %s",
                         host_.c_str(), std::strerror(errno));
                    running_ = false;
                    return;
                }
                blog(LOG_INFO, "[17Live HTTP Server] Bound to %s:%d", host_.c_str(), port_);
                if (!svr_.listen_after_bind()) {
                    blog(LOG_ERROR, "[17Live HTTP Server] Failed to listen on %s:%d after bind: %s",
                         host_.c_str(), port_, std::strerror(errno));
                    running_ = false;
                }
            } else {
                // Listen on the specified port
                blog(LOG_INFO, "[17Live HTTP Server] Starting server on %s:%d", host_.c_str(),
                     port_);
                if (!svr_.listen(host_.c_str(), port_)) {
                    blog(LOG_ERROR, "[17Live HTTP Server] Failed to listen on %s:%d: %s",
                         host_.c_str(), port_, std::strerror(errno));
                    running_ = false;  // Ensure correct state
                }
            }
        } catch (const std::exception& e) {
            blog(LOG_ERROR, "[17Live HTTP Server] Exception during server startup: %s", e.what());
            running_ = false;
        } catch (...) {
            blog(LOG_ERROR, "[17Live HTTP Server] Unknown exception during server startup");
            running_ = false;
        }
    });

    // Wait a bit to see if server can start successfully. This is not perfect, but can catch some
    // immediate errors. A better approach would be to use condition variables or futures to wait
    // for server to actually start listening.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // listen failure will print logs within thread, but we assume it will start here
    // is_running() depends on svr_.is_running(), but listen is blocking, so svr_.is_running() may
    // return false before listen succeeds We need a more reliable way to check if server is
    // actually running. For this implementation, we optimistically assume it will run and handle
    // properly in stop.
    running_ = svr_.is_running();  // This may not immediately reflect real state since listen is in
                                   // another thread
    if (!running_) {
        // Try to check if port is occupied etc., but httplib may not directly provide this check
        // The logic here is, if listen fails quickly (e.g. port occupied), svr_.stop() will be
        // called, running_ will be false But if listen is trying, it will block, is_running() may
        // still be false This is a simplified handling, actual projects may need more complex
        // startup confirmation mechanism
        blog(LOG_INFO, "[17Live HTTP Server] Server thread started. Checking status shortly.");
        // Temporarily assume startup success, let stop and destructor handle cleanup
        running_ = true;
    }

    return running_;
}

void OneSevenLiveHttpServer::stop() {
    if (running_) {
        blog(LOG_INFO, "[17Live HTTP Server] Stopping server...");
        svr_.stop();  // Stop server listening
        if (server_thread_ && server_thread_->joinable()) {
            server_thread_->join();  // Wait for server thread to end
        }
        server_thread_.reset();
        running_ = false;
        blog(LOG_INFO, "[17Live HTTP Server] Server stopped.");
    } else {
        // blog(LOG_INFO, "[17Live HTTP Server] Server not running or already stopped.");
    }
}

bool OneSevenLiveHttpServer::is_running() const {
    // svr_.is_running() checks if server is listening.
    // However, if listen fails in another thread, this state may not update immediately.
    // Our running_ member aims to provide a more direct control state.
    return running_ && svr_.is_running();
}

int OneSevenLiveHttpServer::getPort() const {
    if (running_) {
        return port_;
    }
    return -1;  // Or some other indicator that the server is not running or port is not set
}

// Security-related method implementations
bool OneSevenLiveHttpServer::is_safe_path(const std::string& path) const {
    // Check for empty path
    if (path.empty()) {
        return false;
    }

    // Check for path traversal attacks
    if (path.find("..") != std::string::npos) {
        return false;
    }

    // Check for absolute paths
    if (!path.empty() && path.front() == '/' && path.find(base_dir_) != 0) {
        return false;
    }

    // Check for dangerous characters
    const std::vector<std::string> dangerous_patterns = {"\\", "<", ">", "|", ":", "*", "?"};

    for (const auto& pattern : dangerous_patterns) {
        if (path.find(pattern) != std::string::npos) {
            return false;
        }
    }

    return true;
}

bool OneSevenLiveHttpServer::check_rate_limit(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    const auto now = std::chrono::steady_clock::now();
    auto& requests = rate_limit_map_[client_ip];

    // Pre-calculate the cutoff time to avoid repeated calculations in lambda
    const auto cutoff_time = now - std::chrono::seconds(RATE_LIMIT_WINDOW_SECONDS);

    // Clean up expired request records - use cached cutoff time
    requests.erase(std::remove_if(requests.begin(), requests.end(),
                                  [cutoff_time](const std::chrono::steady_clock::time_point& time) {
                                      return time < cutoff_time;
                                  }),
                   requests.end());

    // Check if rate limit is exceeded
    if (requests.size() >= RATE_LIMIT_REQUESTS) {
        blog(LOG_WARNING, "[17Live HTTP Server] Rate limit exceeded for IP: %s", client_ip.c_str());
        return false;
    }

    // Record current request
    requests.push_back(now);
    return true;
}

bool OneSevenLiveHttpServer::validate_request_size(const httplib::Request& req) const {
    if (req.body.size() > MAX_REQUEST_SIZE) {
        blog(LOG_WARNING, "[17Live HTTP Server] Request size too large: %zu bytes",
             req.body.size());
        return false;
    }
    return true;
}

std::string OneSevenLiveHttpServer::generate_csrf_token() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << dis(gen);
    }

    return ss.str();
}

bool OneSevenLiveHttpServer::validate_csrf_token(const std::string& token) const {
    return !csrf_token_.empty() && token == csrf_token_;
}

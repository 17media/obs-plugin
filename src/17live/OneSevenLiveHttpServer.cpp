#include "OneSevenLiveHttpServer.hpp"

#include <obs-module.h>

#include <fstream>
#include <iostream>
#include <vector>

#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveCoreManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "json11.hpp"
#include "plugin-support.h"

// Helper function to get module data path
std::string get_obs_module_data_path_str() {
    const char *path = obs_get_module_data_path(obs_current_module());
    if (path) {
        return std::string(path);
    }
    return "";  // Or throw exception, or return a default known path
}

std::string OneSevenLiveHttpServer::get_file_extension(const std::string &file_path) const {
    size_t dot_pos = file_path.rfind('.');
    if (dot_pos != std::string::npos) {
        return file_path.substr(dot_pos + 1);
    }
    return "";
}

std::string OneSevenLiveHttpServer::get_mime_type(const std::string &file_path) const {
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

OneSevenLiveHttpServer::OneSevenLiveHttpServer(const std::string &host, int port,
                                               const std::string &base_dir_relative_to_module_data)
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
}

OneSevenLiveHttpServer::~OneSevenLiveHttpServer() {
    stop();
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

    // Provide index.html by default
    svr_.Get("/", [this](const httplib::Request &req, httplib::Response &res) {
        obs_log(LOG_INFO, "[17Live HTTP Server] Handling request for %s", req.path.c_str());
        std::filesystem::path path_obj = std::filesystem::path(base_dir_) / "index.html";
        std::string path_str = path_obj.string();

        if (!std::filesystem::exists(path_obj)) {
            path_obj = std::filesystem::path(base_dir_) / "index.html";
            path_str = path_obj.string();
        }

        std::ifstream ifs(path_str, std::ios::in | std::ios::binary);
        if (ifs) {
            std::string content((std::istreambuf_iterator<char>(ifs)),
                                (std::istreambuf_iterator<char>()));
            res.set_content(content, get_mime_type(path_str).c_str());
        } else {
            blog(LOG_WARNING, "[17Live HTTP Server] File not found for /: %s", path_str.c_str());
            res.status = 404;
            res.set_content("File not found: " + path_str, "text/plain");
        }
    });

    svr_.Get("/ping", [](const httplib::Request & /*req*/, httplib::Response &res) {
        res.set_content("PONG", "text/plain");
    });

    // Add /lapi route to handle API requests
    svr_.Post("/lapi", [](const httplib::Request &req, httplib::Response &res) {
        // obs_log(LOG_INFO, "[17Live HTTP Server] Handling API request to /lapi");

        // Set response headers
        res.set_header("Content-Type", "application/json");

        // Get OneSevenLiveCoreManager instance
        auto &coreManager = OneSevenLiveCoreManager::getInstance();

        // Parse JSON data from request body
        std::string error;
        json11::Json requestJson = json11::Json::parse(req.body, error);

        if (!error.empty()) {
            // JSON parsing error
            json11::Json errorResponse =
                json11::Json::object{{"success", json11::Json(false)},
                                     {"error", json11::Json("Invalid JSON: " + error)}};
            res.set_content(errorResponse.dump(), "application/json");
            return;
        }

        // Get requested action
        std::string action = requestJson["action"].string_value();

        if (action.empty()) {
            // Missing action parameter
            json11::Json errorResponse =
                json11::Json::object{{"success", false}, {"error", "Missing 'action' parameter"}};
            res.set_content(errorResponse.dump(), "application/json");
            return;
        }

        // Call API and return result
        json11::Json apiResult;
        bool success = false;

        try {
            // Get apiWrapper instance
            auto apiWrapper = coreManager.getApiWrapper();
            auto configManager = coreManager.getConfigManager();

            if (!apiWrapper) {
                // API Wrapper not initialized
                json11::Json errorResponse =
                    json11::Json::object{{"success", false}, {"error", "API not initialized"}};
                res.set_content(errorResponse.dump(), "application/json");
                return;
            }

            // Call corresponding API function based on action
            if (action == ACTION_GETABLYTOKEN) {
                std::string roomID;
                configManager->getConfigValue("RoomID", roomID);
                success = apiWrapper->GetAblyToken(roomID, apiResult);
            } else if (action == ACTION_GETGIFTS) {
                std::string language;
                configManager->getConfigValue("Region", language);
                success = apiWrapper->GetGifts(language, apiResult);
                configManager->saveGifts(apiResult);
            } else if (action == ACTION_GETROOMINFO) {
                OneSevenLiveLoginData loginData;
                configManager->getLoginData(loginData);

                OneSevenLiveRoomInfo roomInfo;
                success = apiWrapper->GetRoomInfo(loginData.userInfo.roomID, roomInfo);
                if (success) {
                    OneSevenLiveRoomInfoToJson(roomInfo, apiResult);
                }
            } else {
                // Unsupported action
                json11::Json errorResponse = json11::Json::object{
                    {"success", false}, {"error", "Unsupported action: " + action}};
                res.set_content(errorResponse.dump(), "application/json");
                return;
            }

            if (!success) {
                // API call failed
                json11::Json errorResponse = json11::Json::object{
                    {"success", json11::Json(false)},
                    {"error", json11::Json(apiWrapper->getLastErrorMessage().toStdString())}};
                res.set_content(errorResponse.dump(), "application/json");
                return;
            }

            // Build response
            json11::Json response = apiResult;

            res.set_content(response.dump(), "application/json");
        } catch (const std::exception &e) {
            // Handle exceptions
            json11::Json errorResponse = json11::Json::object{
                {"success", false}, {"error", std::string("Exception: ") + e.what()}};
            res.set_content(errorResponse.dump(), "application/json");
        }
    });

    // Start server in new thread to avoid blocking main thread
    server_thread_ = std::make_unique<std::thread>([this]() {
        if (port_ == 0) {
            // Bind to any available port if port_ is 0
            port_ = svr_.bind_to_any_port(host_.c_str());
            if (port_ < 0) {  // bind_to_any_port returns -1 on failure
                blog(LOG_ERROR, "[17Live HTTP Server] Failed to bind to any port on %s",
                     host_.c_str());
                running_ = false;
                return;
            }
            blog(LOG_INFO, "[17Live HTTP Server] Bound to %s:%d", host_.c_str(), port_);
            if (!svr_.listen_after_bind()) {
                blog(LOG_ERROR, "[17Live HTTP Server] Failed to listen on %s:%d after bind",
                     host_.c_str(), port_);
                running_ = false;
            }
        } else {
            // Listen on the specified port
            blog(LOG_INFO, "[17Live HTTP Server] Starting server on %s:%d", host_.c_str(), port_);
            if (!svr_.listen(host_.c_str(), port_)) {
                blog(LOG_ERROR, "[17Live HTTP Server] Failed to listen on %s:%d", host_.c_str(),
                     port_);
                running_ = false;  // Ensure correct state
            }
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

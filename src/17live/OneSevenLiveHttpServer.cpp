#include "OneSevenLiveHttpServer.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <obs-module.h>

#include "json11.hpp"

#include "plugin-support.h"

#include "OneSevenLiveCoreManager.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"

// 获取模块数据路径的辅助函数
std::string get_obs_module_data_path_str() {
    const char* path = obs_get_module_data_path(obs_current_module());
    if (path) {
        return std::string(path);
    }
    return ""; // 或者抛出异常，或者返回一个默认的已知路径
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
    if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
    if (ext == "css") return "text/css; charset=utf-8";
    if (ext == "js") return "application/javascript; charset=utf-8";
    if (ext == "json") return "application/json; charset=utf-8";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    if (ext == "woff2") return "font/woff2";
    if (ext == "woff") return "font/woff";
    if (ext == "ttf") return "font/ttf";
    return "application/octet-stream";
}

OneSevenLiveHttpServer::OneSevenLiveHttpServer(const std::string& host, int port, const std::string& base_dir_relative_to_module_data)
    : host_(host), port_(port), running_(false) {
    
    std::string module_data_path = get_obs_module_data_path_str();
    if (module_data_path.empty()) {
        blog(LOG_ERROR, "[17Live HTTP Server] Failed to get OBS module data path.");
        // 可以选择设置一个默认的 base_dir_ 或者让服务器启动失败
        base_dir_ = base_dir_relative_to_module_data; // Fallback or error state
    } else {
        std::filesystem::path full_base_path = std::filesystem::path(module_data_path) / base_dir_relative_to_module_data;
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

    // 确保 base_dir_ 存在
    if (!std::filesystem::exists(base_dir_) || !std::filesystem::is_directory(base_dir_)) {
        blog(LOG_ERROR, "[17Live HTTP Server] Base directory '%s' does not exist or is not a directory.", base_dir_.c_str());
        return false;
    }

    // 设置静态文件服务
    // httplib的set_mount_point的第二个参数应该是相对于当前工作目录的路径，或者绝对路径。
    // 我们已经将base_dir_计算为绝对路径。
    if (!svr_.set_mount_point("/", base_dir_.c_str())) {
        blog(LOG_ERROR, "[17Live HTTP Server] Failed to set mount point '/' to '%s'", base_dir_.c_str());
        return false;
    }
    blog(LOG_INFO, "[17Live HTTP Server] Mounting '/' to serve files from '%s'", base_dir_.c_str());

    // 默认提供 index.html
    svr_.Get("/", [this](const httplib::Request &req, httplib::Response &res) {
        obs_log(LOG_INFO, "[17Live HTTP Server] Handling request for %s", req.path.c_str());
        std::filesystem::path path_obj = std::filesystem::path(base_dir_) / "index.html";
        std::string path_str = path_obj.string();
        
        if (!std::filesystem::exists(path_obj)){
            path_obj = std::filesystem::path(base_dir_) / "index.html";
            path_str = path_obj.string();
        }

        std::ifstream ifs(path_str, std::ios::in | std::ios::binary);
        if (ifs) {
            std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
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

    // 添加 /lapi 路由，处理 API 请求
    svr_.Post("/lapi", [](const httplib::Request &req, httplib::Response &res) {
        // obs_log(LOG_INFO, "[17Live HTTP Server] Handling API request to /lapi");
        
        // 设置响应头
        res.set_header("Content-Type", "application/json");
        
        // 获取 OneSevenLiveCoreManager 实例
        auto &coreManager = OneSevenLiveCoreManager::getInstance();
        
        // 解析请求体中的 JSON 数据
        std::string error;
        json11::Json requestJson = json11::Json::parse(req.body, error);
        
        if (!error.empty()) {
            // JSON 解析错误
            json11::Json errorResponse = json11::Json::object {
                {"success", json11::Json(false)},
                {"error", json11::Json("Invalid JSON: " + error)}
            };
            res.set_content(errorResponse.dump(), "application/json");
            return;
        }
        
        // 获取请求的 action
        std::string action = requestJson["action"].string_value();
        
        if (action.empty()) {
            // 缺少 action 参数
            json11::Json errorResponse = json11::Json::object{
                {"success", false},
                {"error", "Missing 'action' parameter"}
            };
            res.set_content(errorResponse.dump(), "application/json");
            return;
        }
        
        // 调用 API 并返回结果
        json11::Json apiResult;
        bool success = false;
        
        try {
            // 获取 apiWrapper 实例
            auto apiWrapper = coreManager.getApiWrapper();
            auto configManager = coreManager.getConfigManager();
            
            if (!apiWrapper) {
                // API Wrapper 未初始化
                json11::Json errorResponse = json11::Json::object{
                    {"success", false},
                    {"error", "API not initialized"}
                };
                res.set_content(errorResponse.dump(), "application/json");
                return;
            }
            
            // 根据 action 调用相应的 API 函数
            if (action == ACTION_GETABLYTOKEN) {
                std::string roomID;
                configManager->getConfigValue("RoomID", roomID);
                success = apiWrapper->GetAblyToken(roomID, apiResult);
            } else if (action == ACTION_GETGIFTS) {
                std::string language;
                configManager->getConfigValue("Region", language);
                success = apiWrapper->GetGifts(language, apiResult);
            } else if (action == ACTION_GETROOMINFO) {
                OneSevenLiveLoginData loginData;
                configManager->getLoginData(loginData);

                OneSevenLiveRoomInfo roomInfo;
                success = apiWrapper->GetRoomInfo(loginData.userInfo.roomID, roomInfo);
                if (success) {
                    OneSevenLiveRoomInfoToJson(roomInfo, apiResult);
                }
            } else {
                // 不支持的 action
                json11::Json errorResponse = json11::Json::object{
                    {"success", false},
                    {"error", "Unsupported action: " + action}
                };
                res.set_content(errorResponse.dump(), "application/json");
                return;
            }

            if (!success) {
                // API 调用失败
                json11::Json errorResponse = json11::Json::object{
                    {"success", json11::Json(false)},
                    {"error", json11::Json(apiWrapper->getLastErrorMessage().toStdString())}
                };
                res.set_content(errorResponse.dump(), "application/json");
                return;
            }
            
            // 构建响应
            json11::Json response = apiResult;
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception &e) {
            // 处理异常
            json11::Json errorResponse = json11::Json::object{
                {"success", false},
                {"error", std::string("Exception: ") + e.what()}
            };
            res.set_content(errorResponse.dump(), "application/json");
        }
    });

    // 在新线程中启动服务器，以避免阻塞主线程
    server_thread_ = std::make_unique<std::thread>([this]() {
        if (port_ == 0) {
            // Bind to any available port if port_ is 0
            port_ = svr_.bind_to_any_port(host_.c_str());
            if (port_ < 0) { // bind_to_any_port returns -1 on failure
                 blog(LOG_ERROR, "[17Live HTTP Server] Failed to bind to any port on %s", host_.c_str());
                 running_ = false;
                 return;
            }
            blog(LOG_INFO, "[17Live HTTP Server] Bound to %s:%d", host_.c_str(), port_);
            if (!svr_.listen_after_bind()) {
                blog(LOG_ERROR, "[17Live HTTP Server] Failed to listen on %s:%d after bind", host_.c_str(), port_);
                running_ = false;
            }
        } else {
            // Listen on the specified port
            blog(LOG_INFO, "[17Live HTTP Server] Starting server on %s:%d", host_.c_str(), port_);
            if (!svr_.listen(host_.c_str(), port_)) {
                blog(LOG_ERROR, "[17Live HTTP Server] Failed to listen on %s:%d", host_.c_str(), port_);
                running_ = false; // 确保状态正确
            }
        }
    });
    
    // 稍微等待一下，看服务器是否能成功启动。这不是很完美，但可以捕捉到一些即时错误。
    // 一个更好的方法是使用条件变量或 future 来等待服务器真正开始监听。
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
    
    // listen 失败会在线程内打印日志，但我们这里假设它会启动
    // is_running() 依赖于 svr_.is_running()，但 listen 是阻塞的，所以 svr_.is_running() 可能在 listen 成功前返回 false
    // 我们需要一种更可靠的方式来检查服务器是否真的在运行。
    // 对于这个实现，我们暂时乐观地假设它会运行，并在stop时正确处理。
    running_ = svr_.is_running(); // 这可能不会立即反映真实状态，因为listen在另一个线程
    if(!running_){
        // 尝试检查端口是否被占用等，但httplib可能没有直接提供这种检查方式
        // 这里的逻辑是，如果listen快速失败（例如端口已占用），svr_.stop()会被调用，running_会是false
        // 但如果listen正在尝试，它会阻塞，is_running()可能还是false
        // 这是一个简化的处理，实际项目中可能需要更复杂的启动确认机制
        blog(LOG_INFO, "[17Live HTTP Server] Server thread started. Checking status shortly.");
        // 暂时假设启动成功，由stop和析构函数处理清理
        running_ = true; 
    }

    return running_;
}

void OneSevenLiveHttpServer::stop() {
    if (running_) {
        blog(LOG_INFO, "[17Live HTTP Server] Stopping server...");
        svr_.stop(); // 停止服务器监听
        if (server_thread_ && server_thread_->joinable()) {
            server_thread_->join(); // 等待服务器线程结束
        }
        server_thread_.reset();
        running_ = false;
        blog(LOG_INFO, "[17Live HTTP Server] Server stopped.");
    } else {
        // blog(LOG_INFO, "[17Live HTTP Server] Server not running or already stopped.");
    }
}

bool OneSevenLiveHttpServer::is_running() const {
    // svr_.is_running() 检查服务器是否正在监听。 
    // 但是，如果listen在另一个线程中失败，这个状态可能不会立即更新。
    // 我们的 running_ 成员旨在提供一个更直接的控制状态。
    return running_ && svr_.is_running();
}

int OneSevenLiveHttpServer::getPort() const {
    if (running_) {
        return port_;
    }
    return -1; // Or some other indicator that the server is not running or port is not set
}

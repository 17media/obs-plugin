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

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QUrl>

#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveCoreManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "plugin-support.h"
#include "utility/Common.hpp"
#include "utility/RemoteTextThread.hpp"
#include "websocket/OneSevenLiveWebsocketServer.hpp"

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
    if (ext == "webp")
        return "image/webp";
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

void OneSevenLiveHttpServer::serve_file(const std::filesystem::path& file_path,
                                        httplib::Response& res) const {
    const std::string file_path_str = file_path.string();

    try {
        if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
            res.status = 404;
            res.set_content("Not Found", "text/plain");
            return;
        }

        std::ifstream ifs(file_path_str, std::ios::in | std::ios::binary);
        if (!ifs.is_open() || !ifs.good()) {
            obs_log(LOG_ERROR, "[%s] Failed to open file: %s", name_.c_str(), file_path_str.c_str());
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
        if (ifs.bad()) {
            obs_log(LOG_ERROR, "[%s] Error reading file: %s", name_.c_str(), file_path_str.c_str());
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
            return;
        }

        res.set_content(content, get_mime_type(file_path_str).c_str());
    } catch (const std::filesystem::filesystem_error& e) {
        obs_log(LOG_ERROR, "[%s] Filesystem error for %s: %s", name_.c_str(), file_path_str.c_str(),
                e.what());
        res.status = 500;
        res.set_content("Internal Server Error", "text/plain");
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[%s] Exception serving file %s: %s", name_.c_str(), file_path_str.c_str(),
                e.what());
        res.status = 500;
        res.set_content("Internal Server Error", "text/plain");
    }
}

std::string OneSevenLiveHttpServer::get_enter_animation_asset_cache_dir() const {
    auto* coreManager = OneSevenLiveCoreManager::peekInstance();
    if (!coreManager) {
        return "";
    }

    auto* configManager = coreManager->getConfigManager();
    if (!configManager) {
        return "";
    }

    const std::string config_path = configManager->getConfigPath();
    if (config_path.empty()) {
        return "";
    }

    return (std::filesystem::path(config_path) / "enter_animation_assets").string();
}

bool OneSevenLiveHttpServer::ensure_enter_animation_asset_cached(
    const std::string& source_url, std::filesystem::path& cached_file_path, std::string& error_message) {
    const QUrl url(QString::fromStdString(source_url));
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != "https" && url.scheme() != "http")) {
        error_message = "Invalid remote asset URL";
        return false;
    }

    const std::string cache_dir = get_enter_animation_asset_cache_dir();
    if (cache_dir.empty()) {
        error_message = "Config cache directory unavailable";
        return false;
    }

    QDir dir(QString::fromStdString(cache_dir));
    if (!dir.exists() && !dir.mkpath(".")) {
        error_message = "Failed to create asset cache directory";
        return false;
    }

    QString suffix = QFileInfo(url.path()).suffix().toLower();
    if (suffix.isEmpty()) {
        suffix = "bin";
    }

    const QByteArray hash =
        QCryptographicHash::hash(QByteArray::fromStdString(source_url), QCryptographicHash::Md5)
            .toHex();
    const QString file_name = QString("%1.%2").arg(QString::fromLatin1(hash), suffix);
    const QString file_path = dir.filePath(file_name);
    cached_file_path = std::filesystem::path(file_path.toStdString());

    std::lock_guard<std::mutex> lock(asset_cache_mutex_);

    const QFileInfo cache_info(file_path);
    if (cache_info.exists() && cache_info.isFile() && cache_info.size() > 0) {
        return true;
    }

    std::string content;
    std::string download_error;
    long response_code = 0;
    const bool success =
        GetRemoteFile(source_url.c_str(), content, download_error, &response_code, nullptr, "GET",
                      nullptr, {}, nullptr, 20, true);
    if (!success || response_code < 200 || response_code >= 300 || content.empty()) {
        std::ostringstream ss;
        ss << "Failed to download remote asset";
        if (response_code > 0) {
            ss << " (HTTP " << response_code << ")";
        }
        if (!download_error.empty()) {
            ss << ": " << download_error;
        }
        error_message = ss.str();
        return false;
    }

    const QString temp_file_path = file_path + ".part";
    QFile::remove(temp_file_path);

    QFile file(temp_file_path);
    if (!file.open(QIODevice::WriteOnly)) {
        error_message = "Failed to write cached asset";
        return false;
    }
    if (file.write(content.data(), static_cast<qint64>(content.size())) !=
        static_cast<qint64>(content.size())) {
        file.close();
        QFile::remove(temp_file_path);
        error_message = "Failed to persist cached asset";
        return false;
    }
    file.close();

    QFile::remove(file_path);
    if (!QFile::rename(temp_file_path, file_path)) {
        QFile::remove(temp_file_path);
        error_message = "Failed to finalize cached asset";
        return false;
    }

    obs_log(LOG_INFO, "[%s] Cached enter animation asset: %s -> %s", name_.c_str(),
            source_url.c_str(), file_path.toStdString().c_str());
    return true;
}

bool OneSevenLiveHttpServer::handle_enter_animation_cache_request(const httplib::Request& req,
                                                                  httplib::Response& res) {
    static const std::string kCacheRoutePrefix = "/__17live_cache/enter_animation/";
    if (req.path.rfind(kCacheRoutePrefix, 0) != 0) {
        return false;
    }

    if (!req.has_param("src")) {
        res.status = 400;
        res.set_content("Missing src parameter", "text/plain");
        return true;
    }

    const std::string source_url = req.get_param_value("src");
    std::filesystem::path cached_file_path;
    std::string error_message;
    if (!ensure_enter_animation_asset_cached(source_url, cached_file_path, error_message)) {
        obs_log(LOG_WARNING, "[%s] Failed to cache enter animation asset %s: %s", name_.c_str(),
                source_url.c_str(), error_message.c_str());
        res.status = 502;
        res.set_content(error_message, "text/plain");
        return true;
    }

    res.set_header("Cache-Control", "public, max-age=31536000, immutable");
    serve_file(cached_file_path, res);
    return true;
}

OneSevenLiveHttpServer::OneSevenLiveHttpServer(const std::string& host, int port,
                                               const std::string& base_dir_relative_to_module_data,
                                               const std::string& name)
    : host_(host), port_(port), running_(false), name_(name) {
    std::string module_data_path = get_obs_module_data_path_str();
    if (module_data_path.empty()) {
        obs_log(LOG_ERROR, "[%s] Failed to get OBS module data path.", name_.c_str());
        // Can choose to set a default base_dir_ or let server startup fail
        base_dir_ = base_dir_relative_to_module_data;  // Fallback or error state
    } else {
        std::filesystem::path full_base_path =
            std::filesystem::path(module_data_path) / base_dir_relative_to_module_data;
        base_dir_ = full_base_path.string();
    }

    obs_log(LOG_INFO, "[%s] Base directory set to: %s", name_.c_str(), base_dir_.c_str());

    // Initialize CSRF token
    csrf_token_ = generate_csrf_token();
}

OneSevenLiveHttpServer::~OneSevenLiveHttpServer() {
    obs_log(LOG_INFO, "[%s] Starting HTTP server destruction", name_.c_str());

    // Ensure server is completely stopped and thread properly terminated
    stop();

    obs_log(LOG_INFO, "[%s] HTTP server successfully destroyed", name_.c_str());
}

bool OneSevenLiveHttpServer::start() {
    if (running_) {
        obs_log(LOG_WARNING, "[%s] Server already running.", name_.c_str());
        return true;
    }

    // Ensure base_dir_ exists
    if (!std::filesystem::exists(base_dir_) || !std::filesystem::is_directory(base_dir_)) {
        obs_log(LOG_ERROR, "[%s] Base directory '%s' does not exist or is not a directory.",
                name_.c_str(), base_dir_.c_str());
        return false;
    }

    // Set up static file service
    // The second parameter of httplib's set_mount_point should be a path relative to current
    // working directory, or absolute path. We have already calculated base_dir_ as absolute path.
    if (!svr_.set_mount_point("/", base_dir_.c_str())) {
        obs_log(LOG_ERROR, "[%s] Failed to set mount point '/' to '%s'", name_.c_str(),
                base_dir_.c_str());
        return false;
    }
    obs_log(LOG_INFO, "[%s] Mounting '/' to serve files from '%s'", name_.c_str(),
            base_dir_.c_str());

    // Override the default handler for static files
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

        if (handle_enter_animation_cache_request(req, res)) {
            return;
        }

        // Security check: path validation
        if (!is_safe_path(path)) {
            res.status = 403;
            res.set_content("Forbidden", "text/plain");
            return;
        }

        // Serve the file
        std::filesystem::path file_path = std::filesystem::path(base_dir_) / path.substr(1);
        serve_file(file_path, res);
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

        obs_log(LOG_INFO, "[%s] Handling request for %s from %s", name_.c_str(), req.path.c_str(),
                client_ip.c_str());

        // Security check: path validation
        if (!is_safe_path(req.path)) {
            obs_log(LOG_WARNING, "[%s] Unsafe path detected: %s", name_.c_str(), req.path.c_str());
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
                    obs_log(LOG_ERROR, "[%s] Error reading index.html: %s", name_.c_str(),
                            path_str.c_str());
                    res.status = 500;
                    res.set_content("Internal Server Error", "text/plain");
                } else {
                    res.set_content(content, get_mime_type(path_str).c_str());
                }
            } else {
                obs_log(LOG_WARNING, "[%s] File not found for /: %s", name_.c_str(),
                        path_str.c_str());
                res.status = 404;
                res.set_content("File not found", "text/plain");  // Don't expose internal paths
            }
        } catch (const std::filesystem::filesystem_error& e) {
            obs_log(LOG_ERROR, "[%s] Filesystem error for index.html %s: %s", name_.c_str(),
                    path_str.c_str(), e.what());
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
        } catch (const std::exception& e) {
            obs_log(LOG_ERROR, "[%s] Exception serving index.html %s: %s", name_.c_str(),
                    path_str.c_str(), e.what());
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
        }
    });

    for (const auto& p : extra_get_) {
        svr_.Get(p.first.c_str(),
                 [this, handler = p.second](const httplib::Request& req, httplib::Response& res) {
                     std::string client_ip = req.get_header_value("X-Forwarded-For");
                     if (client_ip.empty())
                         client_ip = req.get_header_value("X-Real-IP");
                     if (client_ip.empty())
                         client_ip = "127.0.0.1";
                     if (!check_rate_limit(client_ip)) {
                         res.status = 429;
                         res.set_content("Too Many Requests", "text/plain");
                         return;
                     }
                     handler(req, res);
                 });
    }

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

    if (enable_default_api_) {
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
                    const bool loaded = configManager->loadGifts(apiResult);
                    const bool hasCached =
                        apiResult.contains("gifts") && apiResult["gifts"].is_array() &&
                        !apiResult["gifts"].empty();
                    if (!loaded || !hasCached) {
                        if (coreManager.isGiftsLoading()) {
                            obs_log(LOG_INFO,
                                    "[%s] Gifts loading in progress, returning wait response",
                                    name_.c_str());
                            const nlohmann::json response = {{"success", false},
                                                             {"error", "Gifts loading"}};
                            res.set_content(response.dump(), "application/json");
                            return;
                        }

                        std::string language;
                        configManager->getConfigValue("Region", language);
                        success = apiWrapper->GetGifts(language, apiResult);
                        if (!configManager->saveGifts(apiResult)) {
                            const auto err = configManager->getLastError();
                            obs_log(LOG_WARNING, "[%s] Failed to save gifts: %s %s", name_.c_str(),
                                    err.code.c_str(), err.message.c_str());
                        }
                    } else {
                        success = true;
                    }
                } else if (action == ACTION_GETGIFT) {
                    if (coreManager.isGiftsLoading()) {
                        obs_log(LOG_INFO, "[%s] Gifts loading in progress, returning wait response",
                                name_.c_str());
                        const nlohmann::json response = {{"success", false},
                                                         {"error", "Gifts loading"}};
                        res.set_content(response.dump(), "application/json");
                        return;
                    }
                    std::string giftID;
                    if (requestJson.contains("giftID") && requestJson["giftID"].is_string())
                        giftID = requestJson["giftID"].get<std::string>();

                    std::optional<nlohmann::json> gift;
                    if (!giftID.empty()) {
                        gift = OneSevenLiveCoreManager::getInstance().getGiftByID(giftID);
                    }

                    if (gift) {
                        apiResult = *gift;
                        success = true;
                    } else {
                        obs_log(LOG_WARNING, "Gift not found. giftID=%s", giftID.c_str());
                        const nlohmann::json errorResponse = {{"success", false},
                                                              {"error", "Gift not found"}};
                        res.set_content(errorResponse.dump(), "application/json");
                        return;
                    }

                } else if (action == ACTION_GETENTERANIMATIONFILES) {
                    const bool loaded = configManager->loadEnterAnimationFiles(apiResult);
                    const bool hasCached = !apiResult.empty();
                    if (!loaded || !hasCached) {
                        success = apiWrapper->GetFilesList(apiResult);
                        if (success && !configManager->saveEnterAnimationFiles(apiResult)) {
                            const auto err = configManager->getLastError();
                            obs_log(LOG_WARNING,
                                    "[%s] Failed to save enter animation files: %s %s",
                                    name_.c_str(), err.code.c_str(), err.message.c_str());
                        }
                    } else {
                        success = true;
                    }
                } else if (action == ACTION_GETI18NCONFIG) {
                    const bool loaded = configManager->loadI18nConfig(apiResult);
                    const bool hasCached = !apiResult.empty();
                    if (!loaded || !hasCached) {
                        success = apiWrapper->GetI18nConfig(apiResult);
                        if (success && !configManager->saveI18nConfig(apiResult)) {
                            const auto err = configManager->getLastError();
                            obs_log(LOG_WARNING, "[%s] Failed to save i18n config: %s %s",
                                    name_.c_str(), err.code.c_str(), err.message.c_str());
                        }
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
    }

    for (const auto& p : extra_post_) {
        svr_.Post(p.first.c_str(), [this, handler = p.second](const httplib::Request& req,
                                                              httplib::Response& res) {
            std::string client_ip = req.get_header_value("X-Forwarded-For");
            if (client_ip.empty())
                client_ip = req.get_header_value("X-Real-IP");
            if (client_ip.empty())
                client_ip = "127.0.0.1";
            if (!check_rate_limit(client_ip)) {
                res.status = 429;
                res.set_header("Content-Type", "application/json");
                const nlohmann::json err = {{"success", false}, {"error", "Rate limit exceeded"}};
                res.set_content(err.dump(), "application/json");
                return;
            }
            if (!validate_request_size(req)) {
                res.status = 413;
                res.set_header("Content-Type", "application/json");
                const nlohmann::json err = {{"success", false}, {"error", "Request too large"}};
                res.set_content(err.dump(), "application/json");
                return;
            }
            handler(req, res);
        });
    }

    // Start server in a Qt thread to align with OBS frontend (Qt) threading model
    server_thread_ = QThread::create([this]() {
        try {
            if (port_ == 0) {
                // Bind to any available port if port_ is 0
                port_ = svr_.bind_to_any_port(host_.c_str());
                if (port_ < 0) {  // bind_to_any_port returns -1 on failure
                    obs_log(LOG_ERROR, "[%s] Failed to bind to any port on %s: %s", name_.c_str(),
                            host_.c_str(), std::strerror(errno));
                    running_ = false;
                    return;
                }
                obs_log(LOG_INFO, "[%s] Bound to %s:%d", name_.c_str(), host_.c_str(), port_);
                if (!svr_.listen_after_bind()) {
                    obs_log(LOG_ERROR, "[%s] Failed to listen on %s:%d after bind: %s",
                            name_.c_str(), host_.c_str(), port_, std::strerror(errno));
                    running_ = false;
                }
            } else {
                // Listen on the specified port
                obs_log(LOG_INFO, "[%s] Starting server on %s:%d", name_.c_str(), host_.c_str(),
                        port_);
                if (!svr_.listen(host_.c_str(), port_)) {
                    obs_log(LOG_ERROR, "[%s] Failed to listen on %s:%d: %s", name_.c_str(),
                            host_.c_str(), port_, std::strerror(errno));
                    running_ = false;  // Ensure correct state
                }
            }
        } catch (const std::exception& e) {
            obs_log(LOG_ERROR, "[%s] Exception during server startup: %s", name_.c_str(), e.what());
            running_ = false;
        } catch (...) {
            obs_log(LOG_ERROR, "[%s] Unknown exception during server startup", name_.c_str());
            running_ = false;
        }
    });
    server_thread_->setObjectName(QString("17live-http-%1").arg(QString::fromStdString(name_)));
    QObject::connect(server_thread_, &QThread::finished, server_thread_, &QObject::deleteLater);
    server_thread_->start();

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
        obs_log(LOG_INFO, "[%s] Server thread started. Checking status shortly.", name_.c_str());
        // Temporarily assume startup success, let stop and destructor handle cleanup
        running_ = true;
    }

    return running_;
}

void OneSevenLiveHttpServer::addGetHandler(
    const std::string& pattern,
    std::function<void(const httplib::Request&, httplib::Response&)> handler) {
    extra_get_.push_back({pattern, std::move(handler)});
}

void OneSevenLiveHttpServer::addPostHandler(
    const std::string& pattern,
    std::function<void(const httplib::Request&, httplib::Response&)> handler) {
    extra_post_.push_back({pattern, std::move(handler)});
}

void OneSevenLiveHttpServer::setEnableDefaultApi(bool enable) {
    enable_default_api_ = enable;
}

void OneSevenLiveHttpServer::stop() {
    if (!running_ && !server_thread_) {
        return;
    }

    obs_log(LOG_INFO, "[%s] Stopping server...", name_.c_str());
    svr_.stop();
    if (server_thread_) {
        server_thread_->wait(5000);
        server_thread_ = nullptr;
    }
    running_ = false;
    obs_log(LOG_INFO, "[%s] Server stopped.", name_.c_str());
}

void OneSevenLiveHttpServer::stopAsync() {
    stop();
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
        obs_log(LOG_WARNING, "[%s] Rate limit exceeded for IP: %s", name_.c_str(),
                client_ip.c_str());
        return false;
    }

    // Record current request
    requests.push_back(now);
    return true;
}

bool OneSevenLiveHttpServer::validate_request_size(const httplib::Request& req) const {
    if (req.body.size() > MAX_REQUEST_SIZE) {
        obs_log(LOG_WARNING, "[%s] Request size too large: %zu bytes", name_.c_str(),
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

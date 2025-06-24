#ifndef ONESEVENLIVEHTTPSERVER_HPP
#define ONESEVENLIVEHTTPSERVER_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <thread>

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

    httplib::Server svr_;
    std::string host_;
    int port_ = 0;  // Default to 0, meaning find an available port
    std::string base_dir_;
    std::unique_ptr<std::thread> server_thread_;
    bool running_ = false;
};

#endif  // ONESEVENLIVEHTTPSERVER_HPP

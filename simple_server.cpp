#define CPPHTTPLIB_OPENSSL_SUPPORT // 如果你需要HTTPS，请取消注释这一行，并确保链接OpenSSL库
#include "deps/cpp-httplib/httplib.h"
#include <fstream>
#include <iostream>
#include <string>

// 获取文件扩展名
std::string get_file_extension(const std::string& file_path) {
    size_t dot_pos = file_path.rfind('.');
    if (dot_pos != std::string::npos) {
        return file_path.substr(dot_pos + 1);
    }
    return "";
}

// 根据文件扩展名获取MIME类型
std::string get_mime_type(const std::string& file_path) {
    std::string ext = get_file_extension(file_path);
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    return "application/octet-stream"; // 默认MIME类型
}

int main() {
    httplib::Server svr;

    // 设置静态文件服务的根目录
    // 注意：这个路径是相对于你编译和运行simple_server可执行文件的位置而言的。
    // 为了简单起见，这里我们假设可执行文件在项目根目录，data/html也在项目根目录下。
    // 如果你的可执行文件在build目录下，你可能需要调整这个路径，例如 "../data/html"
    const char *mount_point = "/";
    const char *base_dir = "./data/html";

    // 尝试挂载静态文件目录
    if (!svr.set_mount_point(mount_point, base_dir)) {
        std::cerr << "Error: Could not set mount point for " << base_dir << std::endl;
        std::cerr << "Please ensure the directory exists and the program has permissions to access it." << std::endl;
        std::cerr << "Current working directory: " << std::filesystem::current_path() << std::endl;
        return 1;
    }
    std::cout << "Serving files from: " << std::filesystem::absolute(base_dir) << " at " << mount_point << std::endl;

    // 你也可以为特定路径添加处理器，如果set_mount_point不能满足所有需求
    // 例如，处理 / 或 /index.html 请求，如果set_mount_point没有自动处理
    svr.Get("/", [base_dir](const httplib::Request &req, httplib::Response &res) {
        std::string path = std::string(base_dir) + "/chat/index.html"; // 默认服务 index.html
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        if (ifs) {
            std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
            res.set_content(content, get_mime_type(path).c_str());
        } else {
            res.status = 404;
            res.set_content("File not found: " + path, "text/plain");
        }
    });

    svr.Get("/ping", [](const httplib::Request & /*req*/, httplib::Response &res) {
        res.set_content("PONG", "text/plain");
    });

    std::cout << "Server listening on port 3000..." << std::endl;
    svr.listen("localhost", 3000);

    return 0;
}
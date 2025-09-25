/******************************************************************************
    Copyright (C) 2024 by 17Live

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "NetworkDiagnostics.hpp"

#include <curl/curl.h>
#include <obs-module.h>

#include <chrono>
#include <regex>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "plugin-support.h"

using namespace std;

// Helper function to convert gai_strerror result to std::string across platforms
static string getGaiErrorString(int error_code) {
#ifdef _WIN32
    // On Windows, gai_strerror returns WCHAR*, need to convert to std::string
    WCHAR* wide_str = gai_strerror(error_code);
    if (wide_str == nullptr) {
        return "Unknown error";
    }

    // Convert WCHAR* to std::string
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        return "Error converting error message";
    }

    string result(size_needed - 1, '\0');  // -1 to exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, &result[0], size_needed, nullptr, nullptr);
    return result;
#else
    // On Unix/Linux, gai_strerror returns const char*
    const char* error_str = gai_strerror(error_code);
    return error_str ? string(error_str) : "Unknown error";
#endif
}

double NetworkDiagnostics::getCurrentTimeMs() {
    auto now = chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return chrono::duration_cast<chrono::microseconds>(duration).count() / 1000.0;
}

bool NetworkDiagnostics::parseUrl(const string& url, string& hostname, int& port) {
    // Regular expression to parse URL: protocol://hostname:port/path
    regex url_regex(R"(^(https?):\/\/([^:\/\s]+)(?::(\d+))?(?:\/.*)?$)");
    smatch matches;

    if (!regex_match(url, matches, url_regex)) {
        return false;
    }

    string protocol = matches[1].str();
    hostname = matches[2].str();

    if (matches[3].matched) {
        port = stoi(matches[3].str());
    } else {
        port = (protocol == "https") ? 443 : 80;
    }

    return true;
}

NetworkDiagnosticResult NetworkDiagnostics::testDnsResolution(const string& hostname,
                                                              int timeout_seconds) {
    NetworkDiagnosticResult result;
    double start_time = getCurrentTimeMs();

    obs_log(LOG_INFO, "[Network Diagnostics] Testing DNS resolution for: %s (timeout: %ds)",
            hostname.c_str(), timeout_seconds);

    struct addrinfo* res = nullptr;

    try {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;  // Allow IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM;

        int status = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
        result.dns_resolution_time_ms = getCurrentTimeMs() - start_time;

        // Check if DNS resolution took longer than expected timeout
        if (result.dns_resolution_time_ms > timeout_seconds * 1000.0) {
            obs_log(
                LOG_WARNING,
                "[Network Diagnostics] DNS resolution for %s took %.2fms, longer than timeout %ds",
                hostname.c_str(), result.dns_resolution_time_ms, timeout_seconds);
        }

        if (status != 0) {
            result.dns_resolution_success = false;
            result.error_message = "DNS resolution failed: " + getGaiErrorString(status);
            obs_log(LOG_ERROR, "[Network Diagnostics] DNS resolution failed for %s: %s (%.2fms)",
                    hostname.c_str(), getGaiErrorString(status).c_str(),
                    result.dns_resolution_time_ms);
            return result;
        }

        result.dns_resolution_success = true;

        // Collect all resolved IP addresses
        for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
            char ip_str[INET6_ADDRSTRLEN];
            void* addr;

            if (p->ai_family == AF_INET) {
                struct sockaddr_in* ipv4 = (struct sockaddr_in*) p->ai_addr;
                addr = &(ipv4->sin_addr);
            } else if (p->ai_family == AF_INET6) {
                struct sockaddr_in6* ipv6 = (struct sockaddr_in6*) p->ai_addr;
                addr = &(ipv6->sin6_addr);
            } else {
                continue;
            }

            inet_ntop(p->ai_family, addr, ip_str, INET6_ADDRSTRLEN);
            result.resolved_ips.push_back(string(ip_str));
        }

        stringstream ss;
        ss << "DNS resolution successful (";
        for (size_t i = 0; i < result.resolved_ips.size(); ++i) {
            if (i > 0)
                ss << ", ";
            ss << result.resolved_ips[i];
        }
        ss << ")";
        result.detailed_info = ss.str();

        obs_log(LOG_INFO, "[Network Diagnostics] DNS resolution successful for %s: %s (%.2fms)",
                hostname.c_str(), result.detailed_info.c_str(), result.dns_resolution_time_ms);
    } catch (const std::exception& e) {
        result.dns_resolution_success = false;
        result.error_message = "DNS resolution test exception: " + string(e.what());
        result.dns_resolution_time_ms = getCurrentTimeMs() - start_time;
        obs_log(LOG_ERROR,
                "[Network Diagnostics] DNS resolution test exception for %s: %s (%.2fms)",
                hostname.c_str(), e.what(), result.dns_resolution_time_ms);
    } catch (...) {
        result.dns_resolution_success = false;
        result.error_message = "DNS resolution test unknown exception";
        result.dns_resolution_time_ms = getCurrentTimeMs() - start_time;
        obs_log(LOG_ERROR,
                "[Network Diagnostics] DNS resolution test unknown exception for %s (%.2fms)",
                hostname.c_str(), result.dns_resolution_time_ms);
    }

    if (res) {
        freeaddrinfo(res);
    }

    return result;
}

NetworkDiagnosticResult NetworkDiagnostics::testTcpConnection(const string& hostname, int port,
                                                              int timeout_seconds) {
    NetworkDiagnosticResult result;
    double start_time = getCurrentTimeMs();

    obs_log(LOG_INFO, "[Network Diagnostics] Testing TCP connection to: %s:%d", hostname.c_str(),
            port);

    struct addrinfo* res = nullptr;
#ifdef _WIN32
    SOCKET sockfd = INVALID_SOCKET;
#else
    int sockfd = -1;
#endif

    try {
        // First resolve the hostname
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        string port_str = to_string(port);
        int status = getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &res);

        if (status != 0) {
            result.tcp_connection_success = false;
            result.error_message =
                "DNS resolution failed for TCP test: " + getGaiErrorString(status);
            result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
            obs_log(LOG_ERROR,
                    "[Network Diagnostics] TCP test DNS resolution failed for %s:%d: %s (%.2fms)",
                    hostname.c_str(), port, getGaiErrorString(status).c_str(),
                    result.tcp_connection_time_ms);
            return result;
        }

        // Try to connect to the first resolved address
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
#ifdef _WIN32
        if (sockfd == INVALID_SOCKET) {
#else
        if (sockfd == -1) {
#endif
            result.tcp_connection_success = false;
            result.error_message = "Failed to create socket";
            result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
            obs_log(LOG_ERROR, "[Network Diagnostics] Failed to create socket for %s:%d (%.2fms)",
                    hostname.c_str(), port, result.tcp_connection_time_ms);
            freeaddrinfo(res);
            return result;
        }

        // Set socket to non-blocking mode for timeout control
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sockfd, FIONBIO, &mode);
#else
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
#endif

        // Attempt connection
        int connect_result = connect(sockfd, res->ai_addr, static_cast<int>(res->ai_addrlen));

        if (connect_result == 0) {
            // Connection succeeded immediately
            result.tcp_connection_success = true;
            result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
            result.detailed_info = "TCP connection successful";
            obs_log(LOG_INFO, "[Network Diagnostics] TCP connection successful to %s:%d (%.2fms)",
                    hostname.c_str(), port, result.tcp_connection_time_ms);
        } else {
            // Check if connection is in progress
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
            if (errno == EINPROGRESS) {
#endif
                // Use select to wait for connection with timeout
                fd_set write_fds;
                FD_ZERO(&write_fds);
                FD_SET(sockfd, &write_fds);

                struct timeval timeout;
                timeout.tv_sec = timeout_seconds;
                timeout.tv_usec = 0;

#ifdef _WIN32
                int select_result = select(0, nullptr, &write_fds, nullptr, &timeout);
#else
                int select_result = select(sockfd + 1, nullptr, &write_fds, nullptr, &timeout);
#endif

                if (select_result > 0 && FD_ISSET(sockfd, &write_fds)) {
                    // Check if connection actually succeeded
                    int error = 0;
                    socklen_t len = sizeof(error);
                    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*) &error, &len);

                    if (error == 0) {
                        result.tcp_connection_success = true;
                        result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
                        result.detailed_info = "TCP connection successful";
                        obs_log(LOG_INFO,
                                "[Network Diagnostics] TCP connection successful to %s:%d (%.2fms)",
                                hostname.c_str(), port, result.tcp_connection_time_ms);
                    } else {
                        result.tcp_connection_success = false;
                        result.error_message = "TCP connection failed: " + string(strerror(error));
                        result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
                        obs_log(LOG_ERROR,
                                "[Network Diagnostics] TCP connection failed to %s:%d: %s (%.2fms)",
                                hostname.c_str(), port, strerror(error),
                                result.tcp_connection_time_ms);
                    }
                } else {
                    result.tcp_connection_success = false;
                    result.error_message = "TCP connection timeout";
                    result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
                    obs_log(LOG_ERROR,
                            "[Network Diagnostics] TCP connection timeout to %s:%d (%.2fms)",
                            hostname.c_str(), port, result.tcp_connection_time_ms);
                }
            } else {
                result.tcp_connection_success = false;
                result.error_message =
                    "TCP connection failed immediately: " + string(strerror(errno));
                result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
                obs_log(
                    LOG_ERROR,
                    "[Network Diagnostics] TCP connection failed immediately to %s:%d: %s (%.2fms)",
                    hostname.c_str(), port, strerror(errno), result.tcp_connection_time_ms);
            }
        }
    } catch (const std::exception& e) {
        result.tcp_connection_success = false;
        result.error_message = "TCP connection test exception: " + string(e.what());
        result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
        obs_log(LOG_ERROR,
                "[Network Diagnostics] TCP connection test exception for %s:%d: %s (%.2fms)",
                hostname.c_str(), port, e.what(), result.tcp_connection_time_ms);
    } catch (...) {
        result.tcp_connection_success = false;
        result.error_message = "TCP connection test unknown exception";
        result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
        obs_log(LOG_ERROR,
                "[Network Diagnostics] TCP connection test unknown exception for %s:%d (%.2fms)",
                hostname.c_str(), port, result.tcp_connection_time_ms);
    }

    // Cleanup
#ifdef _WIN32
    if (sockfd != INVALID_SOCKET) {
        closesocket(sockfd);
    }
#else
    if (sockfd != -1) {
        close(sockfd);
    }
#endif
    if (res) {
        freeaddrinfo(res);
    }

    return result;
}

static size_t network_diagnostics_write_callback(void* contents, size_t size, size_t nmemb,
                                                 void* userp) {
    // Suppress unused parameter warnings
    (void) contents;
    (void) userp;

    // We don't need the response data, just check if we can connect
    return size * nmemb;
}

NetworkDiagnosticResult NetworkDiagnostics::testHttpConnectivity(const string& url,
                                                                 int timeout_seconds) {
    NetworkDiagnosticResult result;
    double start_time = getCurrentTimeMs();

    obs_log(LOG_INFO, "[Network Diagnostics] Testing HTTP connectivity to: %s", url.c_str());

    CURL* curl = nullptr;
    try {
        curl = curl_easy_init();
        if (!curl) {
            result.error_message = "Failed to initialize libcurl";
            result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
            obs_log(LOG_ERROR, "[Network Diagnostics] Failed to initialize libcurl for %s",
                    url.c_str());
            return result;
        }

        // Configure curl options
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, network_diagnostics_write_callback);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "OBS-17Live-Plugin/1.0");
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  // HEAD request only

        // Perform the request
        CURLcode res = curl_easy_perform(curl);
        result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;

        if (res == CURLE_OK) {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            result.tcp_connection_success = true;
            result.detailed_info =
                "HTTP connectivity successful (HTTP " + to_string(response_code) + ")";
            obs_log(LOG_INFO,
                    "[Network Diagnostics] HTTP connectivity successful to %s: HTTP %ld (%.2fms)",
                    url.c_str(), response_code, result.tcp_connection_time_ms);
        } else {
            result.tcp_connection_success = false;
            result.error_message = "HTTP connectivity failed: " + string(curl_easy_strerror(res));

            // Provide more specific error information
            if (res == CURLE_COULDNT_RESOLVE_HOST) {
                result.error_message += " (DNS resolution failed)";
            } else if (res == CURLE_COULDNT_CONNECT) {
                result.error_message += " (TCP connection failed)";
            } else if (res == CURLE_OPERATION_TIMEDOUT) {
                result.error_message += " (Connection timeout)";
            }

            obs_log(LOG_ERROR, "[Network Diagnostics] HTTP connectivity failed to %s: %s (%.2fms)",
                    url.c_str(), curl_easy_strerror(res), result.tcp_connection_time_ms);
        }
    } catch (const std::exception& e) {
        result.tcp_connection_success = false;
        result.error_message = "HTTP connectivity test exception: " + string(e.what());
        result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
        obs_log(LOG_ERROR,
                "[Network Diagnostics] HTTP connectivity test exception for %s: %s (%.2fms)",
                url.c_str(), e.what(), result.tcp_connection_time_ms);
    } catch (...) {
        result.tcp_connection_success = false;
        result.error_message = "HTTP connectivity test unknown exception";
        result.tcp_connection_time_ms = getCurrentTimeMs() - start_time;
        obs_log(LOG_ERROR,
                "[Network Diagnostics] HTTP connectivity test unknown exception for %s (%.2fms)",
                url.c_str(), result.tcp_connection_time_ms);
    }

    if (curl) {
        curl_easy_cleanup(curl);
    }
    return result;
}

NetworkDiagnosticResult NetworkDiagnostics::diagnoseUrl(const string& url, int timeout_seconds) {
    NetworkDiagnosticResult result;

    obs_log(LOG_INFO, "[Network Diagnostics] Starting comprehensive diagnostics for: %s",
            url.c_str());

    string hostname;
    int port;
    if (!parseUrl(url, hostname, port)) {
        result.error_message = "Failed to parse URL: " + url;
        obs_log(LOG_ERROR, "[Network Diagnostics] Failed to parse URL: %s", url.c_str());
        return result;
    }

    // Test DNS resolution
    NetworkDiagnosticResult dns_result = testDnsResolution(hostname, timeout_seconds);
    result.dns_resolution_success = dns_result.dns_resolution_success;
    result.resolved_ips = dns_result.resolved_ips;
    result.dns_resolution_time_ms = dns_result.dns_resolution_time_ms;

    if (!dns_result.dns_resolution_success) {
        result.success = false;
        result.error_message = dns_result.error_message;
        return result;
    }

    // Test TCP connection
    NetworkDiagnosticResult tcp_result = testTcpConnection(hostname, port, timeout_seconds);
    result.tcp_connection_success = tcp_result.tcp_connection_success;
    result.tcp_connection_time_ms = tcp_result.tcp_connection_time_ms;

    if (!tcp_result.tcp_connection_success) {
        result.success = false;
        result.error_message = tcp_result.error_message;
        return result;
    }

    // Test HTTP connectivity
    NetworkDiagnosticResult http_result = testHttpConnectivity(url, timeout_seconds);
    if (!http_result.tcp_connection_success) {
        result.success = false;
        result.error_message = http_result.error_message;
        return result;
    }

    result.success = true;
    result.detailed_info = "All network diagnostics passed";

    obs_log(LOG_INFO,
            "[Network Diagnostics] Comprehensive diagnostics completed successfully for: %s",
            url.c_str());
    obs_log(LOG_INFO, "[Network Diagnostics] DNS: %.2fms, TCP: %.2fms, HTTP: %.2fms",
            result.dns_resolution_time_ms, result.tcp_connection_time_ms,
            http_result.tcp_connection_time_ms);

    return result;
}

void NetworkDiagnostics::runStartupDiagnostics(const string& api_url) {
    obs_log(LOG_INFO, "[Network Diagnostics] ========== Starting Network Diagnostics ==========");
    obs_log(LOG_INFO, "[Network Diagnostics] Target API URL: %s", api_url.c_str());

    NetworkDiagnosticResult result = diagnoseUrl(api_url, 10);

    if (result.success) {
        obs_log(LOG_INFO, "[Network Diagnostics] ✓ Network connectivity is healthy");
        obs_log(LOG_INFO, "[Network Diagnostics] ✓ DNS resolution: %.2fms",
                result.dns_resolution_time_ms);
        obs_log(LOG_INFO, "[Network Diagnostics] ✓ TCP connection: %.2fms",
                result.tcp_connection_time_ms);

        if (!result.resolved_ips.empty()) {
            obs_log(LOG_INFO, "[Network Diagnostics] ✓ Resolved IPs: %s",
                    result.detailed_info.c_str());
        }
    } else {
        obs_log(LOG_ERROR, "[Network Diagnostics] ✗ Network connectivity issues detected");
        obs_log(LOG_ERROR, "[Network Diagnostics] ✗ Error: %s", result.error_message.c_str());

        if (!result.dns_resolution_success) {
            obs_log(LOG_ERROR, "[Network Diagnostics] ✗ DNS Resolution: FAILED (%.2fms)",
                    result.dns_resolution_time_ms);
            obs_log(LOG_ERROR,
                    "[Network Diagnostics] ✗ Recommendation: Check DNS settings, try different DNS "
                    "servers (8.8.8.8, 1.1.1.1)");
        } else {
            obs_log(LOG_INFO, "[Network Diagnostics] ✓ DNS Resolution: OK (%.2fms)",
                    result.dns_resolution_time_ms);

            if (!result.tcp_connection_success) {
                obs_log(LOG_ERROR, "[Network Diagnostics] ✗ TCP Connection: FAILED (%.2fms)",
                        result.tcp_connection_time_ms);
                obs_log(LOG_ERROR,
                        "[Network Diagnostics] ✗ Recommendation: Check firewall settings, proxy "
                        "configuration");
            } else {
                obs_log(LOG_INFO, "[Network Diagnostics] ✓ TCP Connection: OK (%.2fms)",
                        result.tcp_connection_time_ms);
            }
        }
    }

    obs_log(LOG_INFO, "[Network Diagnostics] ========== Network Diagnostics Complete ==========");
}

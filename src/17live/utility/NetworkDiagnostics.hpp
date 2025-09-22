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

#pragma once

#include <string>
#include <vector>

/**
 * Network diagnostic result structure
 */
struct NetworkDiagnosticResult {
    bool success;
    std::string error_message;
    std::string detailed_info;

    // DNS resolution details
    bool dns_resolution_success;
    std::vector<std::string> resolved_ips;
    double dns_resolution_time_ms;

    // TCP connection details
    bool tcp_connection_success;
    double tcp_connection_time_ms;

    NetworkDiagnosticResult()
        : success(false),
          dns_resolution_success(false),
          dns_resolution_time_ms(0.0),
          tcp_connection_success(false),
          tcp_connection_time_ms(0.0) {}
};

/**
 * Network diagnostics class for testing DNS resolution and TCP connectivity
 */
class NetworkDiagnostics {
   public:
    /**
     * Perform comprehensive network diagnostics for a given URL
     * @param url The URL to test (e.g., "https://sta-wap-api.17app.co")
     * @param timeout_seconds Timeout for each test in seconds (default: 10)
     * @return NetworkDiagnosticResult containing detailed test results
     */
    static NetworkDiagnosticResult diagnoseUrl(const std::string& url, int timeout_seconds = 10);

    /**
     * Test DNS resolution for a hostname
     * @param hostname The hostname to resolve
     * @param timeout_seconds Timeout in seconds
     * @return NetworkDiagnosticResult with DNS-specific results
     */
    static NetworkDiagnosticResult testDnsResolution(const std::string& hostname,
                                                     int timeout_seconds = 10);

    /**
     * Test TCP connection to a host and port
     * @param hostname The hostname to connect to
     * @param port The port to connect to
     * @param timeout_seconds Timeout in seconds
     * @return NetworkDiagnosticResult with TCP-specific results
     */
    static NetworkDiagnosticResult testTcpConnection(const std::string& hostname, int port,
                                                     int timeout_seconds = 10);

    /**
     * Perform a quick HTTP connectivity test using libcurl
     * @param url The URL to test
     * @param timeout_seconds Timeout in seconds
     * @return NetworkDiagnosticResult with HTTP-specific results
     */
    static NetworkDiagnosticResult testHttpConnectivity(const std::string& url,
                                                        int timeout_seconds = 10);

    /**
     * Run all network diagnostics and log results
     * @param api_url The API URL to test (from ONESEVENLIVE_API_URL)
     */
    static void runStartupDiagnostics(const std::string& api_url);

   private:
    /**
     * Parse URL to extract hostname and port
     * @param url The URL to parse
     * @param hostname Output hostname
     * @param port Output port (default 80 for http, 443 for https)
     * @return true if parsing successful
     */
    static bool parseUrl(const std::string& url, std::string& hostname, int& port);

    /**
     * Get current timestamp in milliseconds
     */
    static double getCurrentTimeMs();
};

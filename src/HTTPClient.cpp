/*
 * HTTPClient.cpp - Simple HTTP client implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// Static member initialization
std::map<std::string, HTTPClient::Connection> HTTPClient::s_connection_pool;
std::mutex HTTPClient::s_pool_mutex;
std::chrono::seconds HTTPClient::s_connection_timeout{30};
int HTTPClient::s_total_requests = 0;
int HTTPClient::s_reused_connections = 0;

// OpenSSL initialization
static SSL_CTX* s_ssl_ctx = nullptr;
static std::mutex s_ssl_mutex;
static bool s_ssl_initialized = false;

void HTTPClient::Connection::close() {
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (socket >= 0) {
        closeSocket(socket);
        socket = -1;
    }
}

HTTPClient::Response HTTPClient::get(const std::string& url,
                                    const std::map<std::string, std::string>& headers,
                                    int timeoutSeconds) {
    std::string host, path;
    int port;
    bool isHttps;
    
    if (!parseURL(url, host, port, path, isHttps)) {
        Response response;
        response.statusMessage = "Invalid URL";
        return response;
    }
    
    // HTTPS is now supported via OpenSSL
    
    Connection conn = getConnection(host, port, timeoutSeconds);
    if (conn.socket < 0) {
        Response response;
        response.statusMessage = "Connection failed";
        return response;
    }
    
    std::string request = buildRequest("GET", path, host, headers, "", true);
    std::string rawResponse = sendRequestOnConnection(conn, request, timeoutSeconds);
    
    if (rawResponse.empty()) {
        conn.close();
        Response response;
        response.statusMessage = "Request failed";
        return response;
    }
    
    Response response = parseResponse(rawResponse);
    response.connection_reused = (conn.requests_made > 0);
    
    // Determine if connection should be kept alive
    bool keep_alive = shouldKeepAlive(response.headers) && response.success;
    returnConnection(conn, keep_alive);
    
    s_total_requests++;
    if (response.connection_reused) {
        s_reused_connections++;
    }
    
    return response;
}

HTTPClient::Response HTTPClient::post(const std::string& url,
                                     const std::string& data,
                                     const std::string& contentType,
                                     const std::map<std::string, std::string>& headers,
                                     int timeoutSeconds) {
    std::string host, path;
    int port;
    bool isHttps;
    
    if (!parseURL(url, host, port, path, isHttps)) {
        Response response;
        response.statusMessage = "Invalid URL";
        return response;
    }
    
    // HTTPS is now supported via OpenSSL
    
    int socket = connectToHost(host, port, timeoutSeconds);
    if (socket < 0) {
        Response response;
        response.statusMessage = "Connection failed";
        return response;
    }
    
    std::map<std::string, std::string> postHeaders = headers;
    postHeaders["Content-Type"] = contentType;
    postHeaders["Content-Length"] = std::to_string(data.length());
    
    std::string request = buildRequest("POST", path, host, postHeaders, data, false);
    std::string rawResponse = sendRequest(socket, request, timeoutSeconds);
    closeSocket(socket);
    
    return parseResponse(rawResponse);
}

std::string HTTPClient::urlEncode(const std::string& input) {
    std::ostringstream encoded;
    for (unsigned char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return encoded.str();
}

bool HTTPClient::parseURL(const std::string& url, std::string& host, int& port,
                         std::string& path, bool& isHttps) {
    // Simple URL parsing for http://host:port/path format
    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        return false;
    }
    
    std::string scheme = url.substr(0, schemeEnd);
    isHttps = (scheme == "https");
    
    size_t hostStart = schemeEnd + 3;
    size_t pathStart = url.find('/', hostStart);
    
    std::string hostAndPort;
    if (pathStart == std::string::npos) {
        hostAndPort = url.substr(hostStart);
        path = "/";
    } else {
        hostAndPort = url.substr(hostStart, pathStart - hostStart);
        path = url.substr(pathStart);
    }
    
    // Parse host and port
    size_t colonPos = hostAndPort.find(':');
    if (colonPos == std::string::npos) {
        host = hostAndPort;
        port = isHttps ? 443 : 80;
    } else {
        host = hostAndPort.substr(0, colonPos);
        try {
            port = std::stoi(hostAndPort.substr(colonPos + 1));
        } catch (const std::exception&) {
            return false;
        }
    }
    
    return !host.empty();
}

int HTTPClient::connectToHost(const std::string& host, int port, int timeoutSeconds) {
    // Resolve hostname using thread-safe getaddrinfo (replaces deprecated gethostbyname)
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    
    std::string port_str = std::to_string(port);
    int status = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
    if (status != 0) {
        return -1;
    }
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        freeaddrinfo(result);
        return -1;
    }
    
    // Set socket to non-blocking for timeout support
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Setup address structure from getaddrinfo result
    struct sockaddr_in address;
    memcpy(&address, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
    
    // Attempt connection
    int connect_result = connect(sock, (struct sockaddr*)&address, sizeof(address));
    
    if (connect_result < 0 && !isSocketInProgress(getSocketError())) {
        closeSocket(sock);
        return -1;
    }
    
    // Wait for connection with timeout
    if (connect_result < 0) {
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLOUT;
        
        int pollResult = poll(&pfd, 1, timeoutSeconds * 1000);
        if (pollResult <= 0) {
            closeSocket(sock);
            return -1;
        }
        
        // Check for connection errors
        int error;
        socklen_t len = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            closeSocket(sock);
            return -1;
        }
    }
    
    // Set socket back to blocking mode
    fcntl(sock, F_SETFL, flags);
    
    return sock;
}

std::string HTTPClient::sendRequest(int socket, const std::string& request, int timeoutSeconds) {
    // Send request
    ssize_t sent = send(socket, request.c_str(), request.length(), 0);
    if (sent < 0) {
        return "";
    }
    
    // Receive response with timeout
    std::string response;
    char buffer[4096];
    
    while (true) {
        struct pollfd pfd;
        pfd.fd = socket;
        pfd.events = POLLIN;
        
        int pollResult = poll(&pfd, 1, timeoutSeconds * 1000);
        if (pollResult <= 0) {
            break; // Timeout or error
        }
        
        ssize_t received = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            break; // Connection closed or error
        }
        
        buffer[received] = '\0';
        response += buffer;
        
        // Check if we have complete headers (simple check for \r\n\r\n)
        if (response.find("\r\n\r\n") != std::string::npos) {
            // Parse content length if present to determine when body is complete
            size_t headerEnd = response.find("\r\n\r\n");
            std::string headers = response.substr(0, headerEnd);
            
            size_t contentLengthPos = headers.find("Content-Length:");
            if (contentLengthPos != std::string::npos) {
                size_t valueStart = headers.find(':', contentLengthPos) + 1;
                size_t valueEnd = headers.find('\r', valueStart);
                
                try {
                    int contentLength = std::stoi(headers.substr(valueStart, valueEnd - valueStart));
                    size_t bodyStart = headerEnd + 4;
                    int currentBodyLength = response.length() - bodyStart;
                    
                    if (currentBodyLength >= contentLength) {
                        break; // Body complete
                    }
                } catch (const std::exception&) {
                    // Invalid content length, continue reading
                }
            } else {
                // No content length, assume response is complete after headers
                break;
            }
        }
    }
    
    return response;
}

std::string HTTPClient::sendSSLRequest(SSL* ssl, const std::string& request, int timeout_seconds) {
    // Send request
    int sent = SSL_write(ssl, request.c_str(), request.length());
    if (sent <= 0) {
        return "";
    }
    
    // Receive response
    std::string response;
    char buffer[4096];
    
    while (true) {
        int received = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        if (received <= 0) {
            int ssl_error = SSL_get_error(ssl, received);
            if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
                continue; // Try again
            }
            break; // Connection closed or error
        }
        
        buffer[received] = '\0';
        response += buffer;
        
        // Check if we have complete headers
        if (response.find("\r\n\r\n") != std::string::npos) {
            // Parse content length if present
            size_t headerEnd = response.find("\r\n\r\n");
            std::string headers = response.substr(0, headerEnd);
            
            size_t contentLengthPos = headers.find("Content-Length:");
            if (contentLengthPos != std::string::npos) {
                size_t valueStart = headers.find(':', contentLengthPos) + 1;
                size_t valueEnd = headers.find('\r', valueStart);
                
                try {
                    int contentLength = std::stoi(headers.substr(valueStart, valueEnd - valueStart));
                    size_t bodyStart = headerEnd + 4;
                    int currentBodyLength = response.length() - bodyStart;
                    
                    if (currentBodyLength >= contentLength) {
                        break; // Body complete
                    }
                } catch (const std::exception&) {
                    // Invalid content length, continue reading
                }
            } else {
                // No content length, assume response is complete after headers
                break;
            }
        }
    }
    
    return response;
}

HTTPClient::Response HTTPClient::parseResponse(const std::string& rawResponse) {
    Response response;
    
    if (rawResponse.empty()) {
        response.statusMessage = "Empty response";
        return response;
    }
    
    size_t headerEnd = rawResponse.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        response.statusMessage = "Invalid response format";
        return response;
    }
    
    std::string headerSection = rawResponse.substr(0, headerEnd);
    response.body = rawResponse.substr(headerEnd + 4);
    
    // Parse status line
    size_t firstLine = headerSection.find("\r\n");
    std::string statusLine = headerSection.substr(0, firstLine);
    
    std::istringstream statusStream(statusLine);
    std::string httpVersion;
    statusStream >> httpVersion >> response.statusCode;
    std::getline(statusStream, response.statusMessage);
    
    // Trim whitespace from status message
    if (!response.statusMessage.empty() && response.statusMessage[0] == ' ') {
        response.statusMessage = response.statusMessage.substr(1);
    }
    
    // Parse headers
    std::istringstream headerStream(headerSection.substr(firstLine + 2));
    std::string headerLine;
    while (std::getline(headerStream, headerLine)) {
        if (headerLine.back() == '\r') {
            headerLine.pop_back();
        }
        
        size_t colonPos = headerLine.find(':');
        if (colonPos != std::string::npos) {
            std::string name = headerLine.substr(0, colonPos);
            std::string value = headerLine.substr(colonPos + 1);
            
            // Trim whitespace
            if (!value.empty() && value[0] == ' ') {
                value = value.substr(1);
            }
            
            response.headers[name] = value;
        }
    }
    
    response.success = (response.statusCode >= 200 && response.statusCode < 300);
    return response;
}

std::string HTTPClient::buildRequest(const std::string& method,
                                    const std::string& path,
                                    const std::string& host,
                                    const std::map<std::string, std::string>& headers,
                                    const std::string& body,
                                    bool keep_alive) {
    std::ostringstream request;
    
    // Request line
    request << method << " " << path << " HTTP/1.1\r\n";
    
    // Host header (required for HTTP/1.1)
    request << "Host: " << host << "\r\n";
    
    // Default headers
    request << "User-Agent: PsyMP3/" << PSYMP3_VERSION << "\r\n";
    request << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
    
    // Additional headers
    for (const auto& header : headers) {
        request << header.first << ": " << header.second << "\r\n";
    }
    
    // End headers
    request << "\r\n";
    
    // Body (for POST requests)
    if (!body.empty()) {
        request << body;
    }
    
    return request.str();
}

HTTPClient::Response HTTPClient::head(const std::string& url,
                                     const std::map<std::string, std::string>& headers,
                                     int timeoutSeconds) {
    std::string host, path;
    int port;
    bool isHttps;
    
    if (!parseURL(url, host, port, path, isHttps)) {
        Response response;
        response.statusMessage = "Invalid URL";
        return response;
    }
    
    // HTTPS is now supported via OpenSSL
    
    int socket = connectToHost(host, port, timeoutSeconds);
    if (socket < 0) {
        Response response;
        response.statusMessage = "Connection failed";
        return response;
    }
    
    std::string request = buildRequest("HEAD", path, host, headers, "", false);
    std::string rawResponse = sendRequest(socket, request, timeoutSeconds);
    closeSocket(socket);
    
    return parseResponse(rawResponse);
}

HTTPClient::Response HTTPClient::getRange(const std::string& url,
                                         long start_byte,
                                         long end_byte,
                                         const std::map<std::string, std::string>& headers,
                                         int timeoutSeconds) {
    std::string host, path;
    int port;
    bool isHttps;
    
    if (!parseURL(url, host, port, path, isHttps)) {
        Response response;
        response.statusMessage = "Invalid URL";
        return response;
    }
    
    // HTTPS is now supported via OpenSSL
    
    int socket = connectToHost(host, port, timeoutSeconds);
    if (socket < 0) {
        Response response;
        response.statusMessage = "Connection failed";
        return response;
    }
    
    // Add Range header to the request headers
    std::map<std::string, std::string> range_headers = headers;
    if (end_byte >= 0) {
        range_headers["Range"] = "bytes=" + std::to_string(start_byte) + "-" + std::to_string(end_byte);
    } else {
        range_headers["Range"] = "bytes=" + std::to_string(start_byte) + "-";
    }
    
    std::string request = buildRequest("GET", path, host, range_headers, "", false);
    std::string rawResponse = sendRequest(socket, request, timeoutSeconds);
    closeSocket(socket);
    
    return parseResponse(rawResponse);
}
// Connection pool management methods

std::string HTTPClient::getConnectionKey(const std::string& host, int port) {
    return host + ":" + std::to_string(port);
}

HTTPClient::Connection HTTPClient::getConnection(const std::string& host, int port, int timeout_seconds) {
    std::lock_guard<std::mutex> lock(s_pool_mutex);
    
    // Clean up expired connections first
    cleanupExpiredConnections();
    
    std::string key = getConnectionKey(host, port);
    auto it = s_connection_pool.find(key);
    
    if (it != s_connection_pool.end() && it->second.isValid() && !it->second.isExpired(s_connection_timeout)) {
        // Reuse existing connection
        Connection conn = it->second;
        s_connection_pool.erase(it);
        conn.last_used = std::chrono::steady_clock::now();
        return conn;
    }
    
    // Create new connection
    Connection conn;
    conn.host = host;
    conn.port = port;
    conn.is_https = (port == 443);
    conn.socket = connectToHost(host, port, timeout_seconds);
    conn.last_used = std::chrono::steady_clock::now();
    conn.keep_alive = false; // Will be determined from response
    conn.requests_made = 0;
    conn.ssl = nullptr;
    
    // Initialize SSL if needed
    if (conn.is_https && conn.socket >= 0) {
        initializeSSL();
        if (s_ssl_initialized && s_ssl_ctx) {
            conn.ssl = SSL_new(s_ssl_ctx);
            if (conn.ssl) {
                SSL_set_fd(conn.ssl, conn.socket);
                if (SSL_connect(conn.ssl) <= 0) {
                    ERR_print_errors_fp(stderr);
                    SSL_free(conn.ssl);
                    conn.ssl = nullptr;
                    closeSocket(conn.socket);
                    conn.socket = -1;
                }
            }
        }
    }
    
    return conn;
}

void HTTPClient::returnConnection(Connection& conn, bool keep_alive) {
    std::lock_guard<std::mutex> lock(s_pool_mutex);
    
    if (!keep_alive || !conn.isValid()) {
        conn.close();
        return;
    }
    
    conn.keep_alive = keep_alive;
    conn.requests_made++;
    conn.last_used = std::chrono::steady_clock::now();
    
    std::string key = getConnectionKey(conn.host, conn.port);
    s_connection_pool[key] = conn;
}

void HTTPClient::cleanupExpiredConnections() {
    // Note: This function should be called with s_pool_mutex already locked
    auto it = s_connection_pool.begin();
    while (it != s_connection_pool.end()) {
        if (it->second.isExpired(s_connection_timeout) || !it->second.isValid()) {
            it->second.close();
            it = s_connection_pool.erase(it);
        } else {
            ++it;
        }
    }
}

std::string HTTPClient::sendRequestOnConnection(Connection& conn, const std::string& request, int timeout_seconds) {
    if (conn.ssl) {
        return sendSSLRequest(conn.ssl, request, timeout_seconds);
    } else {
        return sendRequest(conn.socket, request, timeout_seconds);
    }
}

bool HTTPClient::shouldKeepAlive(const std::map<std::string, std::string>& headers) {
    auto it = headers.find("connection");
    if (it != headers.end()) {
        std::string conn_header = it->second;
        std::transform(conn_header.begin(), conn_header.end(), conn_header.begin(), ::tolower);
        return conn_header.find("keep-alive") != std::string::npos;
    }
    
    // HTTP/1.1 defaults to keep-alive
    return true;
}

void HTTPClient::closeAllConnections() {
    std::lock_guard<std::mutex> lock(s_pool_mutex);
    for (auto& pair : s_connection_pool) {
        pair.second.close();
    }
    s_connection_pool.clear();
}

void HTTPClient::setConnectionTimeout(int timeout_seconds) {
    std::lock_guard<std::mutex> lock(s_pool_mutex);
    s_connection_timeout = std::chrono::seconds(timeout_seconds);
}

std::map<std::string, int> HTTPClient::getConnectionPoolStats() {
    std::lock_guard<std::mutex> lock(s_pool_mutex);
    
    std::map<std::string, int> stats;
    stats["active_connections"] = static_cast<int>(s_connection_pool.size());
    stats["total_requests"] = s_total_requests;
    stats["reused_connections"] = s_reused_connections;
    stats["connection_reuse_rate"] = s_total_requests > 0 ? (s_reused_connections * 100 / s_total_requests) : 0;
    
    return stats;
}

void HTTPClient::initializeSSL() {
    std::lock_guard<std::mutex> lock(s_ssl_mutex);
    
    if (s_ssl_initialized) return;
    
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    // Create SSL context
    s_ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!s_ssl_ctx) {
        ERR_print_errors_fp(stderr);
        return;
    }
    
    // Disable certificate verification - we don't care about invalid certificates
    SSL_CTX_set_verify(s_ssl_ctx, SSL_VERIFY_NONE, nullptr);
    
    s_ssl_initialized = true;
}

void HTTPClient::cleanupSSL() {
    std::lock_guard<std::mutex> lock(s_ssl_mutex);
    
    if (!s_ssl_initialized) return;
    
    if (s_ssl_ctx) {
        SSL_CTX_free(s_ssl_ctx);
        s_ssl_ctx = nullptr;
    }
    
    EVP_cleanup();
    ERR_free_strings();
    
    s_ssl_initialized = false;
}

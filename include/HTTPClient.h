/*
 * HTTPClient.h - Simple HTTP client for Last.fm API
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <chrono>
#include <mutex>

/**
 * @brief Simple HTTP client with Keep-Alive support for basic GET/POST operations
 * 
 * Lightweight HTTP client with HTTP/1.1 Keep-Alive connection pooling.
 * Uses system sockets for cross-platform compatibility without external dependencies.
 */
class HTTPClient {
public:
    /**
     * @brief HTTP response structure
     */
    struct Response {
        int statusCode = 0;
        std::string statusMessage;
        std::map<std::string, std::string> headers;
        std::string body;
        bool success = false;
        bool connection_reused = false;
    };
    
    /**
     * @brief Persistent HTTP connection for Keep-Alive
     */
    struct Connection {
        int socket = -1;
        std::string host;
        int port = 0;
        std::chrono::steady_clock::time_point last_used;
        bool keep_alive = false;
        int max_requests = 100;
        int requests_made = 0;
        
        bool isValid() const {
            return socket >= 0 && keep_alive && requests_made < max_requests;
        }
        
        bool isExpired(std::chrono::seconds timeout = std::chrono::seconds(30)) const {
            return std::chrono::steady_clock::now() - last_used > timeout;
        }
        
        void close();  // Moved to implementation to avoid header dependencies
    };
    
    /**
     * @brief Perform HTTP GET request
     * @param url The complete URL to request
     * @param headers Optional additional headers
     * @param timeoutSeconds Request timeout in seconds (default: 30)
     * @return HTTP response structure
     */
    static Response get(const std::string& url, 
                       const std::map<std::string, std::string>& headers = {},
                       int timeoutSeconds = 30);
    
    /**
     * @brief Perform HTTP POST request
     * @param url The complete URL to post to
     * @param data The POST data (form-encoded or raw)
     * @param contentType Content-Type header value
     * @param headers Optional additional headers
     * @param timeoutSeconds Request timeout in seconds (default: 30)
     * @return HTTP response structure
     */
    static Response post(const std::string& url,
                        const std::string& data,
                        const std::string& contentType = "application/x-www-form-urlencoded",
                        const std::map<std::string, std::string>& headers = {},
                        int timeoutSeconds = 30);
    
    /**
     * @brief Perform HTTP HEAD request to get headers without body
     * @param url The complete URL to request
     * @param headers Optional additional headers
     * @param timeoutSeconds Request timeout in seconds (default: 30)
     * @return HTTP response structure (body will be empty)
     */
    static Response head(const std::string& url, 
                        const std::map<std::string, std::string>& headers = {},
                        int timeoutSeconds = 30);
    
    /**
     * @brief Perform HTTP GET request with range header for partial content
     * @param url The complete URL to request
     * @param start_byte Start byte position for range request
     * @param end_byte End byte position for range request (optional, -1 for end of file)
     * @param headers Optional additional headers
     * @param timeoutSeconds Request timeout in seconds (default: 30)
     * @return HTTP response structure
     */
    static Response getRange(const std::string& url,
                            long start_byte,
                            long end_byte = -1,
                            const std::map<std::string, std::string>& headers = {},
                            int timeoutSeconds = 30);
    
    /**
     * @brief URL encode a string for safe transmission
     * @param input The string to encode
     * @return URL-encoded string
     */
    static std::string urlEncode(const std::string& input);
    
    /**
     * @brief Parse URL into components
     * @param url The URL to parse
     * @param host Output parameter for hostname
     * @param port Output parameter for port number
     * @param path Output parameter for path
     * @param isHttps Output parameter for HTTPS detection
     * @return true if parsing succeeded, false otherwise
     */
    static bool parseURL(const std::string& url, std::string& host, int& port, 
                        std::string& path, bool& isHttps);
    
    /**
     * @brief Close all keep-alive connections and clear connection pool
     */
    static void closeAllConnections();
    
    /**
     * @brief Set connection pool timeout (default: 30 seconds)
     * @param timeout_seconds Timeout in seconds for idle connections
     */
    static void setConnectionTimeout(int timeout_seconds);
    
    /**
     * @brief Get current connection pool statistics
     * @return Map with pool statistics (active_connections, total_requests, etc.)
     */
    static std::map<std::string, int> getConnectionPoolStats();

private:
    /**
     * @brief Connect to a host via TCP socket
     * @param host The hostname to connect to
     * @param port The port number
     * @param timeoutSeconds Connection timeout
     * @return Socket file descriptor, or -1 on failure
     */
    static int connectToHost(const std::string& host, int port, int timeoutSeconds);
    
    /**
     * @brief Send HTTP request and receive response
     * @param socket The connected socket
     * @param request The complete HTTP request string
     * @param timeoutSeconds Read timeout
     * @return Raw HTTP response
     */
    static std::string sendRequest(int socket, const std::string& request, int timeoutSeconds);
    
    /**
     * @brief Parse raw HTTP response into Response structure
     * @param rawResponse The raw HTTP response string
     * @return Parsed Response structure
     */
    static Response parseResponse(const std::string& rawResponse);
    
    /**
     * @brief Build HTTP request string
     * @param method HTTP method (GET, POST, etc.)
     * @param path Request path
     * @param host Host header value
     * @param headers Additional headers
     * @param body Request body (for POST)
     * @param keep_alive Whether to request Keep-Alive
     * @return Complete HTTP request string
     */
    static std::string buildRequest(const std::string& method,
                                   const std::string& path,
                                   const std::string& host,
                                   const std::map<std::string, std::string>& headers,
                                   const std::string& body = "",
                                   bool keep_alive = true);
    
    // Connection pool management
    static std::map<std::string, Connection> s_connection_pool;
    static std::mutex s_pool_mutex;
    static std::chrono::seconds s_connection_timeout;
    static int s_total_requests;
    static int s_reused_connections;
    
    /**
     * @brief Get connection key for connection pooling
     * @param host The hostname
     * @param port The port number
     * @return Connection key string
     */
    static std::string getConnectionKey(const std::string& host, int port);
    
    /**
     * @brief Get or create a connection for the given host/port
     * @param host The hostname
     * @param port The port number
     * @param timeout_seconds Connection timeout
     * @return Connection object (may be reused or new)
     */
    static Connection getConnection(const std::string& host, int port, int timeout_seconds);
    
    /**
     * @brief Return a connection to the pool or close it
     * @param conn Connection to return
     * @param keep_alive Whether the connection supports keep-alive
     */
    static void returnConnection(Connection& conn, bool keep_alive);
    
    /**
     * @brief Clean up expired connections from the pool
     */
    static void cleanupExpiredConnections();
    
    /**
     * @brief Send request and receive response using connection
     * @param conn Connection to use
     * @param request The complete HTTP request string
     * @param timeout_seconds Read timeout
     * @return Raw HTTP response
     */
    static std::string sendRequestOnConnection(Connection& conn, const std::string& request, int timeout_seconds);
    
    /**
     * @brief Check if response indicates connection should be kept alive
     * @param headers Response headers
     * @return true if connection should be kept alive
     */
    static bool shouldKeepAlive(const std::map<std::string, std::string>& headers);
};

#endif // HTTPCLIENT_H
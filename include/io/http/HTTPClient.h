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

// All external headers included via psymp3.h
// Forward declarations handled in psymp3.h

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
        SSL* ssl = nullptr;
        std::string host;
        int port = 0;
        bool is_https = false;
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
    
    /**
     * @brief Initialize OpenSSL library (called automatically)
     */
    static void initializeSSL();
    
    /**
     * @brief Cleanup OpenSSL library
     */
    static void cleanupSSL();

private:
    static Response performRequest(const std::string& method,
                                   const std::string& url,
                                   const std::string& postData,
                                   const std::map<std::string, std::string>& headers,
                                   int timeoutSeconds);
};

#endif // HTTPCLIENT_H
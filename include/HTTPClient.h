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

/**
 * @brief Simple HTTP client for basic GET/POST operations
 * 
 * Lightweight HTTP client designed specifically for Last.fm API communication.
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
     * @return Complete HTTP request string
     */
    static std::string buildRequest(const std::string& method,
                                   const std::string& path,
                                   const std::string& host,
                                   const std::map<std::string, std::string>& headers,
                                   const std::string& body = "");
};

#endif // HTTPCLIENT_H
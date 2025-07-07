/*
 * HTTPClient.cpp - Simple HTTP client implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

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
    
    if (isHttps) {
        Response response;
        response.statusMessage = "HTTPS not supported";
        return response;
    }
    
    int socket = connectToHost(host, port, timeoutSeconds);
    if (socket < 0) {
        Response response;
        response.statusMessage = "Connection failed";
        return response;
    }
    
    std::string request = buildRequest("GET", path, host, headers);
    std::string rawResponse = sendRequest(socket, request, timeoutSeconds);
    close(socket);
    
    return parseResponse(rawResponse);
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
    
    if (isHttps) {
        Response response;
        response.statusMessage = "HTTPS not supported";
        return response;
    }
    
    int socket = connectToHost(host, port, timeoutSeconds);
    if (socket < 0) {
        Response response;
        response.statusMessage = "Connection failed";
        return response;
    }
    
    std::map<std::string, std::string> postHeaders = headers;
    postHeaders["Content-Type"] = contentType;
    postHeaders["Content-Length"] = std::to_string(data.length());
    
    std::string request = buildRequest("POST", path, host, postHeaders, data);
    std::string rawResponse = sendRequest(socket, request, timeoutSeconds);
    close(socket);
    
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
    // Resolve hostname
    struct hostent* hostEntry = gethostbyname(host.c_str());
    if (!hostEntry) {
        return -1;
    }
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    // Set socket to non-blocking for timeout support
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Setup address structure
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    memcpy(&address.sin_addr, hostEntry->h_addr_list[0], hostEntry->h_length);
    
    // Attempt connection
    int result = connect(sock, (struct sockaddr*)&address, sizeof(address));
    
    if (result < 0 && errno != EINPROGRESS) {
        close(sock);
        return -1;
    }
    
    // Wait for connection with timeout
    if (result < 0) {
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLOUT;
        
        int pollResult = poll(&pfd, 1, timeoutSeconds * 1000);
        if (pollResult <= 0) {
            close(sock);
            return -1;
        }
        
        // Check for connection errors
        int error;
        socklen_t len = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            close(sock);
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
                                    const std::string& body) {
    std::ostringstream request;
    
    // Request line
    request << method << " " << path << " HTTP/1.1\r\n";
    
    // Host header (required for HTTP/1.1)
    request << "Host: " << host << "\r\n";
    
    // Default headers
    request << "User-Agent: PsyMP3/" << PSYMP3_VERSION << "\r\n";
    request << "Connection: close\r\n";
    
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
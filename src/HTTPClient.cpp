/*
 * HTTPClient.cpp - libcurl-based HTTP client implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// Thread-safe RAII wrapper for global curl initialization
class CurlLifecycleManager {
private:
    static std::once_flag s_init_flag;
    static bool s_initialized;
    
public:
    CurlLifecycleManager() {
        std::call_once(s_init_flag, []() {
            CURLcode result = curl_global_init(CURL_GLOBAL_ALL);
            s_initialized = (result == CURLE_OK);
        });
    }
    
    ~CurlLifecycleManager() {
        // Note: curl_global_cleanup() should only be called once at program exit
        // We let the static destructor handle this
    }
    
    static bool isInitialized() { return s_initialized; }
};

std::once_flag CurlLifecycleManager::s_init_flag;
bool CurlLifecycleManager::s_initialized = false;
static CurlLifecycleManager s_curl_manager;

// Callback function for libcurl to write received data into a std::string
static size_t write_callback(void *contents, size_t size, size_t nmemb, std::string *s) {
    size_t newLength = size * nmemb;
    if (newLength == 0) return 0;
    
    try {
        s->append(static_cast<const char*>(contents), newLength);
    } catch(const std::bad_alloc&) {
        // Return 0 to signal error to libcurl
        return 0;
    }
    return newLength;
}

// Callback function for libcurl to write header data
static size_t header_callback(void *contents, size_t size, size_t nmemb, std::string *s) {
    size_t newLength = size * nmemb;
    if (newLength == 0) return 0;
    
    try {
        s->append(static_cast<const char*>(contents), newLength);
    } catch(const std::bad_alloc&) {
        return 0;
    }
    return newLength;
}

HTTPClient::Response HTTPClient::get(const std::string& url,
                                    const std::map<std::string, std::string>& headers,
                                    int timeoutSeconds) {
    return performRequest("GET", url, "", headers, timeoutSeconds);
}

HTTPClient::Response HTTPClient::post(const std::string& url,
                                     const std::string& data,
                                     const std::string& contentType,
                                     const std::map<std::string, std::string>& headers,
                                     int timeoutSeconds) {
    std::map<std::string, std::string> postHeaders = headers;
    postHeaders["Content-Type"] = contentType;
    return performRequest("POST", url, data, postHeaders, timeoutSeconds);
}

HTTPClient::Response HTTPClient::head(const std::string& url,
                                     const std::map<std::string, std::string>& headers,
                                     int timeoutSeconds) {
    return performRequest("HEAD", url, "", headers, timeoutSeconds);
}

HTTPClient::Response HTTPClient::getRange(const std::string& url,
                                         long start_byte,
                                         long end_byte,
                                         const std::map<std::string, std::string>& headers,
                                         int timeoutSeconds) {
    std::map<std::string, std::string> range_headers = headers;
    if (end_byte >= 0) {
        range_headers["Range"] = "bytes=" + std::to_string(start_byte) + "-" + std::to_string(end_byte);
    } else {
        range_headers["Range"] = "bytes=" + std::to_string(start_byte) + "-";
    }
    return performRequest("GET", url, "", range_headers, timeoutSeconds);
}

HTTPClient::Response HTTPClient::performRequest(const std::string& method,
                                               const std::string& url,
                                               const std::string& postData,
                                               const std::map<std::string, std::string>& headers,
                                               int timeoutSeconds) {
    Response response;
    
    // Check if curl was initialized properly
    if (!CurlLifecycleManager::isInitialized()) {
        response.statusMessage = "libcurl initialization failed";
        return response;
    }
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        response.statusMessage = "Failed to initialize curl handle";
        return response;
    }

    std::string readBuffer;
    std::string headerBuffer;
    struct curl_slist *chunk = nullptr;

    // Basic options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeoutSeconds));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L); // 10 second connect timeout
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerBuffer);
    
    // Security and protocol options
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PsyMP3/3.0");
    
    // Accept encoding for compression
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");
    
    // Method-specific options
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(postData.length()));
    } else if (method == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }

    // Set custom headers
    for (const auto& header : headers) {
        std::string headerStr = header.first + ": " + header.second;
        chunk = curl_slist_append(chunk, headerStr.c_str());
    }
    if (chunk) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        response.statusMessage = std::string("libcurl error: ") + curl_easy_strerror(res);
        response.success = false;
    } else {
        // Get response info
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        response.statusCode = static_cast<int>(http_code);
        response.body = std::move(readBuffer);
        response.success = (response.statusCode >= 200 && response.statusCode < 400);
        
        // Parse headers
        std::istringstream headerStream(headerBuffer);
        std::string headerLine;
        while (std::getline(headerStream, headerLine)) {
            // Remove trailing \r if present
            if (!headerLine.empty() && headerLine.back() == '\r') {
                headerLine.pop_back();
            }
            
            // Skip empty lines and status line
            if (headerLine.empty() || headerLine.substr(0, 4) == "HTTP") {
                continue;
            }
            
            size_t colonPos = headerLine.find(':');
            if (colonPos != std::string::npos && colonPos > 0) {
                std::string name = headerLine.substr(0, colonPos);
                std::string value = headerLine.substr(colonPos + 1);
                
                // Trim whitespace from value
                size_t start = value.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    size_t end = value.find_last_not_of(" \t");
                    value = value.substr(start, end - start + 1);
                } else {
                    value.clear();
                }
                
                response.headers[name] = value;
            }
        }
        
        // Set status message for non-success codes
        if (!response.success) {
            response.statusMessage = "HTTP " + std::to_string(response.statusCode);
        }
    }

    // Cleanup
    if (chunk) {
        curl_slist_free_all(chunk);
    }
    curl_easy_cleanup(curl);

    return response;
}

std::string HTTPClient::urlEncode(const std::string& input) {
    if (!CurlLifecycleManager::isInitialized()) {
        return input; // Fallback - return unencoded
    }
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        return input; // Fallback - return unencoded
    }
    
    char *output = curl_easy_escape(curl, input.c_str(), static_cast<int>(input.length()));
    std::string result;
    
    if (output) {
        result = output;
        curl_free(output);
    } else {
        result = input; // Fallback - return unencoded
    }
    
    curl_easy_cleanup(curl);
    return result;
}

// Implementation of missing methods from header
bool HTTPClient::parseURL(const std::string& url, std::string& host, int& port, 
                         std::string& path, bool& isHttps) {
    // Simple URL parsing - could be enhanced with libcurl's URL parsing if needed
    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        return false;
    }
    
    std::string scheme = url.substr(0, schemeEnd);
    isHttps = (scheme == "https");
    port = isHttps ? 443 : 80;
    
    size_t hostStart = schemeEnd + 3;
    size_t pathStart = url.find('/', hostStart);
    if (pathStart == std::string::npos) {
        pathStart = url.length();
        path = "/";
    } else {
        path = url.substr(pathStart);
    }
    
    std::string hostPort = url.substr(hostStart, pathStart - hostStart);
    size_t portPos = hostPort.find(':');
    if (portPos != std::string::npos) {
        host = hostPort.substr(0, portPos);
        try {
            port = std::stoi(hostPort.substr(portPos + 1));
        } catch (...) {
            return false;
        }
    } else {
        host = hostPort;
    }
    
    return !host.empty();
}

void HTTPClient::closeAllConnections() {
    // For libcurl implementation, this is handled automatically
    // Connection pooling would be implemented here if using keep-alive
}

void HTTPClient::setConnectionTimeout(int timeout_seconds) {
    // This would be stored in a static variable for use in performRequest
    // For now, we use the default 10 second connect timeout
}

std::map<std::string, int> HTTPClient::getConnectionPoolStats() {
    // Return empty stats for libcurl implementation
    return {};
}

void HTTPClient::initializeSSL() {
    // libcurl handles SSL initialization automatically
}

void HTTPClient::cleanupSSL() {
    // libcurl handles SSL cleanup automatically
}
// Implementation of Connection::close() method
void HTTPClient::Connection::close() {
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (socket >= 0) {
#ifdef _WIN32
        closesocket(socket);
#else
        ::close(socket);
#endif
        socket = -1;
    }
    keep_alive = false;
}
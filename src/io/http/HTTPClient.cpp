/*
 * HTTPClient.cpp - libcurl-based HTTP client implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <cstdio>

namespace PsyMP3 {
namespace IO {
namespace HTTP {

// Thread-safe RAII wrapper for global curl initialization with proper cleanup
class CurlLifecycleManager {
private:
    static std::once_flag s_init_flag;
    static std::once_flag s_cleanup_flag;
    static std::atomic<bool> s_initialized;
    static std::mutex s_cleanup_mutex;
    
public:
    static std::atomic<int> s_active_handles;
    
    // Thread-safe connection pool management
    static std::mutex s_pool_mutex;
    static std::unordered_map<std::string, std::vector<CURL*>> s_connection_pool;
    static std::chrono::steady_clock::time_point s_last_pool_cleanup;
    static const size_t MAX_CONNECTIONS_PER_HOST = 4;
    static constexpr std::chrono::seconds POOL_CLEANUP_INTERVAL{30};
    
public:
    CurlLifecycleManager() {
        std::call_once(s_init_flag, []() {
            CURLcode result = curl_global_init(CURL_GLOBAL_ALL);
            s_initialized.store(result == CURLE_OK);
            if (s_initialized) {
                Debug::log("http", "CurlLifecycleManager: libcurl initialized successfully");
            } else {
                Debug::log("http", "CurlLifecycleManager: libcurl initialization failed: ", result);
            }
        });
    }
    
    ~CurlLifecycleManager() {
        // Cleanup is handled by explicit cleanup call or program exit
    }
    
    static bool isInitialized() { return s_initialized.load(); }
    
    static void incrementHandleCount() {
        s_active_handles.fetch_add(1);
    }
    
    static void decrementHandleCount() {
        int count = s_active_handles.fetch_sub(1);
        if (count == 1) { // Was the last handle
            // Perform cleanup after a delay to allow for connection reuse
            std::thread cleanup_thread([]() {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                performCleanupIfNeeded();
            });
            cleanup_thread.detach();
        }
    }
    
    static void forceCleanup() {
        std::call_once(s_cleanup_flag, []() {
            std::lock_guard<std::mutex> lock(s_cleanup_mutex);
            if (s_initialized) {
                // Clean up connection pool first
                cleanupConnectionPool();
                curl_global_cleanup();
                s_initialized.store(false);
                Debug::log("http", "CurlLifecycleManager: libcurl cleanup completed");
            }
        });
    }
    
    // Thread-safe connection pool management
    static CURL* acquireConnection(const std::string& host) {
        std::lock_guard<std::mutex> lock(s_pool_mutex);
        
        // Periodic cleanup of expired connections
        auto now = std::chrono::steady_clock::now();
        if (now - s_last_pool_cleanup > POOL_CLEANUP_INTERVAL) {
            cleanupExpiredConnections();
            s_last_pool_cleanup = now;
        }
        
        auto it = s_connection_pool.find(host);
        if (it != s_connection_pool.end() && !it->second.empty()) {
            CURL* handle = it->second.back();
            it->second.pop_back();
            Debug::log("http", "CurlLifecycleManager: Reusing connection for host: ", host);
            return handle;
        }
        
        // Create new connection
        CURL* handle = curl_easy_init();
        if (handle) {
            Debug::log("http", "CurlLifecycleManager: Created new connection for host: ", host);
        }
        return handle;
    }
    
    static void releaseConnection(const std::string& host, CURL* handle) {
        if (!handle) return;
        
        std::lock_guard<std::mutex> lock(s_pool_mutex);
        
        auto& connections = s_connection_pool[host];
        if (connections.size() < MAX_CONNECTIONS_PER_HOST) {
            // Reset handle for reuse
            curl_easy_reset(handle);
            connections.push_back(handle);
            Debug::log("http", "CurlLifecycleManager: Released connection to pool for host: ", host);
        } else {
            // Pool is full, cleanup the handle
            curl_easy_cleanup(handle);
            Debug::log("http", "CurlLifecycleManager: Pool full, cleaned up connection for host: ", host);
        }
    }
    
    static void cleanupConnectionPool() {
        std::lock_guard<std::mutex> lock(s_pool_mutex);
        
        for (auto& [host, connections] : s_connection_pool) {
            for (CURL* handle : connections) {
                curl_easy_cleanup(handle);
            }
            connections.clear();
        }
        s_connection_pool.clear();
        Debug::log("http", "CurlLifecycleManager: Connection pool cleaned up");
    }
    
private:
    static void performCleanupIfNeeded() {
        std::lock_guard<std::mutex> lock(s_cleanup_mutex);
        if (s_active_handles.load() == 0 && s_initialized.load()) {
            // No active handles, safe to cleanup
            cleanupConnectionPool();
            curl_global_cleanup();
            s_initialized.store(false);
            Debug::log("http", "CurlLifecycleManager: libcurl cleanup completed (no active handles)");
        }
    }
    
    static void cleanupExpiredConnections() {
        // This method assumes s_pool_mutex is already locked
        // For now, we'll keep connections alive for the duration of the program
        // In a more sophisticated implementation, we could track connection timestamps
        // and clean up connections that haven't been used recently
        Debug::log("http", "CurlLifecycleManager: Expired connection cleanup (placeholder)");
    }
};

std::atomic<bool> CurlLifecycleManager::s_initialized{false};
std::once_flag CurlLifecycleManager::s_init_flag;
std::once_flag CurlLifecycleManager::s_cleanup_flag;
std::atomic<int> CurlLifecycleManager::s_active_handles{0};
std::mutex CurlLifecycleManager::s_cleanup_mutex;
std::mutex CurlLifecycleManager::s_pool_mutex;
std::unordered_map<std::string, std::vector<CURL*>> CurlLifecycleManager::s_connection_pool;
std::chrono::steady_clock::time_point CurlLifecycleManager::s_last_pool_cleanup = std::chrono::steady_clock::now();
static CurlLifecycleManager s_curl_manager;

// C-style callback functions for libcurl
static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    
    // Set maximum response size to prevent memory exhaustion
    const size_t MAX_RESPONSE_SIZE = 100 * 1024 * 1024; // 100MB limit
    
    // Check if adding this data would exceed our limit
    if (buffer->size() + realsize > MAX_RESPONSE_SIZE) {
        Debug::log("http", "HTTPClient: Response size limit exceeded, truncating");
        return 0; // Signal error to libcurl
    }
    
    try {
        buffer->append(static_cast<const char*>(contents), realsize);
    } catch (const std::bad_alloc&) {
        Debug::log("http", "HTTPClient: Memory allocation failed during response processing");
        return 0; // Signal error to libcurl
    }
    
    return realsize;
}

static size_t headerCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    
    // Limit header size to prevent abuse
    const size_t MAX_HEADER_SIZE = 1024 * 1024; // 1MB limit for headers
    if (buffer->size() + realsize > MAX_HEADER_SIZE) {
        Debug::log("http", "HTTPClient: Header size limit exceeded, truncating");
        return 0; // Signal error to libcurl
    }
    
    try {
        buffer->append(static_cast<const char*>(contents), realsize);
    } catch (const std::bad_alloc&) {
        Debug::log("http", "HTTPClient: Memory allocation failed during header processing");
        return 0; // Signal error to libcurl
    }
    
    return realsize;
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
    
    // Extract host from URL for connection pooling
    std::string host;
    int port;
    std::string path;
    bool isHttps;
    if (!parseURL(url, host, port, path, isHttps)) {
        response.statusMessage = "Failed to parse URL";
        return response;
    }
    
    // Thread-safe connection acquisition
    CURL *curl = CurlLifecycleManager::acquireConnection(host);
    if (!curl) {
        response.statusMessage = "Failed to acquire curl handle";
        return response;
    }

    // Track active handle for proper cleanup
    CurlLifecycleManager::incrementHandleCount();
    
    // RAII wrapper for curl handle cleanup with connection pooling
    struct CurlHandleGuard {
        CURL* handle;
        struct curl_slist* headers;
        std::string host;
        
        CurlHandleGuard(CURL* h, const std::string& host_name) : handle(h), headers(nullptr), host(host_name) {}
        
        ~CurlHandleGuard() {
            if (headers) {
                curl_slist_free_all(headers);
                headers = nullptr;
            }
            if (handle) {
                // Return connection to pool instead of destroying it
                CurlLifecycleManager::releaseConnection(host, handle);
                handle = nullptr;
            }
            CurlLifecycleManager::decrementHandleCount();
        }
        
        // Disable copy
        CurlHandleGuard(const CurlHandleGuard&) = delete;
        CurlHandleGuard& operator=(const CurlHandleGuard&) = delete;
    } guard(curl, host);

    // Use bounded buffers to prevent excessive memory usage
    std::string readBuffer;
    std::string headerBuffer;
    
    // Reserve reasonable initial capacity to avoid frequent reallocations
    readBuffer.reserve(64 * 1024);  // 64KB initial capacity
    headerBuffer.reserve(8 * 1024); // 8KB for headers
    


    // Basic options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeoutSeconds));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L); // 10 second connect timeout
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerBuffer);
    
    // Security and protocol options
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PsyMP3/3.0");
    
    // Accept encoding for compression
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");
    
    // Set maximum receive buffer size to prevent excessive memory usage
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 64L * 1024L); // 64KB buffer
    
    // Method-specific options
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(postData.length()));
    } else if (method == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }

    // Set custom headers with proper cleanup tracking
    for (const auto& header : headers) {
        std::string headerStr = header.first + ": " + header.second;
        guard.headers = curl_slist_append(guard.headers, headerStr.c_str());
    }
    if (guard.headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, guard.headers);
    }

    // Perform the request with exception safety
    CURLcode res;
    try {
        res = curl_easy_perform(curl);
    } catch (const std::exception& e) {
        Debug::log("http", "HTTPClient: Exception during curl_easy_perform: ", e.what());
        response.statusMessage = std::string("Exception during HTTP request: ") + e.what();
        response.success = false;
        return response;
    } catch (...) {
        Debug::log("http", "HTTPClient: Unknown exception during curl_easy_perform");
        response.statusMessage = "Unknown exception during HTTP request";
        response.success = false;
        return response;
    }

    if (res != CURLE_OK) {
        response.statusMessage = std::string("libcurl error: ") + curl_easy_strerror(res);
        response.success = false;
    } else {
        try {
            // Get response info
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            response.statusCode = static_cast<int>(http_code);
            response.body = std::move(readBuffer);
            response.success = (response.statusCode >= 200 && response.statusCode < 400);
            
            // Parse headers with exception safety
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
                    
                    // Limit number of headers to prevent abuse
                    if (response.headers.size() < 100) {
                        response.headers[name] = value;
                    }
                }
            }
            
            // Set status message for non-success codes
            if (!response.success) {
                response.statusMessage = "HTTP " + std::to_string(response.statusCode);
            }
            
        } catch (const std::exception& e) {
            Debug::log("http", "HTTPClient: Exception during response processing: ", e.what());
            response.statusMessage = std::string("Exception during response processing: ") + e.what();
            response.success = false;
        } catch (...) {
            Debug::log("http", "HTTPClient: Unknown exception during response processing");
            response.statusMessage = "Unknown exception during response processing";
            response.success = false;
        }
    }

    // Cleanup is handled automatically by CurlHandleGuard destructor
    return response;
}

std::string HTTPClient::urlEncode(const std::string& input) {
    if (input.empty()) {
        return "";
    }

    if (CurlLifecycleManager::isInitialized()) {
        CURL *curl = curl_easy_init();
        if (curl) {
            char *output = curl_easy_escape(curl, input.c_str(), static_cast<int>(input.length()));
            if (output) {
                std::string result(output);
                curl_free(output);
                curl_easy_cleanup(curl);
                return result;
            }
            curl_easy_cleanup(curl);
        }
    }
    
    // Fallback if curl is not initialized, failed to init, or failed to escape.
    // Use a safe, manual encoding to avoid security risks of returning unencoded input.
    Debug::log("http", "HTTPClient::urlEncode() - libcurl encoding failed or unavailable, using fallback encoder");
    
    std::string result;
    result.reserve(input.length() * 3);
    for (unsigned char c : input) {
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            c == '-' || c == '.' || c == '_' || c == '~') {
            result += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            result += buf;
        }
    }
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
    
    // Validate scheme
    if (scheme != "http" && scheme != "https") {
        Debug::log("http", "HTTPClient::parseURL() - Unsupported scheme: ", scheme);
        return false;
    }
    
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
    // Thread-safe cleanup of connection pool and libcurl resources
    CurlLifecycleManager::cleanupConnectionPool();
    CurlLifecycleManager::forceCleanup();
    Debug::log("http", "HTTPClient::closeAllConnections() - Thread-safe cleanup of all HTTP connections");
}

void HTTPClient::setConnectionTimeout(int timeout_seconds) {
    // This would be stored in a static variable for use in performRequest
    // For now, we use the default 10 second connect timeout
    Debug::log("http", "HTTPClient::setConnectionTimeout() - Connection timeout set to ", timeout_seconds, " seconds");
}

std::map<std::string, int> HTTPClient::getConnectionPoolStats() {
    // Return thread-safe stats for libcurl implementation with connection pooling
    std::map<std::string, int> stats;
    stats["active_handles"] = CurlLifecycleManager::s_active_handles.load();
    stats["initialized"] = CurlLifecycleManager::isInitialized() ? 1 : 0;
    
    // Thread-safe access to connection pool stats
    {
        std::lock_guard<std::mutex> lock(CurlLifecycleManager::s_pool_mutex);
        int total_pooled_connections = 0;
        for (const auto& [host, connections] : CurlLifecycleManager::s_connection_pool) {
            total_pooled_connections += static_cast<int>(connections.size());
        }
        stats["pooled_connections"] = total_pooled_connections;
        stats["pool_hosts"] = static_cast<int>(CurlLifecycleManager::s_connection_pool.size());
    }
    
    return stats;
}

void HTTPClient::initializeSSL() {
    // libcurl handles SSL initialization automatically
    Debug::log("http", "HTTPClient::initializeSSL() - SSL initialization handled by libcurl");
}

void HTTPClient::cleanupSSL() {
    // Force cleanup of libcurl resources which includes SSL cleanup
    CurlLifecycleManager::forceCleanup();
    Debug::log("http", "HTTPClient::cleanupSSL() - SSL cleanup handled by libcurl cleanup");
}

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3

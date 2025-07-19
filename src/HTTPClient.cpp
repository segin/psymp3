/*
 * HTTPClient.cpp - libcurl-based HTTP client implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <curl/curl.h>

// RAII wrapper for global curl initialization
class CurlLifecycleManager {
public:
    CurlLifecycleManager() {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    ~CurlLifecycleManager() {
        curl_global_cleanup();
    }
};
static CurlLifecycleManager s_curl_manager;

// Callback function for libcurl to write received data into a std::string
static size_t write_callback(void *contents, size_t size, size_t nmemb, std::string *s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
    } catch(std::bad_alloc &e) {
        // Handle memory problem
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
    CURL *curl = curl_easy_init();
    if (!curl) {
        response.statusMessage = "Failed to initialize curl";
        return response;
    }

    std::string readBuffer;
    std::string headerBuffer;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerBuffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    } else if (method == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }

    struct curl_slist *chunk = nullptr;
    for (const auto& header : headers) {
        chunk = curl_slist_append(chunk, (header.first + ": " + header.second).c_str());
    }
    if (chunk) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        response.statusMessage = curl_easy_strerror(res);
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        response.statusCode = static_cast<int>(http_code);
        response.body = readBuffer;
        response.success = (response.statusCode >= 200 && response.statusCode < 300);
        
        // Parse headers
        std::istringstream headerStream(headerBuffer);
        std::string headerLine;
        while (std::getline(headerStream, headerLine)) {
            if (headerLine.back() == '\r') {
                headerLine.pop_back();
            }
            size_t colonPos = headerLine.find(':');
            if (colonPos != std::string::npos) {
                std::string name = headerLine.substr(0, colonPos);
                std::string value = headerLine.substr(colonPos + 2);
                response.headers[name] = value;
            }
        }
    }

    if (chunk) {
        curl_slist_free_all(chunk);
    }
    curl_easy_cleanup(curl);

    return response;
}

std::string HTTPClient::urlEncode(const std::string& input) {
    CURL *curl = curl_easy_init();
    if (curl) {
        char *output = curl_easy_escape(curl, input.c_str(), input.length());
        if (output) {
            std::string result(output);
            curl_free(output);
            curl_easy_cleanup(curl);
            return result;
        }
    }
    return ""; // Fallback
}

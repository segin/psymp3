# IOHandler Subsystem Developer Guide

## Table of Contents

1. [Introduction](#introduction)
2. [Architecture Overview](#architecture-overview)
3. [Implementing New IOHandler Types](#implementing-new-iohandler-types)
4. [HTTPClient Usage Patterns](#httpclient-usage-patterns)
5. [Performance Optimization Techniques](#performance-optimization-techniques)
6. [Troubleshooting Common Issues](#troubleshooting-common-issues)
7. [Testing Guidelines](#testing-guidelines)
8. [Integration Patterns](#integration-patterns)
9. [Best Practices](#best-practices)
10. [Advanced Topics](#advanced-topics)

## Introduction

This developer guide provides comprehensive information for extending and maintaining the IOHandler subsystem in PsyMP3. Whether you're implementing a new I/O handler type, optimizing performance, or troubleshooting issues, this guide covers the essential patterns and practices.

### Prerequisites

- Understanding of C++ inheritance and virtual functions
- Familiarity with PsyMP3 architecture and conventions
- Knowledge of thread safety concepts
- Basic understanding of network protocols (for HTTP-related work)

### Design Principles

The IOHandler subsystem follows these key principles:

1. **Unified Interface**: All I/O sources present the same interface
2. **Thread Safety**: All operations are safe for concurrent access
3. **Performance**: Optimized for media streaming workloads
4. **Extensibility**: Easy to add new I/O source types
5. **Error Resilience**: Graceful handling of failures and recovery

## Architecture Overview

### Class Hierarchy

```
IOHandler (abstract base)
├── FileIOHandler (local files)
├── HTTPIOHandler (HTTP streams)
└── [Your Custom Handler]
```

### Key Components

- **IOHandler**: Abstract base class defining the interface
- **FileIOHandler**: Concrete implementation for local files
- **HTTPIOHandler**: Concrete implementation for HTTP streams
- **HTTPClient**: HTTP client foundation for network operations
- **Buffer Pools**: Shared memory management for efficiency

### Integration Points

- **MediaFactory**: Creates appropriate IOHandler based on URI
- **Demuxers**: Use IOHandler for reading media data
- **Debug System**: Integrated logging and error reporting
- **Exception System**: Uses PsyMP3's exception hierarchy

## Implementing New IOHandler Types

### Step 1: Define Your Handler Class

Create a new header file for your custom IOHandler:

```cpp
// include/MyCustomIOHandler.h
#ifndef MYCUSTOMIOHANDLER_H
#define MYCUSTOMIOHANDLER_H

/**
 * @brief Custom IOHandler for [describe your source type]
 * 
 * This class provides access to [describe functionality]
 * with [list key features].
 */
class MyCustomIOHandler : public IOHandler {
public:
    /**
     * @brief Constructor for MyCustomIOHandler
     * @param source_identifier Identifier for the source (URL, path, etc.)
     */
    explicit MyCustomIOHandler(const std::string& source_identifier);
    
    /**
     * @brief Destructor - cleanup resources
     */
    ~MyCustomIOHandler() override;
    
    // IOHandler interface implementation
    size_t read(void* buffer, size_t size, size_t count) override;
    int seek(off_t offset, int whence) override;
    off_t tell() override;
    int close() override;
    bool eof() override;
    off_t getFileSize() override;
    
    // Custom-specific methods
    bool isConnected() const { return m_connected; }
    std::string getSourceInfo() const { return m_source_info; }

private:
    std::string m_source_identifier;
    bool m_connected = false;
    std::string m_source_info;
    
    // Thread safety
    mutable std::mutex m_source_mutex;
    mutable std::shared_mutex m_buffer_mutex;
    
    // Buffering system
    std::vector<uint8_t> m_buffer;
    size_t m_buffer_offset = 0;
    off_t m_buffer_start_position = 0;
    
    // Private implementation methods
    bool initialize();
    bool fillBuffer(off_t position, size_t min_size);
    size_t readFromBuffer(void* buffer, size_t bytes_requested);
    void cleanup();
};

#endif // MYCUSTOMIOHANDLER_H
```

### Step 2: Implement Core Methods

```cpp
// src/MyCustomIOHandler.cpp
#include "psymp3.h"

MyCustomIOHandler::MyCustomIOHandler(const std::string& source_identifier)
    : m_source_identifier(source_identifier) {
    
    Debug::print("io", "Creating MyCustomIOHandler for: %s", 
                source_identifier.c_str());
    
    try {
        if (!initialize()) {
            throw InvalidMediaException("Failed to initialize custom handler: " + 
                                      source_identifier);
        }
    } catch (const std::exception& e) {
        Debug::print("io", "MyCustomIOHandler initialization failed: %s", e.what());
        throw;
    }
}

MyCustomIOHandler::~MyCustomIOHandler() {
    Debug::print("io", "Destroying MyCustomIOHandler");
    cleanup();
}

size_t MyCustomIOHandler::read(void* buffer, size_t size, size_t count) {
    if (!buffer || size == 0 || count == 0) {
        updateErrorState(EINVAL, "Invalid read parameters");
        return 0;
    }
    
    if (m_closed.load()) {
        updateErrorState(EBADF, "Handler is closed");
        return 0;
    }
    
    if (m_eof.load()) {
        return 0;
    }
    
    std::shared_lock<std::shared_mutex> buffer_lock(m_buffer_mutex);
    
    size_t bytes_requested = size * count;
    size_t bytes_read = 0;
    
    try {
        // Try to read from buffer first
        bytes_read = readFromBuffer(buffer, bytes_requested);
        
        // If buffer doesn't have enough data, fill it
        if (bytes_read < bytes_requested && !m_eof.load()) {
            buffer_lock.unlock();
            
            if (fillBuffer(m_position.load(), bytes_requested - bytes_read)) {
                buffer_lock.lock();
                bytes_read += readFromBuffer(
                    static_cast<char*>(buffer) + bytes_read, 
                    bytes_requested - bytes_read);
            }
        }
        
        // Update position
        updatePosition(m_position.load() + bytes_read);
        
    } catch (const std::exception& e) {
        Debug::print("io", "Read error in MyCustomIOHandler: %s", e.what());
        updateErrorState(EIO, e.what());
        return 0;
    }
    
    return bytes_read / size;
}

int MyCustomIOHandler::seek(off_t offset, int whence) {
    std::lock_guard<std::mutex> source_lock(m_source_mutex);
    
    if (m_closed.load()) {
        updateErrorState(EBADF, "Handler is closed");
        return -1;
    }
    
    off_t new_position;
    off_t current_pos = m_position.load();
    off_t file_size = getFileSize();
    
    switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position = current_pos + offset;
            break;
        case SEEK_END:
            if (file_size < 0) {
                updateErrorState(ESPIPE, "Cannot seek from end - size unknown");
                return -1;
            }
            new_position = file_size + offset;
            break;
        default:
            updateErrorState(EINVAL, "Invalid whence parameter");
            return -1;
    }
    
    if (new_position < 0) {
        updateErrorState(EINVAL, "Seek position cannot be negative");
        return -1;
    }
    
    if (file_size >= 0 && new_position > file_size) {
        updateErrorState(EINVAL, "Seek position beyond end of source");
        return -1;
    }
    
    // Update position and clear EOF if we're not at the end
    updatePosition(new_position);
    if (file_size < 0 || new_position < file_size) {
        updateEofState(false);
    }
    
    // Invalidate buffer if seek moves outside buffered range
    std::lock_guard<std::shared_mutex> buffer_lock(m_buffer_mutex);
    if (new_position < m_buffer_start_position || 
        new_position >= m_buffer_start_position + static_cast<off_t>(m_buffer.size())) {
        m_buffer.clear();
        m_buffer_offset = 0;
        m_buffer_start_position = -1;
    } else {
        // Update buffer offset for new position
        m_buffer_offset = static_cast<size_t>(new_position - m_buffer_start_position);
    }
    
    return 0;
}

off_t MyCustomIOHandler::tell() {
    return m_position.load();
}

int MyCustomIOHandler::close() {
    std::lock_guard<std::mutex> source_lock(m_source_mutex);
    
    if (m_closed.load()) {
        return 0; // Already closed
    }
    
    cleanup();
    updateClosedState(true);
    
    Debug::print("io", "MyCustomIOHandler closed successfully");
    return 0;
}

bool MyCustomIOHandler::eof() {
    return m_eof.load();
}

off_t MyCustomIOHandler::getFileSize() {
    // Implement based on your source type
    // Return -1 if size is unknown
    return -1;
}
```

### Step 3: Implement Private Helper Methods

```cpp
bool MyCustomIOHandler::initialize() {
    try {
        // Initialize your custom source
        // Set up connections, validate parameters, etc.
        
        m_connected = true;
        m_source_info = "Custom source: " + m_source_identifier;
        
        Debug::print("io", "MyCustomIOHandler initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::print("io", "MyCustomIOHandler initialization failed: %s", e.what());
        return false;
    }
}

bool MyCustomIOHandler::fillBuffer(off_t position, size_t min_size) {
    std::lock_guard<std::shared_mutex> buffer_lock(m_buffer_mutex);
    
    try {
        // Determine buffer size (adaptive based on access patterns)
        size_t buffer_size = std::max(min_size, static_cast<size_t>(64 * 1024));
        
        // Allocate buffer
        m_buffer.resize(buffer_size);
        
        // Fill buffer with data from your source
        // This is where you implement the actual data retrieval
        size_t bytes_filled = 0; // Replace with actual implementation
        
        if (bytes_filled == 0) {
            updateEofState(true);
            return false;
        }
        
        m_buffer.resize(bytes_filled);
        m_buffer_offset = 0;
        m_buffer_start_position = position;
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::print("io", "Buffer fill failed: %s", e.what());
        return false;
    }
}

size_t MyCustomIOHandler::readFromBuffer(void* buffer, size_t bytes_requested) {
    if (m_buffer.empty() || m_buffer_offset >= m_buffer.size()) {
        return 0;
    }
    
    size_t available = m_buffer.size() - m_buffer_offset;
    size_t to_copy = std::min(bytes_requested, available);
    
    std::memcpy(buffer, m_buffer.data() + m_buffer_offset, to_copy);
    m_buffer_offset += to_copy;
    
    return to_copy;
}

void MyCustomIOHandler::cleanup() {
    // Clean up your custom resources
    m_connected = false;
    m_buffer.clear();
    m_buffer_offset = 0;
    m_buffer_start_position = -1;
}
```

### Step 4: Integration with MediaFactory

Add your handler to the MediaFactory:

```cpp
// In MediaFactory.cpp
std::unique_ptr<IOHandler> MediaFactory::createIOHandler(const std::string& uri) {
    if (uri.substr(0, 7) == "http://" || uri.substr(0, 8) == "https://") {
        return std::make_unique<HTTPIOHandler>(uri);
    } else if (uri.substr(0, 9) == "custom://") {
        return std::make_unique<MyCustomIOHandler>(uri);
    } else {
        return std::make_unique<FileIOHandler>(TagLib::String(uri.c_str()));
    }
}
```

## HTTPClient Usage Patterns

### Basic HTTP Operations

```cpp
// Simple GET request
HTTPClient::Response response = HTTPClient::get("https://api.example.com/data");
if (response.success) {
    processData(response.body);
} else {
    Debug::print("http", "Request failed: %d %s", 
                response.statusCode, response.statusMessage.c_str());
}

// POST with custom headers
std::map<std::string, std::string> headers;
headers["Authorization"] = "Bearer " + auth_token;
headers["Content-Type"] = "application/json";

HTTPClient::Response post_response = HTTPClient::post(
    "https://api.example.com/submit",
    json_data,
    "application/json",
    headers,
    60  // 60 second timeout
);
```

### Range Requests for Streaming

```cpp
// Download specific byte ranges
class StreamingDownloader {
private:
    std::string m_url;
    long m_content_length = -1;
    
public:
    explicit StreamingDownloader(const std::string& url) : m_url(url) {
        // Get content length with HEAD request
        HTTPClient::Response head_response = HTTPClient::head(url);
        if (head_response.success) {
            auto it = head_response.headers.find("content-length");
            if (it != head_response.headers.end()) {
                m_content_length = std::stol(it->second);
            }
        }
    }
    
    std::vector<uint8_t> downloadRange(long start, long end) {
        HTTPClient::Response response = HTTPClient::getRange(m_url, start, end);
        
        if (response.success && response.statusCode == 206) {
            return std::vector<uint8_t>(response.body.begin(), response.body.end());
        }
        
        return {};
    }
    
    void downloadInChunks(size_t chunk_size = 1024 * 1024) {
        if (m_content_length <= 0) return;
        
        for (long pos = 0; pos < m_content_length; pos += chunk_size) {
            long end = std::min(pos + static_cast<long>(chunk_size) - 1, 
                               m_content_length - 1);
            
            auto chunk = downloadRange(pos, end);
            if (!chunk.empty()) {
                processChunk(chunk, pos);
            }
        }
    }
};
```

### Connection Pool Management

```cpp
// Monitor connection pool performance
void monitorHttpPerformance() {
    auto stats = HTTPClient::getConnectionPoolStats();
    
    Debug::print("http", "Active connections: %d", stats["active_connections"]);
    Debug::print("http", "Total requests: %d", stats["total_requests"]);
    Debug::print("http", "Connection reuse rate: %.2f%%", 
                (stats["reused_connections"] * 100.0) / stats["total_requests"]);
    
    // Adjust timeout based on performance
    if (stats["timeout_errors"] > stats["total_requests"] * 0.1) {
        HTTPClient::setConnectionTimeout(60); // Increase timeout
    }
}

// Clean up connections when needed
void cleanupHttpResources() {
    HTTPClient::closeAllConnections();
    Debug::print("http", "All HTTP connections closed");
}
```

### Error Handling Patterns

```cpp
// Robust HTTP request with retry logic
HTTPClient::Response robustHttpRequest(const std::string& url, int max_retries = 3) {
    HTTPClient::Response response;
    
    for (int attempt = 0; attempt <= max_retries; ++attempt) {
        response = HTTPClient::get(url);
        
        if (response.success) {
            return response;
        }
        
        // Check if error is retryable
        bool retryable = false;
        if (response.statusCode >= 500 && response.statusCode < 600) {
            retryable = true; // Server errors
        } else if (response.statusCode == 429) {
            retryable = true; // Rate limited
        } else if (response.statusCode == 0) {
            retryable = true; // Network error
        }
        
        if (!retryable || attempt == max_retries) {
            break;
        }
        
        // Exponential backoff
        int delay_ms = 1000 * (1 << attempt);
        Debug::print("http", "Request failed, retrying in %dms (attempt %d/%d)", 
                    delay_ms, attempt + 1, max_retries);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    
    return response;
}
```

## Performance Optimization Techniques

### Buffer Size Optimization

```cpp
class AdaptiveBufferManager {
private:
    size_t m_current_buffer_size = 64 * 1024;
    size_t m_min_buffer_size = 8 * 1024;
    size_t m_max_buffer_size = 1024 * 1024;
    
    // Performance tracking
    std::chrono::steady_clock::time_point m_last_measurement;
    size_t m_bytes_processed = 0;
    double m_throughput = 0.0;
    
public:
    size_t getOptimalBufferSize(off_t file_size, bool sequential_access) {
        // Adjust based on file size
        if (file_size > 0) {
            if (file_size < 1024 * 1024) {
                // Small files: use smaller buffers
                return std::max(m_min_buffer_size, static_cast<size_t>(file_size / 8));
            } else if (file_size > 100 * 1024 * 1024) {
                // Large files: use larger buffers
                return m_max_buffer_size;
            }
        }
        
        // Adjust based on access pattern
        if (sequential_access) {
            return std::min(m_max_buffer_size, m_current_buffer_size * 2);
        }
        
        return m_current_buffer_size;
    }
    
    void updatePerformanceMetrics(size_t bytes_processed, 
                                 std::chrono::milliseconds duration) {
        m_bytes_processed += bytes_processed;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_last_measurement);
        
        if (elapsed.count() > 1000) { // Update every second
            m_throughput = (m_bytes_processed * 1000.0) / elapsed.count();
            
            // Adjust buffer size based on throughput
            if (m_throughput < 1024 * 1024) { // < 1MB/s
                m_current_buffer_size = std::max(m_min_buffer_size, 
                                                m_current_buffer_size / 2);
            } else if (m_throughput > 10 * 1024 * 1024) { // > 10MB/s
                m_current_buffer_size = std::min(m_max_buffer_size, 
                                                m_current_buffer_size * 2);
            }
            
            m_last_measurement = now;
            m_bytes_processed = 0;
        }
    }
};
```

### Memory Pool Integration

```cpp
class OptimizedIOHandler : public IOHandler {
private:
    // Use buffer pools for efficient memory management
    struct BufferPool {
        static constexpr size_t POOL_SIZE = 16;
        static constexpr size_t BUFFER_SIZE = 64 * 1024;
        
        std::array<std::vector<uint8_t>, POOL_SIZE> buffers;
        std::bitset<POOL_SIZE> available;
        std::mutex pool_mutex;
        
        BufferPool() {
            available.set(); // All buffers initially available
            for (auto& buffer : buffers) {
                buffer.reserve(BUFFER_SIZE);
            }
        }
        
        std::vector<uint8_t>* acquire() {
            std::lock_guard<std::mutex> lock(pool_mutex);
            for (size_t i = 0; i < POOL_SIZE; ++i) {
                if (available[i]) {
                    available[i] = false;
                    buffers[i].clear();
                    return &buffers[i];
                }
            }
            return nullptr; // Pool exhausted
        }
        
        void release(std::vector<uint8_t>* buffer) {
            std::lock_guard<std::mutex> lock(pool_mutex);
            for (size_t i = 0; i < POOL_SIZE; ++i) {
                if (&buffers[i] == buffer) {
                    available[i] = true;
                    break;
                }
            }
        }
    };
    
    static BufferPool s_buffer_pool;
    std::vector<uint8_t>* m_buffer = nullptr;
    
public:
    OptimizedIOHandler() {
        m_buffer = s_buffer_pool.acquire();
        if (!m_buffer) {
            // Fallback to regular allocation
            m_buffer = new std::vector<uint8_t>();
            m_buffer->reserve(64 * 1024);
        }
    }
    
    ~OptimizedIOHandler() {
        if (m_buffer) {
            // Try to return to pool first
            bool returned_to_pool = false;
            for (size_t i = 0; i < BufferPool::POOL_SIZE; ++i) {
                if (&s_buffer_pool.buffers[i] == m_buffer) {
                    s_buffer_pool.release(m_buffer);
                    returned_to_pool = true;
                    break;
                }
            }
            
            if (!returned_to_pool) {
                delete m_buffer;
            }
        }
    }
};
```

### Access Pattern Detection

```cpp
class AccessPatternAnalyzer {
private:
    struct AccessRecord {
        off_t position;
        size_t size;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    std::deque<AccessRecord> m_access_history;
    static constexpr size_t MAX_HISTORY = 10;
    
    bool m_sequential_detected = false;
    off_t m_predicted_next_position = -1;
    
public:
    void recordAccess(off_t position, size_t size) {
        auto now = std::chrono::steady_clock::now();
        
        m_access_history.push_back({position, size, now});
        if (m_access_history.size() > MAX_HISTORY) {
            m_access_history.pop_front();
        }
        
        analyzePattern();
    }
    
    bool isSequentialAccess() const {
        return m_sequential_detected;
    }
    
    off_t getPredictedNextPosition() const {
        return m_predicted_next_position;
    }
    
    size_t getOptimalReadAheadSize() const {
        if (!m_sequential_detected) return 0;
        
        // Calculate average read size
        size_t total_size = 0;
        for (const auto& record : m_access_history) {
            total_size += record.size;
        }
        
        size_t avg_size = total_size / m_access_history.size();
        return std::min(static_cast<size_t>(256 * 1024), avg_size * 4);
    }
    
private:
    void analyzePattern() {
        if (m_access_history.size() < 3) return;
        
        // Check for sequential access
        bool sequential = true;
        off_t expected_position = m_access_history[0].position + m_access_history[0].size;
        
        for (size_t i = 1; i < m_access_history.size(); ++i) {
            if (std::abs(m_access_history[i].position - expected_position) > 1024) {
                sequential = false;
                break;
            }
            expected_position = m_access_history[i].position + m_access_history[i].size;
        }
        
        m_sequential_detected = sequential;
        if (sequential) {
            m_predicted_next_position = expected_position;
        }
    }
};
```

## Troubleshooting Common Issues

### File Access Problems

#### Issue: Unicode Filename Handling

**Symptoms:**
- Files with non-ASCII names fail to open
- Different behavior on Windows vs Unix

**Diagnosis:**
```cpp
void diagnoseUnicodeIssue(const TagLib::String& path) {
    Debug::print("io", "Original path: %s", path.to8Bit(true).c_str());
    Debug::print("io", "UTF-8 path: %s", path.to8Bit(false).c_str());
    
#ifdef _WIN32
    Debug::print("io", "Windows wide path length: %zu", path.toCWString().length());
#endif
    
    // Test file existence
    struct stat file_stat;
    int result = stat(path.toCString(false), &file_stat);
    Debug::print("io", "stat() result: %d (errno: %d)", result, errno);
}
```

**Solution:**
- Ensure TagLib::String is constructed with proper encoding
- Use `toCWString()` on Windows, `toCString(false)` on Unix
- Validate file paths before passing to IOHandler

#### Issue: Large File Support

**Symptoms:**
- Files >2GB report incorrect sizes
- Seeking fails on large files

**Diagnosis:**
```cpp
void diagnoseLargeFileSupport(const std::string& path) {
    struct stat file_stat;
    if (stat(path.c_str(), &file_stat) == 0) {
        Debug::print("io", "File size (stat): %lld", (long long)file_stat.st_size);
        Debug::print("io", "off_t size: %zu bytes", sizeof(off_t));
        
        FILE* file = fopen(path.c_str(), "rb");
        if (file) {
            fseeko(file, 0, SEEK_END);
            off_t size = ftello(file);
            Debug::print("io", "File size (ftello): %lld", (long long)size);
            fclose(file);
        }
    }
}
```

**Solution:**
- Ensure 64-bit file operations are used (`fseeko`/`ftello`)
- Use `off_t` for all position and size variables
- Enable large file support in build system

### Network Issues

#### Issue: HTTP Connection Failures

**Symptoms:**
- Intermittent connection failures
- Slow response times
- SSL/TLS errors

**Diagnosis:**
```cpp
void diagnoseHttpIssue(const std::string& url) {
    Debug::print("http", "Testing URL: %s", url.c_str());
    
    // Test basic connectivity
    HTTPClient::Response response = HTTPClient::head(url);
    Debug::print("http", "HEAD response: %d %s", 
                response.statusCode, response.statusMessage.c_str());
    
    // Check headers
    for (const auto& header : response.headers) {
        Debug::print("http", "Header: %s = %s", 
                    header.first.c_str(), header.second.c_str());
    }
    
    // Test range support
    HTTPClient::Response range_response = HTTPClient::getRange(url, 0, 1023);
    Debug::print("http", "Range response: %d", range_response.statusCode);
    
    // Connection pool stats
    auto stats = HTTPClient::getConnectionPoolStats();
    Debug::print("http", "Pool stats - Active: %d, Total: %d", 
                stats["active_connections"], stats["total_requests"]);
}
```

**Solution:**
- Check network connectivity and DNS resolution
- Verify SSL certificate validity
- Adjust timeout settings
- Monitor connection pool usage

#### Issue: Range Request Problems

**Symptoms:**
- Seeking doesn't work in HTTP streams
- Server returns 416 Range Not Satisfiable

**Diagnosis:**
```cpp
void diagnoseRangeRequests(const std::string& url) {
    HTTPClient::Response head_response = HTTPClient::head(url);
    
    auto accept_ranges = head_response.headers.find("accept-ranges");
    if (accept_ranges != head_response.headers.end()) {
        Debug::print("http", "Accept-Ranges: %s", accept_ranges->second.c_str());
    } else {
        Debug::print("http", "No Accept-Ranges header found");
    }
    
    auto content_length = head_response.headers.find("content-length");
    if (content_length != head_response.headers.end()) {
        long length = std::stol(content_length->second);
        Debug::print("http", "Content-Length: %ld", length);
        
        // Test range request
        HTTPClient::Response range_response = HTTPClient::getRange(url, 0, 1023);
        Debug::print("http", "Range request result: %d", range_response.statusCode);
        
        if (range_response.statusCode == 206) {
            Debug::print("http", "Range requests supported");
        } else {
            Debug::print("http", "Range requests not supported or failed");
        }
    }
}
```

**Solution:**
- Check server's Accept-Ranges header
- Validate range request parameters
- Implement fallback for servers without range support

### Memory Issues

#### Issue: Memory Leaks

**Symptoms:**
- Increasing memory usage over time
- Out of memory errors during long playback

**Diagnosis:**
```cpp
void diagnoseMemoryLeaks() {
    auto stats = IOHandler::getMemoryStats();
    
    Debug::print("memory", "Total IOHandler memory: %zu bytes", 
                stats["total_memory"]);
    Debug::print("memory", "Active handlers: %zu", 
                stats["active_handlers"]);
    Debug::print("memory", "Buffer pool usage: %zu bytes", 
                stats["buffer_pool_usage"]);
    Debug::print("memory", "Peak memory usage: %zu bytes", 
                stats["peak_memory"]);
    
    // Check for handler leaks
    if (stats["active_handlers"] > expected_handler_count) {
        Debug::print("memory", "WARNING: Possible handler leak detected");
    }
    
    // Check buffer pool efficiency
    double pool_efficiency = static_cast<double>(stats["pool_hits"]) / 
                           (stats["pool_hits"] + stats["pool_misses"]);
    Debug::print("memory", "Buffer pool efficiency: %.2f%%", pool_efficiency * 100);
}
```

**Solution:**
- Use RAII patterns consistently
- Monitor buffer pool usage
- Implement bounded caches
- Regular memory cleanup

### Thread Safety Issues

#### Issue: Race Conditions

**Symptoms:**
- Intermittent crashes or data corruption
- Inconsistent behavior in multi-threaded scenarios

**Diagnosis:**
```cpp
void diagnoseThreadSafety() {
    // Enable thread safety debugging
    Debug::print("thread", "Thread safety diagnosis enabled");
    
    // Test concurrent access
    auto handler = std::make_unique<FileIOHandler>("test.mp3");
    
    std::vector<std::thread> threads;
    std::atomic<int> error_count{0};
    
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&handler, &error_count, i]() {
            char buffer[1024];
            for (int j = 0; j < 100; ++j) {
                size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
                if (bytes_read == 0 && !handler->eof()) {
                    error_count++;
                    Debug::print("thread", "Thread %d: Read error at iteration %d", i, j);
                }
                
                // Test seeking
                handler->seek(j * 1024, SEEK_SET);
                off_t pos = handler->tell();
                if (pos != j * 1024) {
                    error_count++;
                    Debug::print("thread", "Thread %d: Seek error - expected %d, got %lld", 
                                i, j * 1024, (long long)pos);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    Debug::print("thread", "Thread safety test completed - errors: %d", 
                error_count.load());
}
```

**Solution:**
- Use atomic operations for simple state
- Implement proper mutex protection
- Use shared_mutex for read/write scenarios
- Test thoroughly with thread sanitizer

## Testing Guidelines

### Unit Testing Framework

```cpp
// tests/test_my_custom_handler.cpp
#include "psymp3.h"
#include "test_framework.h"

class MyCustomIOHandlerTest : public TestCase {
public:
    void setUp() override {
        // Set up test environment
        m_test_source = "custom://test-source";
    }
    
    void tearDown() override {
        // Clean up test environment
    }
    
    void testBasicConstruction() {
        auto handler = std::make_unique<MyCustomIOHandler>(m_test_source);
        ASSERT_TRUE(handler != nullptr);
        ASSERT_TRUE(handler->isConnected());
    }
    
    void testReadOperation() {
        auto handler = std::make_unique<MyCustomIOHandler>(m_test_source);
        
        char buffer[1024];
        size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
        
        ASSERT_GT(bytes_read, 0);
        ASSERT_FALSE(handler->eof());
    }
    
    void testSeekOperation() {
        auto handler = std::make_unique<MyCustomIOHandler>(m_test_source);
        
        // Test SEEK_SET
        int result = handler->seek(100, SEEK_SET);
        ASSERT_EQ(result, 0);
        ASSERT_EQ(handler->tell(), 100);
        
        // Test SEEK_CUR
        result = handler->seek(50, SEEK_CUR);
        ASSERT_EQ(result, 0);
        ASSERT_EQ(handler->tell(), 150);
    }
    
    void testErrorHandling() {
        // Test invalid construction
        ASSERT_THROWS(MyCustomIOHandler("invalid://source"), InvalidMediaException);
        
        auto handler = std::make_unique<MyCustomIOHandler>(m_test_source);
        handler->close();
        
        // Operations on closed handler should fail
        char buffer[100];
        size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
        ASSERT_EQ(bytes_read, 0);
        ASSERT_NE(handler->getLastError(), 0);
    }
    
    void testThreadSafety() {
        auto handler = std::make_unique<MyCustomIOHandler>(m_test_source);
        
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};
        
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&handler, &success_count]() {
                char buffer[512];
                for (int j = 0; j < 50; ++j) {
                    size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
                    if (bytes_read > 0 || handler->eof()) {
                        success_count++;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_GT(success_count.load(), 0);
    }

private:
    std::string m_test_source;
};

// Register test
REGISTER_TEST(MyCustomIOHandlerTest);
```

### Integration Testing

```cpp
// tests/test_iohandler_integration.cpp
class IOHandlerIntegrationTest : public TestCase {
public:
    void testMediaFactoryIntegration() {
        // Test that MediaFactory creates correct handler types
        auto file_handler = MediaFactory::createIOHandler("test.mp3");
        ASSERT_TRUE(dynamic_cast<FileIOHandler*>(file_handler.get()) != nullptr);
        
        auto http_handler = MediaFactory::createIOHandler("http://example.com/test.mp3");
        ASSERT_TRUE(dynamic_cast<HTTPIOHandler*>(http_handler.get()) != nullptr);
        
        auto custom_handler = MediaFactory::createIOHandler("custom://test");
        ASSERT_TRUE(dynamic_cast<MyCustomIOHandler*>(custom_handler.get()) != nullptr);
    }
    
    void testDemuxerIntegration() {
        // Test IOHandler with actual demuxer
        auto handler = std::make_unique<FileIOHandler>("test_audio.mp3");
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        
        ASSERT_TRUE(demuxer != nullptr);
        ASSERT_TRUE(demuxer->initialize());
        
        // Test reading frames
        auto frame = demuxer->readFrame();
        ASSERT_TRUE(frame != nullptr);
    }
    
    void testPerformanceBenchmark() {
        auto handler = std::make_unique<FileIOHandler>("large_test_file.mp3");
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        char buffer[64 * 1024];
        size_t total_bytes = 0;
        
        while (!handler->eof()) {
            size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
            if (bytes_read == 0) break;
            total_bytes += bytes_read;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        double throughput = (total_bytes * 1000.0) / (duration.count() * 1024 * 1024);
        Debug::print("perf", "Throughput: %.2f MB/s", throughput);
        
        // Assert minimum performance
        ASSERT_GT(throughput, 10.0); // At least 10 MB/s
    }
};
```

### Performance Testing

```cpp
// tests/test_iohandler_performance.cpp
class IOHandlerPerformanceTest : public TestCase {
public:
    void benchmarkFileIO() {
        const std::vector<size_t> buffer_sizes = {
            4 * 1024, 16 * 1024, 64 * 1024, 256 * 1024
        };
        
        for (size_t buffer_size : buffer_sizes) {
            auto handler = std::make_unique<FileIOHandler>("benchmark_file.mp3");
            std::vector<char> buffer(buffer_size);
            
            auto start = std::chrono::high_resolution_clock::now();
            size_t total_bytes = 0;
            
            while (!handler->eof()) {
                size_t bytes_read = handler->read(buffer.data(), 1, buffer.size());
                if (bytes_read == 0) break;
                total_bytes += bytes_read;
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end - start);
            
            double throughput = (total_bytes * 1000000.0) / 
                              (duration.count() * 1024 * 1024);
            
            Debug::print("perf", "Buffer size: %zu KB, Throughput: %.2f MB/s", 
                        buffer_size / 1024, throughput);
        }
    }
    
    void benchmarkHttpStreaming() {
        auto handler = std::make_unique<HTTPIOHandler>("http://example.com/test.mp3");
        
        // Benchmark sequential access
        auto start = std::chrono::high_resolution_clock::now();
        
        char buffer[32 * 1024];
        size_t total_bytes = 0;
        
        for (int i = 0; i < 100; ++i) {
            size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
            if (bytes_read == 0) break;
            total_bytes += bytes_read;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start);
        
        Debug::print("perf", "HTTP streaming: %zu bytes in %lld ms", 
                    total_bytes, duration.count());
        
        // Benchmark seeking
        if (handler->supportsRangeRequests()) {
            start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < 10; ++i) {
                handler->seek(i * 100000, SEEK_SET);
                handler->read(buffer, 1, 1024);
            }
            
            end = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start);
            
            Debug::print("perf", "HTTP seeking: 10 seeks in %lld ms", 
                        duration.count());
        }
    }
};
```

## Integration Patterns

### MediaFactory Integration

```cpp
// Enhanced MediaFactory with IOHandler support
class MediaFactory {
public:
    static std::unique_ptr<IOHandler> createIOHandler(const std::string& uri) {
        std::string scheme = extractScheme(uri);
        
        if (scheme == "http" || scheme == "https") {
            return createHttpHandler(uri);
        } else if (scheme == "file" || scheme.empty()) {
            return createFileHandler(uri);
        } else if (scheme == "custom") {
            return createCustomHandler(uri);
        } else {
            throw InvalidMediaException("Unsupported URI scheme: " + scheme);
        }
    }
    
    static std::unique_ptr<Demuxer> createDemuxer(std::unique_ptr<IOHandler> handler) {
        // Detect format from handler
        std::string format = detectFormat(handler.get());
        
        if (format == "mp3") {
            return std::make_unique<MP3Demuxer>(std::move(handler));
        } else if (format == "flac") {
            return std::make_unique<FLACDemuxer>(std::move(handler));
        } else if (format == "ogg") {
            return std::make_unique<OggDemuxer>(std::move(handler));
        } else {
            throw InvalidMediaException("Unsupported format: " + format);
        }
    }

private:
    static std::unique_ptr<IOHandler> createHttpHandler(const std::string& uri) {
        // Add HTTP-specific configuration
        auto handler = std::make_unique<HTTPIOHandler>(uri);
        
        // Configure based on URI parameters
        if (uri.find("?buffer=") != std::string::npos) {
            // Extract buffer size parameter
        }
        
        return handler;
    }
    
    static std::unique_ptr<IOHandler> createFileHandler(const std::string& uri) {
        std::string path = uri;
        if (path.substr(0, 7) == "file://") {
            path = path.substr(7);
        }
        
        return std::make_unique<FileIOHandler>(TagLib::String(path.c_str()));
    }
    
    static std::string detectFormat(IOHandler* handler) {
        // Read magic bytes to detect format
        char magic[16];
        size_t bytes_read = handler->read(magic, 1, sizeof(magic));
        handler->seek(0, SEEK_SET); // Reset position
        
        if (bytes_read >= 3 && memcmp(magic, "ID3", 3) == 0) {
            return "mp3";
        } else if (bytes_read >= 4 && memcmp(magic, "fLaC", 4) == 0) {
            return "flac";
        } else if (bytes_read >= 4 && memcmp(magic, "OggS", 4) == 0) {
            return "ogg";
        }
        
        return "unknown";
    }
};
```

### Configuration Integration

```cpp
// Configuration system integration
class IOHandlerConfig {
public:
    struct FileConfig {
        size_t buffer_size = 64 * 1024;
        bool enable_read_ahead = true;
        size_t read_ahead_size = 128 * 1024;
        int timeout_seconds = 30;
    };
    
    struct HttpConfig {
        size_t buffer_size = 64 * 1024;
        size_t max_buffer_size = 512 * 1024;
        int connection_timeout = 30;
        int request_timeout = 60;
        bool enable_range_requests = true;
        size_t min_range_size = 8 * 1024;
    };
    
    static FileConfig getFileConfig() {
        FileConfig config;
        
        // Load from PsyMP3 configuration system
        config.buffer_size = Config::getInt("io.file.buffer_size", 64 * 1024);
        config.enable_read_ahead = Config::getBool("io.file.read_ahead", true);
        config.read_ahead_size = Config::getInt("io.file.read_ahead_size", 128 * 1024);
        config.timeout_seconds = Config::getInt("io.file.timeout", 30);
        
        return config;
    }
    
    static HttpConfig getHttpConfig() {
        HttpConfig config;
        
        config.buffer_size = Config::getInt("io.http.buffer_size", 64 * 1024);
        config.max_buffer_size = Config::getInt("io.http.max_buffer_size", 512 * 1024);
        config.connection_timeout = Config::getInt("io.http.connection_timeout", 30);
        config.request_timeout = Config::getInt("io.http.request_timeout", 60);
        config.enable_range_requests = Config::getBool("io.http.range_requests", true);
        config.min_range_size = Config::getInt("io.http.min_range_size", 8 * 1024);
        
        return config;
    }
};
```

## Best Practices

### Error Handling Best Practices

1. **Use Appropriate Exception Types**
```cpp
// Good: Specific exception with context
throw InvalidMediaException("Failed to open file: " + path + 
                           " (error: " + std::to_string(errno) + ")");

// Bad: Generic exception
throw std::runtime_error("Error");
```

2. **Provide Recovery Information**
```cpp
void handleIOError(const std::exception& e) {
    Debug::print("io", "Error: %s", e.what());
    
    // Provide recovery suggestions
    if (dynamic_cast<const InvalidMediaException*>(&e)) {
        Debug::print("io", "Suggestion: Check file permissions and path");
    }
}
```

3. **Use RAII for Resource Management**
```cpp
class RAIIIOHandler {
private:
    std::unique_ptr<IOHandler> m_handler;
    
public:
    explicit RAIIIOHandler(std::unique_ptr<IOHandler> handler) 
        : m_handler(std::move(handler)) {}
    
    ~RAIIIOHandler() {
        if (m_handler) {
            m_handler->close();
        }
    }
    
    IOHandler* get() { return m_handler.get(); }
    IOHandler* operator->() { return m_handler.get(); }
};
```

### Performance Best Practices

1. **Choose Appropriate Buffer Sizes**
```cpp
size_t getOptimalBufferSize(IOHandler* handler) {
    off_t file_size = handler->getFileSize();
    
    if (file_size < 0) {
        return 64 * 1024; // Default for unknown size
    } else if (file_size < 1024 * 1024) {
        return 16 * 1024; // Small files
    } else if (file_size > 100 * 1024 * 1024) {
        return 256 * 1024; // Large files
    } else {
        return 64 * 1024; // Medium files
    }
}
```

2. **Minimize Seeking Operations**
```cpp
// Good: Read sequentially when possible
void efficientReading(IOHandler* handler) {
    char buffer[65536];
    while (!handler->eof()) {
        size_t bytes = handler->read(buffer, 1, sizeof(buffer));
        if (bytes == 0) break;
        processData(buffer, bytes);
    }
}

// Avoid: Frequent seeking
void inefficientReading(IOHandler* handler) {
    for (int i = 0; i < 1000; ++i) {
        handler->seek(i * 1000, SEEK_SET);
        char byte;
        handler->read(&byte, 1, 1);
    }
}
```

3. **Use Connection Pooling**
```cpp
// HTTPClient automatically pools connections
void efficientHttpRequests() {
    // These will reuse connections
    auto response1 = HTTPClient::get("https://api.example.com/track1");
    auto response2 = HTTPClient::get("https://api.example.com/track2");
    auto response3 = HTTPClient::get("https://api.example.com/track3");
}
```

### Thread Safety Best Practices

1. **Use Atomic Operations for Simple State**
```cpp
class ThreadSafeHandler : public IOHandler {
private:
    std::atomic<bool> m_initialized{false};
    std::atomic<size_t> m_bytes_read{0};
    
public:
    bool isInitialized() const { return m_initialized.load(); }
    size_t getBytesRead() const { return m_bytes_read.load(); }
};
```

2. **Use Shared Mutex for Read/Write Scenarios**
```cpp
class BufferedHandler : public IOHandler {
private:
    mutable std::shared_mutex m_buffer_mutex;
    std::vector<uint8_t> m_buffer;
    
public:
    size_t read(void* buffer, size_t size, size_t count) override {
        std::shared_lock<std::shared_mutex> lock(m_buffer_mutex);
        // Multiple readers can access buffer simultaneously
        return readFromBuffer(buffer, size * count);
    }
    
    void invalidateBuffer() {
        std::unique_lock<std::shared_mutex> lock(m_buffer_mutex);
        // Exclusive access for buffer modification
        m_buffer.clear();
    }
};
```

3. **Protect Complex Operations with Mutex**
```cpp
class NetworkHandler : public IOHandler {
private:
    mutable std::mutex m_network_mutex;
    
public:
    int seek(off_t offset, int whence) override {
        std::lock_guard<std::mutex> lock(m_network_mutex);
        // Network operations need exclusive access
        return performNetworkSeek(offset, whence);
    }
};
```

## Advanced Topics

### Custom Buffer Pool Implementation

```cpp
template<size_t BufferSize, size_t PoolSize>
class CustomBufferPool {
private:
    struct Buffer {
        alignas(64) std::array<uint8_t, BufferSize> data;
        std::atomic<bool> in_use{false};
        std::chrono::steady_clock::time_point last_used;
    };
    
    std::array<Buffer, PoolSize> m_buffers;
    std::atomic<size_t> m_next_index{0};
    
public:
    class BufferHandle {
    private:
        Buffer* m_buffer;
        CustomBufferPool* m_pool;
        
    public:
        BufferHandle(Buffer* buffer, CustomBufferPool* pool) 
            : m_buffer(buffer), m_pool(pool) {}
        
        ~BufferHandle() {
            if (m_buffer && m_pool) {
                m_pool->releaseBuffer(m_buffer);
            }
        }
        
        uint8_t* data() { return m_buffer->data.data(); }
        size_t size() const { return BufferSize; }
        
        // Move-only semantics
        BufferHandle(const BufferHandle&) = delete;
        BufferHandle& operator=(const BufferHandle&) = delete;
        
        BufferHandle(BufferHandle&& other) noexcept 
            : m_buffer(other.m_buffer), m_pool(other.m_pool) {
            other.m_buffer = nullptr;
            other.m_pool = nullptr;
        }
        
        BufferHandle& operator=(BufferHandle&& other) noexcept {
            if (this != &other) {
                if (m_buffer && m_pool) {
                    m_pool->releaseBuffer(m_buffer);
                }
                m_buffer = other.m_buffer;
                m_pool = other.m_pool;
                other.m_buffer = nullptr;
                other.m_pool = nullptr;
            }
            return *this;
        }
    };
    
    std::optional<BufferHandle> acquireBuffer() {
        auto now = std::chrono::steady_clock::now();
        
        // Try to find an available buffer
        for (size_t attempts = 0; attempts < PoolSize * 2; ++attempts) {
            size_t index = m_next_index.fetch_add(1) % PoolSize;
            Buffer& buffer = m_buffers[index];
            
            bool expected = false;
            if (buffer.in_use.compare_exchange_weak(expected, true)) {
                buffer.last_used = now;
                return BufferHandle(&buffer, this);
            }
        }
        
        return std::nullopt; // Pool exhausted
    }
    
    void releaseBuffer(Buffer* buffer) {
        buffer->in_use.store(false);
    }
    
    size_t getUsageStats() const {
        size_t in_use_count = 0;
        for (const auto& buffer : m_buffers) {
            if (buffer.in_use.load()) {
                ++in_use_count;
            }
        }
        return in_use_count;
    }
};
```

### Adaptive Network Optimization

```cpp
class AdaptiveNetworkOptimizer {
private:
    struct NetworkMetrics {
        double bandwidth_mbps = 0.0;
        double latency_ms = 0.0;
        double packet_loss_rate = 0.0;
        std::chrono::steady_clock::time_point last_update;
    };
    
    NetworkMetrics m_metrics;
    std::deque<double> m_bandwidth_history;
    std::deque<double> m_latency_history;
    
    static constexpr size_t HISTORY_SIZE = 20;
    
public:
    void updateMetrics(size_t bytes_transferred, 
                      std::chrono::milliseconds duration,
                      bool success) {
        auto now = std::chrono::steady_clock::now();
        
        if (success && duration.count() > 0) {
            double bandwidth = (bytes_transferred * 8.0 * 1000.0) / 
                             (duration.count() * 1024.0 * 1024.0);
            
            m_bandwidth_history.push_back(bandwidth);
            if (m_bandwidth_history.size() > HISTORY_SIZE) {
                m_bandwidth_history.pop_front();
            }
            
            // Calculate moving average
            double total_bandwidth = 0.0;
            for (double bw : m_bandwidth_history) {
                total_bandwidth += bw;
            }
            m_metrics.bandwidth_mbps = total_bandwidth / m_bandwidth_history.size();
        }
        
        m_metrics.last_update = now;
    }
    
    size_t getOptimalBufferSize() const {
        // Adjust buffer size based on bandwidth
        if (m_metrics.bandwidth_mbps < 1.0) {
            return 32 * 1024; // Low bandwidth: smaller buffers
        } else if (m_metrics.bandwidth_mbps > 10.0) {
            return 512 * 1024; // High bandwidth: larger buffers
        } else {
            return 128 * 1024; // Medium bandwidth: medium buffers
        }
    }
    
    int getOptimalTimeout() const {
        // Adjust timeout based on latency
        int base_timeout = 30;
        if (m_metrics.latency_ms > 1000.0) {
            return base_timeout * 2; // High latency: longer timeout
        } else if (m_metrics.latency_ms < 100.0) {
            return base_timeout / 2; // Low latency: shorter timeout
        }
        return base_timeout;
    }
    
    bool shouldUseRangeRequests() const {
        // Disable range requests on very slow connections
        return m_metrics.bandwidth_mbps > 0.5;
    }
};
```

This comprehensive developer guide covers all aspects of extending and maintaining the IOHandler subsystem, from implementing new handler types to advanced optimization techniques and troubleshooting common issues.
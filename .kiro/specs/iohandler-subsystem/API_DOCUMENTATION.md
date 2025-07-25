# IOHandler Subsystem API Documentation

## Overview

The IOHandler subsystem provides a unified abstraction layer for accessing media content from various sources in PsyMP3. This documentation covers all public interfaces, usage patterns, and implementation guidelines for the IOHandler system.

## Table of Contents

1. [Core Interfaces](#core-interfaces)
2. [IOHandler Base Class](#iohandler-base-class)
3. [FileIOHandler Implementation](#fileiohandler-implementation)
4. [HTTPIOHandler Implementation](#httpiohandler-implementation)
5. [HTTPClient Foundation](#httpclient-foundation)
6. [Usage Examples](#usage-examples)
7. [Error Handling](#error-handling)
8. [Cross-Platform Considerations](#cross-platform-considerations)
9. [Thread Safety](#thread-safety)
10. [Performance Guidelines](#performance-guidelines)

## Core Interfaces

### IOHandler Base Class

The `IOHandler` class provides the fundamental interface for all I/O operations in PsyMP3. All concrete implementations inherit from this base class and must implement its virtual methods.

#### Class Declaration

```cpp
class IOHandler {
public:
    IOHandler();
    virtual ~IOHandler();
    
    // Core I/O operations
    virtual size_t read(void* buffer, size_t size, size_t count);
    virtual int seek(off_t offset, int whence);
    virtual off_t tell();
    virtual int close();
    virtual bool eof();
    virtual off_t getFileSize();
    virtual int getLastError() const;
    
    // Utility methods
    static std::map<std::string, size_t> getMemoryStats();
    
protected:
    // Thread-safe state management
    std::atomic<bool> m_closed{false};
    std::atomic<bool> m_eof{false};
    std::atomic<off_t> m_position{0};
    std::atomic<int> m_error{0};
    
    // Thread synchronization
    mutable std::mutex m_state_mutex;
    mutable std::shared_mutex m_operation_mutex;
    
    // Memory management
    std::atomic<size_t> m_memory_usage{0};
    
    // Protected utility methods
    void updateMemoryUsage(size_t new_usage);
    bool updatePosition(off_t new_position);
    void updateErrorState(int error_code, const std::string& error_message = "");
    void updateEofState(bool eof_state);
    void updateClosedState(bool closed_state);
    bool checkMemoryLimits(size_t additional_bytes) const;
};
```

#### Method Documentation

##### `read(void* buffer, size_t size, size_t count)`

Reads data from the I/O source with fread-like semantics.

**Parameters:**
- `buffer`: Destination buffer for read data
- `size`: Size of each element to read
- `count`: Number of elements to read

**Returns:** Number of elements successfully read

**Thread Safety:** Safe for concurrent access with proper synchronization

**Example:**
```cpp
std::unique_ptr<IOHandler> handler = std::make_unique<FileIOHandler>("audio.mp3");
char buffer[4096];
size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
```

##### `seek(off_t offset, int whence)`

Seeks to a position in the I/O source.

**Parameters:**
- `offset`: Offset to seek to (supports large files with off_t)
- `whence`: Positioning mode (SEEK_SET, SEEK_CUR, SEEK_END)

**Returns:** 0 on success, -1 on failure

**Thread Safety:** Safe for concurrent access

**Example:**
```cpp
// Seek to beginning of file
handler->seek(0, SEEK_SET);

// Seek 1024 bytes from current position
handler->seek(1024, SEEK_CUR);

// Seek to end of file
handler->seek(0, SEEK_END);
```

##### `tell()`

Returns the current byte offset position in the stream.

**Returns:** Current position as off_t for large file support, -1 on failure

**Thread Safety:** Safe for concurrent access

##### `close()`

Closes the I/O source and releases all associated resources.

**Returns:** 0 on success, standard error codes on failure

**Thread Safety:** Safe for concurrent access

**Note:** Multiple calls to close() are safe and will not cause errors.

##### `eof()`

Checks if the stream has reached end-of-file condition.

**Returns:** true if at end of stream, false otherwise

**Thread Safety:** Safe for concurrent access

##### `getFileSize()`

Returns the total size of the I/O source in bytes.

**Returns:** Size in bytes, or -1 if unknown or not applicable

**Thread Safety:** Safe for concurrent access

##### `getLastError()`

Returns the last error code encountered by this handler.

**Returns:** Error code (0 = no error)

**Thread Safety:** Safe for concurrent access

## FileIOHandler Implementation

The `FileIOHandler` class provides access to local files with cross-platform Unicode support and large file handling.

### Constructor

```cpp
explicit FileIOHandler(const TagLib::String& path);
```

**Parameters:**
- `path`: File path with Unicode support via TagLib::String

**Throws:** `InvalidMediaException` if file cannot be opened

**Example:**
```cpp
try {
    auto handler = std::make_unique<FileIOHandler>(TagLib::String("音楽.mp3"));
    // File opened successfully
} catch (const InvalidMediaException& e) {
    Debug::print("io", "Failed to open file: %s", e.what());
}
```

### Key Features

#### Unicode Filename Support

FileIOHandler supports Unicode filenames across all platforms:

- **Windows**: Uses `_wfopen()` with UTF-16 encoding
- **Unix/Linux**: Preserves original filesystem encoding

#### Large File Support

Supports files larger than 2GB on all platforms:

- Uses 64-bit file operations (`fseeko`/`ftello`)
- Returns `off_t` for position and size information
- Handles integer overflow protection

#### Performance Optimizations

- **Internal Buffering**: 64KB default buffer with adaptive sizing
- **Read-ahead**: Detects sequential access patterns
- **Buffer Pool Integration**: Uses shared buffer pools for memory efficiency

### Usage Example

```cpp
#include "psymp3.h"

void processAudioFile(const TagLib::String& filename) {
    try {
        auto handler = std::make_unique<FileIOHandler>(filename);
        
        // Get file size
        off_t file_size = handler->getFileSize();
        Debug::print("io", "File size: %lld bytes", file_size);
        
        // Read data in chunks
        char buffer[8192];
        size_t total_read = 0;
        
        while (!handler->eof()) {
            size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
            if (bytes_read == 0) break;
            
            // Process buffer data
            processAudioData(buffer, bytes_read);
            total_read += bytes_read;
        }
        
        Debug::print("io", "Processed %zu bytes", total_read);
        
    } catch (const InvalidMediaException& e) {
        Debug::print("io", "File error: %s", e.what());
    }
}
```

## HTTPIOHandler Implementation

The `HTTPIOHandler` class provides HTTP streaming with range request support and intelligent buffering.

### Constructors

```cpp
explicit HTTPIOHandler(const std::string& url);
HTTPIOHandler(const std::string& url, long content_length);
```

**Parameters:**
- `url`: HTTP/HTTPS URL to stream from
- `content_length`: Optional known content length

### HTTP-Specific Methods

#### `getContentLength()`

Returns the content length extracted from HTTP headers.

**Returns:** Content length in bytes, or -1 if unknown

#### `getMimeType()`

Returns the MIME type from the Content-Type header.

**Returns:** MIME type string, empty if not available

#### `supportsRangeRequests()`

Checks if the server supports HTTP range requests for seeking.

**Returns:** true if range requests are supported, false otherwise

#### `isInitialized()`

Checks if the HTTP handler has completed initialization.

**Returns:** true if initialized, false otherwise

### Key Features

#### Range Request Support

HTTPIOHandler automatically detects and uses HTTP range requests:

- Performs HEAD request to check `Accept-Ranges` header
- Uses range requests for seeking when supported
- Falls back to sequential access when ranges not supported

#### Intelligent Buffering

- **Adaptive Buffer Sizing**: Adjusts buffer size based on network conditions
- **Read-ahead**: Anticipates sequential access patterns
- **Connection Reuse**: Leverages HTTP/1.1 keep-alive connections

#### Network Error Recovery

- **Retry Logic**: Automatic retry for transient network errors
- **Timeout Handling**: Configurable timeouts with graceful handling
- **Error Classification**: Distinguishes temporary vs permanent failures

### Usage Example

```cpp
void streamHttpAudio(const std::string& url) {
    try {
        auto handler = std::make_unique<HTTPIOHandler>(url);
        
        // Check server capabilities
        if (handler->supportsRangeRequests()) {
            Debug::print("http", "Server supports seeking");
        }
        
        Debug::print("http", "Content-Type: %s", handler->getMimeType().c_str());
        Debug::print("http", "Content-Length: %ld", handler->getContentLength());
        
        // Stream data
        char buffer[16384];
        while (!handler->eof()) {
            size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
            if (bytes_read == 0) break;
            
            // Process streaming audio data
            processStreamingAudio(buffer, bytes_read);
        }
        
    } catch (const InvalidMediaException& e) {
        Debug::print("http", "Streaming error: %s", e.what());
    }
}
```

## HTTPClient Foundation

The `HTTPClient` class provides robust HTTP functionality for the HTTPIOHandler.

### Response Structure

```cpp
struct Response {
    int statusCode = 0;
    std::string statusMessage;
    std::map<std::string, std::string> headers;
    std::string body;
    bool success = false;
    bool connection_reused = false;
};
```

### Static Methods

#### `get(url, headers, timeoutSeconds)`

Performs HTTP GET request.

**Parameters:**
- `url`: Complete URL to request
- `headers`: Optional additional headers
- `timeoutSeconds`: Request timeout (default: 30)

**Returns:** Response structure

#### `post(url, data, contentType, headers, timeoutSeconds)`

Performs HTTP POST request.

#### `head(url, headers, timeoutSeconds)`

Performs HTTP HEAD request for metadata only.

#### `getRange(url, start_byte, end_byte, headers, timeoutSeconds)`

Performs HTTP GET with Range header for partial content.

### Usage Example

```cpp
// Simple GET request
HTTPClient::Response response = HTTPClient::get("https://example.com/audio.mp3");
if (response.success) {
    Debug::print("http", "Status: %d, Size: %zu", 
                response.statusCode, response.body.size());
}

// Range request
HTTPClient::Response range_response = HTTPClient::getRange(
    "https://example.com/audio.mp3", 1024, 2047);
if (range_response.success && range_response.statusCode == 206) {
    // Process partial content
}
```

## Usage Examples

### Basic File Reading

```cpp
void readLocalFile() {
    try {
        auto handler = std::make_unique<FileIOHandler>("music.flac");
        
        // Read header
        char header[1024];
        size_t header_size = handler->read(header, 1, sizeof(header));
        
        // Seek to specific position
        handler->seek(4096, SEEK_SET);
        
        // Continue reading
        char data[8192];
        while (!handler->eof()) {
            size_t bytes = handler->read(data, 1, sizeof(data));
            if (bytes == 0) break;
            processData(data, bytes);
        }
        
    } catch (const InvalidMediaException& e) {
        handleError(e.what());
    }
}
```

### HTTP Streaming with Seeking

```cpp
void streamWithSeeking() {
    try {
        auto handler = std::make_unique<HTTPIOHandler>("https://example.com/stream.mp3");
        
        if (handler->supportsRangeRequests()) {
            // Seek to middle of file
            off_t file_size = handler->getFileSize();
            if (file_size > 0) {
                handler->seek(file_size / 2, SEEK_SET);
            }
        }
        
        // Stream from current position
        char buffer[32768];
        while (!handler->eof()) {
            size_t bytes = handler->read(buffer, 1, sizeof(buffer));
            if (bytes == 0) break;
            playAudio(buffer, bytes);
        }
        
    } catch (const InvalidMediaException& e) {
        handleStreamingError(e.what());
    }
}
```

### Factory Pattern Usage

```cpp
std::unique_ptr<IOHandler> createHandler(const std::string& uri) {
    if (uri.substr(0, 7) == "http://" || uri.substr(0, 8) == "https://") {
        return std::make_unique<HTTPIOHandler>(uri);
    } else {
        return std::make_unique<FileIOHandler>(TagLib::String(uri.c_str()));
    }
}
```

## Error Handling

### Exception Types

The IOHandler subsystem uses PsyMP3's exception hierarchy:

- `InvalidMediaException`: File/stream cannot be opened or accessed
- `std::runtime_error`: General runtime errors
- `std::bad_alloc`: Memory allocation failures

### Error Codes

IOHandler methods return standard error codes:

- `0`: Success
- `-1`: General failure
- `ENOENT`: File not found
- `EACCES`: Permission denied
- `ETIMEDOUT`: Operation timed out
- `ECONNREFUSED`: Connection refused (HTTP)

### Error Handling Best Practices

```cpp
void robustFileAccess(const TagLib::String& path) {
    std::unique_ptr<IOHandler> handler;
    
    try {
        handler = std::make_unique<FileIOHandler>(path);
    } catch (const InvalidMediaException& e) {
        Debug::print("io", "Failed to open file: %s", e.what());
        return;
    }
    
    char buffer[4096];
    size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
    
    if (bytes_read == 0 && !handler->eof()) {
        int error = handler->getLastError();
        Debug::print("io", "Read failed with error: %d", error);
        
        // Check if error is recoverable
        if (error == EINTR || error == EAGAIN) {
            // Retry operation
            bytes_read = handler->read(buffer, 1, sizeof(buffer));
        }
    }
}
```

## Cross-Platform Considerations

### File Path Handling

- **Windows**: Supports Unicode filenames via UTF-16
- **Unix/Linux**: Preserves original filesystem encoding
- **Path Separators**: Automatically normalized per platform

### Large File Support

- All platforms support files >2GB through 64-bit operations
- Uses `off_t` for position and size information
- Handles platform-specific file stat structures

### Network Stack Differences

- HTTPClient abstracts platform socket differences
- Consistent SSL/TLS support via OpenSSL
- Unified error reporting across platforms

### Platform-Specific Optimizations

```cpp
// Windows-specific optimizations
#ifdef _WIN32
    // Uses _wfopen for Unicode support
    // Uses _fstat64 for large file support
#endif

// Unix-specific optimizations  
#ifdef __unix__
    // Uses fopen with proper encoding
    // Uses fstat for file information
#endif
```

## Thread Safety

### Thread Safety Guarantees

The IOHandler subsystem provides comprehensive thread safety:

#### IOHandler Base Class
- **Atomic Operations**: Position, EOF, and error state use atomic variables
- **Shared Mutex**: Allows concurrent reads, exclusive writes
- **State Mutex**: Protects complex state changes

#### FileIOHandler
- **File Mutex**: Protects file handle operations
- **Buffer Mutex**: Allows concurrent buffer reads
- **Thread-Safe Buffering**: Internal buffers use proper synchronization

#### HTTPIOHandler
- **HTTP Mutex**: Protects HTTP client operations
- **Buffer Mutex**: Allows concurrent buffer access
- **Initialization Mutex**: Protects one-time initialization

### Thread Safety Usage

```cpp
// Safe concurrent access
void concurrentAccess() {
    auto handler = std::make_unique<FileIOHandler>("audio.mp3");
    
    // Multiple threads can safely read
    std::thread reader1([&handler]() {
        char buffer1[4096];
        handler->read(buffer1, 1, sizeof(buffer1));
    });
    
    std::thread reader2([&handler]() {
        char buffer2[4096];
        handler->read(buffer2, 1, sizeof(buffer2));
    });
    
    // Seeking is also thread-safe
    std::thread seeker([&handler]() {
        handler->seek(1024, SEEK_SET);
    });
    
    reader1.join();
    reader2.join();
    seeker.join();
}
```

### Thread Safety Recommendations

1. **Multiple Readers**: Safe for concurrent read operations
2. **Seeking**: Thread-safe but may affect other readers
3. **Error Handling**: Error state is thread-safe
4. **Resource Cleanup**: Destructors are thread-safe
5. **Memory Management**: All memory operations are protected

## Performance Guidelines

### File I/O Optimization

#### Buffer Size Selection
```cpp
// Optimal buffer sizes for different scenarios
const size_t SMALL_FILE_BUFFER = 8 * 1024;    // 8KB for small files
const size_t MEDIUM_FILE_BUFFER = 64 * 1024;  // 64KB for typical files  
const size_t LARGE_FILE_BUFFER = 256 * 1024;  // 256KB for large files

// Adaptive buffer sizing
size_t getOptimalBufferSize(off_t file_size) {
    if (file_size < 1024 * 1024) return SMALL_FILE_BUFFER;
    if (file_size < 100 * 1024 * 1024) return MEDIUM_FILE_BUFFER;
    return LARGE_FILE_BUFFER;
}
```

#### Sequential Access Optimization
```cpp
// FileIOHandler automatically detects sequential patterns
void sequentialRead(IOHandler* handler) {
    char buffer[65536];
    
    // This pattern will trigger read-ahead optimization
    while (!handler->eof()) {
        size_t bytes = handler->read(buffer, 1, sizeof(buffer));
        if (bytes == 0) break;
        processSequentially(buffer, bytes);
    }
}
```

### HTTP Streaming Optimization

#### Connection Reuse
```cpp
// HTTPClient automatically reuses connections
void efficientHttpAccess() {
    // These requests will reuse the same connection
    auto response1 = HTTPClient::get("https://example.com/track1.mp3");
    auto response2 = HTTPClient::get("https://example.com/track2.mp3");
    auto response3 = HTTPClient::get("https://example.com/track3.mp3");
}
```

#### Range Request Optimization
```cpp
// Efficient seeking in HTTP streams
void efficientHttpSeeking(HTTPIOHandler* handler) {
    if (handler->supportsRangeRequests()) {
        // Large seeks are efficient with range requests
        handler->seek(10 * 1024 * 1024, SEEK_SET);  // Seek to 10MB
        
        char buffer[32768];
        handler->read(buffer, 1, sizeof(buffer));
    }
}
```

### Memory Management

#### Buffer Pool Usage
```cpp
// IOHandler implementations use buffer pools automatically
void memoryEfficientReading() {
    // Multiple handlers share buffer pools
    auto handler1 = std::make_unique<FileIOHandler>("file1.mp3");
    auto handler2 = std::make_unique<FileIOHandler>("file2.mp3");
    
    // Buffers are automatically pooled and reused
    // No manual memory management required
}
```

#### Memory Monitoring
```cpp
// Monitor memory usage
void monitorMemoryUsage() {
    auto stats = IOHandler::getMemoryStats();
    
    Debug::print("memory", "Total IOHandler memory: %zu bytes", 
                stats["total_memory"]);
    Debug::print("memory", "Active handlers: %zu", 
                stats["active_handlers"]);
    Debug::print("memory", "Buffer pool usage: %zu bytes", 
                stats["buffer_pool_usage"]);
}
```

### Performance Best Practices

1. **Use Appropriate Buffer Sizes**: Match buffer size to access patterns
2. **Leverage Sequential Access**: IOHandler optimizes for sequential reads
3. **Minimize Seeking**: Frequent seeking can reduce performance
4. **Reuse Connections**: HTTPClient automatically pools connections
5. **Monitor Memory Usage**: Use memory statistics for optimization
6. **Choose Right Handler**: Use FileIOHandler for local files, HTTPIOHandler for streams

This completes the comprehensive API documentation for the IOHandler subsystem. The documentation covers all public interfaces, usage patterns, error handling, cross-platform considerations, thread safety, and performance guidelines as required by the task specifications.
# **IOHANDLER SUBSYSTEM DESIGN**

## **Overview**

This design document specifies the implementation of the IOHandler subsystem in PsyMP3, which provides a unified abstraction layer for accessing media content from various sources. The subsystem consists of the base IOHandler interface and concrete implementations for different I/O sources including local files, HTTP streams, and potentially other protocols.

The design emphasizes cross-platform compatibility, performance, and extensibility while providing a consistent interface for all media access operations.

## **Architecture**

### **Directory Structure**

Following PsyMP3's modular architecture pattern:

```
src/io/                    # I/O subsystem root
  ├── Makefile.am         # Builds libio.a convenience library
  ├── IOHandler.cpp       # Base interface implementation
  ├── URI.cpp             # URI parsing and handling
  ├── StreamingManager.cpp # Stream management
  └── file/               # File I/O handler
      ├── Makefile.am     # Builds libfileio.a
      └── FileIOHandler.cpp
  └── http/               # HTTP I/O handler
      ├── Makefile.am     # Builds libhttpio.a
      ├── HTTPIOHandler.cpp
      └── HTTPClient.cpp

include/io/                # I/O subsystem headers
  ├── IOHandler.h
  ├── URI.h
  ├── StreamingManager.h
  └── file/
      └── FileIOHandler.h
  └── http/
      ├── HTTPIOHandler.h
      └── HTTPClient.h
```

### **Namespace Organization**

All IOHandler components use proper namespacing following PsyMP3 conventions:

```cpp
namespace PsyMP3 {
namespace IO {
    // Base IOHandler interface
    class IOHandler { /* ... */ };
    class URI { /* ... */ };
    class StreamingManager { /* ... */ };
    
    namespace File {
        // File I/O implementation
        class FileIOHandler : public IOHandler { /* ... */ };
    }
    
    namespace HTTP {
        // HTTP I/O implementation
        class HTTPIOHandler : public IOHandler { /* ... */ };
        class HTTPClient { /* ... */ };
    }
}}

// Backward compatibility using declarations in psymp3.h
using PsyMP3::IO::IOHandler;
using PsyMP3::IO::File::FileIOHandler;
using PsyMP3::IO::HTTP::HTTPIOHandler;
using PsyMP3::IO::HTTP::HTTPClient;
```

### **Class Hierarchy**

```cpp
namespace PsyMP3 {
namespace IO {

// Base IOHandler interface
class IOHandler {
public:
    virtual ~IOHandler();
    
    // Core I/O operations
    virtual size_t read(void* buffer, size_t size, size_t count);
    virtual int seek(long offset, int whence);
    virtual off_t tell();
    virtual int close();
    virtual bool eof();
    virtual off_t getFileSize();
    
protected:
    // Common state for derived classes
    bool m_closed = false;
    bool m_eof = false;
    off_t m_position = 0;
};

namespace File {

// Local file implementation
class FileIOHandler : public IOHandler {
public:
    explicit FileIOHandler(const TagLib::String& path);
    ~FileIOHandler() override;
    
    size_t read(void* buffer, size_t size, size_t count) override;
    int seek(long offset, int whence) override;
    off_t tell() override;
    int close() override;
    bool eof() override;
    off_t getFileSize() override;
    
private:
    size_t read_unlocked(void* buffer, size_t size, size_t count);
    int seek_unlocked(long offset, int whence);
    off_t tell_unlocked();
    
    mutable std::mutex m_mutex;
    FILE* m_file_handle;
    std::string m_file_path;
};

} // namespace File

namespace HTTP {

// HTTP streaming implementation
class HTTPIOHandler : public IOHandler {
public:
    explicit HTTPIOHandler(const std::string& url);
    HTTPIOHandler(const std::string& url, long content_length);
    ~HTTPIOHandler() override;
    
    size_t read(void* buffer, size_t size, size_t count) override;
    int seek(long offset, int whence) override;
    off_t tell() override;
    int close() override;
    bool eof() override;
    off_t getFileSize() override;
    
    // HTTP-specific methods
    long getContentLength() const { return m_content_length; }
    std::string getMimeType() const { return m_mime_type; }
    bool supportsRangeRequests() const { return m_supports_ranges; }
    
private:
    size_t read_unlocked(void* buffer, size_t size, size_t count);
    int seek_unlocked(long offset, int whence);
    off_t tell_unlocked();
    void initialize_unlocked();
    bool fillBuffer_unlocked(long position, size_t min_size = MIN_RANGE_SIZE);
    size_t readFromBuffer_unlocked(void* buffer, size_t bytes_to_read);
    bool isPositionBuffered_unlocked(long position) const;
    
    mutable std::mutex m_mutex;
    std::string m_url;
    long m_content_length = -1;
    long m_current_position = 0;
    std::string m_mime_type;
    bool m_supports_ranges = false;
    bool m_initialized = false;
    
    // Buffering system
    std::vector<uint8_t> m_buffer;
    size_t m_buffer_offset = 0;
    long m_buffer_start_position = 0;
    
    static constexpr size_t BUFFER_SIZE = 64 * 1024;
    static constexpr size_t MIN_RANGE_SIZE = 8 * 1024;
};

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3
```

### **HTTP Client Foundation**

```cpp
namespace PsyMP3 {
namespace IO {
namespace HTTP {

class HTTPClient {
public:
    struct Response {
        int statusCode = 0;
        std::string statusMessage;
        std::map<std::string, std::string> headers;
        std::vector<uint8_t> body;
        bool success = false;
        bool connection_reused = false;
    };
    
    // HTTP methods
    static Response get(const std::string& url, 
                       const std::map<std::string, std::string>& headers = {},
                       int timeoutSeconds = 30);
    static Response post(const std::string& url,
                        const std::string& data,
                        const std::string& contentType = "application/x-www-form-urlencoded",
                        const std::map<std::string, std::string>& headers = {},
                        int timeoutSeconds = 30);
    static Response head(const std::string& url, 
                        const std::map<std::string, std::string>& headers = {},
                        int timeoutSeconds = 30);
    static Response getRange(const std::string& url,
                            long start_byte,
                            long end_byte = -1,
                            const std::map<std::string, std::string>& headers = {},
                            int timeoutSeconds = 30);
    
    // Utility methods
    static std::string urlEncode(const std::string& input);
    static bool parseURL(const std::string& url, std::string& host, int& port, 
                        std::string& path, bool& isHttps);
    
private:
    static Response performRequest(const std::string& method,
                                   const std::string& url,
                                   const std::string& postData,
                                   const std::map<std::string, std::string>& headers,
                                   int timeoutSeconds);
};

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3
```

### **Build System Integration**

**Convenience Libraries**:
- `src/io/Makefile.am` builds `libio.a` (core I/O infrastructure)
- `src/io/file/Makefile.am` builds `libfileio.a` (file I/O handler)
- `src/io/http/Makefile.am` builds `libhttpio.a` (HTTP I/O handler)

**Linking**:
```makefile
# In src/Makefile.am
psymp3_LDADD += io/libio.a io/file/libfileio.a io/http/libhttpio.a
```

**Include Paths**:
- Headers included with full path: `#include "io/IOHandler.h"`
- Source files only include `psymp3.h` (master header rule)
- Master header conditionally includes I/O headers

## **Components and Interfaces**

### **1. Base IOHandler Component**

**Purpose**: Provide unified interface for all I/O operations

**Design Decisions**:
- Virtual destructor ensures proper cleanup in derived classes
- Default implementations return safe values (0 bytes read, EOF true, etc.)
- Standard C library semantics for familiarity and consistency
- Support for large files through off_t return types

**Interface Design**:
- `read()`: fread-like semantics with size and count parameters
- `seek()`: Standard SEEK_SET, SEEK_CUR, SEEK_END positioning
- `tell()`: Current position reporting with large file support
- `getFileSize()`: Total size reporting, -1 if unknown

### **2. FileIOHandler Component**

**Purpose**: Handle local file access with cross-platform support

**Key Features**:
- **Unicode Support**: Proper handling of international filenames
- **Large File Support**: 64-bit file operations on all platforms
- **Cross-Platform**: Windows and Unix implementations
- **Error Handling**: Comprehensive error reporting and recovery
- **Thread Safety**: Public/private lock pattern for concurrent access

**Platform-Specific Implementation**:

**Windows**:
```cpp
namespace PsyMP3 {
namespace IO {
namespace File {

FileIOHandler::FileIOHandler(const TagLib::String& path) {
    m_file_handle = _wfopen(path.toCWString(), L"rb");
    if (!m_file_handle) {
        throw InvalidMediaException("Could not open file: " + path.to8Bit(false));
    }
    m_file_path = path.to8Bit(false);
}

size_t FileIOHandler::read(void* buffer, size_t size, size_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return read_unlocked(buffer, size, count);
}

size_t FileIOHandler::read_unlocked(void* buffer, size_t size, size_t count) {
    if (!m_file_handle || m_closed) {
        return 0;
    }
    size_t result = fread(buffer, size, count, m_file_handle);
    m_position = ftello(m_file_handle);
    m_eof = feof(m_file_handle);
    return result;
}

off_t FileIOHandler::getFileSize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    struct _stat64 file_stat;
    int fd = _fileno(m_file_handle);
    if (_fstat64(fd, &file_stat) != 0) {
        return -1;
    }
    return file_stat.st_size;
}

} // namespace File
} // namespace IO
} // namespace PsyMP3
```

**Unix/Linux**:
```cpp
namespace PsyMP3 {
namespace IO {
namespace File {

FileIOHandler::FileIOHandler(const TagLib::String& path) {
    m_file_handle = fopen(path.toCString(false), "rb");
    if (!m_file_handle) {
        throw InvalidMediaException("Could not open file: " + path.to8Bit(false));
    }
    m_file_path = path.to8Bit(false);
}

size_t FileIOHandler::read(void* buffer, size_t size, size_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return read_unlocked(buffer, size, count);
}

size_t FileIOHandler::read_unlocked(void* buffer, size_t size, size_t count) {
    if (!m_file_handle || m_closed) {
        return 0;
    }
    size_t result = fread(buffer, size, count, m_file_handle);
    m_position = ftello(m_file_handle);
    m_eof = feof(m_file_handle);
    return result;
}

off_t FileIOHandler::getFileSize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    struct stat file_stat;
    int fd = fileno(m_file_handle);
    if (fstat(fd, &file_stat) != 0) {
        return -1;
    }
    return file_stat.st_size;
}

} // namespace File
} // namespace IO
} // namespace PsyMP3
```

### **3. HTTPIOHandler Component**

**Purpose**: Handle HTTP streaming with range request support

**Key Features**:
- **Range Request Support**: HTTP/1.1 range requests for seeking
- **Efficient Buffering**: Configurable buffer sizes for optimal performance
- **Metadata Extraction**: Content-Type and Content-Length handling
- **Network Resilience**: Timeout handling and connection recovery

**Initialization Process**:
1. **HEAD Request**: Determine content metadata and server capabilities
2. **Content-Length**: Extract total file size if available
3. **Content-Type**: Extract MIME type for format detection
4. **Range Support**: Check Accept-Ranges header for seeking capability

**Buffering Strategy**:
```cpp
namespace PsyMP3 {
namespace IO {
namespace HTTP {

size_t HTTPIOHandler::read(void* buffer, size_t size, size_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return read_unlocked(buffer, size, count);
}

size_t HTTPIOHandler::read_unlocked(void* buffer, size_t size, size_t count) {
    if (!m_initialized) {
        initialize_unlocked();
    }
    
    size_t bytes_requested = size * count;
    size_t bytes_read = 0;
    
    while (bytes_read < bytes_requested) {
        if (!isPositionBuffered_unlocked(m_current_position)) {
            if (!fillBuffer_unlocked(m_current_position)) {
                break;
            }
        }
        
        size_t chunk = readFromBuffer_unlocked(
            static_cast<uint8_t*>(buffer) + bytes_read,
            bytes_requested - bytes_read
        );
        
        bytes_read += chunk;
        m_current_position += chunk;
        
        if (chunk == 0) {
            break;
        }
    }
    
    return bytes_read / size;
}

bool HTTPIOHandler::fillBuffer_unlocked(long position, size_t min_size) {
    size_t range_size = std::max(min_size, BUFFER_SIZE);
    
    if (m_content_length > 0) {
        range_size = std::min(range_size, 
                             static_cast<size_t>(m_content_length - position));
    }
    
    HTTPClient::Response response;
    if (m_supports_ranges || position > 0) {
        long end_byte = position + range_size - 1;
        response = HTTPClient::getRange(m_url, position, end_byte);
    } else {
        response = HTTPClient::get(m_url);
    }
    
    if (response.success) {
        m_buffer.assign(response.body.begin(), response.body.end());
        m_buffer_offset = 0;
        m_buffer_start_position = position;
        return true;
    }
    
    return false;
}

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3
```

### **4. HTTPClient Component**

**Purpose**: Provide robust HTTP client functionality

**Key Features**:
- **Multiple HTTP Methods**: GET, POST, HEAD, range requests
- **SSL/TLS Support**: HTTPS connections with certificate validation
- **Connection Management**: Efficient connection reuse and pooling
- **Error Handling**: Comprehensive error reporting and recovery

**Implementation Strategy**:
- **libcurl Backend**: Use libcurl for robust HTTP implementation
- **RAII Management**: Automatic resource cleanup and initialization
- **Thread Safety**: Safe for concurrent use from multiple threads
- **Performance**: Optimized for streaming media applications

**Range Request Implementation**:
```cpp
namespace PsyMP3 {
namespace IO {
namespace HTTP {

HTTPClient::Response HTTPClient::getRange(const std::string& url,
                                         long start_byte,
                                         long end_byte,
                                         const std::map<std::string, std::string>& headers,
                                         int timeoutSeconds) {
    std::map<std::string, std::string> range_headers = headers;
    if (end_byte >= 0) {
        range_headers["Range"] = "bytes=" + std::to_string(start_byte) + 
                                "-" + std::to_string(end_byte);
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
    
    // Initialize libcurl handle (thread-safe per-request)
    CURL* curl = curl_easy_init();
    if (!curl) {
        response.success = false;
        response.statusMessage = "Failed to initialize CURL";
        return response;
    }
    
    // RAII cleanup
    struct CURLCleanup {
        CURL* handle;
        ~CURLCleanup() { if (handle) curl_easy_cleanup(handle); }
    } cleanup{curl};
    
    // Configure request
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Set custom headers
    struct curl_slist* header_list = nullptr;
    for (const auto& header : headers) {
        std::string header_str = header.first + ": " + header.second;
        header_list = curl_slist_append(header_list, header_str.c_str());
    }
    if (header_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    if (header_list) {
        curl_slist_free_all(header_list);
    }
    
    if (res == CURLE_OK) {
        long status_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
        response.statusCode = static_cast<int>(status_code);
        response.success = (status_code >= 200 && status_code < 300);
    } else {
        response.success = false;
        response.statusMessage = curl_easy_strerror(res);
    }
    
    return response;
}

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3
```

## **Data Models**

### **I/O Operation Flow**

```
Client Request → IOHandler Interface → Concrete Implementation → System I/O → Data Return
```

### **HTTP Streaming Flow**

```
URL → HTTPIOHandler → HTTPClient → HTTP Request → Server Response → Buffer Management → Data Return
```

### **File Access Flow**

```
File Path → FileIOHandler → System File API → File Operations → Data Return
```

## **Error Handling**

### **File I/O Errors**
- **File Not Found**: Throw InvalidMediaException with descriptive message
- **Permission Denied**: Handle access permission errors gracefully
- **Disk Full**: Handle write errors (future extension)
- **Network Drives**: Handle network file system issues

### **HTTP Errors**
- **Connection Failures**: Retry with exponential backoff
- **HTTP Status Codes**: Handle 4xx and 5xx errors appropriately
- **Timeout Handling**: Configurable timeouts with proper cleanup
- **SSL/TLS Errors**: Certificate validation and secure connection issues

### **General I/O Errors**
- **EOF Conditions**: Proper end-of-file detection and handling
- **Partial Reads**: Handle incomplete read operations
- **Seek Failures**: Validate seek operations and handle failures
- **Resource Exhaustion**: Handle memory and file descriptor limits

## **Thread Safety**

### **Design Principle: Public/Private Lock Pattern**

All IOHandler implementations follow the mandatory public/private lock pattern to prevent deadlocks and ensure thread-safe operation:

**Pattern Structure**:
- **Public methods**: Acquire locks and call private `_unlocked` implementations
- **Private methods**: Append `_unlocked` suffix and perform work without acquiring locks
- **Internal calls**: Always use `_unlocked` versions within the same class

### **FileIOHandler Thread Safety**

**Synchronization Strategy**:
```cpp
class FileIOHandler : public IOHandler {
public:
    size_t read(void* buffer, size_t size, size_t count) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return read_unlocked(buffer, size, count);
    }
    
    int seek(long offset, int whence) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return seek_unlocked(offset, whence);
    }
    
private:
    size_t read_unlocked(void* buffer, size_t size, size_t count);
    int seek_unlocked(long offset, int whence);
    off_t tell_unlocked();
    
    mutable std::mutex m_mutex;
    FILE* m_file_handle;
};
```

**Key Features**:
- **Atomic Position Operations**: File position tracking protected by mutex
- **Consistent State**: All file operations maintain consistent internal state
- **Exception Safety**: RAII lock guards ensure locks released on exceptions
- **No Deadlocks**: Internal methods use `_unlocked` versions exclusively

### **HTTPIOHandler Thread Safety**

**Synchronization Strategy**:
```cpp
class HTTPIOHandler : public IOHandler {
public:
    size_t read(void* buffer, size_t size, size_t count) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return read_unlocked(buffer, size, count);
    }
    
    int seek(long offset, int whence) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return seek_unlocked(offset, whence);
    }
    
private:
    size_t read_unlocked(void* buffer, size_t size, size_t count);
    int seek_unlocked(long offset, int whence);
    bool fillBuffer_unlocked(long position, size_t min_size);
    
    mutable std::mutex m_mutex;
    std::vector<uint8_t> m_buffer;
    size_t m_buffer_offset;
    long m_buffer_start_position;
};
```

**Key Features**:
- **Buffer Protection**: All buffer operations protected from race conditions
- **Position Consistency**: Logical position and buffer state remain synchronized
- **Network Operation Safety**: HTTP requests coordinated with buffer state
- **Error State Management**: Thread-safe error reporting and state tracking

### **HTTPClient Thread Safety**

**Design Approach**:
- **Per-Request Isolation**: Each HTTP request uses independent libcurl handle
- **Thread-Safe libcurl**: Uses libcurl in thread-safe mode (CURL_GLOBAL_ALL)
- **No Shared State**: Static methods avoid shared mutable state
- **Connection Pooling**: If implemented, pool access protected by mutex

**Implementation Pattern**:
```cpp
class HTTPClient {
public:
    static Response get(const std::string& url, 
                       const std::map<std::string, std::string>& headers = {},
                       int timeoutSeconds = 30) {
        // Each call creates independent CURL handle
        // No shared state between concurrent calls
        return performRequest("GET", url, "", headers, timeoutSeconds);
    }
    
private:
    static Response performRequest(const std::string& method,
                                   const std::string& url,
                                   const std::string& postData,
                                   const std::map<std::string, std::string>& headers,
                                   int timeoutSeconds);
};
```

### **Lock Acquisition Order**

To prevent deadlocks when multiple locks are involved, the following order is documented and enforced:

```cpp
// Lock acquisition order (to prevent deadlocks):
// 1. Global/Static locks (e.g., libcurl global initialization)
// 2. IOHandler instance locks (e.g., FileIOHandler::m_mutex, HTTPIOHandler::m_mutex)
// 3. System locks (e.g., FILE* internal locks)
```

### **Thread Safety Guarantees**

**FileIOHandler**:
- ✅ Multiple threads can safely call read/seek/tell on same instance
- ✅ File position remains consistent across concurrent operations
- ✅ No data corruption from concurrent reads
- ⚠️ Concurrent reads may have unpredictable ordering (by design)

**HTTPIOHandler**:
- ✅ Multiple threads can safely call read/seek on same instance
- ✅ Buffer state remains consistent across concurrent operations
- ✅ Network requests coordinated to prevent conflicts
- ⚠️ Concurrent seeks may trigger multiple HTTP requests (expected behavior)

**HTTPClient**:
- ✅ Static methods safe for concurrent use from multiple threads
- ✅ No shared state between concurrent requests
- ✅ libcurl handles properly isolated per request
- ✅ Safe for use by multiple HTTPIOHandler instances simultaneously

### **Debugging and Logging Thread Safety**

All debug logging operations use PsyMP3's thread-safe Debug logging system:
- Debug output properly synchronized across threads
- No interleaved log messages from concurrent operations
- Thread IDs included in debug output for troubleshooting

## **Performance Considerations**

### **File I/O Optimization**
- **Large File Support**: 64-bit file operations for files >2GB
- **Sequential Access**: Optimize for forward reading patterns
- **Buffer Management**: Efficient buffer allocation and reuse
- **Platform Optimization**: Use platform-specific optimizations
- **Lock Overhead**: Minimal mutex overhead for file operations

### **HTTP Streaming Optimization**
- **Connection Reuse**: HTTP/1.1 keep-alive connections
- **Range Request Optimization**: Minimize HTTP requests through intelligent buffering
- **Compression Support**: Handle gzip/deflate content encoding
- **Parallel Connections**: Multiple connections for improved throughput
- **Lock Contention**: Minimize lock hold time during network operations

### **Memory Management**
- **Bounded Buffers**: Prevent memory exhaustion with configurable limits
- **Buffer Reuse**: Minimize allocation/deallocation overhead
- **Smart Pointers**: Automatic resource management and cleanup
- **Memory Pools**: Efficient allocation for frequently used buffers
- **Thread-Local Buffers**: Consider thread-local storage for high-frequency operations

### **Network Efficiency**
- **Read-Ahead Buffering**: Anticipate future read requests
- **Adaptive Buffer Sizes**: Adjust buffer sizes based on network conditions
- **Connection Pooling**: Reuse connections across multiple requests
- **Bandwidth Management**: Respect network bandwidth limitations
- **Concurrent Requests**: Support multiple simultaneous HTTP requests when beneficial

## **Cross-Platform Compatibility**

### **File System Differences**
- **Path Separators**: Handle Windows backslash vs Unix forward slash
- **Unicode Handling**: UTF-16 on Windows, UTF-8 on Unix
- **File Permissions**: Different permission models across platforms
- **Case Sensitivity**: Handle case-sensitive vs case-insensitive filesystems

### **Network Stack Differences**
- **Socket APIs**: Winsock vs BSD sockets abstraction
- **SSL/TLS Libraries**: Consistent OpenSSL usage across platforms
- **DNS Resolution**: Platform-specific hostname resolution
- **Network Interface**: Handle different network interface APIs

### **Compiler and Runtime Differences**
- **Standard Library**: Handle different C++ standard library implementations
- **Threading**: Platform-specific threading primitives
- **Error Codes**: Consistent error reporting across platforms
- **Build Systems**: Support for different build environments

## **Security Considerations**

### **File Access Security**
- **Path Validation**: Prevent directory traversal attacks
- **Permission Checking**: Validate file access permissions
- **Symlink Handling**: Secure handling of symbolic links
- **Temporary Files**: Secure temporary file creation and cleanup

### **Network Security**
- **SSL/TLS Validation**: Proper certificate validation for HTTPS
- **URL Validation**: Prevent malicious URL handling
- **Input Sanitization**: Validate all network input data
- **Timeout Protection**: Prevent denial-of-service through timeouts

### **Memory Safety**
- **Buffer Overflow Protection**: Bounds checking for all buffer operations
- **Integer Overflow**: Prevent overflow in size calculations
- **Resource Limits**: Enforce reasonable limits on resource usage
- **Exception Safety**: Maintain security invariants during exceptions

## **Testing Strategy**

### **Unit Testing Approach**

**FileIOHandler Tests**:
- **Basic Operations**: Read, seek, tell, eof, getFileSize on various file types
- **Unicode Filenames**: Test international characters on all platforms
- **Large Files**: Verify >2GB file support with 64-bit operations
- **Error Conditions**: File not found, permission denied, invalid operations
- **Edge Cases**: Empty files, single-byte files, boundary conditions
- **Platform-Specific**: Windows Unicode paths, Unix encoding preservation

**HTTPIOHandler Tests**:
- **Initialization**: HEAD request metadata extraction and parsing
- **Sequential Reading**: Efficient buffering without excessive requests
- **Range Requests**: Seeking with HTTP range support
- **Non-Seekable Streams**: Proper error handling when ranges not supported
- **Buffer Management**: Buffer reuse, position tracking, EOF detection
- **Network Errors**: Timeout handling, connection failures, HTTP errors
- **Edge Cases**: Zero-length content, missing headers, redirect handling

**HTTPClient Tests**:
- **HTTP Methods**: GET, POST, HEAD with various parameters
- **HTTPS Support**: SSL/TLS connections and certificate validation
- **Range Requests**: Partial content retrieval with byte ranges
- **Error Handling**: Network failures, HTTP status codes, timeouts
- **URL Operations**: Encoding, parsing, validation
- **Header Management**: Custom headers, response header parsing

### **Integration Testing Approach**

**Demuxer Integration**:
- Test IOHandler with actual demuxer implementations (FLAC, Ogg, ISO)
- Verify seamless operation across different container formats
- Test error propagation through demuxer chain
- Validate performance with real media files

**MediaFactory Integration**:
- Test URI-based IOHandler selection (file://, http://, https://)
- Verify automatic handler creation based on URI scheme
- Test error reporting through factory layer
- Validate configuration parameter handling

**PsyMP3 Core Integration**:
- Test with PsyMP3 Debug logging system
- Verify exception hierarchy integration
- Test configuration system integration
- Validate resource management patterns

### **Thread Safety Testing**

**Concurrent Access Tests**:
- Multiple threads reading from same FileIOHandler instance
- Concurrent seeks and reads on HTTPIOHandler
- Simultaneous HTTP requests from multiple threads
- Stress testing with high concurrency levels

**Deadlock Prevention Tests**:
- Verify public/private lock pattern prevents deadlocks
- Test scenarios that would deadlock with incorrect patterns
- Validate lock acquisition order compliance
- Test exception safety with concurrent access

**Race Condition Tests**:
- Concurrent buffer operations in HTTPIOHandler
- Simultaneous position updates in FileIOHandler
- Shared resource access patterns
- Error state consistency across threads

### **Performance Testing**

**Benchmarks**:
- File I/O throughput vs. direct fread operations
- HTTP streaming performance with various buffer sizes
- Seek operation latency for local and network sources
- Memory usage under various workloads

**Stress Tests**:
- Large file handling (>4GB files)
- High-bitrate network streams
- Rapid seeking patterns
- Extended operation duration (memory leak detection)

**Regression Tests**:
- Compare performance against baseline measurements
- Ensure no degradation from previous implementations
- Validate memory usage remains bounded
- Verify no performance impact on existing functionality

### **Cross-Platform Testing**

**Platform Coverage**:
- Windows (Unicode paths, _wfopen, _fstat64)
- Linux (UTF-8 paths, POSIX APIs)
- macOS (BSD APIs, filesystem specifics)
- BSD variants (FreeBSD, OpenBSD compatibility)

**Platform-Specific Tests**:
- Unicode filename handling on each platform
- Large file support verification
- Network stack differences
- Error code consistency

### **Error Handling Testing**

**File Errors**:
- File not found scenarios
- Permission denied conditions
- Disk full situations (write operations)
- Network filesystem issues

**Network Errors**:
- Connection timeouts
- DNS resolution failures
- HTTP error status codes (4xx, 5xx)
- SSL/TLS certificate errors
- Partial response handling

**Resource Exhaustion**:
- Memory allocation failures
- File descriptor limits
- Network connection limits
- Buffer overflow prevention

### **Test Automation**

**Continuous Integration**:
- Automated test execution on all supported platforms
- Performance regression detection
- Memory leak detection with valgrind/sanitizers
- Thread safety validation with ThreadSanitizer

**Test Coverage Goals**:
- >90% code coverage for core IOHandler implementations
- 100% coverage of error handling paths
- All public API methods tested
- All platform-specific code paths validated

## **Integration Points**

### **With Demuxer Architecture**
- **Consistent Interface**: All demuxers use IOHandler exclusively
- **Error Propagation**: Proper error handling through demuxer chain
- **Performance**: Optimized for media streaming requirements
- **Extensibility**: Support for future I/O sources and protocols

### **With MediaFactory**
- **URI Handling**: Automatic IOHandler selection based on URI scheme
- **Format Detection**: Integration with content detection system
- **Error Reporting**: Consistent error reporting through factory
- **Configuration**: Configurable I/O parameters through factory

### **With PsyMP3 Core**
- **Logging Integration**: Use PsyMP3 Debug logging system with categories ("io", "http", "file")
- **Exception Hierarchy**: Integrate with PsyMP3 exception system (InvalidMediaException)
- **Configuration System**: Respect PsyMP3 configuration settings
- **Thread Management**: Coordinate with PsyMP3 threading model using public/private lock pattern

This design provides a robust, efficient, and extensible I/O abstraction that serves as the foundation for all media access in PsyMP3, with excellent cross-platform support, thread safety, and performance characteristics.
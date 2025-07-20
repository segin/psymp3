# **IOHANDLER SUBSYSTEM DESIGN**

## **Overview**

This design document specifies the implementation of the IOHandler subsystem in PsyMP3, which provides a unified abstraction layer for accessing media content from various sources. The subsystem consists of the base IOHandler interface and concrete implementations for different I/O sources including local files, HTTP streams, and potentially other protocols.

The design emphasizes cross-platform compatibility, performance, and extensibility while providing a consistent interface for all media access operations.

## **Architecture**

### **Class Hierarchy**

```cpp
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
    FILE* m_file_handle;
    std::string m_file_path;
};

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
    
    void initialize();
    bool fillBuffer(long position, size_t min_size = MIN_RANGE_SIZE);
    size_t readFromBuffer(void* buffer, size_t bytes_to_read);
    bool isPositionBuffered(long position) const;
};
```

### **HTTP Client Foundation**

```cpp
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
```

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

**Platform-Specific Implementation**:

**Windows**:
```cpp
FileIOHandler::FileIOHandler(const TagLib::String& path) {
    m_file_handle = _wfopen(path.toCWString(), L"rb");
    if (!m_file_handle) {
        throw InvalidMediaException("Could not open file: " + path.to8Bit(false));
    }
}

off_t FileIOHandler::getFileSize() {
    struct _stat64 file_stat;
    int fd = _fileno(m_file_handle);
    if (_fstat64(fd, &file_stat) != 0) {
        return -1;
    }
    return file_stat.st_size;
}
```

**Unix/Linux**:
```cpp
FileIOHandler::FileIOHandler(const TagLib::String& path) {
    m_file_handle = fopen(path.toCString(false), "rb");
    if (!m_file_handle) {
        throw InvalidMediaException("Could not open file: " + path.to8Bit(false));
    }
}

off_t FileIOHandler::getFileSize() {
    struct stat file_stat;
    int fd = fileno(m_file_handle);
    if (fstat(fd, &file_stat) != 0) {
        return -1;
    }
    return file_stat.st_size;
}
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
bool HTTPIOHandler::fillBuffer(long position, size_t min_size) {
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

## **Performance Considerations**

### **File I/O Optimization**
- **Large File Support**: 64-bit file operations for files >2GB
- **Sequential Access**: Optimize for forward reading patterns
- **Buffer Management**: Efficient buffer allocation and reuse
- **Platform Optimization**: Use platform-specific optimizations

### **HTTP Streaming Optimization**
- **Connection Reuse**: HTTP/1.1 keep-alive connections
- **Range Request Optimization**: Minimize HTTP requests through intelligent buffering
- **Compression Support**: Handle gzip/deflate content encoding
- **Parallel Connections**: Multiple connections for improved throughput

### **Memory Management**
- **Bounded Buffers**: Prevent memory exhaustion with configurable limits
- **Buffer Reuse**: Minimize allocation/deallocation overhead
- **Smart Pointers**: Automatic resource management and cleanup
- **Memory Pools**: Efficient allocation for frequently used buffers

### **Network Efficiency**
- **Read-Ahead Buffering**: Anticipate future read requests
- **Adaptive Buffer Sizes**: Adjust buffer sizes based on network conditions
- **Connection Pooling**: Reuse connections across multiple requests
- **Bandwidth Management**: Respect network bandwidth limitations

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
- **Logging Integration**: Use PsyMP3 Debug logging system
- **Exception Hierarchy**: Integrate with PsyMP3 exception system
- **Configuration System**: Respect PsyMP3 configuration settings
- **Thread Management**: Coordinate with PsyMP3 threading model

This design provides a robust, efficient, and extensible I/O abstraction that serves as the foundation for all media access in PsyMP3, with excellent cross-platform support and performance characteristics.
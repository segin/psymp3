# FLAC Demuxer Troubleshooting Guide

## Common Issues and Solutions

### Container Parsing Issues

#### "Failed to parse FLAC container"

**Symptoms:**
- `parseContainer()` returns false
- No streams detected
- Error messages about invalid stream marker

**Possible Causes:**
1. **Invalid fLaC stream marker**
   - File is not a valid FLAC file
   - File corruption at the beginning
   - Wrong file format (e.g., Ogg FLAC instead of native FLAC)

2. **Corrupted metadata blocks**
   - Damaged STREAMINFO block
   - Invalid block headers
   - Truncated metadata

3. **I/O errors**
   - File access permissions
   - Network connectivity issues
   - Disk errors

**Diagnostic Steps:**
```cpp
// Check if file exists and is readable
auto handler = std::make_unique<FileIOHandler>("test.flac");
if (!handler->isValid()) {
    std::cerr << "Cannot access file" << std::endl;
}

// Enable debug logging to see parsing details
Debug::setLevel(Debug::VERBOSE);
auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
bool success = demuxer->parseContainer();
```

**Solutions:**
1. **Verify file format:**
   ```bash
   file music.flac  # Should show "FLAC audio bitstream data"
   flac -t music.flac  # Test file integrity with FLAC tools
   ```

2. **Check file integrity:**
   ```bash
   md5sum music.flac  # Compare with known good checksum
   hexdump -C music.flac | head  # Should start with "66 4C 61 43" (fLaC)
   ```

3. **Test with minimal file:**
   ```cpp
   // Try with a known-good FLAC file first
   auto test_demuxer = std::make_unique<FLACDemuxer>(
       std::make_unique<FileIOHandler>("test_reference.flac"));
   ```

#### "STREAMINFO block missing or invalid"

**Symptoms:**
- Container parsing succeeds but no stream information
- Zero duration or invalid audio parameters
- Playback fails to start

**Possible Causes:**
- STREAMINFO is not the first metadata block
- STREAMINFO block is corrupted
- Invalid audio parameters in STREAMINFO

**Diagnostic Steps:**
```cpp
auto streams = demuxer->getStreams();
if (!streams.empty()) {
    const auto& stream = streams[0];
    std::cout << "Sample rate: " << stream.sample_rate << std::endl;
    std::cout << "Channels: " << stream.channels << std::endl;
    std::cout << "Bit depth: " << stream.bits_per_sample << std::endl;
    std::cout << "Total samples: " << stream.total_samples << std::endl;
}
```

**Solutions:**
1. **Re-encode file with proper STREAMINFO:**
   ```bash
   flac --decode music.flac
   flac --best music.wav  # Re-encode with proper metadata
   ```

2. **Use recovery mode in demuxer:**
   - Demuxer will attempt to derive parameters from first frame
   - May result in approximate duration calculation

### Seeking Issues

#### "Seek operations fail or are inaccurate"

**Symptoms:**
- `seekTo()` returns false
- Seeking to wrong position
- Very slow seeking performance

**Possible Causes:**
1. **Missing SEEKTABLE**
   - File encoded without seek table
   - Seek table corrupted or invalid

2. **Variable bitrate complexity**
   - Binary search estimation is poor
   - Frame structure is irregular

3. **Network stream limitations**
   - HTTP range requests not supported
   - High latency affecting seek performance

**Diagnostic Steps:**
```cpp
// Check if file has seek table
auto streams = demuxer->getStreams();
if (!streams.empty()) {
    // Check metadata for SEEKTABLE presence
    // (Implementation would expose seek table info)
}

// Test different seeking strategies
uint64_t duration = demuxer->getDuration();
std::vector<uint64_t> test_positions = {
    duration / 4, duration / 2, duration * 3 / 4
};

for (uint64_t pos : test_positions) {
    auto start_time = std::chrono::high_resolution_clock::now();
    bool success = demuxer->seekTo(pos);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto seek_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    std::cout << "Seek to " << pos << "ms: " 
              << (success ? "SUCCESS" : "FAILED") 
              << " (" << seek_time << "ms)" << std::endl;
}
```

**Solutions:**
1. **Add seek table to file:**
   ```bash
   flac --decode music.flac
   flac --best --seekpoint=1s music.wav  # Add seek points every second
   ```

2. **Optimize for network streams:**
   ```cpp
   // For HTTP streams, consider local caching
   auto cache_handler = std::make_unique<CachedIOHandler>(
       std::make_unique<HTTPIOHandler>("http://example.com/music.flac"));
   ```

3. **Use progressive seeking:**
   ```cpp
   // For very large files, seek in smaller increments
   uint64_t target = target_position;
   uint64_t current = demuxer->getPosition();
   uint64_t step = std::min(target - current, 30000ULL); // 30 second steps
   
   while (current < target) {
       demuxer->seekTo(current + step);
       current = demuxer->getPosition();
       step = std::min(target - current, step);
   }
   ```

#### "Seeking is very slow"

**Symptoms:**
- Seek operations take several seconds
- UI becomes unresponsive during seeking
- High CPU usage during seeks

**Possible Causes:**
- No SEEKTABLE, falling back to linear search
- Very large file with complex frame structure
- Network latency for HTTP streams
- Inefficient I/O operations

**Solutions:**
1. **Profile seeking performance:**
   ```cpp
   // Measure different seeking strategies
   auto measure_seek = [&](uint64_t pos, const std::string& strategy) {
       auto start = std::chrono::high_resolution_clock::now();
       bool success = demuxer->seekTo(pos);
       auto end = std::chrono::high_resolution_clock::now();
       auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
       std::cout << strategy << " seek: " << duration.count() << "ms" << std::endl;
       return success;
   };
   ```

2. **Optimize file encoding:**
   ```bash
   # Re-encode with optimal seek table
   flac --decode music.flac
   flac --best --seekpoint=10s music.wav  # Seek points every 10 seconds
   ```

3. **Implement seek caching:**
   ```cpp
   // Cache recent seek positions for faster repeated seeks
   class SeekCache {
       std::map<uint64_t, uint64_t> position_cache; // timestamp -> file_offset
   public:
       bool findNearestCachedPosition(uint64_t target, uint64_t& cached_pos);
       void cachePosition(uint64_t timestamp, uint64_t file_offset);
   };
   ```

### Memory Usage Issues

#### "High memory consumption"

**Symptoms:**
- Application memory usage grows over time
- Out of memory errors with large files
- Slow performance due to memory pressure

**Possible Causes:**
1. **Large embedded pictures**
   - High-resolution album artwork
   - Multiple pictures in single file
   - Pictures not being released from cache

2. **Large seek tables**
   - Files with excessive seek points
   - Memory not being freed properly

3. **Buffer accumulation**
   - Frame buffers not being reused
   - Memory leaks in error paths

**Diagnostic Steps:**
```cpp
// Monitor memory usage
class MemoryMonitor {
public:
    static size_t getCurrentMemoryUsage() {
        // Platform-specific memory usage detection
        #ifdef __linux__
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                return std::stoul(line.substr(7)) * 1024; // Convert KB to bytes
            }
        }
        #endif
        return 0;
    }
};

// Check memory before and after operations
size_t before = MemoryMonitor::getCurrentMemoryUsage();
auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
demuxer->parseContainer();
size_t after = MemoryMonitor::getCurrentMemoryUsage();
std::cout << "Memory used by demuxer: " << (after - before) << " bytes" << std::endl;
```

**Solutions:**
1. **Limit picture caching:**
   ```cpp
   // Clear picture cache after extracting artwork
   // (Implementation would provide access to FLACPicture::clearCache())
   ```

2. **Monitor seek table size:**
   ```cpp
   // Check seek table size during parsing
   // Limit to MAX_SEEK_TABLE_ENTRIES constant
   ```

3. **Use memory-mapped I/O for large files:**
   ```cpp
   // Consider memory-mapped file access for very large files
   auto mmap_handler = std::make_unique<MemoryMappedIOHandler>("large_file.flac");
   ```

### Performance Issues

#### "Slow frame reading performance"

**Symptoms:**
- Audio dropouts during playback
- High CPU usage during streaming
- Slow response to user controls

**Possible Causes:**
- Inefficient frame sync detection
- Excessive I/O operations
- Poor buffer management
- Network latency for HTTP streams

**Diagnostic Steps:**
```cpp
// Profile frame reading performance
auto start_time = std::chrono::high_resolution_clock::now();
int frames_read = 0;
const int test_frames = 1000;

while (frames_read < test_frames && !demuxer->isEOF()) {
    MediaChunk chunk = demuxer->readChunk();
    if (!chunk.data.empty()) {
        frames_read++;
    }
}

auto end_time = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
double frames_per_second = (frames_read * 1000.0) / duration.count();

std::cout << "Frame reading performance: " << frames_per_second << " frames/sec" << std::endl;
```

**Solutions:**
1. **Optimize buffer sizes:**
   ```cpp
   // Tune buffer sizes for your use case
   static constexpr size_t OPTIMAL_FRAME_BUFFER_SIZE = 128 * 1024; // 128KB
   static constexpr size_t OPTIMAL_SYNC_BUFFER_SIZE = 16384;       // 16KB
   ```

2. **Implement read-ahead for network streams:**
   ```cpp
   // Pre-fetch data for network streams
   class ReadAheadBuffer {
       std::vector<uint8_t> buffer;
       size_t read_pos = 0;
       size_t write_pos = 0;
   public:
       void prefetch(IOHandler* handler, size_t amount);
       size_t read(uint8_t* dest, size_t size);
   };
   ```

3. **Use threading for I/O:**
   ```cpp
   // Separate I/O thread for network streams
   class AsyncIOHandler {
       std::thread io_thread;
       std::queue<IORequest> request_queue;
       std::mutex queue_mutex;
   public:
       void asyncRead(uint64_t offset, size_t size, std::function<void(std::vector<uint8_t>)> callback);
   };
   ```

### Thread Safety Issues

#### "Crashes or corruption in multi-threaded usage"

**Symptoms:**
- Random crashes during concurrent access
- Corrupted position tracking
- Inconsistent metadata access

**Possible Causes:**
- Race conditions in position tracking
- Unsafe metadata access
- Improper lock ordering

**Diagnostic Steps:**
```cpp
// Test thread safety with concurrent operations
#include <thread>
#include <vector>

void testThreadSafety(FLACDemuxer* demuxer) {
    std::vector<std::thread> threads;
    std::atomic<bool> stop_flag{false};
    std::atomic<int> error_count{0};
    
    // Thread 1: Continuous reading
    threads.emplace_back([&]() {
        while (!stop_flag.load()) {
            try {
                MediaChunk chunk = demuxer->readChunk();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (...) {
                error_count.fetch_add(1);
            }
        }
    });
    
    // Thread 2: Random seeking
    threads.emplace_back([&]() {
        uint64_t duration = demuxer->getDuration();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint64_t> dist(0, duration);
        
        while (!stop_flag.load()) {
            try {
                demuxer->seekTo(dist(gen));
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (...) {
                error_count.fetch_add(1);
            }
        }
    });
    
    // Thread 3: Position monitoring
    threads.emplace_back([&]() {
        while (!stop_flag.load()) {
            try {
                uint64_t pos = demuxer->getPosition();
                uint64_t sample = demuxer->getCurrentSample();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            } catch (...) {
                error_count.fetch_add(1);
            }
        }
    });
    
    // Run test for 10 seconds
    std::this_thread::sleep_for(std::chrono::seconds(10));
    stop_flag.store(true);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Thread safety test completed. Errors: " << error_count.load() << std::endl;
}
```

**Solutions:**
1. **Verify lock ordering:**
   - Always acquire m_state_mutex before m_metadata_mutex
   - Use RAII lock guards consistently
   - Avoid nested locking where possible

2. **Use atomic operations for frequently accessed data:**
   ```cpp
   std::atomic<uint64_t> m_current_sample{0};  // Fast lock-free reads
   ```

3. **Test with thread sanitizer:**
   ```bash
   g++ -fsanitize=thread -g -o test_flac test_flac.cpp
   ./test_flac  # Will detect race conditions
   ```

### Network Stream Issues

#### "HTTP stream playback problems"

**Symptoms:**
- Frequent buffering or dropouts
- Seeking not working with HTTP streams
- Connection timeouts

**Possible Causes:**
- Server doesn't support HTTP range requests
- Network latency or bandwidth limitations
- Inefficient streaming strategy

**Diagnostic Steps:**
```cpp
// Test HTTP stream capabilities
void testHTTPStream(const std::string& url) {
    auto http_handler = std::make_unique<HTTPIOHandler>(url);
    
    // Test basic connectivity
    if (!http_handler->isValid()) {
        std::cerr << "Cannot connect to HTTP stream" << std::endl;
        return;
    }
    
    // Test range request support
    bool supports_ranges = http_handler->supportsRangeRequests();
    std::cout << "Range requests supported: " << (supports_ranges ? "YES" : "NO") << std::endl;
    
    // Test seeking capability
    auto demuxer = std::make_unique<FLACDemuxer>(std::move(http_handler));
    if (demuxer->parseContainer()) {
        uint64_t duration = demuxer->getDuration();
        bool seek_success = demuxer->seekTo(duration / 2);
        std::cout << "Seeking works: " << (seek_success ? "YES" : "NO") << std::endl;
    }
}
```

**Solutions:**
1. **Implement progressive download:**
   ```cpp
   // Cache stream data locally for better seeking
   class ProgressiveDownloader {
       std::string cache_file;
       std::atomic<uint64_t> downloaded_bytes{0};
   public:
       void startDownload(const std::string& url);
       std::unique_ptr<IOHandler> createCachedHandler();
   };
   ```

2. **Optimize for streaming:**
   ```cpp
   // Use larger buffers for network streams
   if (isNetworkStream()) {
       m_readahead_buffer.reserve(1024 * 1024); // 1MB read-ahead
   }
   ```

3. **Handle connection errors gracefully:**
   ```cpp
   // Implement retry logic for network errors
   class RetryableHTTPHandler : public HTTPIOHandler {
       int max_retries = 3;
       std::chrono::milliseconds retry_delay{1000};
   public:
       size_t read(uint8_t* buffer, size_t size) override {
           for (int attempt = 0; attempt < max_retries; ++attempt) {
               try {
                   return HTTPIOHandler::read(buffer, size);
               } catch (const NetworkException& e) {
                   if (attempt == max_retries - 1) throw;
                   std::this_thread::sleep_for(retry_delay);
               }
           }
           return 0;
       }
   };
   ```

## Debugging Tools and Techniques

### Enable Debug Logging

```cpp
// Enable verbose logging to see internal operations
Debug::setLevel(Debug::VERBOSE);
Debug::enableCategory(Debug::DEMUXER);
Debug::enableCategory(Debug::FLAC);
```

### Frame Analysis Tools

```cpp
// Analyze frame structure for debugging
void analyzeFrameStructure(FLACDemuxer* demuxer) {
    std::map<uint32_t, int> block_size_histogram;
    std::map<uint32_t, int> sample_rate_histogram;
    
    while (!demuxer->isEOF()) {
        MediaChunk chunk = demuxer->readChunk();
        if (chunk.data.empty()) break;
        
        // Parse frame header to analyze structure
        // (Implementation would expose frame analysis methods)
    }
    
    std::cout << "Block size distribution:" << std::endl;
    for (const auto& [size, count] : block_size_histogram) {
        std::cout << "  " << size << " samples: " << count << " frames" << std::endl;
    }
}
```

### Memory Leak Detection

```cpp
// Use RAII wrapper to detect memory leaks
class MemoryTracker {
    size_t initial_memory;
public:
    MemoryTracker() : initial_memory(getCurrentMemoryUsage()) {}
    ~MemoryTracker() {
        size_t final_memory = getCurrentMemoryUsage();
        if (final_memory > initial_memory) {
            std::cout << "Potential memory leak: " 
                      << (final_memory - initial_memory) << " bytes" << std::endl;
        }
    }
};

// Use in tests
{
    MemoryTracker tracker;
    auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
    // ... perform operations ...
} // Destructor will report any leaks
```

### Performance Profiling

```cpp
// Profile different operations
class PerformanceProfiler {
    std::map<std::string, std::chrono::nanoseconds> timings;
    std::chrono::high_resolution_clock::time_point start_time;
    std::string current_operation;
    
public:
    void startOperation(const std::string& name) {
        current_operation = name;
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    void endOperation() {
        auto end_time = std::chrono::high_resolution_clock::now();
        timings[current_operation] += (end_time - start_time);
    }
    
    void printReport() {
        for (const auto& [operation, total_time] : timings) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_time);
            std::cout << operation << ": " << ms.count() << "ms" << std::endl;
        }
    }
};
```

## Best Practices for Troubleshooting

1. **Start with known-good files**: Test with reference FLAC files first
2. **Enable debug logging**: Use verbose logging to understand internal operations
3. **Test incrementally**: Isolate issues by testing individual components
4. **Monitor resources**: Track memory usage and I/O operations
5. **Use profiling tools**: Identify performance bottlenecks
6. **Test edge cases**: Try unusual files (very large, minimal metadata, etc.)
7. **Verify thread safety**: Test concurrent access patterns
8. **Check network conditions**: For HTTP streams, test with different network conditions

## Getting Help

If you encounter issues not covered in this guide:

1. **Check the logs**: Enable debug logging and examine the output
2. **Create minimal test case**: Isolate the problem with a simple reproduction case
3. **Gather system information**: OS, compiler version, library versions
4. **Test with reference tools**: Compare behavior with standard FLAC tools
5. **Document the issue**: Include file details, error messages, and steps to reproduce
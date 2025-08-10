# PsyMP3 Demuxer Architecture Troubleshooting Guide

## Table of Contents
- [Common Integration Issues](#common-integration-issues)
- [Format Detection Problems](#format-detection-problems)
- [Parsing and Reading Issues](#parsing-and-reading-issues)
- [Memory and Performance Issues](#memory-and-performance-issues)
- [Thread Safety Issues](#thread-safety-issues)
- [Plugin Development Issues](#plugin-development-issues)
- [Debugging Tools and Techniques](#debugging-tools-and-techniques)
- [Error Code Reference](#error-code-reference)

## Common Integration Issues

### Issue: DemuxerFactory Returns Null

**Symptoms:**
- `DemuxerFactory::createDemuxer()` returns `nullptr`
- No error message provided

**Root Causes:**
1. **Unsupported file format**
2. **File not found or inaccessible**
3. **Corrupted file header**
4. **Missing plugin for the format**

**Diagnostic Steps:**

```cpp
// Step 1: Verify file exists and is readable
auto handler = std::make_unique<FileIOHandler>(filename);
if (!handler || handler->tell() < 0) {
    std::cerr << "Cannot open file: " << filename << std::endl;
    return;
}

// Step 2: Check format detection
std::string detected_format = DemuxerFactory::probeFormat(handler.get());
std::cout << "Detected format: " << (detected_format.empty() ? "UNKNOWN" : detected_format) << std::endl;

// Step 3: Examine file header
handler->seek(0, SEEK_SET);
uint8_t header[32];
size_t bytes_read = handler->read(header, 1, sizeof(header));

std::cout << "File header (" << bytes_read << " bytes): ";
for (size_t i = 0; i < bytes_read; ++i) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(header[i]) << " ";
}
std::cout << std::endl;

// Step 4: Check registered formats
auto supported_formats = MediaFactory::getSupportedFormats();
std::cout << "Supported formats:" << std::endl;
for (const auto& format : supported_formats) {
    std::cout << "  " << format.format_id << ": " << format.display_name << std::endl;
}
```

**Solutions:**

1. **For unsupported formats:**
   ```cpp
   // Register custom format or load appropriate plugin
   registerMyFormatPlugin();
   ```

2. **For file access issues:**
   ```cpp
   // Check file permissions and path
   if (access(filename.c_str(), R_OK) != 0) {
       std::cerr << "File not readable: " << strerror(errno) << std::endl;
   }
   ```

3. **For corrupted headers:**
   ```cpp
   // Try with error recovery enabled
   auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
   if (demuxer) {
       // Enable fallback mode before parsing
       demuxer->clearError();
       if (!demuxer->parseContainer()) {
           const auto& error = demuxer->getLastError();
           if (error.recovery != DemuxerErrorRecovery::NONE) {
               std::cout << "Attempting recovery..." << std::endl;
               // Recovery is attempted automatically
           }
       }
   }
   ```

### Issue: parseContainer() Fails

**Symptoms:**
- `parseContainer()` returns `false`
- Error information available via `getLastError()`

**Diagnostic Approach:**

```cpp
auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
if (!demuxer->parseContainer()) {
    const auto& error = demuxer->getLastError();
    
    std::cout << "Parse Error Details:" << std::endl;
    std::cout << "  Category: " << error.category << std::endl;
    std::cout << "  Message: " << error.message << std::endl;
    std::cout << "  Error Code: " << error.error_code << std::endl;
    std::cout << "  File Offset: " << error.file_offset << std::endl;
    std::cout << "  Recovery: " << static_cast<int>(error.recovery) << std::endl;
    
    // Analyze error category
    if (error.category == "IO") {
        handleIOError(error);
    } else if (error.category == "Format") {
        handleFormatError(error);
    } else if (error.category == "Memory") {
        handleMemoryError(error);
    }
}
```

**Category-Specific Solutions:**

1. **I/O Errors:**
   ```cpp
   void handleIOError(const DemuxerError& error) {
       std::cout << "I/O Error at offset " << error.file_offset << std::endl;
       
       // Check if file was truncated
       auto handler = std::make_unique<FileIOHandler>(filename);
       handler->seek(0, SEEK_END);
       long file_size = handler->tell();
       
       if (error.file_offset >= static_cast<uint64_t>(file_size)) {
           std::cout << "File appears to be truncated" << std::endl;
           std::cout << "Expected data at offset " << error.file_offset 
                     << " but file is only " << file_size << " bytes" << std::endl;
       }
   }
   ```

2. **Format Errors:**
   ```cpp
   void handleFormatError(const DemuxerError& error) {
       std::cout << "Format Error: " << error.message << std::endl;
       
       if (error.recovery == DemuxerErrorRecovery::SKIP_SECTION) {
           std::cout << "Attempting to skip corrupted section..." << std::endl;
           // Recovery is automatic, but you can retry parsing
       } else if (error.recovery == DemuxerErrorRecovery::FALLBACK_MODE) {
           std::cout << "Enabling fallback parsing mode..." << std::endl;
           // Fallback mode is enabled automatically
       }
   }
   ```

### Issue: No Streams Found

**Symptoms:**
- `parseContainer()` succeeds but `getStreams()` returns empty vector
- Container appears valid but no playable content

**Diagnostic Steps:**

```cpp
auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
if (demuxer->parseContainer()) {
    auto streams = demuxer->getStreams();
    
    if (streams.empty()) {
        std::cout << "No streams found in container" << std::endl;
        
        // Check if this is a data-only container
        uint64_t duration = demuxer->getDuration();
        std::cout << "Container duration: " << duration << "ms" << std::endl;
        
        // Try reading raw chunks to see if data exists
        MediaChunk chunk = demuxer->readChunk();
        if (chunk.isValid()) {
            std::cout << "Found raw data chunk (" << chunk.getDataSize() << " bytes)" << std::endl;
            std::cout << "Stream ID: " << chunk.stream_id << std::endl;
        } else {
            std::cout << "No data chunks available" << std::endl;
        }
    } else {
        // Analyze found streams
        for (const auto& stream : streams) {
            std::cout << "Stream " << stream.stream_id << ":" << std::endl;
            std::cout << "  Type: " << stream.codec_type << std::endl;
            std::cout << "  Codec: " << stream.codec_name << std::endl;
            std::cout << "  Valid: " << (stream.isValid() ? "YES" : "NO") << std::endl;
        }
    }
}
```

**Solutions:**

1. **For containers with unrecognized codecs:**
   ```cpp
   // Check if codec support is available
   for (const auto& stream : streams) {
       if (stream.codec_name == "unknown" || stream.codec_name.empty()) {
           std::cout << "Stream " << stream.stream_id 
                     << " has unknown codec (tag: " << stream.codec_tag << ")" << std::endl;
           
           // You may need to add codec support or update format detection
       }
   }
   ```

2. **For containers with only non-audio streams:**
   ```cpp
   bool has_audio = false;
   for (const auto& stream : streams) {
       if (stream.isAudio()) {
           has_audio = true;
           break;
       }
   }
   
   if (!has_audio) {
       std::cout << "Container has no audio streams" << std::endl;
       // This might be a video file or data container
   }
   ```

## Format Detection Problems

### Issue: Wrong Format Detected

**Symptoms:**
- File is detected as wrong format
- Parsing fails with format-specific errors
- Known format not recognized

**Debugging Format Detection:**

```cpp
class FormatDetectionDebugger {
public:
    static void debugDetection(const std::string& filename) {
        auto handler = std::make_unique<FileIOHandler>(filename);
        
        // Test all registered signatures
        auto signatures = DemuxerFactory::getSignatures();
        std::cout << "Testing " << signatures.size() << " format signatures:" << std::endl;
        
        for (const auto& sig : signatures) {
            bool matches = testSignature(handler.get(), sig);
            std::cout << "  " << sig.format_id << ": " 
                      << (matches ? "MATCH" : "no match") 
                      << " (priority: " << sig.priority << ")" << std::endl;
        }
        
        // Test extension-based detection
        std::string extension = getFileExtension(filename);
        std::cout << "File extension: " << extension << std::endl;
        
        if (MediaFactory::supportsExtension(extension)) {
            std::cout << "Extension is supported by MediaFactory" << std::endl;
        }
        
        // Test content analysis
        auto content_info = MediaFactory::analyzeContent(filename);
        std::cout << "Content analysis result:" << std::endl;
        std::cout << "  Format: " << content_info.detected_format << std::endl;
        std::cout << "  Confidence: " << content_info.confidence << std::endl;
        std::cout << "  MIME type: " << content_info.mime_type << std::endl;
    }
    
private:
    static bool testSignature(IOHandler* handler, const FormatSignature& sig) {
        handler->seek(sig.offset, SEEK_SET);
        
        std::vector<uint8_t> buffer(sig.signature.size());
        size_t bytes_read = handler->read(buffer.data(), 1, buffer.size());
        
        if (bytes_read != sig.signature.size()) {
            return false;
        }
        
        return std::memcmp(buffer.data(), sig.signature.data(), sig.signature.size()) == 0;
    }
    
    static std::string getFileExtension(const std::string& filename) {
        size_t dot_pos = filename.find_last_of('.');
        if (dot_pos != std::string::npos) {
            return filename.substr(dot_pos);
        }
        return "";
    }
};

// Usage
FormatDetectionDebugger::debugDetection("problematic_file.ext");
```

**Solutions:**

1. **For signature conflicts:**
   ```cpp
   // Register more specific signature with higher priority
   FormatSignature specific_sig("myformat", {0x4D, 0x59, 0x46, 0x4D, 0x76, 0x31}, 0, 95);
   DemuxerFactory::registerSignature(specific_sig);
   ```

2. **For extension-based issues:**
   ```cpp
   // Register additional extensions
   MediaFormat format = getExistingFormat("myformat");
   format.extensions.push_back(".myext");
   MediaFactory::registerFormat(format, existing_factory);
   ```

### Issue: Format Detection Too Slow

**Symptoms:**
- Long delays during file opening
- High I/O activity during detection

**Performance Analysis:**

```cpp
class DetectionProfiler {
public:
    static void profileDetection(const std::string& filename) {
        auto start = std::chrono::high_resolution_clock::now();
        
        auto handler = std::make_unique<FileIOHandler>(filename);
        auto checkpoint1 = std::chrono::high_resolution_clock::now();
        
        std::string format = DemuxerFactory::probeFormat(handler.get());
        auto checkpoint2 = std::chrono::high_resolution_clock::now();
        
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        auto checkpoint3 = std::chrono::high_resolution_clock::now();
        
        if (demuxer) {
            demuxer->parseContainer();
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        // Report timings
        auto file_open_time = std::chrono::duration_cast<std::chrono::milliseconds>(checkpoint1 - start);
        auto probe_time = std::chrono::duration_cast<std::chrono::milliseconds>(checkpoint2 - checkpoint1);
        auto create_time = std::chrono::duration_cast<std::chrono::milliseconds>(checkpoint3 - checkpoint2);
        auto parse_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - checkpoint3);
        
        std::cout << "Detection Performance:" << std::endl;
        std::cout << "  File open: " << file_open_time.count() << "ms" << std::endl;
        std::cout << "  Format probe: " << probe_time.count() << "ms" << std::endl;
        std::cout << "  Demuxer create: " << create_time.count() << "ms" << std::endl;
        std::cout << "  Container parse: " << parse_time.count() << "ms" << std::endl;
        std::cout << "  Detected format: " << format << std::endl;
    }
};
```

**Optimization Strategies:**

1. **Optimize signature order:**
   ```cpp
   // Register most common formats with higher priority
   FormatSignature common_format("wav", {0x52, 0x49, 0x46, 0x46}, 0, 100);
   FormatSignature less_common("rare_format", {0x52, 0x41, 0x52, 0x45}, 0, 50);
   ```

2. **Use extension hints:**
   ```cpp
   // Provide file path for extension-based optimization
   auto demuxer = DemuxerFactory::createDemuxer(std::move(handler), filename);
   ```

## Parsing and Reading Issues

### Issue: readChunk() Returns Empty Chunks

**Symptoms:**
- `readChunk()` returns invalid chunks immediately
- `isEOF()` returns false but no data available

**Diagnostic Process:**

```cpp
void diagnoseReadingIssues(Demuxer* demuxer) {
    std::cout << "Demuxer State:" << std::endl;
    std::cout << "  Parsed: " << (demuxer->isParsed() ? "YES" : "NO") << std::endl;
    std::cout << "  EOF: " << (demuxer->isEOF() ? "YES" : "NO") << std::endl;
    std::cout << "  Duration: " << demuxer->getDuration() << "ms" << std::endl;
    std::cout << "  Position: " << demuxer->getPosition() << "ms" << std::endl;
    std::cout << "  Has Error: " << (demuxer->hasError() ? "YES" : "NO") << std::endl;
    
    if (demuxer->hasError()) {
        const auto& error = demuxer->getLastError();
        std::cout << "  Error: " << error.message << std::endl;
    }
    
    auto streams = demuxer->getStreams();
    std::cout << "  Streams: " << streams.size() << std::endl;
    
    for (const auto& stream : streams) {
        std::cout << "    Stream " << stream.stream_id << ": "
                  << stream.codec_type << "/" << stream.codec_name
                  << " (" << stream.duration_ms << "ms)" << std::endl;
    }
    
    // Try reading from specific streams
    for (const auto& stream : streams) {
        MediaChunk chunk = demuxer->readChunk(stream.stream_id);
        std::cout << "  Stream " << stream.stream_id << " chunk: "
                  << (chunk.isValid() ? "VALID" : "INVALID");
        if (chunk.isValid()) {
            std::cout << " (" << chunk.getDataSize() << " bytes)";
        }
        std::cout << std::endl;
    }
}
```

**Common Causes and Solutions:**

1. **Position at end of file:**
   ```cpp
   if (demuxer->getPosition() >= demuxer->getDuration()) {
       std::cout << "Demuxer is at end of file" << std::endl;
       // Seek back to beginning
       demuxer->seekTo(0);
   }
   ```

2. **Stream selection issues:**
   ```cpp
   // Try reading from specific streams instead of auto-selection
   auto streams = demuxer->getStreams();
   for (const auto& stream : streams) {
       if (stream.isAudio()) {
           MediaChunk chunk = demuxer->readChunk(stream.stream_id);
           if (chunk.isValid()) {
               std::cout << "Successfully read from stream " << stream.stream_id << std::endl;
               break;
           }
       }
   }
   ```

3. **Container structure issues:**
   ```cpp
   // Some containers require specific reading patterns
   // Check demuxer-specific documentation
   ```

### Issue: Seeking Inaccurate or Fails

**Symptoms:**
- `seekTo()` returns true but position is wrong
- Seeking to specific time doesn't work as expected
- Audio glitches after seeking

**Seeking Diagnostics:**

```cpp
void testSeeking(Demuxer* demuxer) {
    uint64_t duration = demuxer->getDuration();
    if (duration == 0) {
        std::cout << "Cannot test seeking: unknown duration" << std::endl;
        return;
    }
    
    std::vector<uint64_t> test_positions = {
        0,                    // Beginning
        duration / 4,         // 25%
        duration / 2,         // 50%
        3 * duration / 4,     // 75%
        duration - 1000       // Near end (1 second before)
    };
    
    for (uint64_t target : test_positions) {
        std::cout << "Seeking to " << target << "ms..." << std::endl;
        
        bool success = demuxer->seekTo(target);
        uint64_t actual = demuxer->getPosition();
        int64_t error = static_cast<int64_t>(actual) - static_cast<int64_t>(target);
        
        std::cout << "  Result: " << (success ? "SUCCESS" : "FAILED") << std::endl;
        std::cout << "  Target: " << target << "ms" << std::endl;
        std::cout << "  Actual: " << actual << "ms" << std::endl;
        std::cout << "  Error: " << error << "ms" << std::endl;
        
        // Test reading after seek
        MediaChunk chunk = demuxer->readChunk();
        std::cout << "  Read after seek: " << (chunk.isValid() ? "SUCCESS" : "FAILED") << std::endl;
        
        if (chunk.isValid()) {
            std::cout << "  Chunk timestamp: " << chunk.timestamp_samples << " samples" << std::endl;
        }
        
        std::cout << std::endl;
    }
}
```

**Solutions by Format Type:**

1. **For sample-accurate formats (PCM):**
   ```cpp
   // These should support precise seeking
   // If not working, check byte alignment calculations
   ```

2. **For packet-based formats (Ogg, MP4):**
   ```cpp
   // Seeking may only be accurate to packet boundaries
   // This is expected behavior - check if error is within acceptable range
   const uint64_t ACCEPTABLE_SEEK_ERROR = 100; // 100ms tolerance
   if (std::abs(error) <= ACCEPTABLE_SEEK_ERROR) {
       std::cout << "Seek error within acceptable range" << std::endl;
   }
   ```

3. **For streaming formats:**
   ```cpp
   // Some formats may not support seeking at all
   // Check format capabilities
   auto format_info = MediaFactory::getFormatInfo(format_id);
   if (format_info && !format_info->supports_seeking) {
       std::cout << "Format does not support seeking" << std::endl;
   }
   ```

## Memory and Performance Issues

### Issue: Memory Usage Growing During Playback

**Symptoms:**
- Memory usage increases continuously
- Application eventually runs out of memory
- Performance degrades over time

**Memory Diagnostics:**

```cpp
class MemoryMonitor {
private:
    size_t initial_memory;
    size_t peak_memory;
    
public:
    MemoryMonitor() : initial_memory(getCurrentMemoryUsage()), peak_memory(initial_memory) {}
    
    void checkpoint(const std::string& label) {
        size_t current = getCurrentMemoryUsage();
        peak_memory = std::max(peak_memory, current);
        
        std::cout << "Memory at " << label << ": " 
                  << current << " bytes (+" << (current - initial_memory) << ")" << std::endl;
        
        // Check buffer pool statistics
        auto pool_stats = BufferPool::getInstance().getStats();
        std::cout << "  Buffer pool: " << pool_stats.total_buffers << " buffers, "
                  << pool_stats.total_memory_bytes << " bytes" << std::endl;
    }
    
    ~MemoryMonitor() {
        std::cout << "Peak memory usage: " << peak_memory 
                  << " bytes (+" << (peak_memory - initial_memory) << ")" << std::endl;
    }
    
private:
    size_t getCurrentMemoryUsage() {
        // Platform-specific memory usage detection
        // This is a simplified example
        #ifdef __linux__
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::istringstream iss(line);
                std::string label, value, unit;
                iss >> label >> value >> unit;
                return std::stoull(value) * 1024; // Convert KB to bytes
            }
        }
        #endif
        return 0;
    }
};

void testMemoryUsage(const std::string& filename) {
    MemoryMonitor monitor;
    
    auto handler = std::make_unique<FileIOHandler>(filename);
    auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
    monitor.checkpoint("after demuxer creation");
    
    demuxer->parseContainer();
    monitor.checkpoint("after parsing");
    
    size_t chunk_count = 0;
    while (!demuxer->isEOF() && chunk_count < 1000) {
        MediaChunk chunk = demuxer->readChunk();
        if (chunk.isValid()) {
            chunk_count++;
            
            // Process chunk immediately and let it go out of scope
            processChunk(chunk);
            
            if (chunk_count % 100 == 0) {
                monitor.checkpoint("after " + std::to_string(chunk_count) + " chunks");
            }
        }
    }
    
    monitor.checkpoint("after processing");
}
```

**Memory Leak Solutions:**

1. **Ensure proper chunk lifecycle:**
   ```cpp
   // BAD: Storing chunks indefinitely
   std::vector<MediaChunk> all_chunks;
   while (!demuxer->isEOF()) {
       all_chunks.push_back(demuxer->readChunk()); // Memory leak!
   }
   
   // GOOD: Process chunks immediately
   while (!demuxer->isEOF()) {
       MediaChunk chunk = demuxer->readChunk();
       if (chunk.isValid()) {
           processChunk(chunk);
           // chunk goes out of scope and returns buffer to pool
       }
   }
   ```

2. **Use bounded buffers:**
   ```cpp
   class BoundedChunkProcessor {
   private:
       std::queue<MediaChunk> buffer;
       static constexpr size_t MAX_BUFFER_SIZE = 10;
       
   public:
       void addChunk(MediaChunk chunk) {
           if (buffer.size() >= MAX_BUFFER_SIZE) {
               // Process oldest chunk first
               processChunk(buffer.front());
               buffer.pop();
           }
           buffer.push(std::move(chunk));
       }
   };
   ```

3. **Monitor buffer pool:**
   ```cpp
   void monitorBufferPool() {
       auto stats = BufferPool::getInstance().getStats();
       
       if (stats.total_memory_bytes > 10 * 1024 * 1024) { // 10MB threshold
           std::cout << "Buffer pool using " << stats.total_memory_bytes 
                     << " bytes, clearing..." << std::endl;
           BufferPool::getInstance().clear();
       }
   }
   ```

### Issue: Poor Performance

**Symptoms:**
- Slow file opening
- Choppy playback
- High CPU usage

**Performance Profiling:**

```cpp
class PerformanceProfiler {
private:
    std::map<std::string, std::chrono::nanoseconds> timings;
    std::map<std::string, size_t> counters;
    std::chrono::high_resolution_clock::time_point start_time;
    
public:
    void startTimer(const std::string& operation) {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    void endTimer(const std::string& operation) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = end_time - start_time;
        timings[operation] += duration;
        counters[operation]++;
    }
    
    void printReport() {
        std::cout << "Performance Report:" << std::endl;
        for (const auto& [operation, total_time] : timings) {
            auto count = counters[operation];
            auto avg_time = total_time / count;
            auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_time);
            auto avg_us = std::chrono::duration_cast<std::chrono::microseconds>(avg_time);
            
            std::cout << "  " << operation << ":" << std::endl;
            std::cout << "    Total: " << total_ms.count() << "ms" << std::endl;
            std::cout << "    Count: " << count << std::endl;
            std::cout << "    Average: " << avg_us.count() << "μs" << std::endl;
        }
    }
};

void profileDemuxerPerformance(const std::string& filename) {
    PerformanceProfiler profiler;
    
    profiler.startTimer("file_open");
    auto handler = std::make_unique<FileIOHandler>(filename);
    profiler.endTimer("file_open");
    
    profiler.startTimer("demuxer_create");
    auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
    profiler.endTimer("demuxer_create");
    
    profiler.startTimer("parse_container");
    demuxer->parseContainer();
    profiler.endTimer("parse_container");
    
    // Profile chunk reading
    size_t chunk_count = 0;
    while (!demuxer->isEOF() && chunk_count < 1000) {
        profiler.startTimer("read_chunk");
        MediaChunk chunk = demuxer->readChunk();
        profiler.endTimer("read_chunk");
        
        if (chunk.isValid()) {
            chunk_count++;
            
            profiler.startTimer("process_chunk");
            processChunk(chunk);
            profiler.endTimer("process_chunk");
        }
    }
    
    profiler.printReport();
}
```

**Performance Optimization:**

1. **I/O optimization:**
   ```cpp
   // Use larger read buffers for network streams
   class OptimizedIOHandler : public IOHandler {
   private:
       static constexpr size_t BUFFER_SIZE = 64 * 1024; // 64KB
       std::vector<uint8_t> buffer;
       size_t buffer_pos = 0;
       size_t buffer_valid = 0;
       
   public:
       size_t read(void* ptr, size_t size, size_t count) override {
           // Implement buffered reading
           return bufferedRead(ptr, size * count) / size;
       }
   };
   ```

2. **Reduce memory allocations:**
   ```cpp
   // Reuse MediaChunk objects
   class ChunkRecycler {
   private:
       std::queue<MediaChunk> recycled_chunks;
       
   public:
       MediaChunk getChunk(size_t size) {
           if (!recycled_chunks.empty()) {
               MediaChunk chunk = std::move(recycled_chunks.front());
               recycled_chunks.pop();
               chunk.data.resize(size);
               return chunk;
           }
           return MediaChunk(1, size);
       }
       
       void recycleChunk(MediaChunk chunk) {
           chunk.clear();
           recycled_chunks.push(std::move(chunk));
       }
   };
   ```

## Thread Safety Issues

### Issue: Crashes in Multi-threaded Environment

**Symptoms:**
- Segmentation faults or access violations
- Inconsistent behavior between runs
- Deadlocks or race conditions

**Thread Safety Diagnostics:**

```cpp
class ThreadSafetyTester {
public:
    static void testConcurrentAccess(const std::string& filename) {
        const int NUM_THREADS = 4;
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    // Each thread uses its own demuxer instance
                    auto handler = std::make_unique<FileIOHandler>(filename);
                    auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
                    
                    if (demuxer && demuxer->parseContainer()) {
                        // Process some chunks
                        int chunks_processed = 0;
                        while (!demuxer->isEOF() && chunks_processed < 100) {
                            MediaChunk chunk = demuxer->readChunk();
                            if (chunk.isValid()) {
                                chunks_processed++;
                            }
                        }
                        success_count++;
                        std::cout << "Thread " << i << " processed " 
                                  << chunks_processed << " chunks" << std::endl;
                    } else {
                        failure_count++;
                        std::cout << "Thread " << i << " failed to initialize" << std::endl;
                    }
                } catch (const std::exception& e) {
                    failure_count++;
                    std::cout << "Thread " << i << " exception: " << e.what() << std::endl;
                }
            });
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "Thread safety test results:" << std::endl;
        std::cout << "  Successful threads: " << success_count.load() << std::endl;
        std::cout << "  Failed threads: " << failure_count.load() << std::endl;
    }
    
    static void testFactoryThreadSafety() {
        const int NUM_THREADS = 10;
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};
        
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    // Test concurrent factory operations
                    auto formats = MediaFactory::getSupportedFormats();
                    
                    for (const auto& format : formats) {
                        bool supports_ext = MediaFactory::supportsExtension(format.extensions[0]);
                        bool supports_mime = MediaFactory::supportsMimeType(format.mime_types[0]);
                        
                        if (supports_ext && supports_mime) {
                            success_count++;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cout << "Factory thread " << i << " exception: " << e.what() << std::endl;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "Factory thread safety: " << success_count.load() 
                  << " successful operations" << std::endl;
    }
};
```

**Thread Safety Solutions:**

1. **Use separate instances:**
   ```cpp
   // GOOD: Each thread has its own demuxer
   void workerThread(const std::string& filename) {
       auto handler = std::make_unique<FileIOHandler>(filename);
       auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
       // Use demuxer safely within this thread
   }
   ```

2. **Synchronize shared access:**
   ```cpp
   // If you must share a demuxer (not recommended)
   class SynchronizedDemuxer {
   private:
       std::unique_ptr<Demuxer> demuxer;
       mutable std::mutex mutex;
       
   public:
       MediaChunk readChunk() {
           std::lock_guard<std::mutex> lock(mutex);
           return demuxer->readChunk();
       }
       
       bool seekTo(uint64_t timestamp_ms) {
           std::lock_guard<std::mutex> lock(mutex);
           return demuxer->seekTo(timestamp_ms);
       }
   };
   ```

## Plugin Development Issues

### Issue: Plugin Not Loading

**Symptoms:**
- Custom format not recognized
- Plugin initialization fails
- Missing symbols or linking errors

**Plugin Diagnostics:**

```cpp
class PluginDiagnostics {
public:
    static void testPluginLoading(const std::string& plugin_path) {
        std::cout << "Testing plugin: " << plugin_path << std::endl;
        
        // Test if file exists and is readable
        if (access(plugin_path.c_str(), R_OK) != 0) {
            std::cout << "Plugin file not accessible: " << strerror(errno) << std::endl;
            return;
        }
        
        // Try to load the plugin (platform-specific)
        #ifdef _WIN32
        HMODULE handle = LoadLibrary(plugin_path.c_str());
        if (!handle) {
            std::cout << "Failed to load plugin: " << GetLastError() << std::endl;
            return;
        }
        
        // Check for required symbols
        auto init_func = GetProcAddress(handle, "initializePlugin");
        auto name_func = GetProcAddress(handle, "getPluginName");
        
        if (!init_func || !name_func) {
            std::cout << "Plugin missing required symbols" << std::endl;
            FreeLibrary(handle);
            return;
        }
        
        FreeLibrary(handle);
        #else
        void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cout << "Failed to load plugin: " << dlerror() << std::endl;
            return;
        }
        
        // Check for required symbols
        auto init_func = dlsym(handle, "initializePlugin");
        auto name_func = dlsym(handle, "getPluginName");
        
        if (!init_func || !name_func) {
            std::cout << "Plugin missing required symbols: " << dlerror() << std::endl;
            dlclose(handle);
            return;
        }
        
        dlclose(handle);
        #endif
        
        std::cout << "Plugin appears to be valid" << std::endl;
    }
    
    static void testPluginRegistration() {
        // Test if custom formats are registered
        auto formats = MediaFactory::getSupportedFormats();
        
        std::cout << "Registered formats:" << std::endl;
        for (const auto& format : formats) {
            std::cout << "  " << format.format_id << ": " << format.display_name << std::endl;
        }
        
        // Test format detection
        auto signatures = DemuxerFactory::getSignatures();
        std::cout << "Registered signatures: " << signatures.size() << std::endl;
    }
};
```

**Plugin Development Solutions:**

1. **Check symbol visibility:**
   ```cpp
   // Ensure C linkage for plugin functions
   extern "C" {
       __attribute__((visibility("default"))) 
       bool initializePlugin() {
           // Implementation
           return true;
       }
   }
   ```

2. **Verify plugin interface:**
   ```cpp
   // Plugin must implement all required functions
   extern "C" {
       bool initializePlugin();
       void shutdownPlugin();
       const char* getPluginName();
       const char* getPluginVersion();
   }
   ```

3. **Test plugin in isolation:**
   ```cpp
   // Create minimal test program for plugin
   int main() {
       if (initializePlugin()) {
           std::cout << "Plugin: " << getPluginName() << std::endl;
           std::cout << "Version: " << getPluginVersion() << std::endl;
           
           // Test format registration
           PluginDiagnostics::testPluginRegistration();
           
           shutdownPlugin();
           return 0;
       }
       return 1;
   }
   ```

## Debugging Tools and Techniques

### Debug Logging

```cpp
class DemuxerDebugLogger {
private:
    static bool debug_enabled;
    
public:
    static void enable(bool enabled = true) {
        debug_enabled = enabled;
    }
    
    template<typename... Args>
    static void log(const std::string& category, Args&&... args) {
        if (debug_enabled) {
            std::cout << "[DEMUXER:" << category << "] ";
            ((std::cout << args), ...);
            std::cout << std::endl;
        }
    }
};

bool DemuxerDebugLogger::debug_enabled = false;

// Usage in demuxer implementation
void MyDemuxer::parseContainer() {
    DemuxerDebugLogger::log("PARSE", "Starting container parsing");
    
    // Read header
    DemuxerDebugLogger::log("PARSE", "Reading header at offset ", m_handler->tell());
    
    // ... implementation
}
```

### Memory Debugging

```cpp
class MemoryDebugger {
public:
    static void trackAllocation(void* ptr, size_t size, const std::string& context) {
        std::lock_guard<std::mutex> lock(mutex_);
        allocations_[ptr] = {size, context, std::chrono::steady_clock::now()};
        total_allocated_ += size;
        
        std::cout << "ALLOC: " << ptr << " (" << size << " bytes) - " << context << std::endl;
    }
    
    static void trackDeallocation(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = allocations_.find(ptr);
        if (it != allocations_.end()) {
            total_allocated_ -= it->second.size;
            std::cout << "FREE: " << ptr << " (" << it->second.size << " bytes)" << std::endl;
            allocations_.erase(it);
        }
    }
    
    static void printLeaks() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!allocations_.empty()) {
            std::cout << "MEMORY LEAKS DETECTED:" << std::endl;
            for (const auto& [ptr, info] : allocations_) {
                std::cout << "  " << ptr << ": " << info.size << " bytes (" 
                          << info.context << ")" << std::endl;
            }
        }
        std::cout << "Total allocated: " << total_allocated_ << " bytes" << std::endl;
    }
    
private:
    struct AllocationInfo {
        size_t size;
        std::string context;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    static std::map<void*, AllocationInfo> allocations_;
    static size_t total_allocated_;
    static std::mutex mutex_;
};
```

## Error Code Reference

### DemuxerError Categories

| Category | Description | Common Causes | Recovery Strategies |
|----------|-------------|---------------|-------------------|
| `"IO"` | Input/output errors | File not found, network timeout, disk full | Retry, fallback to cached data |
| `"Format"` | Container format errors | Corrupted header, unsupported version | Skip section, fallback mode |
| `"Memory"` | Memory allocation failures | Out of memory, fragmentation | Clear buffers, reduce quality |
| `"Validation"` | Data validation failures | Invalid checksums, size mismatches | Skip invalid data, continue |
| `"Exception"` | Unexpected exceptions | Programming errors, system issues | Log and continue, reset state |

### Common Error Codes

| Error Code | Meaning | Action |
|------------|---------|--------|
| `0` | Generic error | Check error message |
| `1` | File not found | Verify file path |
| `2` | Permission denied | Check file permissions |
| `3` | Invalid format | Verify file format |
| `4` | Corrupted data | Try error recovery |
| `5` | Unsupported version | Update demuxer |
| `6` | Out of memory | Reduce buffer sizes |
| `7` | Network timeout | Retry with longer timeout |

This troubleshooting guide provides comprehensive solutions for the most common issues encountered when working with the PsyMP3 Demuxer Architecture.
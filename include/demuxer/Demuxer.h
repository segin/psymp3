/*
 * Demuxer.h - Generic container demuxer base classes
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DEMUXER_H
#define DEMUXER_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Demuxer {

/**
 * @brief Information about a media stream within a container
 * 
 * This structure contains comprehensive metadata about a media stream,
 * including codec information, timing data, and embedded metadata.
 * 
 * @note All timing values are provided in both samples and milliseconds
 *       for convenience. Sample-based timing is more accurate for audio processing.
 * 
 * @thread_safety This structure is copyable and movable. Individual instances
 *                are not thread-safe for modification, but can be safely read
 *                from multiple threads once populated.
 */
struct StreamInfo {
    uint32_t stream_id = 0;           ///< Unique stream identifier within the container
    std::string codec_type;        ///< Stream type: "audio", "video", "subtitle", etc.
    std::string codec_name;        ///< Codec name: "pcm", "mp3", "aac", "flac", "vorbis", etc.
    uint32_t codec_tag = 0;        ///< Format-specific codec identifier (e.g., WAVE format tag)
    
    // Audio-specific properties
    uint32_t sample_rate = 0;      ///< Sample rate in Hz (e.g., 44100, 48000)
    uint16_t channels = 0;         ///< Number of audio channels (1=mono, 2=stereo, etc.)
    uint16_t bits_per_sample = 0;  ///< Bits per sample (8, 16, 24, 32)
    uint32_t bitrate = 0;          ///< Average bitrate in bits per second (0 if unknown)
    
    // Additional codec-specific data
    std::vector<uint8_t> codec_data; ///< Extra data needed by codec (e.g., AAC AudioSpecificConfig, FLAC STREAMINFO)
    
    // Timing information
    uint64_t duration_samples = 0; ///< Total duration in sample frames (0 if unknown)
    uint64_t duration_ms = 0;      ///< Total duration in milliseconds (0 if unknown)
    
    // Metadata (extracted from container, if available)
    std::string artist;            ///< Track artist name
    std::string title;             ///< Track title
    std::string album;             ///< Album name
    
    // Constructors
    StreamInfo() = default;
    
    StreamInfo(uint32_t id, const std::string& type, const std::string& name)
        : stream_id(id), codec_type(type), codec_name(name) {}
    
    // Copy constructor and assignment (default is fine for this structure)
    StreamInfo(const StreamInfo&) = default;
    StreamInfo& operator=(const StreamInfo&) = default;
    
    // Move constructor and assignment
    StreamInfo(StreamInfo&&) = default;
    StreamInfo& operator=(StreamInfo&&) = default;
    
    /**
     * @brief Validate that the stream info contains required fields
     * @return true if valid, false otherwise
     */
    bool isValid() const {
        return stream_id != 0 && !codec_type.empty() && !codec_name.empty();
    }
    
    /**
     * @brief Check if this is an audio stream
     */
    bool isAudio() const {
        return codec_type == "audio";
    }
    
    /**
     * @brief Check if this is a video stream
     */
    bool isVideo() const {
        return codec_type == "video";
    }
    
    /**
     * @brief Check if this is a subtitle stream
     */
    bool isSubtitle() const {
        return codec_type == "subtitle";
    }
};

/**
 * @brief Memory pool for efficient buffer reuse
 */
class BufferPool {
public:
    static BufferPool& getInstance();
    
    /**
     * @brief Get a buffer of at least the specified size
     * @param min_size Minimum required size
     * @return Reusable buffer vector
     */
    std::vector<uint8_t> getBuffer(size_t min_size);
    
    /**
     * @brief Return a buffer to the pool for reuse
     * @param buffer Buffer to return (will be moved)
     */
    void returnBuffer(std::vector<uint8_t>&& buffer);
    
    /**
     * @brief Clear all pooled buffers (for memory cleanup)
     */
    void clear();
    
    /**
     * @brief Get current pool statistics
     */
    struct PoolStats {
        size_t total_buffers;
        size_t total_memory_bytes;
        size_t largest_buffer_size;
    };
    PoolStats getStats() const;
    
private:
    BufferPool() = default;
    mutable std::mutex m_mutex;
    std::vector<std::vector<uint8_t>> m_buffers;
    static constexpr size_t MAX_POOLED_BUFFERS = 32;
    static constexpr size_t MAX_BUFFER_SIZE = 1024 * 1024; // 1MB max per buffer
};

/**
 * @brief A chunk of media data with metadata and optimized memory management
 * 
 * MediaChunk represents a discrete unit of compressed media data from a container.
 * It includes timing information and uses an internal buffer pool for efficient
 * memory management.
 * 
 * @note The data vector contains compressed data that needs to be decoded by
 *       an appropriate codec. For raw PCM formats, the data may be uncompressed.
 * 
 * @memory_management Uses BufferPool for automatic buffer reuse. Large buffers
 *                    are automatically returned to the pool when the chunk is destroyed.
 * 
 * @thread_safety Individual instances are not thread-safe for modification,
 *                but can be safely moved between threads.
 */
struct MediaChunk {
    uint32_t stream_id = 0;           ///< Stream this chunk belongs to
    std::vector<uint8_t> data;        ///< Raw compressed media data
    uint64_t granule_position = 0;    ///< Ogg-specific timing (0 for non-Ogg formats)
    uint64_t timestamp_samples = 0;   ///< Timestamp in sample frames (for non-Ogg formats)
    bool is_keyframe = true;          ///< Whether this is a keyframe (usually true for audio)
    uint64_t file_offset = 0;         ///< Original file offset (used for seeking optimization)
    
    // Constructors
    MediaChunk() = default;
    
    MediaChunk(uint32_t id, std::vector<uint8_t>&& chunk_data)
        : stream_id(id), data(std::move(chunk_data)) {}
    
    MediaChunk(uint32_t id, const std::vector<uint8_t>& chunk_data)
        : stream_id(id), data(chunk_data) {}
    
    // Optimized constructor using buffer pool
    MediaChunk(uint32_t id, size_t data_size)
        : stream_id(id), data(EnhancedBufferPool::getInstance().getBuffer(data_size)) {
        data.resize(data_size);
    }
    
    // Copy constructor and assignment (default is fine)
    MediaChunk(const MediaChunk&) = default;
    MediaChunk& operator=(const MediaChunk&) = default;
    
    // Move constructor and assignment
    MediaChunk(MediaChunk&&) = default;
    MediaChunk& operator=(MediaChunk&&) = default;
    
    // Destructor that returns buffer to pool
    ~MediaChunk() {
        if (!data.empty() && data.capacity() >= 1024) { // Only pool reasonably sized buffers
            EnhancedBufferPool::getInstance().returnBuffer(std::move(data));
        }
    }
    
    /**
     * @brief Check if this chunk contains valid data
     * @return true if chunk has data and valid stream ID
     */
    bool isValid() const {
        return stream_id != 0 && !data.empty();
    }
    
    /**
     * @brief Get the size of the data in bytes
     */
    size_t getDataSize() const {
        return data.size();
    }
    
    /**
     * @brief Check if this chunk is empty
     */
    bool isEmpty() const {
        return data.empty();
    }
    
    /**
     * @brief Clear the chunk data
     */
    void clear() {
        data.clear();
        stream_id = 0;
        granule_position = 0;
        timestamp_samples = 0;
        is_keyframe = true;
        file_offset = 0;
    }
};

/**
 * @brief Error recovery strategies for demuxer operations
 */
enum class DemuxerErrorRecovery {
    NONE,           // No recovery attempted
    SKIP_SECTION,   // Skip corrupted section and continue
    RESET_STATE,    // Reset internal state and retry
    FALLBACK_MODE   // Use fallback parsing mode
};

/**
 * @brief Error information for demuxer operations
 */
struct DemuxerError {
    std::string category;       // Error category (e.g., "IO", "Format", "Memory")
    std::string message;        // Human-readable error message
    int error_code = 0;         // Numeric error code
    uint64_t file_offset = 0;   // File offset where error occurred
    DemuxerErrorRecovery recovery = DemuxerErrorRecovery::NONE;
    
    DemuxerError() = default;
    DemuxerError(const std::string& cat, const std::string& msg, int code = 0)
        : category(cat), message(msg), error_code(code) {}
};

/**
 * @brief Base class for all container demuxers
 * 
 * A demuxer is responsible for parsing container formats (RIFF, Ogg, MP4, etc.)
 * and extracting individual streams of media data. It does not decode the actual
 * audio/video data - that's the job of codec classes.
 * 
 * ## Design Principles
 * - **Separation of Concerns**: Demuxing is separate from decoding
 * - **Streaming Architecture**: Processes data incrementally without loading entire files
 * - **Error Recovery**: Comprehensive error handling with recovery strategies
 * - **Memory Efficiency**: Uses bounded buffers and buffer pooling
 * 
 * ## Usage Pattern
 * 1. Create demuxer using DemuxerFactory
 * 2. Call parseContainer() to analyze the file structure
 * 3. Query available streams with getStreams()
 * 4. Read data chunks with readChunk() methods
 * 5. Optionally seek to different positions with seekTo()
 * 
 * ## Thread Safety
 * Individual demuxer instances are **NOT** thread-safe. Use separate instances
 * for concurrent access or implement external synchronization.
 * 
 * ## Error Handling
 * Methods return false/empty results on error. Use getLastError() for detailed
 * error information. The demuxer attempts automatic recovery where possible.
 * 
 * @see DemuxerFactory for creating demuxer instances
 * @see MediaChunk for the data structure returned by read operations
 * @see StreamInfo for stream metadata structure
 */
class Demuxer {
public:
    explicit Demuxer(std::unique_ptr<IOHandler> handler);
    virtual ~Demuxer() = default;
    
    /**
     * @brief Parse the container headers and identify streams
     * 
     * This method performs the initial analysis of the container format,
     * extracting stream information, metadata, and building internal
     * structures needed for data access.
     * 
     * @return true if container was successfully parsed, false on error
     * 
     * @post If successful, getStreams() will return valid stream information
     * @post If successful, getDuration() may return the total duration
     * @post On failure, getLastError() contains detailed error information
     * 
     * @note This method must be called successfully before any other operations
     * @note This method should only be called once per demuxer instance
     * 
     * @thread_safety Not thread-safe. Do not call concurrently with other methods.
     */
    virtual bool parseContainer() = 0;
    
    /**
     * @brief Get information about all streams in the container
     * 
     * Returns comprehensive metadata about all streams found in the container.
     * This includes audio, video, and subtitle streams with their respective
     * codec information, timing data, and embedded metadata.
     * 
     * @return Vector of StreamInfo objects, one for each stream
     * @return Empty vector if no streams found or container not parsed
     * 
     * @pre parseContainer() must have been called successfully
     * 
     * @thread_safety Safe to call concurrently after parseContainer() completes
     * 
     * @note Stream IDs in the returned StreamInfo objects are used with
     *       readChunk(stream_id) and other stream-specific methods
     */
    virtual std::vector<StreamInfo> getStreams() const = 0;
    
    /**
     * @brief Get information about a specific stream
     */
    virtual StreamInfo getStreamInfo(uint32_t stream_id) const = 0;
    
    /**
     * @brief Read the next chunk of data from any stream
     * 
     * Reads the next available chunk of compressed media data from any stream
     * in the container. The specific stream is determined by the container's
     * internal structure and interleaving pattern.
     * 
     * @return MediaChunk containing compressed data and metadata
     * @return Empty/invalid chunk if EOF reached or error occurred
     * 
     * @pre parseContainer() must have been called successfully
     * 
     * @post Updates internal position tracking
     * @post May set EOF state if end of container reached
     * 
     * @thread_safety Not thread-safe. Serialize calls or use separate instances.
     * 
     * @note The returned data is compressed and needs codec processing
     * @note Check MediaChunk::isValid() to distinguish valid data from EOF/error
     * @note Use isEOF() to distinguish EOF from error conditions
     * 
     * @see readChunk(uint32_t) for reading from specific streams
     * @see MediaChunk for the returned data structure
     */
    virtual MediaChunk readChunk() = 0;
    
    /**
     * @brief Read the next chunk of data from a specific stream
     * @param stream_id Stream ID to read from
     * @return MediaChunk with data, or empty chunk if EOF/no data for this stream
     */
    virtual MediaChunk readChunk(uint32_t stream_id) = 0;
    
    /**
     * @brief Seek to a specific time position
     * 
     * Attempts to seek to the specified time position in the media.
     * The actual seek position may be adjusted to the nearest keyframe
     * or packet boundary depending on the container format.
     * 
     * @param timestamp_ms Target time position in milliseconds
     * @return true if seek was successful, false on error
     * 
     * @pre parseContainer() must have been called successfully
     * 
     * @post If successful, subsequent readChunk() calls return data from new position
     * @post If successful, getPosition() reflects the new position (may be approximate)
     * @post Resets EOF state if seeking away from end of file
     * @post May require codec reset in calling code due to discontinuity
     * 
     * @thread_safety Not thread-safe. Do not call concurrently with read operations.
     * 
     * @note Seeking may not be sample-accurate due to container format limitations
     * @note Seeking beyond file duration is clamped to the end of the file
     * @note Some formats (e.g., raw audio) support precise seeking, others approximate
     * 
     * @see getPosition() to query current position after seeking
     * @see getDuration() to get valid seek range
     */
    virtual bool seekTo(uint64_t timestamp_ms) = 0;
    
    /**
     * @brief Check if we've reached the end of the container
     */
    virtual bool isEOF() const = 0;
    
    /**
     * @brief Get total duration of the container in milliseconds
     */
    virtual uint64_t getDuration() const = 0;
    
    /**
     * @brief Get current position in milliseconds
     */
    virtual uint64_t getPosition() const = 0;
    
    /**
     * @brief Get the last known granule position for a stream
     */
    virtual uint64_t getGranulePosition(uint32_t stream_id) const {
        return 0; // Default implementation for non-Ogg formats
    }
    
    /**
     * @brief Get the last error that occurred during demuxer operations
     */
    const DemuxerError& getLastError() const {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        return m_last_error;
    }
    
    /**
     * @brief Check if the demuxer has encountered any errors
     */
    bool hasError() const {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        return !m_last_error.category.empty();
    }
    
    /**
     * @brief Clear the last error state
     */
    void clearError() {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        m_last_error = DemuxerError{};
    }
    
    /**
     * @brief Thread-safe accessor for parsed state
     */
    bool isParsed() const {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        return m_parsed;
    }
    
    /**
     * @brief Thread-safe accessor for EOF state
     */
    bool isEOFAtomic() const {
        return m_eof_flag.load();
    }
    
    /**
     * @brief Thread-safe setter for EOF state
     */
    void setEOF(bool eof) const {
        m_eof_flag.store(eof);
    }
    
protected:
    std::unique_ptr<IOHandler> m_handler;
    std::vector<StreamInfo> m_streams;
    uint64_t m_duration_ms = 0;
    uint64_t m_position_ms = 0;
    bool m_parsed = false;
    
    // Additional state for concrete implementations
    std::map<uint32_t, uint64_t> m_stream_positions; // Per-stream position tracking
    
    // Error handling state
    mutable DemuxerError m_last_error;
    mutable std::mutex m_error_mutex; // Thread-safe error reporting
    
    // Thread safety for demuxer state
    mutable std::mutex m_state_mutex;        // Protects position, duration, parsed state
    mutable std::mutex m_streams_mutex;      // Protects streams vector and stream_positions
    mutable std::mutex m_io_mutex;           // Protects IOHandler operations
    mutable std::atomic<bool> m_eof_flag{false}; // Atomic EOF flag for thread-safe access
    
    /**
     * @brief Thread-safe helper to read little-endian values
     */
    template<typename T>
    T readLE() {
        std::lock_guard<std::mutex> lock(m_io_mutex);
        T value;
        if (m_handler->read(&value, sizeof(T), 1) != 1) {
            throw std::runtime_error("Unexpected end of file");
        }
        
        // Convert from little-endian to host byte order
        #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        if constexpr (sizeof(T) == 2) {
            return __builtin_bswap16(value);
        } else if constexpr (sizeof(T) == 4) {
            return __builtin_bswap32(value);
        } else if constexpr (sizeof(T) == 8) {
            return __builtin_bswap64(value);
        }
        #endif
        return value;
    }
    
    /**
     * @brief Thread-safe helper to read big-endian values
     */
    template<typename T>
    T readBE() {
        std::lock_guard<std::mutex> lock(m_io_mutex);
        T value;
        if (m_handler->read(&value, sizeof(T), 1) != 1) {
            throw std::runtime_error("Unexpected end of file");
        }
        
        // Convert from big-endian to host byte order
        #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        if constexpr (sizeof(T) == 2) {
            return __builtin_bswap16(value);
        } else if constexpr (sizeof(T) == 4) {
            return __builtin_bswap32(value);
        } else if constexpr (sizeof(T) == 8) {
            return __builtin_bswap64(value);
        }
        #endif
        return value;
    }
    
    /**
     * @brief Thread-safe helper to read a FourCC code
     */
    uint32_t readFourCC() {
        return readLE<uint32_t>();
    }
    
    /**
     * @brief Thread-safe helper to read a null-terminated string
     * @param max_length Maximum length to read (safety limit)
     */
    std::string readString(size_t max_length = 256) {
        std::lock_guard<std::mutex> lock(m_io_mutex);
        std::string result;
        result.reserve(max_length);
        
        char c;
        while (result.length() < max_length && 
               m_handler->read(&c, 1, 1) == 1 && c != '\0') {
            result += c;
        }
        return result;
    }
    
    /**
     * @brief Thread-safe helper to read a fixed-length string
     * @param length Number of bytes to read
     */
    std::string readFixedString(size_t length) {
        std::lock_guard<std::mutex> lock(m_io_mutex);
        std::vector<char> buffer(length);
        size_t bytes_read = m_handler->read(buffer.data(), 1, length);
        return std::string(buffer.data(), bytes_read);
    }
    
    /**
     * @brief Thread-safe helper to skip bytes in the stream
     * @param count Number of bytes to skip
     * @return true if successful
     */
    bool skipBytes(size_t count) {
        std::lock_guard<std::mutex> lock(m_io_mutex);
        return m_handler->seek(static_cast<long>(count), SEEK_CUR) == 0;
    }
    
    /**
     * @brief Thread-safe helper to align to a specific boundary
     * @param alignment Alignment boundary (e.g., 4 for 32-bit alignment)
     * @return true if successful
     */
    bool alignTo(size_t alignment) {
        std::lock_guard<std::mutex> lock(m_io_mutex);
        off_t current_pos = m_handler->tell();
        if (current_pos < 0) return false;
        
        size_t remainder = static_cast<size_t>(current_pos) % alignment;
        if (remainder != 0) {
            size_t padding = alignment - remainder;
            return m_handler->seek(static_cast<long>(padding), SEEK_CUR) == 0;
        }
        return true;
    }
    
    /**
     * @brief Thread-safe helper to validate stream ID
     * @param stream_id Stream ID to validate
     * @return true if stream ID is valid
     */
    bool isValidStreamId(uint32_t stream_id) const {
        std::lock_guard<std::mutex> lock(m_streams_mutex);
        for (const auto& stream : m_streams) {
            if (stream.stream_id == stream_id) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Thread-safe helper to find stream by ID
     * @param stream_id Stream ID to find
     * @return Pointer to StreamInfo or nullptr if not found
     */
    const StreamInfo* findStream(uint32_t stream_id) const {
        std::lock_guard<std::mutex> lock(m_streams_mutex);
        for (const auto& stream : m_streams) {
            if (stream.stream_id == stream_id) {
                return &stream;
            }
        }
        return nullptr;
    }
    
    // Error handling and recovery methods
    
    /**
     * @brief Report an error with automatic recovery attempt
     * @param category Error category
     * @param message Error message
     * @param error_code Optional error code
     * @param recovery Recovery strategy to attempt
     * @return true if recovery was successful
     */
    bool reportError(const std::string& category, const std::string& message, 
                    int error_code = 0, DemuxerErrorRecovery recovery = DemuxerErrorRecovery::NONE) const {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        
        m_last_error.category = category;
        m_last_error.message = message;
        m_last_error.error_code = error_code;
        m_last_error.file_offset = m_handler ? static_cast<uint64_t>(m_handler->tell()) : 0;
        m_last_error.recovery = recovery;
        
        // Log the error
        Debug::log("demuxer", "Demuxer error [", category, "]: ", message, 
                   " at offset ", m_last_error.file_offset);
        
        // Attempt recovery if specified
        return attemptErrorRecovery(recovery);
    }
    
    /**
     * @brief Perform I/O operation with error handling and retry
     * @param operation Lambda function performing the I/O operation
     * @param operation_name Description of the operation for error reporting
     * @param max_retries Maximum number of retry attempts
     * @return true if operation succeeded
     */
    template<typename Operation>
    bool performIOWithRetry(Operation&& operation, const std::string& operation_name, 
                           int max_retries = 3) const {
        for (int attempt = 0; attempt <= max_retries; ++attempt) {
            try {
                if (operation()) {
                    return true;
                }
            } catch (const std::exception& e) {
                if (attempt == max_retries) {
                    reportError("IO", "I/O operation failed after " + std::to_string(max_retries + 1) + 
                               " attempts: " + operation_name + " - " + e.what());
                    return false;
                }
                
                // Brief delay before retry
                std::this_thread::sleep_for(std::chrono::milliseconds(10 * (attempt + 1)));
            }
        }
        
        reportError("IO", "I/O operation failed: " + operation_name);
        return false;
    }
    
    /**
     * @brief Validate data integrity with checksum or size checks
     * @param data Data to validate
     * @param expected_size Expected size (0 to skip size check)
     * @param context Context description for error reporting
     * @return true if data is valid
     */
    bool validateData(const std::vector<uint8_t>& data, size_t expected_size = 0, 
                     const std::string& context = "data") const {
        if (data.empty()) {
            reportError("Validation", "Empty " + context + " detected");
            return false;
        }
        
        if (expected_size > 0 && data.size() != expected_size) {
            reportError("Validation", context + " size mismatch: expected " + 
                       std::to_string(expected_size) + ", got " + std::to_string(data.size()));
            return false;
        }
        
        // Additional validation could include checksums, magic bytes, etc.
        return true;
    }
    
    /**
     * @brief Handle memory allocation failures with recovery
     * @param requested_size Size that failed to allocate
     * @param context Context description
     * @return true if recovery was successful
     */
    bool handleMemoryFailure(size_t requested_size, const std::string& context) const {
        reportError("Memory", "Failed to allocate " + std::to_string(requested_size) + 
                   " bytes for " + context, 0, DemuxerErrorRecovery::FALLBACK_MODE);
        
        // Try to free some memory from buffer pool
        EnhancedBufferPool::getInstance().clear();
        
        // Force garbage collection if possible
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        return false; // Memory failures are generally not recoverable
    }
    
    /**
     * @brief Thread-safe validation of file position and size constraints
     * @param offset File offset to validate
     * @param size Size of data to read
     * @return true if position is valid
     */
    bool validateFilePosition(uint64_t offset, uint64_t size = 0) const {
        std::lock_guard<std::mutex> lock(m_io_mutex);
        
        if (!m_handler) {
            reportError("IO", "No I/O handler available for position validation");
            return false;
        }
        
        // Get file size
        long current_pos = m_handler->tell();
        if (current_pos < 0) {
            reportError("IO", "Failed to get current file position");
            return false;
        }
        
        if (m_handler->seek(0, SEEK_END) != 0) {
            reportError("IO", "Failed to seek to end of file for size check");
            return false;
        }
        
        long file_size = m_handler->tell();
        m_handler->seek(current_pos, SEEK_SET); // Restore position
        
        if (file_size < 0) {
            reportError("IO", "Failed to determine file size");
            return false;
        }
        
        if (offset >= static_cast<uint64_t>(file_size)) {
            reportError("IO", "Offset " + std::to_string(offset) + 
                       " exceeds file size " + std::to_string(file_size));
            return false;
        }
        
        if (size > 0 && offset + size > static_cast<uint64_t>(file_size)) {
            reportError("IO", "Read operation would exceed file bounds: offset=" + 
                       std::to_string(offset) + ", size=" + std::to_string(size) + 
                       ", file_size=" + std::to_string(file_size));
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Thread-safe position update
     * @param new_position New position in milliseconds
     */
    void updatePosition(uint64_t new_position) {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_position_ms = new_position;
    }
    
    /**
     * @brief Thread-safe duration update
     * @param new_duration New duration in milliseconds
     */
    void updateDuration(uint64_t new_duration) {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_duration_ms = new_duration;
    }
    
    /**
     * @brief Thread-safe parsed state update
     * @param parsed New parsed state
     */
    void setParsed(bool parsed) {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_parsed = parsed;
    }
    
    /**
     * @brief Thread-safe stream position update
     * @param stream_id Stream ID
     * @param position New position for the stream
     */
    void updateStreamPosition(uint32_t stream_id, uint64_t position) {
        std::lock_guard<std::mutex> lock(m_streams_mutex);
        m_stream_positions[stream_id] = position;
    }
    
    /**
     * @brief Thread-safe stream position getter
     * @param stream_id Stream ID
     * @return Current position for the stream
     */
    uint64_t getStreamPosition(uint32_t stream_id) const {
        std::lock_guard<std::mutex> lock(m_streams_mutex);
        auto it = m_stream_positions.find(stream_id);
        return (it != m_stream_positions.end()) ? it->second : 0;
    }

protected:
    /**
     * @brief Attempt error recovery based on strategy
     * @param recovery Recovery strategy
     * @return true if recovery was successful
     */
    bool attemptErrorRecovery(DemuxerErrorRecovery recovery) const {
        switch (recovery) {
            case DemuxerErrorRecovery::NONE:
                return false;
                
            case DemuxerErrorRecovery::SKIP_SECTION:
                // Try to skip to next valid section
                return skipToNextValidSection();
                
            case DemuxerErrorRecovery::RESET_STATE:
                // Reset internal state
                return resetInternalState();
                
            case DemuxerErrorRecovery::FALLBACK_MODE:
                // Enable fallback parsing mode
                return enableFallbackMode();
                
            default:
                return false;
        }
    }
    
    /**
     * @brief Skip to next valid section in the file
     */
    virtual bool skipToNextValidSection() const {
        // This is format-specific and should be overridden by derived classes
        return false;
    }
    
    /**
     * @brief Reset internal parsing state
     */
    virtual bool resetInternalState() const {
        // This is format-specific and should be overridden by derived classes
        return false;
    }
    
    /**
     * @brief Enable fallback parsing mode
     */
    virtual bool enableFallbackMode() const {
        // This is format-specific and should be overridden by derived classes
        return false;
    }

private:
};

} // namespace Demuxer
} // namespace PsyMP3

// DemuxerFactory class moved to DemuxerFactory.h

#endif // DEMUXER_H


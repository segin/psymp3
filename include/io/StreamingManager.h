/*
 * StreamingManager.h - Advanced streaming management with memory optimization
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

#ifndef STREAMING_MANAGER_H
#define STREAMING_MANAGER_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace IO {

// Bring demuxer types into this namespace
using PsyMP3::Demuxer::Demuxer;
using PsyMP3::Demuxer::StreamInfo;
using PsyMP3::Demuxer::MediaChunk;

/**
 * @brief Advanced streaming manager with memory optimization
 * 
 * This class provides optimized streaming capabilities for demuxers,
 * with adaptive buffering, memory pressure awareness, and efficient
 * resource utilization.
 */
class StreamingManager {
public:
    /**
     * @brief Constructor
     * @param demuxer Demuxer to manage
     * @param stream_id Stream ID to read from
     */
    StreamingManager(std::shared_ptr<Demuxer> demuxer, uint32_t stream_id = 0);
    
    /**
     * @brief Destructor
     */
    ~StreamingManager();
    
    /**
     * @brief Start streaming
     * @return true if successful
     */
    bool start();
    
    /**
     * @brief Stop streaming
     */
    void stop();
    
    /**
     * @brief Seek to a specific time position
     * @param timestamp_ms Time in milliseconds
     * @return true if seek was successful
     */
    bool seekTo(uint64_t timestamp_ms);
    
    /**
     * @brief Read a chunk from the stream
     * @return MediaChunk with data, or empty chunk if EOF
     */
    MediaChunk readChunk();
    
    /**
     * @brief Check if we've reached the end of the stream
     */
    bool isEOF() const;
    
    /**
     * @brief Get current position in milliseconds
     */
    uint64_t getPosition() const;
    
    /**
     * @brief Get total duration of the stream in milliseconds
     */
    uint64_t getDuration() const;
    
    /**
     * @brief Get information about the current stream
     */
    StreamInfo getStreamInfo() const;
    
    /**
     * @brief Set the buffer size in chunks
     * @param max_chunks Maximum number of chunks to buffer
     */
    void setBufferSize(size_t max_chunks);
    
    /**
     * @brief Set the buffer size in bytes
     * @param max_bytes Maximum number of bytes to buffer
     */
    void setBufferSizeBytes(size_t max_bytes);
    
    /**
     * @brief Get current buffer statistics
     */
    struct BufferStats {
        size_t buffered_chunks;
        size_t buffered_bytes;
        size_t max_chunks;
        size_t max_bytes;
        bool is_buffering;
        float buffer_fullness; // 0.0 to 1.0
    };
    BufferStats getBufferStats() const;
    
    /**
     * @brief Set read-ahead mode for network streams
     * @param enable_read_ahead true to enable read-ahead
     */
    void setReadAheadMode(bool enable_read_ahead);
    
    /**
     * @brief Set adaptive buffer mode
     * @param enable_adaptive true to enable adaptive buffering
     */
    void setAdaptiveBuffering(bool enable_adaptive);
    
private:
    std::shared_ptr<Demuxer> m_demuxer;
    uint32_t m_stream_id;
    
    // Bounded buffer for chunks
    BoundedQueue<MediaChunk> m_chunk_queue;
    
    // Streaming thread
    std::thread m_streaming_thread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_buffering;
    std::atomic<bool> m_eof;
    std::atomic<uint64_t> m_position_ms;
    
    // Synchronization
    std::mutex m_mutex;
    std::condition_variable m_buffer_cv;
    
    // Buffer configuration
    size_t m_max_chunks;
    size_t m_max_bytes;
    bool m_read_ahead_mode;
    bool m_adaptive_buffering;
    
    // Memory pressure callback
    int m_memory_pressure_callback_id;
    
    // Thread function
    void streamingThreadFunc();
    
    // Memory pressure handler
    void handleMemoryPressure(int pressure_level);
    
    // Calculate chunk memory usage
    static size_t calculateChunkMemory(const MediaChunk& chunk);
};

} // namespace IO
} // namespace PsyMP3

#endif // STREAMING_MANAGER_H

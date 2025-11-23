/*
 * StreamingManager.cpp - Advanced streaming management with memory optimization
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace IO {

// Default buffer sizes
static constexpr size_t DEFAULT_MAX_CHUNKS = 32;
static constexpr size_t DEFAULT_MAX_BYTES = 1024 * 1024; // 1MB

StreamingManager::StreamingManager(std::shared_ptr<Demuxer> demuxer, uint32_t stream_id)
    : m_demuxer(demuxer)
    , m_stream_id(stream_id)
    , m_chunk_queue(DEFAULT_MAX_CHUNKS, DEFAULT_MAX_BYTES, calculateChunkMemory)
    , m_running(false)
    , m_buffering(false)
    , m_eof(false)
    , m_position_ms(0)
    , m_max_chunks(DEFAULT_MAX_CHUNKS)
    , m_max_bytes(DEFAULT_MAX_BYTES)
    , m_read_ahead_mode(false)
    , m_adaptive_buffering(true)
    , m_memory_pressure_callback_id(0) {
    
    // Register for memory pressure updates
    m_memory_pressure_callback_id = MemoryTracker::getInstance().registerMemoryPressureCallback(
        [this](int pressure) { this->handleMemoryPressure(pressure); }
    );
}

StreamingManager::~StreamingManager() {
    // Stop streaming thread
    stop();
    
    // Unregister memory pressure callback
    if (m_memory_pressure_callback_id != 0) {
        MemoryTracker::getInstance().unregisterMemoryPressureCallback(m_memory_pressure_callback_id);
    }
}

bool StreamingManager::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_running) {
        return true; // Already running
    }
    
    if (!m_demuxer) {
        return false;
    }
    
    // Reset state
    m_running = true;
    m_buffering = true;
    m_eof = false;
    
    // Start streaming thread
    m_streaming_thread = std::thread(&StreamingManager::streamingThreadFunc, this);
    
    return true;
}

void StreamingManager::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_running) {
            return; // Already stopped
        }
        
        m_running = false;
    }
    
    // Notify any waiting threads
    m_buffer_cv.notify_all();
    
    // Wait for thread to finish
    if (m_streaming_thread.joinable()) {
        m_streaming_thread.join();
    }
    
    // Clear buffer
    m_chunk_queue.clear();
}

bool StreamingManager::seekTo(uint64_t timestamp_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_demuxer) {
        return false;
    }
    
    // Clear buffer
    m_chunk_queue.clear();
    
    // Seek demuxer
    if (!m_demuxer->seekTo(timestamp_ms)) {
        return false;
    }
    
    // Update position
    m_position_ms = timestamp_ms;
    m_eof = false;
    m_buffering = true;
    
    // Notify streaming thread to refill buffer
    m_buffer_cv.notify_one();
    
    return true;
}

MediaChunk StreamingManager::readChunk() {
    MediaChunk chunk;
    
    // Try to get a chunk from the buffer
    if (m_chunk_queue.tryPop(chunk)) {
        // Update position based on chunk timestamp
        if (chunk.timestamp_samples > 0) {
            StreamInfo info = m_demuxer->getStreamInfo(m_stream_id);
            if (info.sample_rate > 0) {
                m_position_ms = (chunk.timestamp_samples * 1000) / info.sample_rate;
            }
        }
        
        // Signal buffer space available
        m_buffer_cv.notify_one();
        
        return chunk;
    }
    
    // If buffer is empty and we're at EOF, return empty chunk
    if (m_eof) {
        return MediaChunk();
    }
    
    // If buffer is empty but not at EOF, we're buffering
    m_buffering = true;
    
    // If we're in read-ahead mode, wait for buffer to fill
    if (m_read_ahead_mode) {
        // Wait a short time for buffer to fill
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Try again
        if (m_chunk_queue.tryPop(chunk)) {
            // Update position based on chunk timestamp
            if (chunk.timestamp_samples > 0) {
                StreamInfo info = m_demuxer->getStreamInfo(m_stream_id);
                if (info.sample_rate > 0) {
                    m_position_ms = (chunk.timestamp_samples * 1000) / info.sample_rate;
                }
            }
            
            // Signal buffer space available
            m_buffer_cv.notify_one();
            
            return chunk;
        }
    }
    
    // If still no chunk, read directly from demuxer
    if (m_demuxer) {
        chunk = m_demuxer->readChunk(m_stream_id);
        
        // Update EOF status
        if (chunk.isEmpty() && m_demuxer->isEOF()) {
            m_eof = true;
        }
        
        // Update position based on chunk timestamp
        if (!chunk.isEmpty() && chunk.timestamp_samples > 0) {
            StreamInfo info = m_demuxer->getStreamInfo(m_stream_id);
            if (info.sample_rate > 0) {
                m_position_ms = (chunk.timestamp_samples * 1000) / info.sample_rate;
            }
        }
    }
    
    return chunk;
}

bool StreamingManager::isEOF() const {
    return m_eof && m_chunk_queue.empty();
}

uint64_t StreamingManager::getPosition() const {
    return m_position_ms;
}

uint64_t StreamingManager::getDuration() const {
    return m_demuxer ? m_demuxer->getDuration() : 0;
}

StreamInfo StreamingManager::getStreamInfo() const {
    return m_demuxer ? m_demuxer->getStreamInfo(m_stream_id) : StreamInfo();
}

void StreamingManager::setBufferSize(size_t max_chunks) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_max_chunks = max_chunks;
    m_chunk_queue.setMaxItems(max_chunks);
}

void StreamingManager::setBufferSizeBytes(size_t max_bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_max_bytes = max_bytes;
    m_chunk_queue.setMaxMemoryBytes(max_bytes);
}

StreamingManager::BufferStats StreamingManager::getBufferStats() const {
    BufferStats stats;
    stats.buffered_chunks = m_chunk_queue.size();
    stats.buffered_bytes = m_chunk_queue.memoryUsage();
    stats.max_chunks = m_max_chunks;
    stats.max_bytes = m_max_bytes;
    stats.is_buffering = m_buffering;
    
    // Calculate buffer fullness
    if (stats.max_chunks > 0) {
        stats.buffer_fullness = static_cast<float>(stats.buffered_chunks) / stats.max_chunks;
    } else {
        stats.buffer_fullness = 0.0f;
    }
    
    return stats;
}

void StreamingManager::setReadAheadMode(bool enable_read_ahead) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_read_ahead_mode = enable_read_ahead;
}

void StreamingManager::setAdaptiveBuffering(bool enable_adaptive) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_adaptive_buffering = enable_adaptive;
}

void StreamingManager::streamingThreadFunc() {
    Debug::log("streaming", "StreamingManager: Streaming thread started");
    
    while (m_running) {
        MediaChunk chunk;
        bool buffer_full = false;
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // Check if we're at EOF
            if (m_demuxer && m_demuxer->isEOF()) {
                m_eof = true;
            }
            
            // If we're at EOF or buffer is full, wait for space
            if (m_eof || m_chunk_queue.size() >= m_max_chunks) {
                buffer_full = true;
                m_buffering = false;
            } else {
                // Read a chunk from the demuxer
                if (m_demuxer) {
                    chunk = m_demuxer->readChunk(m_stream_id);
                    
                    // Check for EOF
                    if (chunk.isEmpty() && m_demuxer->isEOF()) {
                        m_eof = true;
                        m_buffering = false;
                    }
                }
            }
        }
        
        // If we have a valid chunk, add it to the buffer
        if (!chunk.isEmpty()) {
            if (!m_chunk_queue.tryPush(std::move(chunk))) {
                // Buffer is full, wait for space
                buffer_full = true;
                m_buffering = false;
            }
        }
        
        // If buffer is full or we're at EOF, wait for a signal
        if (buffer_full || m_eof) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_buffer_cv.wait_for(lock, std::chrono::milliseconds(100), 
                               [this]() { return !m_running || m_chunk_queue.size() < m_max_chunks; });
        } else {
            // If we're in read-ahead mode, read as fast as possible
            if (m_read_ahead_mode) {
                continue;
            }
            
            // Otherwise, throttle reading to avoid consuming too much CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    Debug::log("streaming", "StreamingManager: Streaming thread stopped");
}

void StreamingManager::handleMemoryPressure(int pressure_level) {
    if (!m_adaptive_buffering) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Adjust buffer size based on memory pressure using a more granular approach
    size_t new_max_chunks = DEFAULT_MAX_CHUNKS;
    size_t new_max_bytes = DEFAULT_MAX_BYTES;
    
    if (pressure_level < 30) {
        // Low pressure - use larger buffers
        float scale_factor = 1.0f + (0.5f * (30 - pressure_level) / 30);
        new_max_chunks = static_cast<size_t>(DEFAULT_MAX_CHUNKS * scale_factor);
        new_max_bytes = static_cast<size_t>(DEFAULT_MAX_BYTES * scale_factor);
    } else if (pressure_level > 50) {
        // Medium to high pressure - use smaller buffers
        float scale_factor = 1.0f - (0.7f * (pressure_level - 50) / 50);
        new_max_chunks = static_cast<size_t>(DEFAULT_MAX_CHUNKS * scale_factor);
        new_max_bytes = static_cast<size_t>(DEFAULT_MAX_BYTES * scale_factor);
        
        // Ensure minimum sizes
        new_max_chunks = std::max(new_max_chunks, size_t(4));
        new_max_bytes = std::max(new_max_bytes, size_t(64 * 1024)); // 64KB minimum
    }
    
    // Update buffer limits
    m_max_chunks = new_max_chunks;
    m_max_bytes = new_max_bytes;
    m_chunk_queue.setMaxItems(new_max_chunks);
    m_chunk_queue.setMaxMemoryBytes(new_max_bytes);
    
    Debug::log("streaming", "StreamingManager: Adjusted buffer size due to memory pressure: ",
               pressure_level, "% (chunks=", new_max_chunks, ", bytes=", new_max_bytes, ")");
    
    // Under extreme memory pressure (>85%), proactively clear some buffers
    if (pressure_level > 85) {
        // Get current queue size
        size_t current_size = m_chunk_queue.size();
        
        // If we have more than 4 chunks, remove oldest chunks to reduce to half
        if (current_size > 4) {
            size_t to_remove = current_size / 2;
            Debug::log("streaming", "StreamingManager: High memory pressure - removing ", 
                      to_remove, " oldest chunks");
            
            // Remove oldest chunks (we'll need to pop and push back the ones we want to keep)
            std::vector<MediaChunk> keep_chunks;
            MediaChunk chunk;
            
            // Skip the chunks we want to remove
            for (size_t i = 0; i < to_remove; i++) {
                if (m_chunk_queue.tryPop(chunk)) {
                    // Just discard these chunks
                }
            }
            
            // Keep the remaining chunks
            while (m_chunk_queue.tryPop(chunk)) {
                keep_chunks.push_back(std::move(chunk));
            }
            
            // Push back the chunks we want to keep
            for (auto& keep_chunk : keep_chunks) {
                m_chunk_queue.tryPush(std::move(keep_chunk));
            }
        }
    }
}

size_t StreamingManager::calculateChunkMemory(const MediaChunk& chunk) {
    // Base size of the MediaChunk structure
    size_t base_size = sizeof(MediaChunk);
    
    // Size of the data vector
    size_t data_size = chunk.data.capacity() * sizeof(uint8_t);
    
    return base_size + data_size;
}

} // namespace IO
} // namespace PsyMP3

/*
 * BufferPool.cpp - Memory pool implementations for efficient buffer reuse
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// BufferPool implementation for MediaChunk data buffers
BufferPool& BufferPool::getInstance() {
    static BufferPool instance;
    return instance;
}

std::vector<uint8_t> BufferPool::getBuffer(size_t min_size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Don't pool very large buffers to avoid memory waste
    if (min_size > MAX_BUFFER_SIZE) {
        std::vector<uint8_t> buffer;
        buffer.reserve(min_size);
        return buffer;
    }
    
    // Find a suitable buffer from the pool
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
        if (it->capacity() >= min_size) {
            std::vector<uint8_t> buffer = std::move(*it);
            m_buffers.erase(it);
            buffer.clear(); // Clear contents but keep capacity
            return buffer;
        }
    }
    
    // No suitable buffer found, create new one
    std::vector<uint8_t> buffer;
    buffer.reserve(std::max(min_size, size_t(8192))); // Minimum 8KB allocation
    return buffer;
}

void BufferPool::returnBuffer(std::vector<uint8_t>&& buffer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only pool buffers that are reasonably sized and not too large
    if (buffer.capacity() >= 1024 && buffer.capacity() <= MAX_BUFFER_SIZE && 
        m_buffers.size() < MAX_POOLED_BUFFERS) {
        buffer.clear(); // Clear contents but keep capacity
        m_buffers.push_back(std::move(buffer));
    }
    // Otherwise, let the buffer be destroyed naturally
}

void BufferPool::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffers.clear();
}

BufferPool::PoolStats BufferPool::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    PoolStats stats{};
    stats.total_buffers = m_buffers.size();
    
    for (const auto& buffer : m_buffers) {
        stats.total_memory_bytes += buffer.capacity();
        stats.largest_buffer_size = std::max(stats.largest_buffer_size, buffer.capacity());
    }
    
    return stats;
}

// AudioBufferPool implementation for AudioFrame sample buffers
AudioBufferPool& AudioBufferPool::getInstance() {
    static AudioBufferPool instance;
    return instance;
}

std::vector<int16_t> AudioBufferPool::getSampleBuffer(size_t min_samples) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Don't pool very large buffers
    if (min_samples > MAX_SAMPLES_PER_BUFFER) {
        std::vector<int16_t> buffer;
        buffer.reserve(min_samples);
        return buffer;
    }
    
    // Find a suitable buffer from the pool
    for (auto it = m_sample_buffers.begin(); it != m_sample_buffers.end(); ++it) {
        if (it->capacity() >= min_samples) {
            std::vector<int16_t> buffer = std::move(*it);
            m_sample_buffers.erase(it);
            buffer.clear(); // Clear contents but keep capacity
            return buffer;
        }
    }
    
    // No suitable buffer found, create new one
    std::vector<int16_t> buffer;
    buffer.reserve(std::max(min_samples, size_t(4096))); // Minimum ~85ms at 48kHz stereo
    return buffer;
}

void AudioBufferPool::returnSampleBuffer(std::vector<int16_t>&& buffer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only pool buffers that are reasonably sized
    if (buffer.capacity() >= 4096 && buffer.capacity() <= MAX_SAMPLES_PER_BUFFER && 
        m_sample_buffers.size() < MAX_POOLED_BUFFERS) {
        buffer.clear(); // Clear contents but keep capacity
        m_sample_buffers.push_back(std::move(buffer));
    }
    // Otherwise, let the buffer be destroyed naturally
}

void AudioBufferPool::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sample_buffers.clear();
}
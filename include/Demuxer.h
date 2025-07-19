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

#include "IOHandler.h"

/**
 * @brief Information about a media stream within a container
 */
struct StreamInfo {
    uint32_t stream_id;
    std::string codec_type;        // "audio", "video", "subtitle", etc.
    std::string codec_name;        // "pcm", "mp3", "aac", "flac", etc.
    uint32_t codec_tag;            // Format-specific codec identifier
    
    // Audio-specific properties
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    uint32_t bitrate = 0;
    
    // Additional codec-specific data
    std::vector<uint8_t> codec_data; // Extra data needed by codec (e.g., decoder config)
    
    // Timing information
    uint64_t duration_samples = 0;
    uint64_t duration_ms = 0;
    
    // Metadata
    std::string artist;
    std::string title;
    std::string album;
};

/**
 * @brief A chunk of media data with metadata
 */
struct MediaChunk {
    uint32_t stream_id;
    std::vector<uint8_t> data;
    uint64_t granule_position = 0;     // For Ogg-based formats
    uint64_t timestamp_samples = 0;    // For other formats
    bool is_keyframe = true;           // For audio, usually always true
    uint64_t file_offset = 0;          // Original offset in file (for seeking)
};

/**
 * @brief Base class for all container demuxers
 * 
 * A demuxer is responsible for parsing container formats (RIFF, Ogg, MP4, etc.)
 * and extracting individual streams of media data. It does not decode the actual
 * audio/video data - that's the job of codec classes.
 */
class Demuxer {
public:
    explicit Demuxer(std::unique_ptr<IOHandler> handler);
    virtual ~Demuxer() = default;
    
    /**
     * @brief Parse the container headers and identify streams
     * @return true if container was successfully parsed, false otherwise
     */
    virtual bool parseContainer() = 0;
    
    /**
     * @brief Get information about all streams in the container
     */
    virtual std::vector<StreamInfo> getStreams() const = 0;
    
    /**
     * @brief Get information about a specific stream
     */
    virtual StreamInfo getStreamInfo(uint32_t stream_id) const = 0;
    
    /**
     * @brief Read the next chunk of data from any stream
     * @return MediaChunk with data, or empty chunk if EOF
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
     * @param timestamp_ms Time in milliseconds
     * @return true if seek was successful
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
    
protected:
    std::unique_ptr<IOHandler> m_handler;
    std::vector<StreamInfo> m_streams;
    uint64_t m_duration_ms = 0;
    uint64_t m_position_ms = 0;
    bool m_parsed = false;
    
    /**
     * @brief Helper to read little-endian values
     */
    template<typename T>
    T readLE() {
        T value;
        if (m_handler->read(&value, sizeof(T), 1) != 1) {
            throw std::runtime_error("Unexpected end of file");
        }
        return value;
    }
    
    /**
     * @brief Helper to read big-endian values
     */
    template<typename T>
    T readBE() {
        T value;
        if (m_handler->read(&value, sizeof(T), 1) != 1) {
            throw std::runtime_error("Unexpected end of file");
        }
        // Convert from big-endian to host byte order
        if constexpr (sizeof(T) == 2) {
            return __builtin_bswap16(value);
        } else if constexpr (sizeof(T) == 4) {
            return __builtin_bswap32(value);
        } else if constexpr (sizeof(T) == 8) {
            return __builtin_bswap64(value);
        }
        return value;
    }
    
    /**
     * @brief Helper to read a FourCC code
     */
    uint32_t readFourCC() {
        return readLE<uint32_t>();
    }
};

/**
 * @brief Factory for creating appropriate demuxers based on file content
 */
class DemuxerFactory {
public:
    /**
     * @brief Create a demuxer for the given file
     * @param handler IOHandler for the file (will be moved to the demuxer)
     * @return Unique pointer to appropriate demuxer, or nullptr if format not supported
     */
    static std::unique_ptr<Demuxer> createDemuxer(std::unique_ptr<IOHandler> handler);
    
    /**
     * @brief Create a demuxer for the given file with path hint
     * @param handler IOHandler for the file (will be moved to the demuxer)
     * @param file_path Original file path (for extension-based raw format detection)
     * @return Unique pointer to appropriate demuxer, or nullptr if format not supported
     */
    static std::unique_ptr<Demuxer> createDemuxer(std::unique_ptr<IOHandler> handler, 
                                                  const std::string& file_path);
    
private:
    /**
     * @brief Probe file format by examining magic bytes
     */
    static std::string probeFormat(IOHandler* handler);
};

#endif // DEMUXER_H
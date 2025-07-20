/*
 * DemuxerFactory.h - Factory for creating appropriate demuxers based on file content
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

#ifndef DEMUXERFACTORY_H
#define DEMUXERFACTORY_H

#include "Demuxer.h"
#include <memory>
#include <string>
#include <vector>
#include <map>

/**
 * @brief Format signature for magic byte detection
 */
struct FormatSignature {
    std::string format_id;           // Format identifier (e.g., "riff", "ogg", "mp4")
    std::vector<uint8_t> signature;  // Magic bytes to match
    size_t offset;                   // Offset in file where signature should appear
    int priority;                    // Priority for ambiguous cases (higher = preferred)
    std::string description;         // Human-readable description
    
    FormatSignature(const std::string& id, const std::vector<uint8_t>& sig, 
                   size_t off = 0, int prio = 100, const std::string& desc = "")
        : format_id(id), signature(sig), offset(off), priority(prio), description(desc) {}
};

/**
 * @brief Factory for creating appropriate demuxers based on file content
 * 
 * The factory uses multiple detection methods:
 * 1. Magic byte detection - examines file headers for format signatures
 * 2. File extension hints - used for raw audio formats and fallback detection
 * 3. Priority-based matching - handles ambiguous cases where multiple formats match
 * 
 * Supported formats:
 * - RIFF/WAV files (ChunkDemuxer)
 * - AIFF files (ChunkDemuxer) 
 * - Ogg files (OggDemuxer)
 * - MP4/M4A files (ISODemuxer)
 * - FLAC files (FLACDemuxer)
 * - Raw audio files (RawAudioDemuxer)
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
    
    /**
     * @brief Probe file format by examining magic bytes and content
     * @param handler IOHandler to examine (position will be restored)
     * @return Format identifier string, or empty string if unknown
     */
    static std::string probeFormat(IOHandler* handler);
    
    /**
     * @brief Probe file format with file path hint for better detection
     * @param handler IOHandler to examine (position will be restored)
     * @param file_path Original file path for extension-based hints
     * @return Format identifier string, or empty string if unknown
     */
    static std::string probeFormat(IOHandler* handler, const std::string& file_path);
    
    /**
     * @brief Get all supported format signatures
     * @return Vector of format signatures used for detection
     */
    static std::vector<FormatSignature> getSupportedFormats();
    
    /**
     * @brief Check if a format is supported by the factory
     * @param format_id Format identifier to check
     * @return true if format is supported
     */
    static bool isFormatSupported(const std::string& format_id);
    
    /**
     * @brief Get format description
     * @param format_id Format identifier
     * @return Human-readable description of the format
     */
    static std::string getFormatDescription(const std::string& format_id);
    
private:
    /**
     * @brief Format signature database with priority-based matching
     */
    static const std::vector<FormatSignature> s_format_signatures;
    
    /**
     * @brief Detect format using magic byte signatures
     * @param handler IOHandler to examine
     * @param buffer Buffer containing file header data
     * @param buffer_size Size of buffer data
     * @return Format identifier or empty string
     */
    static std::string detectByMagicBytes(IOHandler* handler, const uint8_t* buffer, size_t buffer_size);
    
    /**
     * @brief Detect format using file extension hints
     * @param file_path File path to examine
     * @return Format identifier or empty string
     */
    static std::string detectByExtension(const std::string& file_path);
    
    /**
     * @brief Check if signature matches at given offset
     * @param buffer Buffer to check
     * @param buffer_size Size of buffer
     * @param signature Signature to match
     * @param offset Offset in buffer to check
     * @return true if signature matches
     */
    static bool matchesSignature(const uint8_t* buffer, size_t buffer_size,
                                const std::vector<uint8_t>& signature, size_t offset);
    
    /**
     * @brief Handle ambiguous format detection cases
     * @param candidates Vector of matching format IDs with priorities
     * @param file_path Optional file path for additional hints
     * @return Best format ID or empty string
     */
    static std::string resolveAmbiguousFormat(const std::vector<std::pair<std::string, int>>& candidates,
                                            const std::string& file_path = "");
    
    /**
     * @brief Create demuxer instance for detected format
     * @param format_id Format identifier
     * @param handler IOHandler (will be moved to demuxer)
     * @param file_path Optional file path for format-specific configuration
     * @return Demuxer instance or nullptr
     */
    static std::unique_ptr<Demuxer> createDemuxerForFormat(const std::string& format_id,
                                                          std::unique_ptr<IOHandler> handler,
                                                          const std::string& file_path = "");
    
    /**
     * @brief Validate IOHandler before format detection
     * @param handler IOHandler to validate
     * @return true if handler is valid and ready for use
     */
    static bool validateIOHandler(IOHandler* handler);
    
    /**
     * @brief Read file header for format detection
     * @param handler IOHandler to read from
     * @param buffer Buffer to fill with header data
     * @param buffer_size Size of buffer
     * @return Number of bytes actually read
     */
    static size_t readFileHeader(IOHandler* handler, uint8_t* buffer, size_t buffer_size);
};

#endif // DEMUXERFACTORY_H
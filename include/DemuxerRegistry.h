/*
 * DemuxerRegistry.h - Registry for demuxer factories
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

#ifndef DEMUXERREGISTRY_H
#define DEMUXERREGISTRY_H

#include "Demuxer.h"
#include "IOHandler.h"
#include <functional>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

/**
 * @brief Exception thrown when a format is not supported
 */
class UnsupportedFormatException : public std::runtime_error {
public:
    explicit UnsupportedFormatException(const std::string& format_id, const std::string& reason);
    
    /**
     * @brief Get the format ID that was not supported
     */
    const std::string& getFormatId() const { return m_format_id; }
    
    /**
     * @brief Get the reason why the format is not supported
     */
    const std::string& getReason() const { return m_reason; }

private:
    std::string m_format_id;
    std::string m_reason;
};

/**
 * @brief Exception thrown when format detection fails
 */
class FormatDetectionException : public std::runtime_error {
public:
    explicit FormatDetectionException(const std::string& file_path, const std::string& reason);
    
    /**
     * @brief Get the file path that failed detection
     */
    const std::string& getFilePath() const { return m_file_path; }
    
    /**
     * @brief Get the reason why detection failed
     */
    const std::string& getReason() const { return m_reason; }

private:
    std::string m_file_path;
    std::string m_reason;
};

/**
 * @brief Factory function type for creating demuxer instances
 */
using DemuxerFactoryFunc = std::function<std::unique_ptr<Demuxer>(std::unique_ptr<IOHandler>)>;

/**
 * @brief Registry for demuxer factories
 * 
 * This registry provides a centralized system for managing demuxer factories.
 * Demuxers register themselves at application startup through registration functions,
 * eliminating the need for conditional compilation checks throughout the codebase.
 * 
 * The registry supports:
 * - Dynamic demuxer registration and lookup
 * - Graceful handling of missing demuxers
 * - Introspection of available formats
 * - Factory-based demuxer creation
 */
class DemuxerRegistry {
public:
    /**
     * @brief Register a demuxer factory function
     * @param format_id Format identifier (e.g., "riff", "ogg", "mp4")
     * @param factory Factory function that creates demuxer instances
     */
    static void registerDemuxer(const std::string& format_id, DemuxerFactoryFunc factory);
    
    /**
     * @brief Create a demuxer instance for the given format
     * @param format_id Format identifier
     * @param handler IOHandler for the media file (will be moved to the demuxer)
     * @return Unique pointer to appropriate demuxer
     * @throws UnsupportedFormatException if format is not supported
     */
    static std::unique_ptr<Demuxer> createDemuxer(const std::string& format_id, std::unique_ptr<IOHandler> handler);
    
    /**
     * @brief Check if a format is supported
     * @param format_id Format identifier to check
     * @return true if format is registered and supported
     */
    static bool isFormatSupported(const std::string& format_id);
    
    /**
     * @brief Get list of all supported format IDs
     * @return Vector of format IDs that are currently registered
     */
    static std::vector<std::string> getSupportedFormats();
    
    /**
     * @brief Unregister a demuxer (for testing or dynamic unloading)
     * @param format_id Format identifier to unregister
     * @return true if format was found and unregistered
     */
    static bool unregisterDemuxer(const std::string& format_id);
    
    /**
     * @brief Clear all registered demuxers (for testing)
     */
    static void clearRegistry();
    
    /**
     * @brief Get the number of registered demuxers
     * @return Number of demuxers currently in the registry
     */
    static size_t getRegisteredDemuxerCount();
    
    /**
     * @brief Check if the registry has been initialized
     * @return true if at least one demuxer is registered
     */
    static bool isInitialized();

private:
    static std::map<std::string, DemuxerFactoryFunc> s_demuxer_factories;
    
    /**
     * @brief Validate format ID for registration
     * @param format_id Format ID to validate
     * @return true if format ID is valid
     */
    static bool isValidFormatId(const std::string& format_id);
    
    /**
     * @brief Validate factory function
     * @param factory Factory function to validate
     * @return true if factory is valid
     */
    static bool isValidFactory(const DemuxerFactoryFunc& factory);
};

#endif // DEMUXERREGISTRY_H
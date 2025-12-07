/*
 * DemuxerFactory.h - Factory for creating demuxers with optimized detection
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

#ifndef DEMUXER_FACTORY_H
#define DEMUXER_FACTORY_H

#include <functional>
#include <memory>
#include <string>
// Forward declare IOHandler properly
namespace PsyMP3 { namespace IO { class IOHandler; } }

namespace PsyMP3 {
namespace Demuxer {

/**
 * @brief Format signature for efficient detection
 */
struct FormatSignature {
    std::string format_id;
    std::vector<uint8_t> signature;
    size_t offset;
    int priority;
    
    FormatSignature(const std::string& id, const std::vector<uint8_t>& sig, 
                   size_t off = 0, int prio = 100)
        : format_id(id), signature(sig), offset(off), priority(prio) {}
};

/**
 * @brief Factory for creating demuxers with optimized detection
 */
class DemuxerFactory {
public:
    /**
     * @brief Create a demuxer for the given I/O handler
     * @param handler I/O handler for media source
     * @return Unique pointer to appropriate demuxer
     */
    static std::unique_ptr<Demuxer> createDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler);
    
    /**
     * @brief Create a demuxer with file path hint
     * @param handler I/O handler for media source
     * @param file_path Path hint for format detection
     * @return Unique pointer to appropriate demuxer
     */
    static std::unique_ptr<Demuxer> createDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler, 
                                                 const std::string& file_path);
    
    /**
     * @brief Probe format of the given I/O handler
     * @param handler I/O handler for media source
     * @return Format ID string, or empty if unknown
     */
    static std::string probeFormat(PsyMP3::IO::IOHandler* handler);
    
    /**
     * @brief Probe format with file path hint
     * @param handler I/O handler for media source
     * @param file_path Path hint for format detection
     * @return Format ID string, or empty if unknown
     */
    static std::string probeFormat(PsyMP3::IO::IOHandler* handler, const std::string& file_path);
    
    /**
     * @brief Register a demuxer factory function
     * @param format_id Format ID string
     * @param factory_func Function that creates demuxer instances
     */
    using DemuxerFactoryFunc = std::function<std::unique_ptr<Demuxer>(std::unique_ptr<PsyMP3::IO::IOHandler>)>;
    static void registerDemuxer(const std::string& format_id, DemuxerFactoryFunc factory_func);
    
    /**
     * @brief Register a format signature
     * @param signature Format signature information
     */
    static void registerSignature(const FormatSignature& signature);
    
    /**
     * @brief Get all registered format signatures
     * @return Vector of format signatures
     */
    static const std::vector<FormatSignature>& getSignatures();
    
private:
    static std::map<std::string, DemuxerFactoryFunc> s_demuxer_factories;
    static std::vector<FormatSignature> s_signatures;
    static std::map<std::string, std::string> s_extension_to_format;
    static std::mutex s_factory_mutex; // Thread safety for factory operations
    
    // Fast signature matching
    static bool matchSignature(const uint8_t* data, size_t data_size, 
                              const FormatSignature& signature);
    
    // Extension-based detection
    static std::string detectFormatFromExtension(const std::string& file_path);
    
    // Initialize built-in formats
    static void initializeBuiltInFormats();
    static bool s_initialized;
    
    // Private unlocked version for internal use
    static void registerSignature_unlocked(const FormatSignature& signature);
};

} // namespace Demuxer
} // namespace PsyMP3

#endif // DEMUXER_FACTORY_H
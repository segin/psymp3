/*
 * DemuxerRegistry.h - Registry for demuxer implementations with optimized lookup
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

#ifndef DEMUXER_REGISTRY_H
#define DEMUXER_REGISTRY_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Registry for demuxer implementations with optimized lookup
 */
class DemuxerRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static DemuxerRegistry& getInstance();
    
    /**
     * @brief Register a demuxer factory function
     * @param format_id Format ID string
     * @param factory_func Function that creates demuxer instances
     * @param format_name Human-readable format name
     * @param extensions File extensions for this format
     */
    void registerDemuxer(const std::string& format_id, 
                        DemuxerFactory::DemuxerFactoryFunc factory_func,
                        const std::string& format_name,
                        const std::vector<std::string>& extensions);
    
    /**
     * @brief Register a format signature
     * @param signature Format signature information
     */
    void registerSignature(const FormatSignature& signature);
    
    /**
     * @brief Create a demuxer for the given I/O handler
     * @param handler I/O handler for media source
     * @return Unique pointer to appropriate demuxer
     */
    std::unique_ptr<Demuxer> createDemuxer(std::unique_ptr<IOHandler> handler);
    
    /**
     * @brief Create a demuxer with file path hint
     * @param handler I/O handler for media source
     * @param file_path Path hint for format detection
     * @return Unique pointer to appropriate demuxer
     */
    std::unique_ptr<Demuxer> createDemuxer(std::unique_ptr<IOHandler> handler, 
                                          const std::string& file_path);
    
    /**
     * @brief Get information about supported formats
     */
    struct FormatInfo {
        std::string format_id;
        std::string format_name;
        std::vector<std::string> extensions;
        bool has_signature;
    };
    std::vector<FormatInfo> getSupportedFormats() const;
    
    /**
     * @brief Check if a format is supported
     * @param format_id Format ID to check
     * @return true if format is supported
     */
    bool isFormatSupported(const std::string& format_id) const;
    
    /**
     * @brief Check if a file extension is supported
     * @param extension File extension to check
     * @return true if extension is supported
     */
    bool isExtensionSupported(const std::string& extension) const;
    
private:
    DemuxerRegistry();
    ~DemuxerRegistry();
    
    // Internal method that assumes mutex is already held
    void registerSignatureInternal(const FormatSignature& signature);
    
    // Thread safety
    mutable std::mutex m_mutex;
    
    // Format registry
    struct FormatRegistration {
        std::string format_id;
        std::string format_name;
        std::vector<std::string> extensions;
        DemuxerFactory::DemuxerFactoryFunc factory_func;
    };
    std::map<std::string, FormatRegistration> m_formats;
    
    // Extension lookup (optimized)
    std::map<std::string, std::string> m_extension_to_format;
    
    // Format signatures
    std::vector<FormatSignature> m_signatures;
    
    // Format detection
    std::string probeFormat(IOHandler* handler);
    std::string detectFormatFromExtension(const std::string& file_path);
    bool matchSignature(const uint8_t* data, size_t data_size, 
                       const FormatSignature& signature);
    
    // Initialize built-in formats
    void initializeBuiltInFormats();
    bool m_initialized;
};

/**
 * @brief Helper class for automatic demuxer registration
 */
class DemuxerRegistration {
public:
    /**
     * @brief Constructor that registers a demuxer
     * @param format_id Format ID string
     * @param factory_func Function that creates demuxer instances
     * @param format_name Human-readable format name
     * @param extensions File extensions for this format
     * @param signatures Format signatures
     */
    DemuxerRegistration(const std::string& format_id,
                       DemuxerFactory::DemuxerFactoryFunc factory_func,
                       const std::string& format_name,
                       const std::vector<std::string>& extensions,
                       const std::vector<FormatSignature>& signatures = {});
};

#endif // DEMUXER_REGISTRY_H
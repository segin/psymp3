/*
 * DemuxerRegistry.cpp - Registry for demuxer implementations with optimized lookup
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Demuxer {

// DemuxerRegistry implementation
DemuxerRegistry& DemuxerRegistry::getInstance() {
    static DemuxerRegistry instance;
    return instance;
}

DemuxerRegistry::DemuxerRegistry() : m_initialized(false) {
    // Initialize built-in formats
    initializeBuiltInFormats();
}

DemuxerRegistry::~DemuxerRegistry() {
    // Nothing to clean up
}

void DemuxerRegistry::registerDemuxer(const std::string& format_id, 
                                    DemuxerFactory::DemuxerFactoryFunc factory_func,
                                    const std::string& format_name,
                                    const std::vector<std::string>& extensions) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Register format
    FormatRegistration reg;
    reg.format_id = format_id;
    reg.format_name = format_name;
    reg.extensions = extensions;
    reg.factory_func = factory_func;
    
    m_formats[format_id] = reg;
    
    // Register extensions
    for (const auto& ext : extensions) {
        std::string lower_ext = ext;
        std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        m_extension_to_format[lower_ext] = format_id;
    }
    
    // Register with DemuxerFactory for backward compatibility
    DemuxerFactory::registerDemuxer(format_id, factory_func);
}

void DemuxerRegistry::registerSignature(const FormatSignature& signature) {
    std::lock_guard<std::mutex> lock(m_mutex);
    registerSignatureInternal(signature);
}

void DemuxerRegistry::registerSignatureInternal(const FormatSignature& signature) {
    // This method assumes the mutex is already held
    m_signatures.push_back(signature);
    
    // Sort signatures by priority (highest first)
    std::sort(m_signatures.begin(), m_signatures.end(), 
             [](const auto& a, const auto& b) { return a.priority > b.priority; });
    
    // Register with DemuxerFactory for backward compatibility
    DemuxerFactory::registerSignature(signature);
}

std::unique_ptr<Demuxer> DemuxerRegistry::createDemuxer(std::unique_ptr<IOHandler> handler) {
    if (!handler) {
        return nullptr;
    }
    
    // Probe format
    std::string format_id = probeFormat(handler.get());
    
    // Create demuxer based on format
    if (!format_id.empty()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_formats.find(format_id);
        if (it != m_formats.end()) {
            return it->second.factory_func(std::move(handler));
        }
    }
    
    // Format not supported
    return nullptr;
}

std::unique_ptr<Demuxer> DemuxerRegistry::createDemuxer(std::unique_ptr<IOHandler> handler, 
                                                      const std::string& file_path) {
    if (!handler) {
        return nullptr;
    }
    
    // Try content-based detection first
    std::string format_id = probeFormat(handler.get());
    
    // Fall back to extension-based detection if needed
    if (format_id.empty()) {
        format_id = detectFormatFromExtension(file_path);
    }
    
    // Create demuxer based on format
    if (!format_id.empty()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_formats.find(format_id);
        if (it != m_formats.end()) {
            return it->second.factory_func(std::move(handler));
        }
    }
    
    // Format not supported
    return nullptr;
}

std::vector<DemuxerRegistry::FormatInfo> DemuxerRegistry::getSupportedFormats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<FormatInfo> result;
    
    for (const auto& pair : m_formats) {
        const auto& reg = pair.second;
        
        FormatInfo info;
        info.format_id = reg.format_id;
        info.format_name = reg.format_name;
        info.extensions = reg.extensions;
        
        // Check if format has a signature
        info.has_signature = false;
        for (const auto& sig : m_signatures) {
            if (sig.format_id == reg.format_id) {
                info.has_signature = true;
                break;
            }
        }
        
        result.push_back(info);
    }
    
    return result;
}

bool DemuxerRegistry::isFormatSupported(const std::string& format_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_formats.find(format_id) != m_formats.end();
}

bool DemuxerRegistry::isExtensionSupported(const std::string& extension) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string lower_ext = extension;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    
    return m_extension_to_format.find(lower_ext) != m_extension_to_format.end();
}

std::string DemuxerRegistry::probeFormat(IOHandler* handler) {
    if (!handler) {
        return "";
    }
    
    // Save current position
    long original_pos = handler->tell();
    
    // Read header bytes
    std::vector<uint8_t> header(128, 0);
    handler->seek(0, SEEK_SET);
    size_t bytes_read = handler->read(header.data(), 1, header.size());
    
    // Restore original position
    handler->seek(original_pos, SEEK_SET);
    
    if (bytes_read < 4) {
        return ""; // Not enough data to identify format
    }
    
    // Match signatures
    std::vector<std::pair<std::string, int>> matches;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (const auto& signature : m_signatures) {
            if (matchSignature(header.data(), bytes_read, signature)) {
                matches.emplace_back(signature.format_id, signature.priority);
            }
        }
    }
    
    // Sort matches by priority (highest first)
    std::sort(matches.begin(), matches.end(), 
             [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Return highest priority match
    if (!matches.empty()) {
        return matches.front().first;
    }
    
    return ""; // No match found
}

std::string DemuxerRegistry::detectFormatFromExtension(const std::string& file_path) {
    // Extract extension from file path
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos || dot_pos == file_path.length() - 1) {
        return ""; // No extension
    }
    
    std::string extension = file_path.substr(dot_pos + 1);
    
    // Convert to lowercase
    std::transform(extension.begin(), extension.end(), extension.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    
    // Look up extension
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_extension_to_format.find(extension);
    if (it != m_extension_to_format.end()) {
        return it->second;
    }
    
    return ""; // Unknown extension
}

bool DemuxerRegistry::matchSignature(const uint8_t* data, size_t data_size, 
                                   const FormatSignature& signature) {
    // Check if we have enough data
    if (signature.offset + signature.signature.size() > data_size) {
        return false;
    }
    
    // Compare signature bytes using SIMD if available
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    // Use SSE2 for faster comparison
    if (signature.signature.size() >= 16) {
        // Compare 16 bytes at a time
        size_t i = 0;
        for (; i + 16 <= signature.signature.size(); i += 16) {
            __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + signature.offset + i));
            __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(signature.signature.data() + i));
            __m128i cmp = _mm_cmpeq_epi8(a, b);
            int mask = _mm_movemask_epi8(cmp);
            if (mask != 0xFFFF) {
                return false;
            }
        }
        
        // Compare remaining bytes
        for (; i < signature.signature.size(); ++i) {
            if (data[signature.offset + i] != signature.signature[i]) {
                return false;
            }
        }
        
        return true;
    }
#endif
    
    // Fall back to standard comparison
    return std::memcmp(data + signature.offset, signature.signature.data(), 
                      signature.signature.size()) == 0;
}

void DemuxerRegistry::initializeBuiltInFormats() {
    if (m_initialized) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        return; // Double-check after acquiring lock
    }
    
    // Register built-in format signatures
    
    // RIFF/WAV signature
    registerSignatureInternal(FormatSignature("riff", {0x52, 0x49, 0x46, 0x46}, 0, 100)); // "RIFF"
    
    // AIFF signature
    registerSignatureInternal(FormatSignature("aiff", {0x46, 0x4F, 0x52, 0x4D}, 0, 100)); // "FORM"
    
    // Ogg signature
    registerSignatureInternal(FormatSignature("ogg", {0x4F, 0x67, 0x67, 0x53}, 0, 100)); // "OggS"
    
    // FLAC signature
    registerSignatureInternal(FormatSignature("flac", {0x66, 0x4C, 0x61, 0x43}, 0, 100)); // "fLaC"
    
    // MP4/ISO signature (ftyp box)
    registerSignatureInternal(FormatSignature("mp4", {0x66, 0x74, 0x79, 0x70}, 4, 90)); // "ftyp" at offset 4
    
    // MP3 signature (ID3v2)
    registerSignatureInternal(FormatSignature("mp3", {0x49, 0x44, 0x33}, 0, 80)); // "ID3"
    
    // MP3 signature (frame sync)
    registerSignatureInternal(FormatSignature("mp3", {0xFF, 0xFB}, 0, 70)); // MPEG frame sync
    
    m_initialized = true;
}

// DemuxerRegistration implementation
DemuxerRegistration::DemuxerRegistration(const std::string& format_id,
                                       DemuxerFactory::DemuxerFactoryFunc factory_func,
                                       const std::string& format_name,
                                       const std::vector<std::string>& extensions,
                                       const std::vector<FormatSignature>& signatures) {
    // Register with registry
    DemuxerRegistry& registry = DemuxerRegistry::getInstance();
    registry.registerDemuxer(format_id, factory_func, format_name, extensions);
    
    // Register signatures
    for (const auto& signature : signatures) {
        registry.registerSignature(signature);
    }
}

} // namespace Demuxer
} // namespace PsyMP3

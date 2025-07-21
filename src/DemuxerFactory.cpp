/*
 * DemuxerFactory.cpp - Factory for creating appropriate demuxers based on file content
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "DemuxerFactory.h"

// Format signature database with priority-based matching
const std::vector<FormatSignature> DemuxerFactory::s_format_signatures = {
    // RIFF/WAV format - highest priority for RIFF signature
    FormatSignature("riff", {0x52, 0x49, 0x46, 0x46}, 0, 150, "RIFF container (WAV, AVI, etc.)"),
    
    // AIFF/IFF format - high priority for FORM signature  
    FormatSignature("aiff", {0x46, 0x4F, 0x52, 0x4D}, 0, 140, "IFF container (AIFF, etc.)"),
    
    // Ogg format - high priority, very distinctive signature
    FormatSignature("ogg", {0x4F, 0x67, 0x67, 0x53}, 0, 130, "Ogg container (Vorbis, Opus, FLAC, etc.)"),
    
    // MP4/M4A format - check for ftyp box at offset 4
    FormatSignature("mp4", {0x66, 0x74, 0x79, 0x70}, 4, 120, "ISO Base Media (MP4, M4A, 3GP, etc.)"),
    
    // FLAC format - native FLAC files
    FormatSignature("flac", {0x66, 0x4C, 0x61, 0x43}, 0, 110, "FLAC audio"),
    
    // Additional MP4 variants with different ftyp brands
    FormatSignature("mp4", {0x4D, 0x34, 0x41, 0x20}, 8, 115, "M4A audio file"), // "M4A " brand
    FormatSignature("mp4", {0x6D, 0x70, 0x34, 0x31}, 8, 115, "MP4 version 1"),  // "mp41" brand
    FormatSignature("mp4", {0x6D, 0x70, 0x34, 0x32}, 8, 115, "MP4 version 2"),  // "mp42" brand
    FormatSignature("mp4", {0x69, 0x73, 0x6F, 0x6D}, 8, 115, "ISO Base Media"), // "isom" brand
    
    // AIFF-specific signatures - check for AIFF form type at offset 8
    FormatSignature("aiff", {0x41, 0x49, 0x46, 0x46}, 8, 135, "AIFF audio"),     // "AIFF"
    FormatSignature("aiff", {0x41, 0x49, 0x46, 0x43}, 8, 135, "AIFF-C audio"),   // "AIFC"
    
    // RIFF-specific signatures - check for WAVE form type at offset 8  
    FormatSignature("riff", {0x57, 0x41, 0x56, 0x45}, 8, 145, "WAVE audio"),     // "WAVE"
    
    // Additional format signatures for comprehensive detection
    FormatSignature("mp3", {0xFF, 0xFB}, 0, 90, "MP3 audio (MPEG-1 Layer 3)"),  // MP3 sync word
    FormatSignature("mp3", {0xFF, 0xFA}, 0, 90, "MP3 audio (MPEG-1 Layer 3)"),  // MP3 sync word variant
    FormatSignature("mp3", {0x49, 0x44, 0x33}, 0, 85, "MP3 with ID3 tag"),      // "ID3" tag
};

std::unique_ptr<Demuxer> DemuxerFactory::createDemuxer(std::unique_ptr<IOHandler> handler) {
    return createDemuxer(std::move(handler), ""); // No file path hint
}

std::unique_ptr<Demuxer> DemuxerFactory::createDemuxer(std::unique_ptr<IOHandler> handler, 
                                                       const std::string& file_path) {
    Debug::log("loader", "DemuxerFactory::createDemuxer called for file: ", file_path);
    
    if (!handler || !validateIOHandler(handler.get())) {
        Debug::log("loader", "DemuxerFactory: Invalid IOHandler provided");
        Debug::log("demuxer", "DemuxerFactory: Invalid IOHandler provided");
        return nullptr;
    }
    
    // First check for raw audio formats using file extension
    if (!file_path.empty()) {
        auto raw_config = RawAudioDetector::detectRawAudio(file_path);
        if (raw_config.has_value()) {
            Debug::log("loader", "DemuxerFactory: Detected raw audio format from extension: ", file_path);
            Debug::log("demuxer", "DemuxerFactory: Detected raw audio format from extension: ", file_path);
            return std::make_unique<RawAudioDemuxer>(std::move(handler), raw_config.value());
        }
    }
    
    // Probe the format using magic bytes and content analysis
    Debug::log("loader", "DemuxerFactory: Probing format for file: ", file_path);
    std::string format = probeFormat(handler.get(), file_path);
    
    if (format.empty()) {
        Debug::log("loader", "DemuxerFactory: Could not detect format for file: ", file_path);
        Debug::log("demuxer", "DemuxerFactory: Could not detect format for file: ", file_path);
        return nullptr;
    }
    
    Debug::log("loader", "DemuxerFactory: Detected format '", format, "' for file: ", file_path);
    Debug::log("demuxer", "DemuxerFactory: Detected format '", format, "' for file: ", file_path);
    
    // Create appropriate demuxer for the detected format
    auto demuxer = createDemuxerForFormat(format, std::move(handler), file_path);
    if (demuxer) {
        Debug::log("loader", "DemuxerFactory: Successfully created demuxer for format: ", format);
        Debug::log("demuxer", "DemuxerFactory: Successfully created demuxer for format: ", format);
    } else {
        Debug::log("loader", "DemuxerFactory: Failed to create demuxer for format: ", format);
        Debug::log("demuxer", "DemuxerFactory: Failed to create demuxer for format: ", format);
    }
    return demuxer;
}

std::string DemuxerFactory::probeFormat(IOHandler* handler) {
    return probeFormat(handler, "");
}

std::string DemuxerFactory::probeFormat(IOHandler* handler, const std::string& file_path) {
    Debug::log("loader", "DemuxerFactory::probeFormat called for file: ", file_path);
    
    if (!handler || !validateIOHandler(handler)) {
        Debug::log("loader", "DemuxerFactory: Invalid IOHandler for probing");
        return "";
    }
    
    // Save current position to restore later
    long original_pos = handler->tell();
    if (original_pos < 0) {
        Debug::log("loader", "DemuxerFactory: Could not get current file position");
        Debug::log("demuxer", "DemuxerFactory: Could not get current file position");
        return "";
    }
    
    // Seek to beginning for format detection
    if (handler->seek(0, SEEK_SET) != 0) {
        Debug::log("loader", "DemuxerFactory: Could not seek to beginning of file");
        Debug::log("demuxer", "DemuxerFactory: Could not seek to beginning of file");
        return "";
    }
    
    // Read header data for format detection (read more data for thorough analysis)
    constexpr size_t HEADER_SIZE = 64; // Increased from 12 to 64 bytes for better detection
    uint8_t header[HEADER_SIZE];
    size_t bytes_read = readFileHeader(handler, header, HEADER_SIZE);
    
    // Restore original position
    handler->seek(original_pos, SEEK_SET);
    
    if (bytes_read < 4) {
        Debug::log("loader", "DemuxerFactory: File too small for format detection (", bytes_read, " bytes)");
        Debug::log("demuxer", "DemuxerFactory: File too small for format detection (", bytes_read, " bytes)");
        return "";
    }
    
    Debug::log("loader", "DemuxerFactory: Read ", bytes_read, " bytes for format detection");
    Debug::log("demuxer", "DemuxerFactory: Read ", bytes_read, " bytes for format detection");
    
    // Log first few bytes for debugging
    std::stringstream hex_bytes;
    for (size_t i = 0; i < std::min(bytes_read, size_t(16)); i++) {
        hex_bytes << std::hex << std::setfill('0') << std::setw(2) 
                 << static_cast<int>(header[i]) << " ";
    }
    Debug::log("loader", "DemuxerFactory: First 16 bytes: ", hex_bytes.str());
    
    // Try magic byte detection first
    std::string format = detectByMagicBytes(handler, header, bytes_read);
    
    // If magic byte detection failed, try extension-based detection as fallback
    if (format.empty() && !file_path.empty()) {
        format = detectByExtension(file_path);
        if (!format.empty()) {
            Debug::log("loader", "DemuxerFactory: Used extension-based detection: ", format);
            Debug::log("demuxer", "DemuxerFactory: Used extension-based detection: ", format);
        }
    }
    
    Debug::log("loader", "DemuxerFactory::probeFormat result: ", format.empty() ? "unknown" : format);
    return format;
}

std::vector<FormatSignature> DemuxerFactory::getSupportedFormats() {
    return s_format_signatures;
}

bool DemuxerFactory::isFormatSupported(const std::string& format_id) {
    for (const auto& sig : s_format_signatures) {
        if (sig.format_id == format_id) {
            return true;
        }
    }
    
    // Also check for raw audio formats
    return format_id == "raw_audio";
}

std::string DemuxerFactory::getFormatDescription(const std::string& format_id) {
    for (const auto& sig : s_format_signatures) {
        if (sig.format_id == format_id) {
            return sig.description;
        }
    }
    
    if (format_id == "raw_audio") {
        return "Raw audio data (PCM, A-law, μ-law)";
    }
    
    return "Unknown format";
}

std::string DemuxerFactory::detectByMagicBytes(IOHandler* handler, const uint8_t* buffer, size_t buffer_size) {
    Debug::log("loader", "DemuxerFactory::detectByMagicBytes checking signatures against buffer");
    
    std::vector<std::pair<std::string, int>> candidates;
    
    // Check all format signatures
    for (const auto& sig : s_format_signatures) {
        if (matchesSignature(buffer, buffer_size, sig.signature, sig.offset)) {
            candidates.emplace_back(sig.format_id, sig.priority);
            Debug::log("loader", "DemuxerFactory: Signature match for format '", sig.format_id, 
                      "' (priority ", sig.priority, "): ", sig.description);
            Debug::log("demuxer", "DemuxerFactory: Signature match for format '", sig.format_id, 
                      "' (priority ", sig.priority, "): ", sig.description);
            
            // Special logging for Ogg format
            if (sig.format_id == "ogg") {
                Debug::log("ogg", "DemuxerFactory: Found Ogg signature match (OggS) at offset ", sig.offset);
            }
        }
    }
    
    if (candidates.empty()) {
        Debug::log("loader", "DemuxerFactory::detectByMagicBytes no signature matches found");
        return "";
    }
    
    // If only one candidate, return it
    if (candidates.size() == 1) {
        Debug::log("loader", "DemuxerFactory::detectByMagicBytes single match: ", candidates[0].first);
        return candidates[0].first;
    }
    
    // Handle multiple candidates by priority
    Debug::log("loader", "DemuxerFactory::detectByMagicBytes multiple matches, resolving by priority");
    return resolveAmbiguousFormat(candidates);
}

std::string DemuxerFactory::detectByExtension(const std::string& file_path) {
    if (file_path.empty()) {
        return "";
    }
    
    // Find the last dot for extension
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "";
    }
    
    std::string ext = file_path.substr(dot_pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Map common extensions to format IDs
    static const std::map<std::string, std::string> extension_map = {
        // Audio container formats
        {".wav", "riff"},
        {".wave", "riff"},
        {".aif", "aiff"},
        {".aiff", "aiff"},
        {".aifc", "aiff"},
        {".ogg", "ogg"},
        {".oga", "ogg"},
        {".opus", "ogg"},
        {".spx", "ogg"},
        {".mp4", "mp4"},
        {".m4a", "mp4"},
        {".m4b", "mp4"},
        {".m4p", "mp4"},
        {".3gp", "mp4"},
        {".3g2", "mp4"},
        {".flac", "flac"},
        {".fla", "flac"},
        
        // Raw audio formats (handled by RawAudioDemuxer)
        {".pcm", "raw_audio"},
        {".raw", "raw_audio"},
        {".ulaw", "raw_audio"},
        {".alaw", "raw_audio"},
        {".ul", "raw_audio"},
        {".al", "raw_audio"},
        {".mulaw", "raw_audio"},
        
        // Other audio formats
        {".mp3", "mp3"},
        {".mp2", "mp3"},
        {".mpa", "mp3"},
    };
    
    auto it = extension_map.find(ext);
    if (it != extension_map.end()) {
        Debug::log("demuxer", "DemuxerFactory: Extension-based detection: ", ext, " -> ", it->second);
        return it->second;
    }
    
    return "";
}

bool DemuxerFactory::matchesSignature(const uint8_t* buffer, size_t buffer_size,
                                     const std::vector<uint8_t>& signature, size_t offset) {
    if (offset + signature.size() > buffer_size) {
        return false;
    }
    
    bool matches = std::memcmp(buffer + offset, signature.data(), signature.size()) == 0;
    
    // For debugging, log Ogg signature checks
    if (signature.size() >= 4 && 
        signature[0] == 'O' && signature[1] == 'g' && 
        signature[2] == 'g' && signature[3] == 'S') {
        
        std::stringstream buffer_hex;
        for (size_t i = 0; i < std::min(signature.size(), size_t(4)); i++) {
            buffer_hex << std::hex << std::setfill('0') << std::setw(2) 
                      << static_cast<int>(buffer[offset + i]) << " ";
        }
        
        Debug::log("loader", "DemuxerFactory: Checking for Ogg signature at offset ", offset, 
                  ", buffer bytes: ", buffer_hex.str(), ", match: ", matches ? "yes" : "no");
    }
    
    return matches;
}

std::string DemuxerFactory::resolveAmbiguousFormat(const std::vector<std::pair<std::string, int>>& candidates,
                                                  const std::string& file_path) {
    if (candidates.empty()) {
        return "";
    }
    
    // Sort by priority (highest first)
    auto sorted_candidates = candidates;
    std::sort(sorted_candidates.begin(), sorted_candidates.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Log the resolution process
    Debug::log("demuxer", "DemuxerFactory: Resolving ambiguous format detection:");
    for (const auto& candidate : sorted_candidates) {
        Debug::log("demuxer", "  - ", candidate.first, " (priority ", candidate.second, ")");
    }
    
    // Use file extension as tiebreaker if priorities are equal
    if (sorted_candidates.size() > 1 && 
        sorted_candidates[0].second == sorted_candidates[1].second &&
        !file_path.empty()) {
        
        std::string ext_format = detectByExtension(file_path);
        for (const auto& candidate : sorted_candidates) {
            if (candidate.first == ext_format) {
                Debug::log("demuxer", "DemuxerFactory: Used extension tiebreaker: ", ext_format);
                return candidate.first;
            }
        }
    }
    
    // Return highest priority candidate
    std::string result = sorted_candidates[0].first;
    Debug::log("demuxer", "DemuxerFactory: Selected format by priority: ", result);
    return result;
}

std::unique_ptr<Demuxer> DemuxerFactory::createDemuxerForFormat(const std::string& format_id,
                                                               std::unique_ptr<IOHandler> handler,
                                                               const std::string& file_path) {
    try {
        Debug::log("loader", "DemuxerFactory::createDemuxerForFormat called with format_id=", format_id);
        Debug::log("demuxer", "DemuxerFactory::createDemuxerForFormat called with format_id=", format_id);
        
        if (format_id == "riff" || format_id == "aiff") {
            Debug::log("loader", "Creating ChunkDemuxer for format: ", format_id);
            Debug::log("demuxer", "Creating ChunkDemuxer for format: ", format_id);
            return std::make_unique<ChunkDemuxer>(std::move(handler));
        } else if (format_id == "ogg") {
#ifdef HAVE_OGGDEMUXER
            Debug::log("loader", "Creating OggDemuxer for format: ", format_id);
            Debug::log("demuxer", "Creating OggDemuxer for format: ", format_id);
            Debug::log("ogg", "Creating OggDemuxer for format: ", format_id);
            
            // Check if HAVE_OGGDEMUXER is actually defined at runtime
            #if defined(HAVE_OGGDEMUXER)
                Debug::log("loader", "HAVE_OGGDEMUXER is defined at compile time");
            #else
                Debug::log("loader", "HAVE_OGGDEMUXER is NOT defined at compile time");
            #endif
            
            return std::make_unique<OggDemuxer>(std::move(handler));
#else
            Debug::log("loader", "Ogg format support is disabled (HAVE_OGGDEMUXER not defined)");
            Debug::log("demuxer", "Ogg format support is disabled (HAVE_OGGDEMUXER not defined)");
            throw UnsupportedMediaException("Ogg format support is disabled");
#endif
        } else if (format_id == "mp4") {
            return std::make_unique<ISODemuxer>(std::move(handler));
        } else if (format_id == "flac") {
            // For now, FLAC files are handled by a future FLACDemuxer
            // Fall back to trying Ogg demuxer for FLAC-in-Ogg files
            Debug::log("demuxer", "DemuxerFactory: FLAC demuxer not yet implemented, trying Ogg fallback");
#ifdef HAVE_OGGDEMUXER
            return std::make_unique<OggDemuxer>(std::move(handler));
#else
            throw UnsupportedMediaException("FLAC-in-Ogg format support is disabled");
#endif
        } else if (format_id == "raw_audio") {
            if (!file_path.empty()) {
                return std::make_unique<RawAudioDemuxer>(std::move(handler), file_path);
            } else {
                // Use default raw audio configuration
                return std::make_unique<RawAudioDemuxer>(std::move(handler), RawAudioConfig());
            }
        } else if (format_id == "mp3") {
            // MP3 files don't have a container, they would need a specialized demuxer
            // For now, return nullptr as MP3 demuxing is not implemented
            Debug::log("demuxer", "DemuxerFactory: MP3 demuxer not implemented");
            return nullptr;
        }
        
        Debug::log("demuxer", "DemuxerFactory: Unsupported format: ", format_id);
        return nullptr;
        
    } catch (const std::exception& e) {
        Debug::log("demuxer", "DemuxerFactory: Exception creating demuxer for format '", format_id, "': ", e.what());
        return nullptr;
    }
}

bool DemuxerFactory::validateIOHandler(IOHandler* handler) {
    if (!handler) {
        return false;
    }
    
    // Check if we can get current position
    long pos = handler->tell();
    if (pos < 0) {
        return false;
    }
    
    // Try to seek to current position (should be a no-op)
    if (handler->seek(pos, SEEK_SET) != 0) {
        return false;
    }
    
    return true;
}

size_t DemuxerFactory::readFileHeader(IOHandler* handler, uint8_t* buffer, size_t buffer_size) {
    if (!handler || !buffer || buffer_size == 0) {
        return 0;
    }
    
    size_t bytes_read = handler->read(buffer, 1, buffer_size);
    
    // Log first few bytes for debugging
    if (bytes_read >= 4) {
        Debug::log("demuxer", "DemuxerFactory: File header bytes: ",
                  std::hex, "0x", static_cast<unsigned>(buffer[0]),
                  " 0x", static_cast<unsigned>(buffer[1]),
                  " 0x", static_cast<unsigned>(buffer[2]),
                  " 0x", static_cast<unsigned>(buffer[3]), std::dec);
    }
    
    return bytes_read;
}
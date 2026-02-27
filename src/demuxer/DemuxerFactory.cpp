/*
 * DemuxerFactory.cpp - Factory for creating demuxers with optimized detection
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "tag/ID3v2Tag.h"

namespace PsyMP3 {
namespace Demuxer {

// Static member initialization
std::map<std::string, DemuxerFactory::DemuxerFactoryFunc> DemuxerFactory::s_demuxer_factories;
std::vector<FormatSignature> DemuxerFactory::s_signatures;
std::map<std::string, std::string> DemuxerFactory::s_extension_to_format;
bool DemuxerFactory::s_initialized = false;
std::mutex DemuxerFactory::s_factory_mutex; // Thread safety for factory operations

// Initialize built-in formats
void DemuxerFactory::initializeBuiltInFormats() {
    std::lock_guard<std::mutex> lock(s_factory_mutex);
    if (s_initialized) {
        return;
    }
    
    // Register built-in format signatures
    
    // RIFF/WAV signature
    registerSignature_unlocked(FormatSignature("riff", {0x52, 0x49, 0x46, 0x46}, 0, 100)); // "RIFF"
    
    // AIFF signature
    registerSignature_unlocked(FormatSignature("aiff", {0x46, 0x4F, 0x52, 0x4D}, 0, 100)); // "FORM"
    
    // Ogg signature
    registerSignature_unlocked(FormatSignature("ogg", {0x4F, 0x67, 0x67, 0x53}, 0, 100)); // "OggS"
    
    // FLAC signature
    registerSignature_unlocked(FormatSignature("flac", {0x66, 0x4C, 0x61, 0x43}, 0, 100)); // "fLaC"
    
    // MP4/ISO signature (ftyp box)
    registerSignature_unlocked(FormatSignature("mp4", {0x66, 0x74, 0x79, 0x70}, 4, 90)); // "ftyp" at offset 4
    
    // MP3 signature (ID3v2)
    registerSignature_unlocked(FormatSignature("mp3", {0x49, 0x44, 0x33}, 0, 80)); // "ID3"
    
    // MP3 signature (frame sync)
    registerSignature_unlocked(FormatSignature("mp3", {0xFF, 0xFB}, 0, 70)); // MPEG frame sync
    
    // Register file extensions
    s_extension_to_format["wav"] = "riff";
    s_extension_to_format["wave"] = "riff";
    s_extension_to_format["aif"] = "aiff";
    s_extension_to_format["aiff"] = "aiff";
    s_extension_to_format["ogg"] = "ogg";
    s_extension_to_format["oga"] = "ogg";
    s_extension_to_format["opus"] = "ogg";
    s_extension_to_format["flac"] = "flac";
    s_extension_to_format["mp4"] = "mp4";
    s_extension_to_format["m4a"] = "mp4";
    s_extension_to_format["m4b"] = "mp4";
    s_extension_to_format["mp3"] = "mp3";
    s_extension_to_format["pcm"] = "raw";
    s_extension_to_format["raw"] = "raw";
    s_extension_to_format["alaw"] = "raw";
    s_extension_to_format["ulaw"] = "raw";
    s_extension_to_format["au"] = "raw";
    
    s_initialized = true;
}

std::unique_ptr<Demuxer> DemuxerFactory::createDemuxer(std::unique_ptr<IOHandler> handler) {
    // Initialize built-in formats if needed (thread-safe)
    initializeBuiltInFormats();
    
    // Validate input handler
    if (!handler) {
        Debug::log("demuxer", "DemuxerFactory: Null IOHandler provided");
        return nullptr;
    }
    
    // Test handler functionality
    long initial_pos = handler->tell();
    if (initial_pos < 0) {
        Debug::log("demuxer", "DemuxerFactory: IOHandler tell() failed");
        return nullptr;
    }
    
    // Probe format with error handling
    std::string format_id;
    try {
        format_id = probeFormat(handler.get());
    } catch (const std::exception& e) {
        Debug::log("demuxer", "DemuxerFactory: Exception during format probing: ", e.what());
        return nullptr;
    }
    
    if (format_id.empty()) {
        Debug::log("demuxer", "DemuxerFactory: Unable to identify format");
        return nullptr;
    }
    
    // Create demuxer based on format with error handling (thread-safe lookup)
    DemuxerFactoryFunc factory_func;
    {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        auto it = s_demuxer_factories.find(format_id);
        if (it == s_demuxer_factories.end()) {
            Debug::log("demuxer", "DemuxerFactory: No factory registered for format: ", format_id);
            return nullptr;
        }
        factory_func = it->second;
    }
    
    try {
        auto demuxer = factory_func(std::move(handler));
        if (!demuxer) {
            Debug::log("demuxer", "DemuxerFactory: Factory returned null demuxer for format: ", format_id);
            return nullptr;
        }
        
        Debug::log("demuxer", "DemuxerFactory: Successfully created demuxer for format: ", format_id);
        return demuxer;
        
    } catch (const std::bad_alloc& e) {
        Debug::log("demuxer", "DemuxerFactory: Memory allocation failed creating demuxer for format: ", format_id);
        return nullptr;
    } catch (const std::exception& e) {
        Debug::log("demuxer", "DemuxerFactory: Exception creating demuxer for format: ", format_id, " - ", e.what());
        return nullptr;
    } catch (...) {
        Debug::log("demuxer", "DemuxerFactory: Unknown exception creating demuxer for format: ", format_id);
        return nullptr;
    }
}

std::unique_ptr<Demuxer> DemuxerFactory::createDemuxer(std::unique_ptr<IOHandler> handler, 
                                                     const std::string& file_path) {
    // Initialize built-in formats if needed (thread-safe)
    initializeBuiltInFormats();
    
    // Validate input parameters
    if (!handler) {
        Debug::log("demuxer", "DemuxerFactory: Null IOHandler provided with file path: ", file_path);
        return nullptr;
    }
    
    if (file_path.empty()) {
        Debug::log("demuxer", "DemuxerFactory: Empty file path provided, falling back to content-only detection");
        return createDemuxer(std::move(handler));
    }
    
    // Test handler functionality
    long initial_pos = handler->tell();
    if (initial_pos < 0) {
        Debug::log("demuxer", "DemuxerFactory: IOHandler tell() failed for file: ", file_path);
        return nullptr;
    }
    
    // Probe format with file path hint and error handling
    std::string format_id;
    try {
        format_id = probeFormat(handler.get(), file_path);
    } catch (const std::exception& e) {
        Debug::log("demuxer", "DemuxerFactory: Exception during format probing for file: ", file_path, " - ", e.what());
        return nullptr;
    }
    
    if (format_id.empty()) {
        Debug::log("demuxer", "DemuxerFactory: Unable to identify format for file: ", file_path);
        return nullptr;
    }
    
    // Create demuxer based on format with error handling (thread-safe lookup)
    DemuxerFactoryFunc factory_func;
    {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        auto it = s_demuxer_factories.find(format_id);
        if (it == s_demuxer_factories.end()) {
            Debug::log("demuxer", "DemuxerFactory: No factory registered for format: ", format_id, " (file: ", file_path, ")");
            return nullptr;
        }
        factory_func = it->second;
    }
    
    try {
        auto demuxer = factory_func(std::move(handler));
        if (!demuxer) {
            Debug::log("demuxer", "DemuxerFactory: Factory returned null demuxer for format: ", format_id, " (file: ", file_path, ")");
            return nullptr;
        }
        
        Debug::log("demuxer", "DemuxerFactory: Successfully created demuxer for format: ", format_id, " (file: ", file_path, ")");
        return demuxer;
        
    } catch (const std::bad_alloc& e) {
        Debug::log("demuxer", "DemuxerFactory: Memory allocation failed creating demuxer for format: ", format_id, " (file: ", file_path, ")");
        return nullptr;
    } catch (const std::exception& e) {
        Debug::log("demuxer", "DemuxerFactory: Exception creating demuxer for format: ", format_id, " (file: ", file_path, ") - ", e.what());
        return nullptr;
    } catch (...) {
        Debug::log("demuxer", "DemuxerFactory: Unknown exception creating demuxer for format: ", format_id, " (file: ", file_path, ")");
        return nullptr;
    }
}


std::string DemuxerFactory::probeFormat(IOHandler* handler) {
    if (!handler) {
        Debug::log("demuxer", "DemuxerFactory::probeFormat: Null handler provided");
        return "";
    }
    
    // Initialize built-in formats if needed (thread-safe)
    initializeBuiltInFormats();
    
    // Save current position with error handling
    long original_pos = handler->tell();
    if (original_pos < 0) {
        Debug::log("demuxer", "DemuxerFactory::probeFormat: Failed to get current position");
        return "";
    }
    
    // Seek to beginning with error handling
    if (handler->seek(0, SEEK_SET) != 0) {
        Debug::log("demuxer", "DemuxerFactory::probeFormat: Failed to seek to beginning");
        return "";
    }
    
    // Read header bytes with validation
    std::vector<uint8_t> header(128, 0);
    size_t bytes_read = 0;
    
    try {
        bytes_read = handler->read(header.data(), 1, header.size());
    } catch (const std::exception& e) {
        Debug::log("demuxer", "DemuxerFactory::probeFormat: Exception reading header: ", e.what());
        handler->seek(original_pos, SEEK_SET); // Try to restore position
        return "";
    }
    
    if (bytes_read < 4) {
        // Restore original position
        handler->seek(original_pos, SEEK_SET);
        Debug::log("demuxer", "DemuxerFactory::probeFormat: Insufficient data for format detection: ", bytes_read, " bytes");
        return ""; // Not enough data to identify format
    }
    
    // Check for ID3v2 tag and skip past it to find actual audio format
    // ID3 tags can be prepended to FLAC, MP3, and other formats
    if (bytes_read >= 10 && header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
        size_t id3_size = PsyMP3::Tag::ID3v2Tag::getTagSize(header.data());
        
        if (id3_size > 0) {
            Debug::log("demuxer", "DemuxerFactory::probeFormat: Found ID3v2 tag, size: ", id3_size);
            
            // Read the actual audio data after ID3 tag
            std::vector<uint8_t> post_id3_header(64, 0);
            if (handler->seek(id3_size, SEEK_SET) == 0) {
                size_t post_bytes = handler->read(post_id3_header.data(), 1, post_id3_header.size());
                
                if (post_bytes >= 4) {
                    // Check for FLAC signature after ID3 tag
                    if (post_id3_header[0] == 'f' && post_id3_header[1] == 'L' && 
                        post_id3_header[2] == 'a' && post_id3_header[3] == 'C') {
                        Debug::log("demuxer", "DemuxerFactory::probeFormat: Found FLAC signature after ID3 tag");
                        handler->seek(original_pos, SEEK_SET);
                        return "flac";
                    }
                    
                    // Check for Ogg signature after ID3 tag
                    if (post_id3_header[0] == 'O' && post_id3_header[1] == 'g' && 
                        post_id3_header[2] == 'g' && post_id3_header[3] == 'S') {
                        Debug::log("demuxer", "DemuxerFactory::probeFormat: Found Ogg signature after ID3 tag");
                        handler->seek(original_pos, SEEK_SET);
                        return "ogg";
                    }
                    
                    // Check for MPEG audio sync after ID3 tag (confirms it's actually MP3)
                    if ((post_id3_header[0] == 0xFF) && ((post_id3_header[1] & 0xE0) == 0xE0)) {
                        Debug::log("demuxer", "DemuxerFactory::probeFormat: Found MPEG sync after ID3 tag");
                        handler->seek(original_pos, SEEK_SET);
                        return "mp3";
                    }
                }
            }
            
            // ID3 tag found but couldn't determine format after it - fall through to normal detection
            Debug::log("demuxer", "DemuxerFactory::probeFormat: ID3 tag found but format after tag unclear, trying normal detection");
        }
    }
    
    // Restore original position with error handling
    if (handler->seek(original_pos, SEEK_SET) != 0) {
        Debug::log("demuxer", "DemuxerFactory::probeFormat: Failed to restore original position");
        // Continue anyway - this is not fatal for format detection
    }
    
    // Validate header data
    if (header.empty()) {
        Debug::log("demuxer", "DemuxerFactory::probeFormat: Empty header data");
        return "";
    }
    
    // Match signatures with error handling (thread-safe access to signatures)
    std::vector<std::pair<std::string, int>> matches;
    
    try {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        for (const auto& signature : s_signatures) {
            if (matchSignature(header.data(), bytes_read, signature)) {
                matches.emplace_back(signature.format_id, signature.priority);
                Debug::log("demuxer", "DemuxerFactory::probeFormat: Matched signature for format: ", signature.format_id, " (priority: ", signature.priority, ")");
            }
        }
    } catch (const std::exception& e) {
        Debug::log("demuxer", "DemuxerFactory::probeFormat: Exception during signature matching: ", e.what());
        return "";
    }
    
    if (matches.empty()) {
        Debug::log("demuxer", "DemuxerFactory::probeFormat: No format signatures matched");
        return "";
    }
    
    // Sort matches by priority (highest first)
    try {
        std::sort(matches.begin(), matches.end(), 
                 [](const auto& a, const auto& b) { return a.second > b.second; });
    } catch (const std::exception& e) {
        Debug::log("demuxer", "DemuxerFactory::probeFormat: Exception sorting matches: ", e.what());
        return "";
    }
    
    // Return highest priority match
    std::string best_format = matches.front().first;
    Debug::log("demuxer", "DemuxerFactory::probeFormat: Selected format: ", best_format, " from ", matches.size(), " matches");
    return best_format;
}

std::string DemuxerFactory::probeFormat(IOHandler* handler, const std::string& file_path) {
    // Initialize built-in formats if needed (thread-safe)
    initializeBuiltInFormats();
    
    // Try content-based detection first
    std::string format_id = probeFormat(handler);
    if (!format_id.empty()) {
        return format_id;
    }
    
    // Fall back to extension-based detection
    return detectFormatFromExtension(file_path);
}

void DemuxerFactory::registerDemuxer(const std::string& format_id, DemuxerFactoryFunc factory_func) {
    std::lock_guard<std::mutex> lock(s_factory_mutex);
    s_demuxer_factories[format_id] = factory_func;
}

void DemuxerFactory::registerSignature(const FormatSignature& signature) {
    std::lock_guard<std::mutex> lock(s_factory_mutex);
    registerSignature_unlocked(signature);
}

void DemuxerFactory::registerSignature_unlocked(const FormatSignature& signature) {
    s_signatures.push_back(signature);
    
    // Sort signatures by priority (highest first)
    std::sort(s_signatures.begin(), s_signatures.end(), 
             [](const auto& a, const auto& b) { return a.priority > b.priority; });
}

const std::vector<FormatSignature>& DemuxerFactory::getSignatures() {
    // Initialize built-in formats if needed (thread-safe)
    initializeBuiltInFormats();
    
    std::lock_guard<std::mutex> lock(s_factory_mutex);
    return s_signatures;
}

bool DemuxerFactory::matchSignature(const uint8_t* data, size_t data_size, 
                                  const FormatSignature& signature) {
    // Validate input parameters
    if (!data || data_size == 0) {
        Debug::log("demuxer", "DemuxerFactory::matchSignature: Invalid data parameters");
        return false;
    }
    
    if (signature.signature.empty()) {
        Debug::log("demuxer", "DemuxerFactory::matchSignature: Empty signature for format: ", signature.format_id);
        return false;
    }
    
    // Check if we have enough data
    if (signature.offset + signature.signature.size() > data_size) {
        Debug::log("demuxer", "DemuxerFactory::matchSignature: Insufficient data for signature check: format=", 
                   signature.format_id, ", need=", signature.offset + signature.signature.size(), ", have=", data_size);
        return false;
    }
    
    // Validate offset
    if (signature.offset >= data_size) {
        Debug::log("demuxer", "DemuxerFactory::matchSignature: Signature offset exceeds data size: format=", 
                   signature.format_id, ", offset=", signature.offset, ", data_size=", data_size);
        return false;
    }
    
    // Compare signature bytes with error handling
    try {
        bool matches = std::memcmp(data + signature.offset, signature.signature.data(), 
                                  signature.signature.size()) == 0;
        
        if (matches) {
            Debug::log("demuxer", "DemuxerFactory::matchSignature: Signature matched for format: ", signature.format_id);
        }
        
        return matches;
    } catch (const std::exception& e) {
        Debug::log("demuxer", "DemuxerFactory::matchSignature: Exception comparing signature for format: ", 
                   signature.format_id, " - ", e.what());
        return false;
    }
}

std::string DemuxerFactory::detectFormatFromExtension(const std::string& file_path) {
    // Extract extension from file path
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos || dot_pos == file_path.length() - 1) {
        return ""; // No extension
    }
    
    std::string extension = file_path.substr(dot_pos + 1);
    
    // Convert to lowercase
    std::transform(extension.begin(), extension.end(), extension.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    
    // Look up extension (thread-safe)
    std::lock_guard<std::mutex> lock(s_factory_mutex);
    auto it = s_extension_to_format.find(extension);
    if (it != s_extension_to_format.end()) {
        return it->second;
    }
    
    return ""; // Unknown extension
}

} // namespace Demuxer
} // namespace PsyMP3

/*
 * DemuxerFactory.cpp - Factory for creating demuxers with optimized detection
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// Static member initialization
std::map<std::string, DemuxerFactory::DemuxerFactoryFunc> DemuxerFactory::s_demuxer_factories;
std::vector<FormatSignature> DemuxerFactory::s_signatures;
std::map<std::string, std::string> DemuxerFactory::s_extension_to_format;
bool DemuxerFactory::s_initialized = false;

// Initialize built-in formats
void DemuxerFactory::initializeBuiltInFormats() {
    if (s_initialized) {
        return;
    }
    
    // Register built-in format signatures
    
    // RIFF/WAV signature
    registerSignature(FormatSignature("riff", {0x52, 0x49, 0x46, 0x46}, 0, 100)); // "RIFF"
    
    // AIFF signature
    registerSignature(FormatSignature("aiff", {0x46, 0x4F, 0x52, 0x4D}, 0, 100)); // "FORM"
    
    // Ogg signature
    registerSignature(FormatSignature("ogg", {0x4F, 0x67, 0x67, 0x53}, 0, 100)); // "OggS"
    
    // FLAC signature
    registerSignature(FormatSignature("flac", {0x66, 0x4C, 0x61, 0x43}, 0, 100)); // "fLaC"
    
    // MP4/ISO signature (ftyp box)
    registerSignature(FormatSignature("mp4", {0x66, 0x74, 0x79, 0x70}, 4, 90)); // "ftyp" at offset 4
    
    // MP3 signature (ID3v2)
    registerSignature(FormatSignature("mp3", {0x49, 0x44, 0x33}, 0, 80)); // "ID3"
    
    // MP3 signature (frame sync)
    registerSignature(FormatSignature("mp3", {0xFF, 0xFB}, 0, 70)); // MPEG frame sync
    
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
    // Initialize built-in formats if needed
    initializeBuiltInFormats();
    
    // Probe format
    std::string format_id = probeFormat(handler.get());
    
    // Create demuxer based on format
    if (!format_id.empty()) {
        auto it = s_demuxer_factories.find(format_id);
        if (it != s_demuxer_factories.end()) {
            return it->second(std::move(handler));
        }
    }
    
    // Format not supported
    return nullptr;
}

std::unique_ptr<Demuxer> DemuxerFactory::createDemuxer(std::unique_ptr<IOHandler> handler, 
                                                     const std::string& file_path) {
    // Initialize built-in formats if needed
    initializeBuiltInFormats();
    
    // Probe format with file path hint
    std::string format_id = probeFormat(handler.get(), file_path);
    
    // Create demuxer based on format
    if (!format_id.empty()) {
        auto it = s_demuxer_factories.find(format_id);
        if (it != s_demuxer_factories.end()) {
            return it->second(std::move(handler));
        }
    }
    
    // Format not supported
    return nullptr;
}

std::string DemuxerFactory::probeFormat(IOHandler* handler) {
    if (!handler) {
        return "";
    }
    
    // Initialize built-in formats if needed
    initializeBuiltInFormats();
    
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
    
    for (const auto& signature : s_signatures) {
        if (matchSignature(header.data(), bytes_read, signature)) {
            matches.emplace_back(signature.format_id, signature.priority);
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

std::string DemuxerFactory::probeFormat(IOHandler* handler, const std::string& file_path) {
    // Initialize built-in formats if needed
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
    s_demuxer_factories[format_id] = factory_func;
}

void DemuxerFactory::registerSignature(const FormatSignature& signature) {
    s_signatures.push_back(signature);
    
    // Sort signatures by priority (highest first)
    std::sort(s_signatures.begin(), s_signatures.end(), 
             [](const auto& a, const auto& b) { return a.priority > b.priority; });
}

const std::vector<FormatSignature>& DemuxerFactory::getSignatures() {
    // Initialize built-in formats if needed
    initializeBuiltInFormats();
    
    return s_signatures;
}

bool DemuxerFactory::matchSignature(const uint8_t* data, size_t data_size, 
                                  const FormatSignature& signature) {
    // Check if we have enough data
    if (signature.offset + signature.signature.size() > data_size) {
        return false;
    }
    
    // Compare signature bytes
    return std::memcmp(data + signature.offset, signature.signature.data(), 
                      signature.signature.size()) == 0;
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
    
    // Look up extension
    auto it = s_extension_to_format.find(extension);
    if (it != s_extension_to_format.end()) {
        return it->second;
    }
    
    return ""; // Unknown extension
}
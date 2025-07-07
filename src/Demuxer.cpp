/*
 * Demuxer.cpp - Generic container demuxer base class implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "Demuxer.h"
#include "RIFFDemuxer.h"
#include "RawAudioDemuxer.h"
#include "exceptions.h"
#include <algorithm>

Demuxer::Demuxer(std::unique_ptr<IOHandler> handler) 
    : m_handler(std::move(handler)) {
}

std::unique_ptr<Demuxer> DemuxerFactory::createDemuxer(std::unique_ptr<IOHandler> handler) {
    return createDemuxer(std::move(handler), ""); // No file path hint
}

std::unique_ptr<Demuxer> DemuxerFactory::createDemuxer(std::unique_ptr<IOHandler> handler, 
                                                       const std::string& file_path) {
    if (!handler) {
        return nullptr;
    }
    
    // Check for raw audio formats first (extension-based)
    if (!file_path.empty()) {
        auto raw_config = RawAudioDetector::detectRawAudio(file_path);
        if (raw_config.has_value()) {
            return std::make_unique<RawAudioDemuxer>(std::move(handler), raw_config.value());
        }
    }
    
    // Probe the format for container-based formats
    std::string format = probeFormat(handler.get());
    
    if (format == "riff") {
        return std::make_unique<RIFFDemuxer>(std::move(handler));
    }
    // Future demuxers will be added here:
    // else if (format == "ogg") {
    //     return std::make_unique<OggDemuxer>(std::move(handler));
    // }
    // else if (format == "mp4") {
    //     return std::make_unique<ISODemuxer>(std::move(handler));
    // }
    
    return nullptr;
}

std::string DemuxerFactory::probeFormat(IOHandler* handler) {
    if (!handler) {
        return "";
    }
    
    // Save current position
    long original_pos = handler->tell();
    handler->seek(0, SEEK_SET);
    
    // Read first 12 bytes for format identification
    uint8_t header[12];
    size_t bytes_read = handler->read(header, 1, sizeof(header));
    
    // Restore position
    handler->seek(original_pos, SEEK_SET);
    
    if (bytes_read < 8) {
        return "";
    }
    
    // Check for RIFF format
    uint32_t riff_magic = *reinterpret_cast<uint32_t*>(header);
    if (riff_magic == 0x46464952) { // "RIFF"
        return "riff";
    }
    
    // Check for Ogg format
    if (bytes_read >= 4 && 
        header[0] == 'O' && header[1] == 'g' && 
        header[2] == 'g' && header[3] == 'S') {
        return "ogg";
    }
    
    // Check for MP4/M4A format (ftyp box)
    if (bytes_read >= 8) {
        uint32_t box_size = __builtin_bswap32(*reinterpret_cast<uint32_t*>(header));
        uint32_t box_type = *reinterpret_cast<uint32_t*>(header + 4);
        if (box_type == 0x70797466) { // "ftyp"
            return "mp4";
        }
    }
    
    // Check for IFF/AIFF format
    uint32_t form_magic = *reinterpret_cast<uint32_t*>(header);
    if (form_magic == 0x4D524F46) { // "FORM"
        if (bytes_read >= 12) {
            uint32_t aiff_magic = *reinterpret_cast<uint32_t*>(header + 8);
            if (aiff_magic == 0x46464941) { // "AIFF"
                return "aiff";
            }
        }
        return "iff";
    }
    
    return "";
}
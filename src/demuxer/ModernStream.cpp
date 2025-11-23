/*
 * ModernStream.cpp - Stream wrapper for modern demuxer/codec architecture
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Demuxer {

ModernStream::ModernStream(const TagLib::String& name) 
    : Stream(name), m_file_name(name) {
}

ModernStream::~ModernStream() {
}

void ModernStream::open(TagLib::String name) {
    m_file_name = name;
    m_opened = initializeAudioChain(name);
    if (!m_opened) {
        throw InvalidMediaException("Failed to initialize audio chain for: " + name);
    }
}

bool ModernStream::initializeAudioChain(const TagLib::String& name) {
    try {
        // Use DemuxedStream which handles the entire chain internally
        m_demuxed_stream = std::make_unique<DemuxedStream>(name);
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

size_t ModernStream::getData(size_t len, void *buf) {
    if (!m_opened || !m_demuxed_stream) {
        return 0;
    }
    
    return m_demuxed_stream->getData(len, buf);
}

unsigned int ModernStream::getPosition() {
    if (!m_opened || !m_demuxed_stream) {
        return 0;
    }
    
    return static_cast<unsigned int>(m_demuxed_stream->getSPosition() / 1000);
}

unsigned long long ModernStream::getSPosition() {
    if (!m_opened || !m_demuxed_stream) {
        return 0;
    }
    
    return m_demuxed_stream->getSPosition();
}

void ModernStream::seekTo(unsigned long pos) {
    if (!m_opened || !m_demuxed_stream) {
        return;
    }
    
    m_demuxed_stream->seekTo(pos);
}

bool ModernStream::eof() {
    if (!m_opened || !m_demuxed_stream) {
        return true;
    }
    
    return m_demuxed_stream->eof();
}

} // namespace Demuxer
} // namespace PsyMP3

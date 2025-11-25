/*
 * OggCodecs.cpp - Audio codec implementations for Ogg-contained formats
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// OggCodecs are built if any Ogg-based codec is enabled
#ifdef HAVE_OGGDEMUXER

#ifdef HAVE_VORBIS
// VorbisPassthroughCodec implementation - now redirects to VorbisCodec
VorbisPassthroughCodec::VorbisPassthroughCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
    // Create the actual VorbisCodec
    m_vorbis_codec = std::make_unique<PsyMP3::Codec::Vorbis::VorbisCodec>(stream_info);
}

VorbisPassthroughCodec::~VorbisPassthroughCodec() {
    // Unique pointer handles cleanup
}

bool VorbisPassthroughCodec::initialize() {
    m_initialized = m_vorbis_codec->initialize();
    return m_initialized;
}

AudioFrame VorbisPassthroughCodec::decode(const MediaChunk& chunk) {
    return m_vorbis_codec->decode(chunk);
}

AudioFrame VorbisPassthroughCodec::flush() {
    return m_vorbis_codec->flush();
}

void VorbisPassthroughCodec::reset() {
    m_vorbis_codec->reset();
}

bool VorbisPassthroughCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "vorbis";
}
#endif // HAVE_VORBIS

// OggFLACPassthroughCodec implementation
OggFLACPassthroughCodec::OggFLACPassthroughCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
}

OggFLACPassthroughCodec::~OggFLACPassthroughCodec() {
    // m_flac_stream is a forward-declared incomplete type and is never allocated
    // No cleanup needed
}

bool OggFLACPassthroughCodec::initialize() {
    // We'll create the FLACStream when we get the first chunk
    m_initialized = true;
    return true;
}

AudioFrame OggFLACPassthroughCodec::decode(const MediaChunk& chunk) {
    AudioFrame frame;
    
    if (chunk.data.empty()) {
        return frame;
    }
    
    // Accumulate data in buffer
    m_buffer.insert(m_buffer.end(), chunk.data.begin(), chunk.data.end());
    
    // Create FLACStream if we haven't yet
    if (!m_flac_stream && m_buffer.size() >= 4) {
        // Try to create from buffered data
        // Note: Similar challenges as with Vorbis - the existing FLACStream
        // expects a file-based interface, not raw FLAC packets from Ogg
        
        // For now, return empty frame
        return frame;
    }
    
    // TODO: Implement proper Ogg FLAC passthrough
    // This is more complex because Ogg FLAC has a different structure
    // than native FLAC files - it doesn't have the native FLAC container
    
    return frame;
}

AudioFrame OggFLACPassthroughCodec::flush() {
    return AudioFrame{};
}

void OggFLACPassthroughCodec::reset() {
    m_buffer.clear();
    m_headers_written = false;
    
    // m_flac_stream is a forward-declared incomplete type and is never allocated
    // No cleanup needed
}

bool OggFLACPassthroughCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "flac";
}

// OpusPassthroughCodec implementation - now redirects to OpusCodec
OpusPassthroughCodec::OpusPassthroughCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
    // Create the actual OpusCodec
    m_opus_codec = std::make_unique<OpusCodec>(stream_info);
}

OpusPassthroughCodec::~OpusPassthroughCodec() {
    // Unique pointer handles cleanup
}

bool OpusPassthroughCodec::initialize() {
    m_initialized = m_opus_codec->initialize();
    return m_initialized;
}

AudioFrame OpusPassthroughCodec::decode(const MediaChunk& chunk) {
    return m_opus_codec->decode(chunk);
}

AudioFrame OpusPassthroughCodec::flush() {
    return m_opus_codec->flush();
}

void OpusPassthroughCodec::reset() {
    m_opus_codec->reset();
}

bool OpusPassthroughCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "opus";
}

// SpeexCodec implementation (placeholder)
SpeexCodec::SpeexCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
}

SpeexCodec::~SpeexCodec() {
    // Cleanup Speex decoder if implemented
}

bool SpeexCodec::initialize() {
    // TODO: Initialize Speex decoder
    // For now, just mark as initialized
    m_initialized_speex = true;
    m_initialized = true;
    return true;
}

AudioFrame SpeexCodec::decode(const MediaChunk& chunk) {
    AudioFrame frame;
    
    if (chunk.data.empty() || !m_initialized_speex) {
        return frame;
    }
    
    // TODO: Implement actual Speex decoding
    // For now, return silence as a placeholder
    
    frame.timestamp_samples = chunk.timestamp_samples;
    frame.sample_rate = m_stream_info.sample_rate;
    frame.channels = m_stream_info.channels;
    
    // Generate silence frames (placeholder)
    size_t samples_per_frame = m_frame_size * frame.channels;
    frame.samples.resize(samples_per_frame, 0); // Silence
    
    return frame;
}

AudioFrame SpeexCodec::flush() {
    // Nothing to flush for placeholder implementation
    return AudioFrame{};
}

void SpeexCodec::reset() {
    // Reset Speex decoder state if implemented
    m_initialized_speex = false;
    if (m_initialized) {
        m_initialized_speex = true; // Re-initialize if we were initialized before
    }
}

bool SpeexCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "speex";
}

#endif // HAVE_OGGDEMUXER

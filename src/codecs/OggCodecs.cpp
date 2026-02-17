/*
 * OggCodecs.cpp - Audio codec implementations for Ogg-contained formats
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

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

// Helper to select the correct FLAC codec implementation
#ifdef HAVE_FLAC
#ifdef HAVE_NATIVE_FLAC
    #include "codecs/flac/NativeFLACCodec.h"
    using FLACCodecImpl = PsyMP3::Codec::FLAC::FLACCodec;
#else
    #include "codecs/FLACCodec.h"
    using FLACCodecImpl = FLACCodec;
#endif
#endif

// OggFLACPassthroughCodec implementation
OggFLACPassthroughCodec::OggFLACPassthroughCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
}

OggFLACPassthroughCodec::~OggFLACPassthroughCodec() {
    // Unique pointer handles cleanup
}

bool OggFLACPassthroughCodec::initialize() {
#ifdef HAVE_FLAC
    try {
        m_flac_codec = std::make_unique<FLACCodecImpl>(m_stream_info);
        m_initialized = m_flac_codec->initialize();
    } catch (...) {
        m_initialized = false;
    }
    return m_initialized;
#else
    return false;
#endif
}

AudioFrame OggFLACPassthroughCodec::decode(const MediaChunk& chunk) {
    if (!m_flac_codec) {
        return AudioFrame{};
    }

    // Check for Ogg FLAC header packet (starts with "fLaC")
    // libFLAC wrapper (FLACCodec) synthesizes the STREAMINFO header based on StreamInfo,
    // so we should skip the actual Ogg packet containing STREAMINFO to avoid confusing it.
    if (chunk.data.size() >= 4 &&
        chunk.data[0] == 'f' && chunk.data[1] == 'L' &&
        chunk.data[2] == 'a' && chunk.data[3] == 'C') {
        return AudioFrame{};
    }

    return m_flac_codec->decode(chunk);
}

AudioFrame OggFLACPassthroughCodec::flush() {
    if (m_flac_codec) {
        return m_flac_codec->flush();
    }
    return AudioFrame{};
}

void OggFLACPassthroughCodec::reset() {
    if (m_flac_codec) {
        m_flac_codec->reset();
    }
}

bool OggFLACPassthroughCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "flac";
}

// OpusPassthroughCodec implementation - now redirects to OpusCodec
OpusPassthroughCodec::OpusPassthroughCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
    // Create the actual OpusCodec
    m_opus_codec = std::make_unique<PsyMP3::Codec::Opus::OpusCodec>(stream_info);
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

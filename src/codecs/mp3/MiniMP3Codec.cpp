/*
 * MiniMP3Codec.cpp - AudioCodec-based MP3 decoder using minimp3
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#define MINIMP3_IMPLEMENTATION
#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Codec {
namespace MP3 {

MiniMP3Codec::MiniMP3Codec(const StreamInfo& stream_info)
    : AudioCodec(stream_info),
      m_sample_rate(stream_info.sample_rate),
      m_channels(stream_info.channels) {
    std::memset(&m_decoder, 0, sizeof(m_decoder));
}

MiniMP3Codec::~MiniMP3Codec() {
    // mp3dec_t has no dynamic resources to clean up
}

bool MiniMP3Codec::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return initialize_unlocked();
}

AudioFrame MiniMP3Codec::decode(const MediaChunk& chunk) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return decode_unlocked(chunk);
}

AudioFrame MiniMP3Codec::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return flush_unlocked();
}

void MiniMP3Codec::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    reset_unlocked();
}

bool MiniMP3Codec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_type == "audio" && stream_info.codec_name == "mp3";
}

bool MiniMP3Codec::initialize_unlocked() {
    mp3dec_init(&m_decoder);
    m_initialized = true;
    Debug::log("minimp3", "MiniMP3Codec: Initialized decoder");
    return true;
}

AudioFrame MiniMP3Codec::decode_unlocked(const MediaChunk& chunk) {
    if (!m_initialized) {
        return AudioFrame();
    }

    if (chunk.data.empty()) {
        return AudioFrame();
    }

    mp3dec_frame_info_t frame_info = {};
    mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

    int samples = mp3dec_decode_frame(
        &m_decoder,
        chunk.data.data(),
        static_cast<int>(chunk.data.size()),
        pcm,
        &frame_info);

    if (samples == 0 || frame_info.channels == 0 || frame_info.hz == 0) {
        return AudioFrame();
    }

    // Update stream info from decoded frame
    m_sample_rate = static_cast<uint32_t>(frame_info.hz);
    m_channels = static_cast<uint16_t>(frame_info.channels);

    // Build AudioFrame with decoded PCM samples
    size_t total_samples = static_cast<size_t>(samples) * frame_info.channels;
    AudioFrame frame;
    frame.samples.assign(pcm, pcm + total_samples);
    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.timestamp_samples = chunk.timestamp_samples;
    if (m_sample_rate != 0) {
        frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / m_sample_rate;
    }

    return frame;
}

AudioFrame MiniMP3Codec::flush_unlocked() {
    return AudioFrame();
}

void MiniMP3Codec::reset_unlocked() {
    mp3dec_init(&m_decoder);
}

// --- Support namespace ---

namespace MiniMP3CodecSupport {

bool isMP3Stream(const StreamInfo& stream_info) {
    return stream_info.codec_type == "audio" && stream_info.codec_name == "mp3";
}

std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info) {
    if (!isMP3Stream(stream_info)) {
        return nullptr;
    }
    return std::make_unique<MiniMP3Codec>(stream_info);
}

void registerCodec() {
    AudioCodecFactory::registerCodec("mp3", createCodec);
    Debug::log("minimp3", "MiniMP3CodecSupport: Registered mp3 codec with AudioCodecFactory");
}

} // namespace MiniMP3CodecSupport

} // namespace MP3
} // namespace Codec
} // namespace PsyMP3

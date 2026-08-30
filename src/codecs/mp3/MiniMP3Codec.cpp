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

// Cap for the compressed input accumulator; a valid stream never gets close
// (the largest MP3 frame is ~2 KB), so exceeding this means garbage input.
static constexpr size_t kMaxPendingInput = 512 * 1024;

// Minimum bytes that must remain past a decode position: the largest MPEG-1
// frame (~1441 bytes) plus the following header minimp3 peeks to confirm it,
// with slack. Below this, decoding is deferred until more data arrives.
static constexpr size_t kDecodeLookahead = 4096;

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
    if (!m_initialized || chunk.data.empty()) {
        return AudioFrame();
    }
    m_input.insert(m_input.end(), chunk.data.begin(), chunk.data.end());
    return decodeBuffered_unlocked(chunk.timestamp_samples, false);
}

AudioFrame MiniMP3Codec::decodeBuffered_unlocked(uint64_t timestamp_samples, bool flushing) {
    AudioFrame frame;
    mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    size_t pos = 0;

    while (pos < m_input.size()) {
        // Lookahead margin: minimp3 confirms a frame by peeking the NEXT
        // header; near the end of the buffer that peek fails and it CONSUMES
        // real frames reporting zero samples (measured: ~2 frames lost per
        // chunk boundary). Never decode inside the margin unless flushing,
        // where end-of-buffer really is end-of-stream.
        if (!flushing && m_input.size() - pos < kDecodeLookahead) {
            break;
        }
        mp3dec_frame_info_t frame_info = {};
        int samples = mp3dec_decode_frame(
            &m_decoder,
            m_input.data() + pos,
            static_cast<int>(m_input.size() - pos),
            pcm,
            &frame_info);

        if (frame_info.frame_bytes <= 0) {
            break; // no complete frame in the remaining bytes: keep the tail
        }
        pos += static_cast<size_t>(frame_info.frame_bytes);

        if (samples > 0 && frame_info.channels > 0 && frame_info.hz > 0) {
            m_sample_rate = static_cast<uint32_t>(frame_info.hz);
            m_channels = static_cast<uint16_t>(frame_info.channels);
            const size_t total = static_cast<size_t>(samples) * frame_info.channels;
            frame.samples.insert(frame.samples.end(), pcm, pcm + total);
        }
        // samples == 0 with frame_bytes > 0 is skipped garbage (leading junk,
        // tag remnants) or a reservoir warm-up frame - keep consuming.
    }

    if (pos > 0) {
        m_input.erase(m_input.begin(), m_input.begin() + static_cast<std::ptrdiff_t>(pos));
    } else if (m_input.size() > kMaxPendingInput) {
        // A pathological stream that never yields a frame must not grow the
        // buffer without bound.
        m_input.clear();
    }

    if (frame.samples.empty()) {
        return AudioFrame();
    }
    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.timestamp_samples = timestamp_samples;
    if (m_sample_rate != 0) {
        frame.timestamp_ms = (timestamp_samples * 1000ULL) / m_sample_rate;
    }
    return frame;
}

AudioFrame MiniMP3Codec::flush_unlocked() {
    if (!m_initialized || m_input.empty()) {
        return AudioFrame();
    }
    // Drain whatever complete frames remain buffered at end of stream.
    return decodeBuffered_unlocked(0, true);
}

void MiniMP3Codec::reset_unlocked() {
    mp3dec_init(&m_decoder);
    m_input.clear(); // seek: buffered bitstream is from the old position
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

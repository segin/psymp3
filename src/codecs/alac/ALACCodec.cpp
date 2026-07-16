/*
 * ALACCodec.cpp - AudioCodec-based Apple Lossless (ALAC) decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The bundled ALAC decoder (third_party/alac) is Copyright © 2011 Apple Inc.
 * and is used under the Apache License, Version 2.0.
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

// Apple's ALAC decoder is compiled straight into this translation unit (which is
// also how it lands in the --enable-final unity build). The lib headers carry
// extern "C" guards, so linkage is consistent within this single TU; the pragmas
// silence the handful of warnings-as-errors in the otherwise-pristine sources.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmultichar"
#pragma GCC diagnostic ignored "-Wregister"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "../../../third_party/alac/ALACBitUtilities.c"
#include "../../../third_party/alac/EndianPortable.c"
#include "../../../third_party/alac/ag_dec.c"
#include "../../../third_party/alac/dp_dec.c"
#include "../../../third_party/alac/matrix_dec.c"
#include "../../../third_party/alac/ALACDecoder.cpp"
#pragma GCC diagnostic pop

namespace PsyMP3 {
namespace Codec {
namespace ALAC {

ALACCodec::ALACCodec(const StreamInfo& stream_info)
    : AudioCodec(stream_info),
      m_magic_cookie(stream_info.codec_data),
      m_sample_rate(stream_info.sample_rate),
      m_channels(stream_info.channels) {}

ALACCodec::~ALACCodec() = default; // ALACDecoder complete here

bool ALACCodec::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return initialize_unlocked();
}

AudioFrame ALACCodec::decode(const MediaChunk& chunk) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return decode_unlocked(chunk);
}

AudioFrame ALACCodec::flush() {
    return AudioFrame(); // per-packet decode; nothing buffered
}

void ALACCodec::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Re-init from the cookie to drop any per-packet state.
    m_decoder.reset();
    m_initialized = false;
    initialize_unlocked();
}

bool ALACCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_type == "audio" && stream_info.codec_name == "alac";
}

bool ALACCodec::initialize_unlocked() {
    if (m_magic_cookie.empty()) {
        Debug::log("alac", "ALACCodec: missing ALAC magic cookie (codec_data)");
        return false;
    }

    const uint8_t* cookie = m_magic_cookie.data();
    uint32_t cookie_size = static_cast<uint32_t>(m_magic_cookie.size());
    // The MP4 'alac' box wraps the ALACSpecificConfig in a 4-byte FullBox
    // version/flags header; strip it (all-zero header + at least the 24-byte
    // config). A bare 24-byte config passes through unchanged.
    if (cookie_size >= 28 &&
        cookie[0] == 0 && cookie[1] == 0 && cookie[2] == 0 && cookie[3] == 0) {
        cookie += 4;
        cookie_size -= 4;
    }

    m_decoder = std::make_unique<ALACDecoder>();
    if (m_decoder->Init(const_cast<uint8_t*>(cookie), cookie_size) != 0) {
        Debug::log("alac", "ALACCodec: ALACDecoder::Init failed");
        m_decoder.reset();
        return false;
    }

    // Adopt the parameters the decoder read from the cookie.
    m_channels     = static_cast<uint16_t>(m_decoder->mConfig.numChannels);
    m_bit_depth    = static_cast<uint16_t>(m_decoder->mConfig.bitDepth);
    m_frame_length = m_decoder->mConfig.frameLength;
    if (m_decoder->mConfig.sampleRate != 0) {
        m_sample_rate = m_decoder->mConfig.sampleRate;
    }
    if (m_channels == 0) m_channels = 2;
    if (m_frame_length == 0) m_frame_length = 4096;

    m_initialized = true;
    Debug::log("alac", "ALACCodec: Initialized ch=", m_channels, " bits=", m_bit_depth,
               " sr=", m_sample_rate, " frameLen=", m_frame_length);
    return true;
}

AudioFrame ALACCodec::decode_unlocked(const MediaChunk& chunk) {
    if (!m_initialized || !m_decoder || chunk.data.empty()) {
        return AudioFrame();
    }

    BitBuffer bits;
    BitBufferInit(&bits, const_cast<uint8_t*>(chunk.data.data()),
                  static_cast<uint32_t>(chunk.data.size()));

    // Output buffer sized for the widest sample format (32-bit) per interleaved
    // sample; the decoder writes at the container's bit depth.
    std::vector<uint8_t> out(static_cast<size_t>(m_frame_length) * m_channels * sizeof(int32_t));
    uint32_t decoded = 0;
    int32_t status = m_decoder->Decode(&bits, out.data(), m_frame_length, m_channels, &decoded);
    if (status != 0 || decoded == 0) {
        return AudioFrame();
    }

    const size_t total = static_cast<size_t>(decoded) * m_channels;
    AudioFrame frame;
    frame.samples.resize(total);

    // The audio pipeline is int16; downconvert wider depths to 16-bit.
    if (m_bit_depth <= 16) {
        const int16_t* s = reinterpret_cast<const int16_t*>(out.data());
        for (size_t i = 0; i < total; ++i) frame.samples[i] = s[i];
    } else if (m_bit_depth <= 24) {
        // 20/24-bit are written as little-endian 3-byte samples.
        const uint8_t* s = out.data();
        for (size_t i = 0; i < total; ++i) {
            int32_t v = static_cast<int32_t>(s[i * 3]) |
                        (static_cast<int32_t>(s[i * 3 + 1]) << 8) |
                        (static_cast<int32_t>(s[i * 3 + 2]) << 16);
            if (v & 0x00800000) v |= ~0x00FFFFFF; // sign-extend 24 -> 32
            frame.samples[i] = static_cast<int16_t>(v >> 8);
        }
    } else {
        const int32_t* s = reinterpret_cast<const int32_t*>(out.data());
        for (size_t i = 0; i < total; ++i) frame.samples[i] = static_cast<int16_t>(s[i] >> 16);
    }

    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.timestamp_samples = chunk.timestamp_samples;
    if (m_sample_rate != 0) {
        frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / m_sample_rate;
    }
    return frame;
}

// --- Support namespace ---

namespace ALACCodecSupport {

bool isALACStream(const StreamInfo& stream_info) {
    return stream_info.codec_type == "audio" && stream_info.codec_name == "alac";
}

std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info) {
    if (!isALACStream(stream_info)) {
        return nullptr;
    }
    return std::make_unique<ALACCodec>(stream_info);
}

void registerCodec() {
    AudioCodecFactory::registerCodec("alac", createCodec);
    Debug::log("alac", "ALACCodecSupport: Registered alac codec with AudioCodecFactory");
}

} // namespace ALACCodecSupport

} // namespace ALAC
} // namespace Codec
} // namespace PsyMP3

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
// ELEMENT_TYPE collides with a Windows SDK typedef under mingw. ALAC only
// *defines* this enum (never references the type name elsewhere), so rename it
// for the duration of these includes and restore it afterwards.
#pragma push_macro("ELEMENT_TYPE")
#define ELEMENT_TYPE ALAC_ELEMENT_TYPE
#include "../../../third_party/alac/ALACBitUtilities.c"
#include "../../../third_party/alac/EndianPortable.c"
#include "../../../third_party/alac/ag_dec.c"
#include "../../../third_party/alac/dp_dec.c"
#include "../../../third_party/alac/matrix_dec.c"
#include "../../../third_party/alac/ALACDecoder.cpp"
#pragma pop_macro("ELEMENT_TYPE")
#pragma GCC diagnostic pop

namespace PsyMP3 {
namespace Codec {
namespace ALAC {

namespace {

// ALACDecoder sizes mMixBufferU/V and mPredictor from frameLength, and this
// wrapper sizes the output buffer from it; anything beyond this is a crafted
// cookie rather than a real encoder, which uses kALACDefaultFramesPerPacket.
constexpr uint32_t kMaxFrameLength = 65536;

// Apple's adaptive-Golomb reader (ag_dec.c) works on a raw pointer, loading 32
// bits at a time and only re-testing its bit position between samples, so it
// reads up to eight bytes beyond the last valid one. The packet copy handed to
// BitBufferInit() therefore carries this much zero padding past the length it
// declares, which is where the readers' end-of-buffer checks still point.
constexpr size_t kBitstreamPadding = 16;

// Decode() switches on bitDepth to emit samples and computes chanBits as
// bitDepth - bytesShifted * 8 in unsigned arithmetic, so a depth its output
// stage does not handle either drops the frame or wraps chanBits negative.
bool isSupportedBitDepth(uint32_t bit_depth) {
    return bit_depth == 16 || bit_depth == 20 || bit_depth == 24 || bit_depth == 32;
}

// ALACDecoder::Init() walks past the optional 'frma' and 'alac' atoms that may
// precede the ALACSpecificConfig, but never consults the cookie length while
// doing so. Repeat the walk with bounds checks and return the config only when
// the cookie actually contains one; every case where the two walks would
// diverge is a cookie this returns nullptr for.
const uint8_t* findSpecificConfig(const uint8_t* cookie, uint32_t size) {
    if (size >= 12 && std::memcmp(cookie + 4, "frma", 4) == 0) {
        cookie += 12;
        size -= 12;
    }
    if (size >= 12 && std::memcmp(cookie + 4, "alac", 4) == 0) {
        cookie += 12;
        size -= 12;
    }
    return size >= sizeof(ALACSpecificConfig) ? cookie : nullptr;
}

} // namespace

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
        throw BadFormatException("ALACCodec: missing ALAC magic cookie (codec_data)");
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

    // The decoder validates none of this: it sizes its buffers with
    // calloc(frameLength * sizeof(int32_t), 1) — zero bytes for a zero
    // frameLength, which calloc still returns non-NULL for — and then decodes a
    // full frame into them, so the cookie has to be rejected before Init().
    const uint8_t* config = findSpecificConfig(cookie, cookie_size);
    if (config == nullptr) {
        throw BadFormatException("ALACCodec: magic cookie holds no ALACSpecificConfig (" +
                                 std::to_string(cookie_size) + " bytes)");
    }

    const uint32_t frame_length = (static_cast<uint32_t>(config[0]) << 24) |
                                  (static_cast<uint32_t>(config[1]) << 16) |
                                  (static_cast<uint32_t>(config[2]) << 8) |
                                   static_cast<uint32_t>(config[3]);
    const uint32_t bit_depth    = config[5];
    const uint32_t rice_k       = config[8];
    const uint32_t num_channels = config[9];

    if (frame_length == 0 || frame_length > kMaxFrameLength) {
        throw BadFormatException("ALACCodec: ALACSpecificConfig frameLength out of range (" +
                                 std::to_string(frame_length) + ")");
    }
    if (!isSupportedBitDepth(bit_depth)) {
        throw BadFormatException("ALACCodec: unsupported ALACSpecificConfig bitDepth (" +
                                 std::to_string(bit_depth) + ")");
    }
    // set_ag_params() derives wb as (1u << kb) - 1, undefined once kb reaches the
    // width of unsigned, and the Golomb reader shifts by 32 - kb, undefined at
    // kb == 0. Real encoders emit KB0 (14).
    if (rice_k == 0 || rice_k > 31) {
        throw BadFormatException("ALACCodec: ALACSpecificConfig kb out of range (" +
                                 std::to_string(rice_k) + ")");
    }
    if (num_channels == 0 || num_channels > static_cast<uint32_t>(kALACMaxChannels)) {
        throw BadFormatException("ALACCodec: ALACSpecificConfig numChannels out of range (" +
                                 std::to_string(num_channels) + ")");
    }

    m_decoder = std::make_unique<ALACDecoder>();
    if (m_decoder->Init(const_cast<uint8_t*>(cookie), cookie_size) != 0) {
        m_decoder.reset();
        throw BadFormatException("ALACCodec: ALACDecoder::Init rejected the magic cookie");
    }

    // Adopt the parameters the decoder read from the cookie.
    m_channels     = static_cast<uint16_t>(m_decoder->mConfig.numChannels);
    m_bit_depth    = static_cast<uint16_t>(m_decoder->mConfig.bitDepth);
    m_frame_length = m_decoder->mConfig.frameLength;
    if (m_decoder->mConfig.sampleRate != 0) {
        m_sample_rate = m_decoder->mConfig.sampleRate;
    }

    m_initialized = true;
    Debug::log("alac", "ALACCodec: Initialized ch=", m_channels, " bits=", m_bit_depth,
               " sr=", m_sample_rate, " frameLen=", m_frame_length);
    return true;
}

AudioFrame ALACCodec::decode_unlocked(const MediaChunk& chunk) {
    if (!m_initialized || !m_decoder || chunk.data.empty()) {
        return AudioFrame();
    }

    m_packet.assign(chunk.data.begin(), chunk.data.end());
    m_packet.resize(chunk.data.size() + kBitstreamPadding, 0);

    BitBuffer bits;
    BitBufferInit(&bits, m_packet.data(), static_cast<uint32_t>(chunk.data.size()));

    // Output buffer sized for the widest sample format (32-bit) per interleaved
    // sample; the decoder writes at the container's bit depth.
    std::vector<uint8_t> out(static_cast<size_t>(m_frame_length) * m_channels * sizeof(int32_t));
    uint32_t decoded = 0;
    int32_t status = m_decoder->Decode(&bits, out.data(), m_frame_length, m_channels, &decoded);
    if (status != 0 || decoded == 0) {
        // A truncated or corrupt packet is recoverable: the demuxer feeds the
        // next one. Matches the other codecs, which skip bad packets rather
        // than tear down playback from the decoder thread.
        Debug::log("alac", "ALACCodec: Decode failed (status=", status, ", decoded=", decoded,
                   ", packet=", chunk.data.size(), " bytes) - skipping packet");
        return AudioFrame();
    }

    const size_t total = static_cast<size_t>(decoded) * m_channels;
    AudioFrame frame;
    frame.samples.resize(total);

    // The pipeline carries full-scale S32, so every depth is scaled UP. This
    // used to downconvert to int16, which threw away 8 bits of every 24-bit
    // ALAC -- exactly the resolution a lossless format exists to preserve.
    if (m_bit_depth <= 16) {
        const int16_t* s = reinterpret_cast<const int16_t*>(out.data());
        for (size_t i = 0; i < total; ++i) {
            frame.samples[i] = static_cast<AudioSample>(s[i]) * 65536;
        }
    } else if (m_bit_depth <= 24) {
        // 20/24-bit are written as little-endian 3-byte samples.
        const uint8_t* s = out.data();
        for (size_t i = 0; i < total; ++i) {
            int32_t v = static_cast<int32_t>(s[i * 3]) |
                        (static_cast<int32_t>(s[i * 3 + 1]) << 8) |
                        (static_cast<int32_t>(s[i * 3 + 2]) << 16);
            if (v & 0x00800000) v |= ~0x00FFFFFF; // sign-extend 24 -> 32
            frame.samples[i] = static_cast<AudioSample>(v) * 256;
        }
    } else {
        const int32_t* s = reinterpret_cast<const int32_t*>(out.data());
        for (size_t i = 0; i < total; ++i) frame.samples[i] = static_cast<AudioSample>(s[i]);
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

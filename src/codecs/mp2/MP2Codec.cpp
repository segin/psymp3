/*
 * MP2Codec.cpp - AudioCodec-based MPEG-1/2 Audio Layer II decoder using kjmp2
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The bundled Layer II decoder (third_party/kjmp2) is Copyright © 2006-2013
 * Martin J. Fiedler and is used under the zlib license; see
 * third_party/kjmp2/kjmp2.txt.
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

// kjmp2's implementation is compiled straight into this translation unit (which
// is also how it lands in the --enable-final unity build). extern "C" gives its
// functions C linkage to match the header pulled in by MP2Codec.h; the pragma
// silences the sole C++17 'register' diagnostic so the otherwise-pristine
// third-party file needs no edits.
extern "C" {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
// windows.h (pulled in via psymp3.h on the mingw build) defines FASTCALL, and
// kjmp2 defines its own; swap ours out across the include and restore it after
// so there is no macro redefinition.
#pragma push_macro("FASTCALL")
#undef FASTCALL
#include "../../../third_party/kjmp2/kjmp2.c"
#pragma pop_macro("FASTCALL")
#pragma GCC diagnostic pop
}

namespace PsyMP3 {
namespace Codec {
namespace MP2 {

MP2Codec::MP2Codec(const StreamInfo& stream_info)
    : AudioCodec(stream_info),
      m_sample_rate(stream_info.sample_rate) {
    std::memset(&m_decoder, 0, sizeof(m_decoder));
}

MP2Codec::~MP2Codec() = default;

bool MP2Codec::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return initialize_unlocked();
}

AudioFrame MP2Codec::decode(const MediaChunk& chunk) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return decode_unlocked(chunk);
}

AudioFrame MP2Codec::flush() {
    return AudioFrame(); // Layer II frames are independent; nothing to drain
}

void MP2Codec::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    reset_unlocked();
}

bool MP2Codec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_type == "audio" && stream_info.codec_name == "mp2";
}

bool MP2Codec::initialize_unlocked() {
    kjmp2_init(&m_decoder);
    m_initialized = true;
    Debug::log("mp2", "MP2Codec: Initialized kjmp2 decoder");
    return true;
}

AudioFrame MP2Codec::decode_unlocked(const MediaChunk& chunk) {
    if (!m_initialized || chunk.data.size() < 4) {
        return AudioFrame();
    }

    // kjmp2 does no bounds checking and reads a full frame from the header's
    // computed size, so confirm the chunk actually holds that many bytes before
    // decoding (pcm==NULL returns the frame size without touching decoder state).
    unsigned long frame_size = kjmp2_decode_frame(&m_decoder, chunk.data.data(), nullptr);
    if (frame_size == 0 || frame_size > chunk.data.size()) {
        return AudioFrame();
    }

    // kjmp2 always emits 1152 interleaved stereo samples (mono is duplicated).
    signed short pcm[KJMP2_SAMPLES_PER_FRAME * 2];
    if (kjmp2_decode_frame(&m_decoder, chunk.data.data(), pcm) == 0) {
        return AudioFrame();
    }

    int sr = kjmp2_get_sample_rate(chunk.data.data());
    if (sr > 0) {
        m_sample_rate = static_cast<uint32_t>(sr);
    }

    AudioFrame frame;
    frame.samples.assign(pcm, pcm + (KJMP2_SAMPLES_PER_FRAME * 2));
    frame.sample_rate = m_sample_rate;
    frame.channels = 2; // kjmp2 output is always stereo-interleaved
    frame.timestamp_samples = chunk.timestamp_samples;
    if (m_sample_rate != 0) {
        frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / m_sample_rate;
    }
    return frame;
}

void MP2Codec::reset_unlocked() {
    kjmp2_init(&m_decoder);
}

// --- Support namespace ---

namespace MP2CodecSupport {

bool isMP2Stream(const StreamInfo& stream_info) {
    return stream_info.codec_type == "audio" && stream_info.codec_name == "mp2";
}

std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info) {
    if (!isMP2Stream(stream_info)) {
        return nullptr;
    }
    return std::make_unique<MP2Codec>(stream_info);
}

void registerCodec() {
    AudioCodecFactory::registerCodec("mp2", createCodec);
    Debug::log("mp2", "MP2CodecSupport: Registered mp2 codec with AudioCodecFactory");
}

} // namespace MP2CodecSupport

} // namespace MP2
} // namespace Codec
} // namespace PsyMP3

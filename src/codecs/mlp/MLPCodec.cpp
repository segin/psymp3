/*
 * MLPCodec.cpp - AudioCodec-based MLP/Dolby TrueHD decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The bundled MLP/TrueHD decoder (third_party/mlp) is used under the
 * Apache License, Version 2.0.
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

// The decoder is its own object rather than being pulled into this translation
// unit (the ALAC arrangement), because the demuxer needs it too: it decodes the
// first access unit to learn the rate and channel layout. Compiling it twice
// would duplicate its symbols, and the --enable-final unity build would make
// that a link error rather than a warning.
#include "../../../third_party/mlp/mlp_decoder.h"

namespace PsyMP3 {
namespace Codec {
namespace MLP {

MLPCodec::MLPCodec(const StreamInfo& stream_info)
    : AudioCodec(stream_info)
{
}

MLPCodec::~MLPCodec() = default;

bool MLPCodec::canDecode(const StreamInfo& stream_info) const
{
    return MLPCodecSupport::isMLPStream(stream_info);
}

std::string MLPCodec::getCodecName() const
{
    return m_stream_info.codec_name == "mlp" ? "mlp" : "truehd";
}

bool MLPCodec::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!canDecode(m_stream_info)) {
        return false;
    }

    // The stream carries its own configuration in the major sync, so there is
    // no out-of-band setup to apply; the first access unit establishes the rate
    // and the channel layout.
    m_decoder = std::make_unique<mlp::Decoder>();
    m_stream_info.bits_per_sample = 24;

    m_initialized = true;
    Debug::log("mlp", "MLPCodec: Initialized (", getCodecName(), ")");
    return true;
}

AudioFrame MLPCodec::decode(const MediaChunk& chunk)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return decode_unlocked(chunk);
}

AudioFrame MLPCodec::decode_unlocked(const MediaChunk& chunk)
{
    if (!m_initialized || !m_decoder || chunk.data.empty()) {
        return AudioFrame();
    }

    m_pcm.clear();
    try {
        m_decoder->decodeAccessUnit(chunk.data.data(), chunk.data.size(), m_pcm);
    } catch (const std::exception& e) {
        // A corrupt access unit is recoverable: the next major sync
        // re-establishes the decoder, and the demuxer keeps feeding. Matches the
        // other codecs, which skip bad packets rather than tear down playback
        // from the decoder thread.
        Debug::log("mlp", "MLPCodec: Access unit failed (", e.what(), ", ",
                   chunk.data.size(), " bytes) - skipping");
        return AudioFrame();
    }

    if (m_pcm.empty()) {
        return AudioFrame();
    }

    const uint32_t sample_rate = m_decoder->sampleRate();
    const uint16_t channels = static_cast<uint16_t>(m_decoder->channels());
    if (sample_rate == 0 || channels == 0) {
        return AudioFrame();
    }

    // What the major sync declared beats what the container guessed.
    m_stream_info.sample_rate = sample_rate;
    m_stream_info.channels = channels;

    AudioFrame frame;
    frame.samples.resize(m_pcm.size());

    // The decoder hands back 24-bit samples and the pipeline carries full-scale
    // S32, so each one is scaled up by 256. Encoders do run slightly past full
    // scale on near-clipping material, and those samples are genuine; clamping
    // them is what a player wants, since letting the multiply wrap would turn a
    // loud peak into a full-amplitude click.
    for (size_t i = 0; i < m_pcm.size(); ++i) {
        int64_t value = static_cast<int64_t>(m_pcm[i]) * 256;
        if (value > INT32_MAX) {
            value = INT32_MAX;
        } else if (value < INT32_MIN) {
            value = INT32_MIN;
        }
        frame.samples[i] = static_cast<AudioSample>(value);
    }

    frame.sample_rate = sample_rate;
    frame.channels = channels;
    frame.timestamp_samples = chunk.timestamp_samples;
    frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / sample_rate;

    return frame;
}

AudioFrame MLPCodec::flush()
{
    // Each access unit decodes to exactly its own samples; nothing is held back.
    return AudioFrame();
}

void MLPCodec::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_decoder) {
        // Drops the configuration with the rest of the state, so the access unit
        // the seek lands on must carry a major sync. The demuxer seeks to one.
        m_decoder->reset();
    }
}

// --- Support namespace ---

namespace MLPCodecSupport {

bool isMLPStream(const StreamInfo& stream_info)
{
    return stream_info.codec_type == "audio" &&
           (stream_info.codec_name == "truehd" || stream_info.codec_name == "mlp");
}

std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info)
{
    if (!isMLPStream(stream_info)) {
        return nullptr;
    }
    return std::make_unique<MLPCodec>(stream_info);
}

void registerCodec()
{
    AudioCodecFactory::registerCodec("truehd", createCodec);
    AudioCodecFactory::registerCodec("mlp", createCodec);
    Debug::log("mlp", "MLPCodecSupport: Registered truehd and mlp codecs with AudioCodecFactory");
}

} // namespace MLPCodecSupport

} // namespace MLP
} // namespace Codec
} // namespace PsyMP3

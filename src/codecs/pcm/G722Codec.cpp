/*
 * G722Codec.cpp - G.722 audio codec implementation
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace PCM {

G722Codec::G722Codec(const StreamInfo& stream_info)
    : AudioCodec(stream_info)
{
}

G722Codec::~G722Codec() = default;

bool G722Codec::canDecode(const StreamInfo& stream_info) const
{
    if (stream_info.codec_type != "audio") {
        return false;
    }

    if (stream_info.codec_name != "g722" &&
        stream_info.codec_name != "pcm_g722" &&
        stream_info.codec_name != "itu_g722") {
        return false;
    }

    if (stream_info.channels != 0 && stream_info.channels != 1) {
        return false;
    }

    // WAV headers for G.722 disagree on wBitsPerSample: 4 (bits per sample,
    // two samples per code octet), 8 (bits per octet), or 0 (unspecified).
    if (stream_info.bits_per_sample != 0 &&
        stream_info.bits_per_sample != 4 &&
        stream_info.bits_per_sample != 8) {
        return false;
    }

    if (stream_info.sample_rate != 0 &&
        stream_info.sample_rate != 16000 &&
        stream_info.sample_rate != 8000) {
        return false;
    }

    return true;
}

bool G722Codec::initialize()
{
    if (!canDecode(m_stream_info)) {
        return false;
    }

    if (m_stream_info.sample_rate == 0) {
        m_stream_info.sample_rate = 16000;
    }

    if (m_stream_info.channels == 0) {
        m_stream_info.channels = 1;
    }

    if (m_stream_info.bits_per_sample == 0) {
        m_stream_info.bits_per_sample = 8;
    }

    // The 8 kHz mode decodes only the lower sub-band, so it yields one sample
    // per octet instead of two.
    m_decoder = std::make_unique<G722Decoder>(selectBitrate_unlocked(),
                                              m_stream_info.sample_rate != 8000);
    m_initialized = true;
    return true;
}

AudioFrame G722Codec::decode(const MediaChunk& chunk)
{
    AudioFrame frame;

    if (!m_initialized || chunk.data.empty() || !m_decoder) {
        return frame;
    }

    frame.sample_rate = m_stream_info.sample_rate;
    frame.channels = m_stream_info.channels;
    frame.timestamp_samples = chunk.timestamp_samples;
    if (m_stream_info.sample_rate > 0) {
        frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / m_stream_info.sample_rate;
    }

    // The decoder writes 16-bit PCM, so decode into a scratch buffer and scale
    // up into the frame: the pipeline carries full-scale S32.
    std::vector<int16_t> pcm(m_decoder->maxSamples(chunk.data.size()));
    const std::size_t decoded =
        m_decoder->decode(chunk.data.data(), chunk.data.size(), pcm.data());

    if (decoded == 0) {
        frame.samples.clear();
        return frame;
    }

    frame.samples.resize(decoded);
    for (std::size_t i = 0; i < decoded; ++i) {
        frame.samples[i] = static_cast<AudioSample>(pcm[i]) * 65536;
    }
    return frame;
}

AudioFrame G722Codec::flush()
{
    return AudioFrame{};
}

void G722Codec::reset()
{
    if (m_decoder) {
        m_decoder->reset();
    }
}

G722Decoder::Bitrate G722Codec::selectBitrate_unlocked() const
{
    switch (m_stream_info.bitrate) {
        case 48000: return G722Decoder::Bitrate::Rate48k;
        case 56000: return G722Decoder::Bitrate::Rate56k;
        default:    return G722Decoder::Bitrate::Rate64k;
    }
}

void registerG722Codec()
{
    AudioCodecFactory::registerCodec("g722", [](const StreamInfo& stream_info) {
        return std::make_unique<G722Codec>(stream_info);
    });

    AudioCodecFactory::registerCodec("pcm_g722", [](const StreamInfo& stream_info) {
        return std::make_unique<G722Codec>(stream_info);
    });

    AudioCodecFactory::registerCodec("itu_g722", [](const StreamInfo& stream_info) {
        return std::make_unique<G722Codec>(stream_info);
    });
}

} // namespace PCM
} // namespace Codec
} // namespace PsyMP3

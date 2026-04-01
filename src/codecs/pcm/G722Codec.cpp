/*
 * G722Codec.cpp - G.722 audio codec implementation
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_G722
#include <spandsp/telephony.h>
#include <spandsp/g722.h>

namespace PsyMP3 {
namespace Codec {
namespace PCM {

G722Codec::G722Codec(const StreamInfo& stream_info)
    : AudioCodec(stream_info)
{
}

G722Codec::~G722Codec()
{
    if (m_decoder) {
        g722_decode_free(static_cast<g722_decode_state_t*>(m_decoder));
        m_decoder = nullptr;
    }
}

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

    if (stream_info.bits_per_sample != 0 && stream_info.bits_per_sample != 8) {
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

    if (m_decoder) {
        g722_decode_free(static_cast<g722_decode_state_t*>(m_decoder));
        m_decoder = nullptr;
    }

    m_initialized = initializeDecoder_unlocked();
    return m_initialized;
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

    frame.samples.resize(chunk.data.size() * 2);
    int samples_decoded = g722_decode(
        static_cast<g722_decode_state_t*>(m_decoder),
        frame.samples.data(),
        chunk.data.data(),
        static_cast<int>(chunk.data.size()));

    if (samples_decoded <= 0) {
        frame.samples.clear();
        return frame;
    }

    frame.samples.resize(static_cast<size_t>(samples_decoded));
    return frame;
}

AudioFrame G722Codec::flush()
{
    return AudioFrame{};
}

void G722Codec::reset()
{
    if (m_decoder) {
        g722_decode_free(static_cast<g722_decode_state_t*>(m_decoder));
        m_decoder = nullptr;
    }

    if (m_initialized) {
        m_initialized = initializeDecoder_unlocked();
    }
}

bool G722Codec::initializeDecoder_unlocked()
{
    m_decoder = g722_decode_init(nullptr, selectBitrate_unlocked(), selectOptions_unlocked());
    return m_decoder != nullptr;
}

int G722Codec::selectBitrate_unlocked() const
{
    switch (m_stream_info.bitrate) {
        case 48000:
        case 56000:
        case 64000:
            return static_cast<int>(m_stream_info.bitrate);
        default:
            return 64000;
    }
}

int G722Codec::selectOptions_unlocked() const
{
    return m_stream_info.sample_rate == 8000 ? G722_SAMPLE_RATE_8000 : 0;
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

#endif // HAVE_G722

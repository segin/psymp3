/*
 * XHEAACCodec.cpp - xHE-AAC (MPEG-D USAC) audio codec via FDK-AAC
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

#ifdef HAVE_XHE_AAC

// Only the plain-C shim: FDK's headers collide with faad2's, so they stay
// confined to xhe_fdk.cpp (see that file).
#include "codecs/aac/xhe_fdk.h"

namespace PsyMP3 {
namespace Codec {
namespace AAC {

namespace {

/// Object type for MPEG-D USAC, what Fraunhofer markets as xHE-AAC.
constexpr unsigned kAotUSAC = 42;

/// Output scratch: 8 channels x 2048 samples covers every USAC frame size
/// (USAC frames are 768, 1024 or 2048 samples per channel).
constexpr int kPcmBufferSamples = 8 * 2048;

/// Target loudness for xHE-AAC's mandatory loudness normalisation, in dBFS.
/// xHE streams carry loudness metadata and expect the decoder to normalise;
/// leaving it off makes these tracks noticeably quieter or louder than the
/// rest of a library. -16 LUFS is the usual streaming target and matches what
/// other players default to.
constexpr int kTargetLoudnessDbfs = -16;

} // namespace

bool XHEAACCodec::isUSACConfig(const std::vector<uint8_t>& asc)
{
    // AudioSpecificConfig begins with a 5-bit audioObjectType; the value 31 is
    // an escape meaning "32 + the next 6 bits". USAC is 42, so it is always
    // spelled with the escape form.
    if (asc.size() < 2) {
        return false;
    }
    const unsigned first5 = static_cast<unsigned>(asc[0] >> 3);
    if (first5 != 31) {
        return first5 == kAotUSAC; // cannot happen (42 > 31), but be explicit
    }
    // Next 6 bits span the low 3 bits of byte 0 and the top 3 of byte 1.
    const unsigned ext = ((static_cast<unsigned>(asc[0]) & 0x07) << 3) |
                         (static_cast<unsigned>(asc[1]) >> 5);
    return (32 + ext) == kAotUSAC;
}

XHEAACCodec::XHEAACCodec(const StreamInfo& stream_info)
    : AudioCodec(stream_info),
      m_sample_rate(stream_info.sample_rate),
      m_channels(stream_info.channels),
      // Kept separately: m_sample_rate is replaced by whatever FDK reports,
      // while chunk timestamps stay in the container's sample units.
      m_container_sample_rate(stream_info.sample_rate) {
}

XHEAACCodec::~XHEAACCodec()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    destroyDecoder_unlocked();
}

bool XHEAACCodec::canDecode(const StreamInfo& stream_info) const
{
    return stream_info.codec_type == "audio" &&
           (stream_info.codec_name == "xhe-aac" || stream_info.codec_name == "usac");
}

bool XHEAACCodec::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return initialize_unlocked();
}

AudioFrame XHEAACCodec::decode(const MediaChunk& chunk)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return decode_unlocked(chunk);
}

AudioFrame XHEAACCodec::flush()
{
    // FDK hands back every frame as it is fed; nothing is held back.
    return AudioFrame();
}

void XHEAACCodec::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_decoder && m_decoder_initialized) {
        // Drops decoder history without discarding the configuration, which
        // is what a seek needs.
        psymp3_xhe_reset(m_decoder);
    }
}

bool XHEAACCodec::initialize_unlocked()
{
    destroyDecoder_unlocked();

    if (!canDecode(m_stream_info)) {
        Debug::log("aac", "XHEAACCodec::initialize: unsupported stream type");
        return false;
    }
    if (m_stream_info.codec_data.empty()) {
        Debug::log("aac", "XHEAACCodec::initialize: missing AudioSpecificConfig");
        return false;
    }

    m_decoder = psymp3_xhe_open();
    if (!m_decoder) {
        Debug::log("aac", "XHEAACCodec::initialize: decoder open failed");
        return false;
    }

    if (psymp3_xhe_configure(m_decoder, m_stream_info.codec_data.data(),
                             static_cast<unsigned>(m_stream_info.codec_data.size()))
            != PSYMP3_XHE_OK) {
        Debug::log("aac", "XHEAACCodec::initialize: decoder rejected the AudioSpecificConfig");
        destroyDecoder_unlocked();
        return false;
    }

    if (!applyLoudnessConfig_unlocked()) {
        // Not fatal: the stream still decodes, only its level may be off.
        Debug::log("aac", "XHEAACCodec::initialize: loudness configuration rejected");
    }

    m_decoder_initialized = true;
    m_initialized = true;
    Debug::log("aac", "XHEAACCodec::initialize: USAC decoder ready");
    return true;
}

bool XHEAACCodec::applyLoudnessConfig_unlocked()
{
    return psymp3_xhe_set_target_loudness(m_decoder, kTargetLoudnessDbfs)
           == PSYMP3_XHE_OK;
}

AudioFrame XHEAACCodec::decode_unlocked(const MediaChunk& chunk)
{
    if (!m_initialized || !m_decoder_initialized || !m_decoder) {
        return AudioFrame();
    }
    if (chunk.data.empty()) {
        return AudioFrame();
    }

    std::vector<int16_t> pcm(kPcmBufferSamples);
    int frame_size = 0, rate = 0, channels = 0;
    const int rc = psymp3_xhe_decode(m_decoder, chunk.data.data(),
                                     static_cast<unsigned>(chunk.data.size()),
                                     pcm.data(), static_cast<int>(pcm.size()),
                                     &frame_size, &rate, &channels);
    if (rc == PSYMP3_XHE_NEED_MORE_DATA) {
        return AudioFrame(); // needs another access unit first
    }
    if (rc != PSYMP3_XHE_OK || frame_size <= 0 || channels <= 0) {
        Debug::log("aac", "XHEAACCodec::decode: frame failed");
        return AudioFrame();
    }

    // USAC changes sample rate and channel count more readily than plain AAC,
    // so follow what the decoder reports rather than the container header.
    m_sample_rate = static_cast<uint32_t>(rate);
    m_channels = static_cast<uint16_t>(channels);
    m_stream_info.sample_rate = m_sample_rate;
    m_stream_info.channels = m_channels;
    m_stream_info.bits_per_sample = 16;

    const size_t sample_count =
        static_cast<size_t>(frame_size) * static_cast<size_t>(channels);

    AudioFrame frame;
    frame.samples.assign(pcm.begin(),
                         pcm.begin() + static_cast<std::ptrdiff_t>(sample_count));
    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.timestamp_samples = chunk.timestamp_samples;
    // Convert with the container's rate: the timestamp is counted in sample
    // table units, which need not match the decoder's output rate.
    const uint32_t ts_rate = m_container_sample_rate ? m_container_sample_rate
                                                     : m_sample_rate;
    if (ts_rate != 0) {
        frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / ts_rate;
    }
    return frame;
}

void XHEAACCodec::destroyDecoder_unlocked()
{
    if (m_decoder) {
        psymp3_xhe_close(m_decoder);
        m_decoder = nullptr;
    }
    m_decoder_initialized = false;
    m_initialized = false;
}

namespace XHEAACCodecSupport {

void registerCodec()
{
    AudioCodecFactory::registerCodec("xhe-aac", [](const StreamInfo& info) {
        return std::make_unique<XHEAACCodec>(info);
    });
    AudioCodecFactory::registerCodec("usac", [](const StreamInfo& info) {
        return std::make_unique<XHEAACCodec>(info);
    });
}

} // namespace XHEAACCodecSupport

} // namespace AAC
} // namespace Codec
} // namespace PsyMP3

#endif // HAVE_XHE_AAC

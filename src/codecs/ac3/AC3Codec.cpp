/*
 * AC3Codec.cpp - AudioCodec-based AC-3 (A/52) decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

AC3Codec::AC3Codec(const StreamInfo& stream_info)
    : AudioCodec(stream_info)
{
}

bool AC3Codec::canDecode(const StreamInfo& stream_info) const
{
    return AC3CodecSupport::isAC3Stream(stream_info);
}

bool AC3Codec::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!canDecode(m_stream_info)) {
        return false;
    }

    // Everything the decoder needs is in each syncframe's own header, so
    // there is no out-of-band configuration to apply.
    m_decoder.reset();
    m_initialized = true;
    Debug::log("ac3", "AC3Codec: Initialized");
    return true;
}

AudioFrame AC3Codec::decode(const MediaChunk& chunk)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return decode_unlocked(chunk);
}

AudioFrame AC3Codec::decode_unlocked(const MediaChunk& chunk)
{
    if (!m_initialized || chunk.data.empty()) {
        return AudioFrame();
    }

    if (m_pending.empty()) {
        m_pending_timestamp = chunk.timestamp_samples;
    }
    m_pending.insert(m_pending.end(), chunk.data.begin(), chunk.data.end());

    m_out.clear();
    uint64_t first_timestamp = m_pending_timestamp;
    size_t offset = 0;
    unsigned decoded = 0;

    while (m_pending.size() - offset >= 7) {
        const uint8_t* p = m_pending.data() + offset;
        const size_t avail = m_pending.size() - offset;

        // Resynchronise on the sync word. A demuxer that frames the stream
        // never needs this; one that slices a RIFF data chunk can still hand
        // over a damaged or truncated frame.
        if (((p[0] << 8) | p[1]) != kSyncWord) {
            ++offset;
            continue;
        }
        AC3FrameHeader header;
        if (!parseAC3FrameHeader(p, avail, header) || header.frame_size == 0) {
            ++offset;
            continue;
        }
        if (header.frame_size > avail) {
            break; // the rest of this frame arrives with the next chunk
        }

        if (m_decoder.decode(p, header.frame_size, m_pcm)) {
            if (decoded == 0) {
                first_timestamp = m_pending_timestamp;
            }
            m_out.insert(m_out.end(), m_pcm.begin(), m_pcm.end());
            ++decoded;
        } else {
            // A damaged syncframe is recoverable: the next one carries its
            // own header and exponent history. Skip it rather than tear down
            // playback from the decoder thread, as the other codecs do.
            Debug::log("ac3", "AC3Codec: Syncframe failed (", header.frame_size,
                       " bytes) - skipping");
        }
        offset += header.frame_size;
        // A dependent E-AC-3 substream extends the frame before it rather than
        // following it, so it does not move the clock.
        if (!(header.isEAC3() && header.strmtyp == 0x1)) {
            m_pending_timestamp += static_cast<uint64_t>(header.blocks) * kSamplesPerBlock;
        }
    }

    m_pending.erase(m_pending.begin(), m_pending.begin() + static_cast<std::ptrdiff_t>(offset));

    const uint32_t sample_rate = m_decoder.sampleRate();
    const uint16_t channels = static_cast<uint16_t>(m_decoder.channels());
    if (decoded == 0 || sample_rate == 0 || channels == 0 || m_out.empty()) {
        return AudioFrame();
    }

    // What the syncframe says beats what the container guessed.
    m_stream_info.sample_rate = sample_rate;
    m_stream_info.channels = channels;

    AudioFrame frame;
    frame.samples.resize(m_out.size());

    // The pipeline carries full-scale S32. The transform already clamps to
    // [-1, 1] -- A/52 §7.9.4.1 step 6 asks for saturating arithmetic -- so
    // scaling here cannot wrap; the explicit bound on the positive side is
    // because +1.0 * 2^31 is one past INT32_MAX.
    constexpr double kScale = 2147483648.0;
    for (size_t i = 0; i < m_out.size(); ++i) {
        double value = static_cast<double>(m_out[i]) * kScale;
        if (value > static_cast<double>(INT32_MAX)) {
            value = static_cast<double>(INT32_MAX);
        } else if (value < static_cast<double>(INT32_MIN)) {
            value = static_cast<double>(INT32_MIN);
        }
        frame.samples[i] = static_cast<AudioSample>(value);
    }

    frame.sample_rate = sample_rate;
    frame.channels = channels;
    frame.timestamp_samples = first_timestamp;
    frame.timestamp_ms = (first_timestamp * 1000ULL) / sample_rate;
    return frame;
}

AudioFrame AC3Codec::flush()
{
    // Each complete syncframe was decoded as it arrived; what is left in
    // m_pending is less than a frame and cannot be decoded. The half-block in
    // the delay line has no following block to overlap with, so it is not
    // signal either.
    return AudioFrame();
}

void AC3Codec::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_decoder.reset();
    // Bytes buffered before a seek belong to the old position.
    m_pending.clear();
    m_pending_timestamp = 0;
}

// --- Support namespace ---

namespace AC3CodecSupport {

bool isAC3Stream(const StreamInfo& stream_info)
{
    return stream_info.codec_type == "audio" &&
           (stream_info.codec_name == "ac3" || stream_info.codec_name == "eac3");
}

std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info)
{
    if (!isAC3Stream(stream_info)) {
        return nullptr;
    }
    return std::make_unique<AC3Codec>(stream_info);
}

void registerCodec()
{
    AudioCodecFactory::registerCodec("ac3", createCodec);
    AudioCodecFactory::registerCodec("eac3", createCodec);
    Debug::log("ac3", "AC3CodecSupport: Registered ac3 and eac3 codecs with AudioCodecFactory");
}

} // namespace AC3CodecSupport

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

/*
 * AACCodec.cpp - Container-agnostic AAC audio codec
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

#ifdef HAVE_AAC

namespace PsyMP3 {
namespace Codec {
namespace AAC {

namespace {

/// Object type for MPEG-D USAC, what Fraunhofer markets as xHE-AAC.
constexpr unsigned kAotUSAC = PSYMP3_FDK_AOT_USAC;

/// Output scratch: 8 channels x 2048 samples covers every frame size in the
/// family (AAC frames are 960 or 1024 samples per channel, doubled to 2048 by
/// SBR; USAC frames are 768, 1024 or 2048).
constexpr int kPcmBufferSamples = 8 * 2048;

/// Target loudness for xHE-AAC's mandatory loudness normalisation, in dBFS.
/// xHE streams carry loudness metadata and expect the decoder to normalise;
/// leaving it off makes these tracks noticeably quieter or louder than the
/// rest of a library. -16 LUFS is the usual streaming target and matches what
/// other players default to. Deliberately NOT applied to AAC-LC/HE-AAC, where
/// it would shift levels relative to every other decoder.
constexpr int kTargetLoudnessDbfs = -16;

} // namespace

bool AACCodec::isUSACConfig(const std::vector<uint8_t>& asc)
{
    // AudioSpecificConfig begins with a 5-bit audioObjectType; the value 31 is
    // an escape meaning "32 + the next 6 bits". USAC is 42, so it is always
    // spelled with the escape form.
    if (asc.size() < 2) {
        return false;
    }
    const unsigned first5 = static_cast<unsigned>(asc[0] >> 3);
    if (first5 != 31) {
        return false; // 42 cannot be spelled without the escape
    }
    // Next 6 bits span the low 3 bits of byte 0 and the top 3 of byte 1.
    const unsigned ext = ((static_cast<unsigned>(asc[0]) & 0x07) << 3) |
                         (static_cast<unsigned>(asc[1]) >> 5);
    return (32 + ext) == kAotUSAC;
}

AACCodec::AACCodec(const StreamInfo& stream_info)
    : AudioCodec(stream_info),
      m_sample_rate(stream_info.sample_rate),
      m_channels(stream_info.channels),
      m_container_sample_rate(stream_info.sample_rate) {
}

AACCodec::~AACCodec() {
    std::lock_guard<std::mutex> lock(m_mutex);
    destroyDecoder_unlocked();
}

bool AACCodec::canDecode(const StreamInfo& stream_info) const {
    return AACCodecSupport::isAACStream(stream_info);
}

std::string AACCodec::getCodecName() const {
    // The demuxer names a USAC stream separately (see BoxParser), and Media
    // Info spells that one xHE-AAC rather than AAC.
    return (m_stream_info.codec_name == "xhe-aac" || m_stream_info.codec_name == "usac")
        ? "xhe-aac" : "aac";
}

bool AACCodec::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return initialize_unlocked();
}

AudioFrame AACCodec::decode(const MediaChunk& chunk) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return decode_unlocked(chunk);
}

AudioFrame AACCodec::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // FDK delays its output, so when the packets run out it is still holding
    // the end of the file -- as many samples as it reported in output_delay.
    // Draining it here is what keeps the tail from going missing.
    if (!m_initialized || !m_decoder_initialized || !m_decoder) {
        return AudioFrame();
    }

    m_flushing = true;
    if (m_flush_remaining == 0) {
        return AudioFrame(); // Fully drained; asking again would never end.
    }

    int frame_size = 0, rate = 0, channels = 0;
    if (psymp3_fdk_flush(m_decoder, m_pcm.data(), static_cast<int>(m_pcm.size()),
                         &frame_size, &rate, &channels) != PSYMP3_FDK_OK) {
        m_flush_remaining = 0;
        return AudioFrame();
    }
    if (frame_size <= 0 || channels <= 0 || rate <= 0) {
        return AudioFrame();
    }

    size_t leading_drop = 0;
    if (m_decoder_delay_remaining > 0) {
        leading_drop = std::min<size_t>(m_decoder_delay_remaining,
                                        static_cast<size_t>(frame_size));
        m_decoder_delay_remaining -= static_cast<uint32_t>(leading_drop);
        if (static_cast<size_t>(frame_size) == leading_drop) {
            return AudioFrame();
        }
    }

    // Never hand back more than the decoder said it was holding.
    size_t usable = static_cast<size_t>(frame_size) - leading_drop;
    usable = std::min<size_t>(usable, m_flush_remaining);
    m_flush_remaining -= static_cast<uint32_t>(usable);
    if (usable == 0) {
        return AudioFrame();
    }

    const size_t sample_count = usable * static_cast<size_t>(channels);
    const size_t source_offset = leading_drop * static_cast<size_t>(channels);
    if (source_offset + sample_count > m_pcm.size()) {
        return AudioFrame();
    }

    AudioFrame frame;
    frame.samples.resize(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        frame.samples[i] = static_cast<AudioSample>(m_pcm[source_offset + i]) * 65536;
    }
    frame.sample_rate = static_cast<uint32_t>(rate);
    frame.channels = static_cast<uint16_t>(channels);
    // No chunk to take a timestamp from; DemuxedStream continues its own
    // running count for this codec.
    return frame;
}

void AACCodec::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_decoder || !m_decoder_initialized) {
        return;
    }

    // Drops decoder history without discarding the configuration, which is
    // what a seek needs.
    psymp3_fdk_reset(m_decoder);

    // The first access unit after a seek has no MDCT overlap from a preceding
    // frame (nor any SBR/PS history), so emitting it produces an audible click
    // on every seek. Decode it for its side effects and throw the audio away,
    // which is what the format expects.
    m_priming_frames = 1;
    m_decoder_delay_known = false;
    m_decoder_delay_remaining = 0;
    m_flush_remaining = 0;
    m_flushing = false;
}

bool AACCodec::initialize_unlocked() {
    destroyDecoder_unlocked();

    if (!canDecode(m_stream_info)) {
        Debug::log("aac", "AACCodec::initialize: unsupported stream type");
        return false;
    }

    if (m_stream_info.codec_data.empty()) {
        Debug::log("aac", "AACCodec::initialize: missing AudioSpecificConfig");
        return false;
    }

    m_is_usac = m_stream_info.codec_name == "xhe-aac" ||
                m_stream_info.codec_name == "usac" ||
                isUSACConfig(m_stream_info.codec_data);

    // FDK can be built with profiles removed and still present the same API
    // and pkg-config name, so ask what this one actually implements. A zero
    // mask means the library declined to say, which is treated as unknown
    // rather than as nothing.
    const unsigned caps = psymp3_fdk_capabilities();
    if (caps != 0) {
        static std::once_flag caps_logged;
        std::call_once(caps_logged, [caps] {
            Debug::log("aac", "FDK-AAC profiles available:",
                       (caps & PSYMP3_FDK_CAP_AAC_LC) ? " AAC-LC" : "",
                       (caps & PSYMP3_FDK_CAP_SBR)    ? " HE-AACv1" : "",
                       (caps & PSYMP3_FDK_CAP_PS)     ? " HE-AACv2" : "",
                       (caps & PSYMP3_FDK_CAP_USAC)   ? " xHE-AAC" : "");
        });

        // Refusing beats decoding it wrong: a stripped build would otherwise
        // render the USAC core as noise rather than failing.
        if (m_is_usac && !(caps & PSYMP3_FDK_CAP_USAC)) {
            Debug::log("aac", "AACCodec::initialize: this FDK-AAC build has no USAC "
                              "support, so the xHE-AAC stream cannot be decoded");
            return false;
        }
    }

    m_decoder = psymp3_fdk_open();
    if (!m_decoder) {
        Debug::log("aac", "AACCodec::initialize: decoder open failed");
        return false;
    }

    if (psymp3_fdk_configure(m_decoder, m_stream_info.codec_data.data(),
                             static_cast<unsigned>(m_stream_info.codec_data.size()))
            != PSYMP3_FDK_OK) {
        Debug::log("aac", "AACCodec::initialize: decoder rejected the AudioSpecificConfig");
        destroyDecoder_unlocked();
        return false;
    }

    if (m_is_usac &&
        psymp3_fdk_set_target_loudness(m_decoder, kTargetLoudnessDbfs) != PSYMP3_FDK_OK) {
        // Not fatal: the stream still decodes, only its level may be off.
        Debug::log("aac", "AACCodec::initialize: loudness configuration rejected");
    }

    m_pcm.resize(kPcmBufferSamples);
    m_stream_info.bits_per_sample = 16;
    m_decoder_initialized = true;
    m_initialized = true;
    Debug::log("aac", "AACCodec::initialize: decoder ready (",
               m_is_usac ? "USAC" : "AAC", ")");
    return true;
}

void AACCodec::updateProfile_unlocked(int aot, int ext_aot) {
    // The profile has to come from a decoded frame rather than the
    // AudioSpecificConfig: HE-AACv1's SBR and HE-AACv2's Parametric Stereo are
    // usually signalled IMPLICITLY, i.e. discovered only once the decoder meets
    // them in the bitstream, so the header alone would call almost every
    // HE-AAC file plain AAC-LC.
    const char* profile = "AAC";

    if (aot == PSYMP3_FDK_AOT_USAC) {
        profile = "xHE-AAC";
    } else if (ext_aot == PSYMP3_FDK_AOT_PS) {
        profile = "HE-AACv2";
    } else if (ext_aot == PSYMP3_FDK_AOT_SBR) {
        profile = "HE-AACv1";
    } else {
        switch (aot) {
            case PSYMP3_FDK_AOT_AAC_MAIN:    profile = "AAC Main"; break;
            case PSYMP3_FDK_AOT_AAC_LC:      profile = "AAC-LC";   break;
            case PSYMP3_FDK_AOT_AAC_SSR:     profile = "AAC-SSR";  break;
            case PSYMP3_FDK_AOT_AAC_LTP:     profile = "AAC-LTP";  break;
            case PSYMP3_FDK_AOT_ER_AAC_LD:   profile = "AAC-LD";   break;
            case PSYMP3_FDK_AOT_ER_AAC_ELD:  profile = "AAC-ELD";  break;
            default: break;
        }
    }

    m_profile = profile;
}

AudioFrame AACCodec::decode_unlocked(const MediaChunk& chunk) {
    if (!m_initialized || !m_decoder_initialized || !m_decoder || chunk.data.empty()) {
        return AudioFrame();
    }

    int frame_size = 0, rate = 0, channels = 0, aot = 0, ext_aot = 0, output_delay = 0;
    const int rc = psymp3_fdk_decode(m_decoder, chunk.data.data(),
                                     static_cast<unsigned>(chunk.data.size()),
                                     m_pcm.data(), static_cast<int>(m_pcm.size()),
                                     &frame_size, &rate, &channels, &aot, &ext_aot,
                                     &output_delay);
    if (rc == PSYMP3_FDK_NEED_MORE_DATA) {
        return AudioFrame(); // needs another access unit first
    }
    if (rc != PSYMP3_FDK_OK || frame_size <= 0 || channels <= 0 || rate <= 0) {
        Debug::log("aac", "AACCodec::decode: frame failed (", chunk.data.size(), " bytes)");
        return AudioFrame();
    }

    if (m_priming_frames > 0) {
        // Post-seek warm-up frame: state is now primed, output is not usable.
        --m_priming_frames;
        return AudioFrame();
    }

    updateProfile_unlocked(aot, ext_aot);

    // Follow what the decoder reports rather than the container header: SBR
    // doubles the output rate, and USAC can change the channel count too.
    m_sample_rate = static_cast<uint32_t>(rate);
    m_channels = static_cast<uint16_t>(channels);
    m_stream_info.sample_rate = m_sample_rate;
    m_stream_info.channels = m_channels;
    m_stream_info.bits_per_sample = 16;

    // FDK delays its output by a fixed number of frames beyond anything the
    // container declares. It only says by how many once a frame has decoded,
    // so the figure is taken from the first one and then worked off.
    if (!m_decoder_delay_known) {
        m_decoder_delay_known = true;
        m_decoder_delay_remaining = (output_delay > 0) ? static_cast<uint32_t>(output_delay) : 0;
        // The same figure bounds the end-of-stream drain.
        m_flush_remaining = m_decoder_delay_remaining;
        if (m_decoder_delay_remaining != 0) {
            Debug::log("aac", "AACCodec: dropping FDK's own ", m_decoder_delay_remaining,
                       "-sample output delay");
        }
    }

    size_t leading_drop = 0;
    if (m_decoder_delay_remaining > 0) {
        leading_drop = std::min<size_t>(m_decoder_delay_remaining,
                                        static_cast<size_t>(frame_size));
        m_decoder_delay_remaining -= static_cast<uint32_t>(leading_drop);
        if (static_cast<size_t>(frame_size) == leading_drop) {
            return AudioFrame(); // Entirely the decoder's own warm-up.
        }
    }

    const size_t sample_count =
        static_cast<size_t>(frame_size - leading_drop) * static_cast<size_t>(channels);
    const size_t source_offset = leading_drop * static_cast<size_t>(channels);
    if (source_offset + sample_count > m_pcm.size()) {
        Debug::log("aac", "AACCodec::decode: frame larger than the scratch buffer");
        return AudioFrame();
    }

    AudioFrame frame;
    // FDK hands back 16-bit PCM; scale to full-scale S32.
    frame.samples.resize(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        frame.samples[i] = static_cast<AudioSample>(m_pcm[source_offset + i]) * 65536;
    }

    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.timestamp_samples = chunk.timestamp_samples;
    // Convert with the CONTAINER rate: chunk.timestamp_samples is counted in
    // the sample table's units, while m_sample_rate is the decoder's and may
    // have been doubled by SBR -- dividing by that halved the reported time
    // for implicit-SBR files.
    const uint32_t ts_rate = m_container_sample_rate ? m_container_sample_rate : m_sample_rate;
    if (ts_rate != 0) {
        frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / ts_rate;
    }

    return frame;
}

void AACCodec::destroyDecoder_unlocked() {
    if (m_decoder) {
        psymp3_fdk_close(m_decoder);
        m_decoder = nullptr;
    }
    m_decoder_initialized = false;
    m_initialized = false;
}

namespace AACCodecSupport {

void registerCodec() {
    Debug::log("aac", "AACCodecSupport::registerCodec: Registering AAC codec");
    // One decoder covers the family, so it answers to every name the demuxers
    // use for it.
    AudioCodecFactory::registerCodec("aac", createCodec);
    AudioCodecFactory::registerCodec("xhe-aac", createCodec);
    AudioCodecFactory::registerCodec("usac", createCodec);
}

std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info) {
    if (!isAACStream(stream_info)) {
        return nullptr;
    }
    return std::make_unique<AACCodec>(stream_info);
}

bool isAACStream(const StreamInfo& stream_info) {
    return stream_info.codec_type == "audio" &&
           (stream_info.codec_name == "aac" ||
            stream_info.codec_name == "xhe-aac" ||
            stream_info.codec_name == "usac");
}

} // namespace AACCodecSupport

} // namespace AAC
} // namespace Codec
} // namespace PsyMP3

#endif // HAVE_AAC

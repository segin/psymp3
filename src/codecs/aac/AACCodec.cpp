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

AACCodec::AACCodec(const StreamInfo& stream_info)
    : AudioCodec(stream_info),
      m_sample_rate(stream_info.sample_rate),
      m_channels(stream_info.channels),
      // The container's rate, kept separately because m_sample_rate is later
      // replaced by the decoder's (which SBR may have doubled). Chunk
      // timestamps are counted in container sample units, so they must be
      // converted with this one.
      m_container_sample_rate(stream_info.sample_rate) {
}

AACCodec::~AACCodec() {
    std::lock_guard<std::mutex> lock(m_mutex);
    destroyDecoder_unlocked();
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
    return flush_unlocked();
}

void AACCodec::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    reset_unlocked();
}

bool AACCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_type == "audio" && stream_info.codec_name == "aac";
}

bool AACCodec::initialize_unlocked() {
    destroyDecoder_unlocked();

    if (!canDecode(m_stream_info)) {
        Debug::log("aac", "AACCodec::initialize_unlocked: unsupported stream type");
        return false;
    }

    if (m_stream_info.codec_data.empty()) {
        Debug::log("aac", "AACCodec::initialize_unlocked: missing AudioSpecificConfig");
        return false;
    }

    m_decoder = NeAACDecOpen();
    if (!m_decoder) {
        Debug::log("aac", "AACCodec::initialize_unlocked: NeAACDecOpen failed");
        return false;
    }

    if (!configureDecoder_unlocked()) {
        destroyDecoder_unlocked();
        return false;
    }

    if (!initializeDecoderFromASC_unlocked()) {
        destroyDecoder_unlocked();
        return false;
    }

    m_initialized = true;
    return true;
}

AudioFrame AACCodec::decode_unlocked(const MediaChunk& chunk) {
    if (!m_initialized || !m_decoder_initialized || !m_decoder) {
        return AudioFrame();
    }

    if (chunk.data.empty()) {
        return AudioFrame();
    }

    NeAACDecFrameInfo frame_info = {};
    void* decoded = NeAACDecDecode(
        m_decoder,
        &frame_info,
        const_cast<unsigned char*>(chunk.data.data()),
        static_cast<unsigned long>(chunk.data.size()));

    if (frame_info.error != 0) {
        // NeAACDecGetErrorMessage returns NULL for an error code outside its
        // table; streaming a null char* into an ostream is undefined.
        const char* msg = NeAACDecGetErrorMessage(frame_info.error);
        Debug::log("aac", "AACCodec::decode_unlocked: decode error ",
                   static_cast<unsigned>(frame_info.error), ": ",
                   msg ? msg : "(unknown error)");
        return AudioFrame();
    }

    if (!decoded || frame_info.samples == 0 || frame_info.channels == 0 || frame_info.samplerate == 0) {
        return AudioFrame();
    }

    if (m_priming_frames > 0) {
        // Post-seek warm-up frame: state is now primed, output is not usable.
        --m_priming_frames;
        return AudioFrame();
    }

    // Profile, for display. It has to come from a decoded frame rather than
    // the AudioSpecificConfig: HE-AAC's SBR and HE-AACv2's Parametric Stereo
    // are usually signalled IMPLICITLY, i.e. discovered only once the decoder
    // meets them in the bitstream, so the header alone would call almost every
    // HE-AAC file plain AAC-LC.
    {
        const char* profile = nullptr;
        if (frame_info.ps != 0) {
            profile = "HE-AACv2";
        } else if (frame_info.sbr == SBR_UPSAMPLED || frame_info.sbr == SBR_DOWNSAMPLED) {
            profile = "HE-AAC";
        } else {
            switch (frame_info.object_type) {
                case MAIN: profile = "AAC Main"; break;
                case LC:   profile = "AAC-LC";   break;
                case SSR:  profile = "AAC-SSR";  break;
                case LTP:  profile = "AAC-LTP";  break;
                case LD:   profile = "AAC-LD";   break;
                default:   profile = "AAC";      break;
            }
        }
        m_profile = profile;
    }

    m_sample_rate = frame_info.samplerate;
    m_channels = frame_info.channels;
    m_stream_info.sample_rate = m_sample_rate;
    m_stream_info.channels = m_channels;
    m_stream_info.bits_per_sample = 16;

    AudioFrame frame;
    frame.samples.assign(
        static_cast<const int16_t*>(decoded),
        static_cast<const int16_t*>(decoded) + frame_info.samples);
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

AudioFrame AACCodec::flush_unlocked() {
    return AudioFrame();
}

void AACCodec::reset_unlocked() {
    if (!m_decoder) {
        return;
    }

    if (m_decoder_initialized) {
        NeAACDecPostSeekReset(m_decoder, 0);
        // The first access unit after a seek has no MDCT overlap from a
        // preceding frame (nor any SBR/PS history), so emitting it produces an
        // audible click on every seek. Decode it for its side effects and
        // throw the audio away, which is what the format expects.
        m_priming_frames = 1;
    }
}

void AACCodec::destroyDecoder_unlocked() {
    if (m_decoder) {
        NeAACDecClose(m_decoder);
        m_decoder = nullptr;
    }
    m_decoder_initialized = false;
    m_initialized = false;
}

bool AACCodec::configureDecoder_unlocked() {
    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(m_decoder);
    if (!config) {
        Debug::log("aac", "AACCodec::configureDecoder_unlocked: failed to get decoder configuration");
        return false;
    }

    config->outputFormat = FAAD_FMT_16BIT;
    config->downMatrix = 0;
    config->dontUpSampleImplicitSBR = 0;

    if (!NeAACDecSetConfiguration(m_decoder, config)) {
        Debug::log("aac", "AACCodec::configureDecoder_unlocked: failed to apply decoder configuration");
        return false;
    }

    return true;
}

bool AACCodec::initializeDecoderFromASC_unlocked() {
    mp4AudioSpecificConfig asc = {};
    // NeAACDecAudioSpecificConfig returns plain `char`, which is UNSIGNED on
    // ARM32/AArch64: comparing its result against < 0 there is always false,
    // so this sanity check silently did nothing and a malformed ASC went
    // straight to NeAACDecInit2. Compare through a signed type.
    const signed char asc_result = static_cast<signed char>(
        NeAACDecAudioSpecificConfig(
            const_cast<unsigned char*>(m_stream_info.codec_data.data()),
            static_cast<unsigned long>(m_stream_info.codec_data.size()),
            &asc));
    if (asc_result < 0) {
        Debug::log("aac", "AACCodec::initializeDecoderFromASC_unlocked: invalid AudioSpecificConfig");
        return false;
    }

    unsigned long sample_rate = 0;
    unsigned char channels = 0;
    if (NeAACDecInit2(
            m_decoder,
            const_cast<unsigned char*>(m_stream_info.codec_data.data()),
            static_cast<unsigned long>(m_stream_info.codec_data.size()),
            &sample_rate,
            &channels) != 0) {
        Debug::log("aac", "AACCodec::initializeDecoderFromASC_unlocked: NeAACDecInit2 failed");
        return false;
    }

    m_sample_rate = sample_rate;
    m_channels = channels;
    m_stream_info.sample_rate = m_sample_rate;
    m_stream_info.channels = m_channels;
    m_stream_info.bits_per_sample = 16;
    m_decoder_initialized = true;
    return true;
}

namespace AACCodecSupport {

void registerCodec() {
    Debug::log("aac", "AACCodecSupport::registerCodec: Registering AAC codec");
    AudioCodecFactory::registerCodec("aac", createCodec);
}

std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info) {
    if (!isAACStream(stream_info)) {
        return nullptr;
    }
    return std::make_unique<AACCodec>(stream_info);
}

bool isAACStream(const StreamInfo& stream_info) {
    return stream_info.codec_type == "audio" && stream_info.codec_name == "aac";
}

} // namespace AACCodecSupport

} // namespace AAC
} // namespace Codec
} // namespace PsyMP3

#endif // HAVE_AAC

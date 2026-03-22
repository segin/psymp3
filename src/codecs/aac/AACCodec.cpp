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
      m_channels(stream_info.channels) {
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
        Debug::log("aac", "AACCodec::decode_unlocked: decode error: ",
                   NeAACDecGetErrorMessage(frame_info.error));
        return AudioFrame();
    }

    if (!decoded || frame_info.samples == 0 || frame_info.channels == 0 || frame_info.samplerate == 0) {
        return AudioFrame();
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
    if (m_sample_rate != 0) {
        frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / m_sample_rate;
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
    if (NeAACDecAudioSpecificConfig(
            const_cast<unsigned char*>(m_stream_info.codec_data.data()),
            static_cast<unsigned long>(m_stream_info.codec_data.size()),
            &asc) < 0) {
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

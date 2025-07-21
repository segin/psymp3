/*
 * AudioCodec.cpp - Generic audio codec base class implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// Static codec factory registry
std::map<std::string, AudioCodecFactory::CodecFactoryFunc> AudioCodecFactory::s_codec_factories;

AudioCodec::AudioCodec(const StreamInfo& stream_info) 
    : m_stream_info(stream_info) {
}

std::unique_ptr<AudioCodec> AudioCodecFactory::createCodec(const StreamInfo& stream_info) {
    // Try registered codecs first
    auto it = s_codec_factories.find(stream_info.codec_name);
    if (it != s_codec_factories.end()) {
        return it->second(stream_info);
    }
    
    // Built-in codec fallbacks
    if (stream_info.codec_name == "pcm") {
        auto codec = std::make_unique<PCMCodec>(stream_info);
        if (codec->canDecode(stream_info)) {
            return std::move(codec);
        }
    } else if (stream_info.codec_name == "alaw") {
        auto codec = std::make_unique<ALawCodec>(stream_info);
        if (codec->canDecode(stream_info)) {
            return std::move(codec);
        }
    } else if (stream_info.codec_name == "mulaw") {
        auto codec = std::make_unique<MuLawCodec>(stream_info);
        if (codec->canDecode(stream_info)) {
            return std::move(codec);
        }
#ifdef HAVE_MP3
    } else if (stream_info.codec_name == "mp3") {
        auto codec = std::make_unique<MP3PassthroughCodec>(stream_info);
        if (codec->canDecode(stream_info)) {
            return std::move(codec);
        }
#endif
#ifdef HAVE_OGGDEMUXER
    } else if (stream_info.codec_name == "vorbis") {
        Debug::log("loader", "AudioCodecFactory: Creating VorbisPassthroughCodec for codec: vorbis");
        auto codec = std::make_unique<VorbisPassthroughCodec>(stream_info);
        if (codec->canDecode(stream_info)) {
            return std::move(codec);
        }
    } else if (stream_info.codec_name == "flac") {
        // Check if this is Ogg FLAC (from OggDemuxer) or native FLAC
        if (stream_info.codec_tag == 0) { // Ogg FLAC
            Debug::log("loader", "AudioCodecFactory: Creating OggFLACPassthroughCodec for codec: flac (Ogg container)");
            auto codec = std::make_unique<OggFLACPassthroughCodec>(stream_info);
            if (codec->canDecode(stream_info)) {
                return std::move(codec);
            }
        }
#ifdef HAVE_FLAC
        else { // Native FLAC
            Debug::log("loader", "AudioCodecFactory: Creating FLAC codec for native FLAC");
            // Handle native FLAC separately
            // This would be implemented elsewhere
        }
#endif
    } else if (stream_info.codec_name == "opus") {
        Debug::log("loader", "AudioCodecFactory: Creating OpusPassthroughCodec for codec: opus");
        auto codec = std::make_unique<OpusPassthroughCodec>(stream_info);
        if (codec->canDecode(stream_info)) {
            return std::move(codec);
        }
    } else if (stream_info.codec_name == "speex") {
        Debug::log("loader", "AudioCodecFactory: Creating SpeexCodec for codec: speex");
        auto codec = std::make_unique<SpeexCodec>(stream_info);
        if (codec->canDecode(stream_info)) {
            return std::move(codec);
        }
#endif
    }
    
    return nullptr;
}

void AudioCodecFactory::registerCodec(const std::string& codec_name, CodecFactoryFunc factory_func) {
    s_codec_factories[codec_name] = factory_func;
}

// SimplePCMCodec implementation
SimplePCMCodec::SimplePCMCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
}

bool SimplePCMCodec::initialize() {
    m_initialized = true;
    return true;
}

AudioFrame SimplePCMCodec::decode(const MediaChunk& chunk) {
    AudioFrame frame;
    
    if (chunk.data.empty()) {
        return frame; // Empty frame
    }
    
    // Set frame properties
    frame.sample_rate = m_stream_info.sample_rate;
    frame.channels = m_stream_info.channels;
    frame.timestamp_samples = chunk.timestamp_samples;
    
    // Convert samples
    convertSamples(chunk.data, frame.samples);
    
    return frame;
}

AudioFrame SimplePCMCodec::flush() {
    // Simple PCM codecs don't buffer data
    return AudioFrame{}; // Empty frame
}

void SimplePCMCodec::reset() {
    // Simple PCM codecs don't have state to reset
}
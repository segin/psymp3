/*
 * OggCodecs.cpp - Audio codec implementations for Ogg-contained formats
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

// OggCodecs are built if any Ogg-based codec is enabled
#ifdef HAVE_OGGDEMUXER

#ifdef HAVE_SPEEX
#include <speex/speex.h>
#include <speex/speex_header.h>
#endif

#ifdef HAVE_VORBIS
// VorbisPassthroughCodec implementation - now redirects to VorbisCodec
VorbisPassthroughCodec::VorbisPassthroughCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
    // Create the actual VorbisCodec
    m_vorbis_codec = std::make_unique<PsyMP3::Codec::Vorbis::VorbisCodec>(stream_info);
}

VorbisPassthroughCodec::~VorbisPassthroughCodec() {
    // Unique pointer handles cleanup
}

bool VorbisPassthroughCodec::initialize() {
    m_initialized = m_vorbis_codec->initialize();
    return m_initialized;
}

AudioFrame VorbisPassthroughCodec::decode(const MediaChunk& chunk) {
    return m_vorbis_codec->decode(chunk);
}

AudioFrame VorbisPassthroughCodec::flush() {
    return m_vorbis_codec->flush();
}

void VorbisPassthroughCodec::reset() {
    m_vorbis_codec->reset();
}

bool VorbisPassthroughCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "vorbis";
}
#endif // HAVE_VORBIS

// Helper to select the correct FLAC codec implementation
#ifdef HAVE_FLAC
#ifdef HAVE_NATIVE_FLAC
    #include "codecs/flac/NativeFLACCodec.h"
    using FLACCodecImpl = PsyMP3::Codec::FLAC::FLACCodec;
#else
    #include "codecs/FLACCodec.h"
    using FLACCodecImpl = FLACCodec;
#endif
#endif

// OggFLACPassthroughCodec implementation
OggFLACPassthroughCodec::OggFLACPassthroughCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
}

OggFLACPassthroughCodec::~OggFLACPassthroughCodec() {
    // Unique pointer handles cleanup
}

bool OggFLACPassthroughCodec::initialize() {
#ifdef HAVE_FLAC
    try {
        m_flac_codec = std::make_unique<FLACCodecImpl>(m_stream_info);
        m_initialized = m_flac_codec->initialize();
    } catch (...) {
        m_initialized = false;
    }
    return m_initialized;
#else
    return false;
#endif
}

AudioFrame OggFLACPassthroughCodec::decode(const MediaChunk& chunk) {
    if (!m_flac_codec) {
        return AudioFrame{};
    }

    // Check for Ogg FLAC header packet (starts with "fLaC")
    // libFLAC wrapper (FLACCodec) synthesizes the STREAMINFO header based on StreamInfo,
    // so we should skip the actual Ogg packet containing STREAMINFO to avoid confusing it.
    if (chunk.data.size() >= 4 &&
        chunk.data[0] == 'f' && chunk.data[1] == 'L' &&
        chunk.data[2] == 'a' && chunk.data[3] == 'C') {
        return AudioFrame{};
    }

    return m_flac_codec->decode(chunk);
}

AudioFrame OggFLACPassthroughCodec::flush() {
    if (m_flac_codec) {
        return m_flac_codec->flush();
    }
    return AudioFrame{};
}

void OggFLACPassthroughCodec::reset() {
    if (m_flac_codec) {
        m_flac_codec->reset();
    }
}

bool OggFLACPassthroughCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "flac";
}

// OpusPassthroughCodec implementation - now redirects to OpusCodec
OpusPassthroughCodec::OpusPassthroughCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
    // Create the actual OpusCodec
    m_opus_codec = std::make_unique<PsyMP3::Codec::Opus::OpusCodec>(stream_info);
}

OpusPassthroughCodec::~OpusPassthroughCodec() {
    // Unique pointer handles cleanup
}

bool OpusPassthroughCodec::initialize() {
    m_initialized = m_opus_codec->initialize();
    return m_initialized;
}

AudioFrame OpusPassthroughCodec::decode(const MediaChunk& chunk) {
    return m_opus_codec->decode(chunk);
}

AudioFrame OpusPassthroughCodec::flush() {
    return m_opus_codec->flush();
}

void OpusPassthroughCodec::reset() {
    m_opus_codec->reset();
}

bool OpusPassthroughCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "opus";
}

// SpeexCodec implementation
SpeexCodec::SpeexCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
    m_sample_rate = stream_info.sample_rate;
    m_channels = stream_info.channels > 0 ? stream_info.channels : 1;
}

SpeexCodec::~SpeexCodec() {
#ifdef HAVE_SPEEX
    if (m_stereo_state) {
        speex_stereo_state_destroy((SpeexStereoState*)m_stereo_state);
        m_stereo_state = nullptr;
    }
    if (m_decoder_state) {
        speex_decoder_destroy(m_decoder_state);
        m_decoder_state = nullptr;
    }
    if (m_bits) {
        speex_bits_destroy((SpeexBits*)m_bits);
        delete (SpeexBits*)m_bits;
        m_bits = nullptr;
    }
#endif
}

bool SpeexCodec::initialize() {
#ifdef HAVE_SPEEX
    // Clean up if already initialized
    if (m_decoder_state) {
        speex_decoder_destroy(m_decoder_state);
        m_decoder_state = nullptr;
    }

    // Allocate SpeexBits
    if (!m_bits) {
        m_bits = new SpeexBits;
        speex_bits_init((SpeexBits*)m_bits);
    } else {
        speex_bits_reset((SpeexBits*)m_bits);
    }

    // If we have codec_data (header), parse it
    if (!m_stream_info.codec_data.empty()) {
        SpeexHeader* header = speex_packet_to_header((char*)m_stream_info.codec_data.data(), m_stream_info.codec_data.size());
        if (header) {
            const SpeexMode* mode = speex_lib_get_mode(header->mode);
            if (mode) {
                m_decoder_state = speex_decoder_init(mode);
                if (m_decoder_state) {
                     int enh = 1;
                     int frame_size = 0;
                     speex_decoder_ctl(m_decoder_state, SPEEX_SET_ENH, &enh);
                     speex_decoder_ctl(m_decoder_state, SPEEX_GET_FRAME_SIZE, &frame_size);
                     m_frame_size = frame_size;
                     m_sample_rate = header->rate;
                     m_channels = header->nb_channels;
                     m_initialized_speex = true;
                }
            }
            speex_header_free(header);
        }
    }

    // Even if not fully initialized (waiting for header packet), we mark as initialized
    // to allow decode() to be called. decode() will handle the header.
    m_initialized = true;
    return true;
#else
    return false;
#endif
}

AudioFrame SpeexCodec::decode(const MediaChunk& chunk) {
    AudioFrame frame;
    
#ifdef HAVE_SPEEX
    if (chunk.data.empty() || !m_bits) {
        return frame;
    }
    
    // Check if it's a header packet
    if (!m_initialized_speex) {
        if (chunk.data.size() >= 8 && memcmp(chunk.data.data(), "Speex   ", 8) == 0) {
            SpeexHeader* header = speex_packet_to_header((char*)chunk.data.data(), chunk.data.size());
            if (header) {
                const SpeexMode* mode = speex_lib_get_mode(header->mode);
                if (mode) {
                    m_decoder_state = speex_decoder_init(mode);
                    if (m_decoder_state) {
                         int enh = 1;
                         int frame_size = 0;
                         speex_decoder_ctl(m_decoder_state, SPEEX_SET_ENH, &enh);
                         speex_decoder_ctl(m_decoder_state, SPEEX_GET_FRAME_SIZE, &frame_size);
                         m_frame_size = frame_size;
                         m_sample_rate = header->rate;
                         m_channels = header->nb_channels;
                         m_initialized_speex = true;
                    }
                }
                speex_header_free(header);
            }
            return frame; // Header packet consumed
        }

        // If not a header but audio, try to infer mode
        if (!m_decoder_state) {
            int modeID = SPEEX_MODEID_NB;
            if (m_sample_rate > 25000) modeID = SPEEX_MODEID_UWB;
            else if (m_sample_rate > 12500) modeID = SPEEX_MODEID_WB;

            const SpeexMode* mode = speex_lib_get_mode(modeID);
            if (mode) {
                m_decoder_state = speex_decoder_init(mode);
                if (m_decoder_state) {
                     int enh = 1;
                     int frame_size = 0;
                     speex_decoder_ctl(m_decoder_state, SPEEX_SET_ENH, &enh);
                     speex_decoder_ctl(m_decoder_state, SPEEX_GET_FRAME_SIZE, &frame_size);
                     m_frame_size = frame_size;
                     m_initialized_speex = true;
                }
            }
        }
    }
    
    if (!m_decoder_state) return frame;

    // Copy data to SpeexBits
    speex_bits_read_from((SpeexBits*)m_bits, (char*)chunk.data.data(), chunk.data.size());
    
    std::vector<int16_t> decoded_samples;
    
    while (true) {
        // Try to decode a frame
        int16_t output[2048]; // Max frame size
        int ret = speex_decode_int(m_decoder_state, (SpeexBits*)m_bits, output);
        if (ret != 0) break; // Error or end of stream (ret==0 is success)

        // Handle stereo
        if (m_channels == 2) {
            if (!m_stereo_state) m_stereo_state = speex_stereo_state_init();
            speex_decode_stereo_int(output, m_frame_size, (SpeexStereoState*)m_stereo_state);
        }

        // Append to samples
        for (int i = 0; i < (int)m_frame_size * m_channels; ++i) {
            decoded_samples.push_back(output[i]);
        }

        if (speex_bits_remaining((SpeexBits*)m_bits) <= 0) break;
    }

    frame.samples = std::move(decoded_samples);
    frame.timestamp_samples = chunk.timestamp_samples;
    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;

#endif
    return frame;
}

AudioFrame SpeexCodec::flush() {
    return AudioFrame{};
}

void SpeexCodec::reset() {
#ifdef HAVE_SPEEX
    if (m_decoder_state) {
        speex_decoder_ctl(m_decoder_state, SPEEX_RESET_STATE, NULL);
    }
    if (m_bits) {
        speex_bits_reset((SpeexBits*)m_bits);
    }
    if (m_stereo_state) {
        speex_stereo_state_reset((SpeexStereoState*)m_stereo_state);
    }
#endif
    // Also reset init state? No, codec config should persist
}

bool SpeexCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "speex";
}

#endif // HAVE_OGGDEMUXER

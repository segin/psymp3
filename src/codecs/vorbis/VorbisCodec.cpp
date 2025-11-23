/*
 * vorbis.cpp - Ogg Vorbis decoder using OggDemuxer and libvorbis directly
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

namespace PsyMP3 {
namespace Codec {
namespace Vorbis {

// ========== Vorbis Stream Class ==========

Vorbis::Vorbis(TagLib::String name) : Stream(name)
{
    // Create a DemuxedStream to handle the actual decoding
    m_demuxed_stream = std::make_unique<DemuxedStream>(name);
    
    // Copy properties from the demuxed stream
    m_rate = m_demuxed_stream->getRate();
    m_channels = m_demuxed_stream->getChannels();
    m_bitrate = m_demuxed_stream->getBitrate();
    m_length = m_demuxed_stream->getLength();
    m_slength = m_demuxed_stream->getSLength();
}

Vorbis::~Vorbis()
{
    // Unique pointer handles cleanup
}

size_t Vorbis::getData(size_t len, void *buf)
{
    return m_demuxed_stream->getData(len, buf);
}

void Vorbis::seekTo(unsigned long pos)
{
    m_demuxed_stream->seekTo(pos);
}

bool Vorbis::eof()
{
    return m_demuxed_stream->eof();
}

// ========== Vorbis Codec Class ==========

VorbisCodec::VorbisCodec(const StreamInfo& stream_info) : AudioCodec(stream_info)
{
    // Initialize Vorbis structures
    vorbis_info_init(&m_vorbis_info);
    vorbis_comment_init(&m_vorbis_comment);
}

VorbisCodec::~VorbisCodec()
{
    // Clean up in reverse order of initialization
    if (m_synthesis_initialized) {
        vorbis_block_clear(&m_vorbis_block);
        vorbis_dsp_clear(&m_vorbis_dsp);
    }
    vorbis_comment_clear(&m_vorbis_comment);
    vorbis_info_clear(&m_vorbis_info);
}

bool VorbisCodec::initialize()
{
    m_header_packets_received = 0;
    m_synthesis_initialized = false;
    m_output_buffer.clear();
    m_initialized = true;
    return true;
}

bool VorbisCodec::canDecode(const StreamInfo& stream_info) const
{
    return stream_info.codec_name == "vorbis";
}

AudioFrame VorbisCodec::decode(const MediaChunk& chunk)
{
    AudioFrame frame;
    
    if (chunk.data.empty()) {
        return frame;
    }
    
    // First 3 packets are headers (identification, comment, setup)
    if (m_header_packets_received < 3) {
        if (processHeaderPacket(chunk.data)) {
            m_header_packets_received++;
            
            // After all 3 headers, initialize synthesis
            if (m_header_packets_received == 3) {
                if (vorbis_synthesis_init(&m_vorbis_dsp, &m_vorbis_info) != 0) {
                    throw BadFormatException("Failed to initialize Vorbis synthesis");
                }
                if (vorbis_block_init(&m_vorbis_dsp, &m_vorbis_block) != 0) {
                    vorbis_dsp_clear(&m_vorbis_dsp);
                    throw BadFormatException("Failed to initialize Vorbis block");
                }
                m_synthesis_initialized = true;
            }
        }
        return frame; // Headers don't produce audio
    }
    
    // Process audio packet
    if (!m_synthesis_initialized) {
        return frame;
    }
    
    
    // Create Ogg packet structure
    ogg_packet packet;
    packet.packet = const_cast<unsigned char*>(chunk.data.data());
    packet.bytes = static_cast<long>(chunk.data.size());
    packet.b_o_s = 0; // Not beginning of stream
    packet.e_o_s = 0; // Not end of stream (we'd need to detect this)
    packet.granulepos = chunk.timestamp_samples;
    packet.packetno = m_header_packets_received + 1; // Packet number
    
    // Decode the packet
    int synthesis_result = vorbis_synthesis(&m_vorbis_block, &packet);
    if (synthesis_result == 0) {
        int blockin_result = vorbis_synthesis_blockin(&m_vorbis_dsp, &m_vorbis_block);
        if (blockin_result == 0) {
            processSynthesis();
        } else {
        }
    } else {
    }
    
    // Return accumulated samples
    if (!m_output_buffer.empty()) {
        frame.sample_rate = m_stream_info.sample_rate;
        frame.channels = m_stream_info.channels;
        frame.samples = std::move(m_output_buffer);
        frame.timestamp_samples = chunk.timestamp_samples;
        m_output_buffer.clear();
    } else {
    }
    
    return frame;
}

AudioFrame VorbisCodec::flush()
{
    AudioFrame frame;
    
    if (m_synthesis_initialized) {
        // Process any remaining samples
        processSynthesis();
        
        if (!m_output_buffer.empty()) {
            frame.sample_rate = m_stream_info.sample_rate;
            frame.channels = m_stream_info.channels;
            frame.samples = std::move(m_output_buffer);
            m_output_buffer.clear();
        }
    }
    
    return frame;
}

void VorbisCodec::reset()
{
    // For seeking, we just need to restart the synthesis engine.
    // We do not re-initialize the vorbis_info struct, as the headers
    // are not sent again by the demuxer for Vorbis streams.
    if (m_synthesis_initialized) {
        vorbis_synthesis_restart(&m_vorbis_dsp);
    }
    // We also clear our internal PCM buffer.
    m_output_buffer.clear();
}

bool VorbisCodec::processHeaderPacket(const std::vector<uint8_t>& packet_data)
{
    // Since OggDemuxer now provides properly parsed packets from libogg,
    // we need to create ogg_packet structures with correct metadata
    ogg_packet packet;
    packet.packet = const_cast<unsigned char*>(packet_data.data());
    packet.bytes = static_cast<long>(packet_data.size());
    packet.b_o_s = (m_header_packets_received == 0) ? 1 : 0; // First packet is BOS
    packet.e_o_s = 0;
    packet.granulepos = -1; // Headers don't have granule positions
    packet.packetno = m_header_packets_received;
    
    // Process the header packet
    int result = vorbis_synthesis_headerin(&m_vorbis_info, &m_vorbis_comment, &packet);
    
    if (result < 0) {
        throw BadFormatException("Failed to process Vorbis header");
    }
    
    return true;
}

bool VorbisCodec::processSynthesis()
{
    float **pcm_channels;
    int samples_available;
    
    // Prevent buffer from growing too large (memory leak protection)
    constexpr size_t MAX_BUFFER_SIZE = 48000 * 2 * 2; // 2 seconds of stereo at 48kHz
    if (m_output_buffer.size() > MAX_BUFFER_SIZE) {
        Debug::log("vorbis", "VorbisCodec: Buffer too large (", m_output_buffer.size(), "), clearing to prevent memory leak");
        m_output_buffer.clear();
    }
    
    // Get available PCM samples
    while ((samples_available = vorbis_synthesis_pcmout(&m_vorbis_dsp, &pcm_channels)) > 0) {
        int channels = m_vorbis_info.channels;
        
        // Prevent excessive accumulation
        if (m_output_buffer.size() + (samples_available * channels) > MAX_BUFFER_SIZE) {
            Debug::log("vorbis", "VorbisCodec: Would exceed buffer limit, processing partial samples");
            samples_available = (MAX_BUFFER_SIZE - m_output_buffer.size()) / channels;
            if (samples_available <= 0) {
                break; // Buffer is full enough
            }
        }
        
        // Convert float samples to 16-bit integers
        for (int i = 0; i < samples_available; i++) {
            for (int channel = 0; channel < channels; channel++) {
                float sample = pcm_channels[channel][i];
                
                // Clamp and convert to 16-bit
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                
                int16_t sample_16 = static_cast<int16_t>(sample * 32767.0f);
                m_output_buffer.push_back(sample_16);
            }
        }
        
        // Tell libvorbis we consumed these samples
        vorbis_synthesis_read(&m_vorbis_dsp, samples_available);
    }
    
    return !m_output_buffer.empty();
}

} // namespace Vorbis
} // namespace Codec
} // namespace PsyMP3

#endif // HAVE_OGGDEMUXER

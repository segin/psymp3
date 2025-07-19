/*
 * opus.cpp - Opus decoder using generic demuxer architecture and libopus directly
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

// ========== OpusFile Stream Class ==========

OpusFile::OpusFile(TagLib::String name) : Stream()
{
    m_path = name; // Set path manually to avoid TagLib FileRef creation
    
    // Create a DemuxedStream to handle all parsing (container + metadata)
    m_demuxed_stream = std::make_unique<DemuxedStream>(name);
    
    // Copy properties from the demuxed stream
    m_rate = m_demuxed_stream->getRate();
    m_channels = m_demuxed_stream->getChannels();
    m_bitrate = m_demuxed_stream->getBitrate();
    m_length = m_demuxed_stream->getLength();
    m_slength = m_demuxed_stream->getSLength();
}

OpusFile::~OpusFile()
{
    // Unique pointer handles cleanup
}

size_t OpusFile::getData(size_t len, void *buf)
{
    return m_demuxed_stream->getData(len, buf);
}

void OpusFile::seekTo(unsigned long pos)
{
    m_demuxed_stream->seekTo(pos);
}

bool OpusFile::eof()
{
    return m_demuxed_stream->eof();
}

TagLib::String OpusFile::getArtist()
{
    return m_demuxed_stream->getArtist();
}

TagLib::String OpusFile::getTitle()
{
    return m_demuxed_stream->getTitle();
}

TagLib::String OpusFile::getAlbum()
{
    return m_demuxed_stream->getAlbum();
}

// ========== Opus Codec Class ==========

OpusCodec::OpusCodec(const StreamInfo& stream_info) : AudioCodec(stream_info)
{
    // Opus always outputs at 48kHz
    m_sample_rate = 48000;
}

OpusCodec::~OpusCodec()
{
    if (m_opus_decoder) {
        opus_decoder_destroy(m_opus_decoder);
        m_opus_decoder = nullptr;
    }
}

bool OpusCodec::initialize()
{
    m_header_packets_received = 0;
    m_decoder_initialized = false;
    m_output_buffer.clear();
    m_initialized = true;
    return true;
}

bool OpusCodec::canDecode(const StreamInfo& stream_info) const
{
    return stream_info.codec_name == "opus";
}

AudioFrame OpusCodec::decode(const MediaChunk& chunk)
{
    Debug::log("opus", "OpusCodec::decode called - chunk.data.size=", chunk.data.size(), ", timestamp=", chunk.timestamp_ms, "ms");
    
    AudioFrame frame;
    
    if (chunk.data.empty()) {
        Debug::log("opus", "Empty chunk, returning empty frame");
        return frame;
    }
    
    // First packet is OpusHead (identification), second is OpusTags (comment)
    if (m_header_packets_received < 2) {
        Debug::log("opus", "Processing header packet ", m_header_packets_received + 1);
        if (processHeaderPacket(chunk.data)) {
            m_header_packets_received++;
            Debug::log("opus", "Header packet ", m_header_packets_received, " processed successfully");
            
            // After OpusHead packet, initialize decoder
            if (m_header_packets_received == 1 && m_channels > 0) {
                int error;
                m_opus_decoder = opus_decoder_create(m_sample_rate, m_channels, &error);
                if (!m_opus_decoder || error != OPUS_OK) {
                    throw BadFormatException("Failed to initialize Opus decoder: " + std::string(opus_strerror(error)));
                }
                m_decoder_initialized = true;
                Debug::log("opus", "Decoder initialized successfully - sample_rate=", m_sample_rate, ", channels=", m_channels);
            }
        } else {
            Debug::log("opus", "Header packet ", m_header_packets_received + 1, " processing failed");
        }
        return frame; // Headers don't produce audio
    }
    
    // Process audio packet
    if (!m_decoder_initialized || !m_opus_decoder) {
        Debug::log("opus", "Decoder not initialized, decoder_init=", m_decoder_initialized, ", opus_decoder=", (void*)m_opus_decoder);
        return frame;
    }
    
    Debug::log("opus", "Decoding packet size=", chunk.data.size(), " bytes, timestamp=", chunk.timestamp_ms, "ms");
    
    // Decode the packet (Opus packets can vary in size)
    // Maximum frame size for Opus is 5760 samples per channel at 48kHz
    constexpr int MAX_FRAME_SIZE = 5760;
    std::vector<opus_int16> decode_buffer(MAX_FRAME_SIZE * m_channels);
    
    int samples_decoded = opus_decode(m_opus_decoder, 
                                     chunk.data.data(), 
                                     static_cast<opus_int32>(chunk.data.size()),
                                     decode_buffer.data(), 
                                     MAX_FRAME_SIZE, 
                                     0); // 0 = normal decode, 1 = FEC decode
    
    if (samples_decoded < 0) {
        // Decoding error - return empty frame but don't throw
        Debug::log("opus", "Decode error: ", opus_strerror(samples_decoded));
        return frame;
    }
    
    if (samples_decoded > 0) {
        // Copy decoded samples to output buffer
        size_t total_samples = samples_decoded * m_channels;
        m_output_buffer.resize(total_samples);
        
        for (size_t i = 0; i < total_samples; i++) {
            m_output_buffer[i] = static_cast<int16_t>(decode_buffer[i]);
        }
        
        Debug::log("opus", "Successfully decoded ", samples_decoded, " samples, total_samples=", total_samples);
        
        // Create frame
        frame.sample_rate = m_sample_rate;
        frame.channels = m_channels;
        frame.samples = std::move(m_output_buffer);
        frame.timestamp_samples = chunk.timestamp_samples;
        frame.timestamp_ms = chunk.timestamp_ms;
        m_output_buffer.clear();
    }
    
    Debug::log("opus", "Returning frame with ", frame.samples.size(), " samples");
    
    return frame;
}

AudioFrame OpusCodec::flush()
{
    AudioFrame frame;
    
    if (m_decoder_initialized && m_opus_decoder) {
        // Opus doesn't have buffered data to flush like Vorbis
        // Return empty frame
    }
    
    return frame;
}

void OpusCodec::reset()
{
    if (m_opus_decoder) {
        opus_decoder_destroy(m_opus_decoder);
        m_opus_decoder = nullptr;
    }
    
    m_header_packets_received = 0;
    m_decoder_initialized = false;
    m_output_buffer.clear();
    m_channels = 0;
}

bool OpusCodec::processHeaderPacket(const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "OpusCodec: processHeaderPacket called, packet_size=", packet_data.size());
    if (packet_data.size() >= 8) {
        std::string header_str;
        for (size_t i = 0; i < std::min(size_t(8), packet_data.size()); i++) {
            char c = packet_data[i];
            if (c >= 32 && c <= 126) {
                header_str += c;
            } else {
                header_str += "\\x" + std::to_string(c);
            }
        }
        Debug::log("opus", "First 8 bytes: '", header_str, "'");
    }
    
    // Check if OpusHead is anywhere in the packet
    if (packet_data.size() >= 19) {
        for (size_t i = 0; i <= packet_data.size() - 8; i++) {
            if (std::memcmp(packet_data.data() + i, "OpusHead", 8) == 0) {
                Debug::log("opus", "Found OpusHead at offset ", i);
                break;
            }
        }
    }
    
    if (m_header_packets_received == 0) {
        // OpusHead packet
        if (packet_data.size() >= 19 && 
            std::memcmp(packet_data.data(), "OpusHead", 8) == 0) {
            
            // Parse OpusHead packet
            // Byte 8: version (should be 1)
            // Byte 9: channel count
            // Bytes 10-11: pre-skip (little endian)
            // Bytes 12-15: input sample rate (little endian, informational only)
            // Bytes 16-17: output gain (little endian, in Q7.8 format)
            // Byte 18: channel mapping family
            
            m_channels = packet_data[9];
            
            if (m_channels < 1 || m_channels > 8) {
                throw BadFormatException("Invalid Opus channel count: " + std::to_string(m_channels));
            }
            
            return true;
        }
    } else if (m_header_packets_received == 1) {
        // OpusTags packet (comment header)
        if (packet_data.size() >= 8 && 
            std::memcmp(packet_data.data(), "OpusTags", 8) == 0) {
            // We don't need to parse the comment data
            Debug::log("opus", "OpusTags header found and processed");
            return true;
        }
    }
    
    Debug::log("opus", "Header packet not recognized, returning false");
    return false;
}
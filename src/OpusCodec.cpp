/*
 * OpusCodec.cpp - Container-agnostic Opus audio codec implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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

// ========== OpusCodec Implementation ==========

OpusCodec::OpusCodec(const StreamInfo& stream_info) : AudioCodec(stream_info)
{
    Debug::log("opus", "OpusCodec constructor called");
    
    // Opus always outputs at 48kHz
    m_sample_rate = 48000;
    
    // Initialize atomic variables
    m_samples_decoded.store(0);
    m_samples_to_skip.store(0);
    m_error_state.store(false);
    
    // Reserve buffer space for efficiency
    m_output_buffer.reserve(5760 * 8); // Max frame size * max channels
    m_float_buffer.reserve(5760 * 8);
    
    Debug::log("opus", "OpusCodec constructor completed");
}

OpusCodec::~OpusCodec()
{
    Debug::log("opus", "OpusCodec destructor called");
    
    // Clean up libopus decoder
    if (m_opus_decoder) {
        opus_decoder_destroy(m_opus_decoder);
        m_opus_decoder = nullptr;
    }
    
    Debug::log("opus", "OpusCodec destructor completed");
}

bool OpusCodec::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("opus", "OpusCodec::initialize called");
    
    // Reset all state
    m_header_packets_received = 0;
    m_decoder_initialized = false;
    m_channels = 0;
    m_pre_skip = 0;
    m_output_gain = 0;
    
    // Clear buffers
    m_output_buffer.clear();
    m_float_buffer.clear();
    
    // Reset atomic counters
    m_samples_decoded.store(0);
    m_samples_to_skip.store(0);
    m_error_state.store(false);
    m_last_error.clear();
    
    // Clean up any existing decoder
    if (m_opus_decoder) {
        opus_decoder_destroy(m_opus_decoder);
        m_opus_decoder = nullptr;
    }
    
    m_initialized = true;
    
    Debug::log("opus", "OpusCodec::initialize completed successfully");
    return true;
}

bool OpusCodec::canDecode(const StreamInfo& stream_info) const
{
    // Thread-safe read-only operation, no lock needed
    return stream_info.codec_name == "opus";
}

AudioFrame OpusCodec::decode(const MediaChunk& chunk)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return decodeAudioPacket_unlocked(chunk.data);
}

AudioFrame OpusCodec::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("opus", "OpusCodec::flush called");
    
    // Opus doesn't buffer data like some other codecs
    // Return empty frame as there's nothing to flush
    AudioFrame frame;
    
    Debug::log("opus", "OpusCodec::flush completed");
    return frame;
}

void OpusCodec::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    resetDecoderState_unlocked();
}

// ========== Private Implementation Methods ==========

AudioFrame OpusCodec::decodeAudioPacket_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "OpusCodec::decodeAudioPacket_unlocked called - packet_size=", packet_data.size());
    
    AudioFrame frame;
    
    if (packet_data.empty()) {
        Debug::log("opus", "Empty packet, returning empty frame");
        return frame;
    }
    
    // Check for error state
    if (m_error_state.load()) {
        Debug::log("opus", "Codec in error state, returning empty frame");
        return frame;
    }
    
    // Process header packets first
    if (m_header_packets_received < 2) {
        Debug::log("opus", "Processing header packet ", m_header_packets_received + 1);
        if (processHeaderPacket_unlocked(packet_data)) {
            m_header_packets_received++;
            Debug::log("opus", "Header packet ", m_header_packets_received, " processed successfully");
            
            // After identification header, initialize decoder
            if (m_header_packets_received == 1 && m_channels > 0) {
                if (!initializeOpusDecoder_unlocked()) {
                    m_error_state.store(true);
                    m_last_error = "Failed to initialize Opus decoder";
                    return frame;
                }
            }
        } else {
            Debug::log("opus", "Header packet ", m_header_packets_received + 1, " processing failed");
            m_error_state.store(true);
            m_last_error = "Invalid header packet";
        }
        return frame; // Headers don't produce audio
    }
    
    // Process audio packet
    if (!m_decoder_initialized || !m_opus_decoder) {
        Debug::log("opus", "Decoder not initialized, skipping packet");
        return frame;
    }
    
    // Validate packet before decoding
    if (!validateOpusPacket_unlocked(packet_data)) {
        Debug::log("opus", "Invalid Opus packet, skipping");
        return frame;
    }
    
    Debug::log("opus", "Decoding audio packet size=", packet_data.size(), " bytes");
    
    // Decode the packet
    // Maximum frame size for Opus is 5760 samples per channel at 48kHz
    constexpr int MAX_FRAME_SIZE = 5760;
    std::vector<opus_int16> decode_buffer(MAX_FRAME_SIZE * m_channels);
    
    int samples_decoded = opus_decode(m_opus_decoder, 
                                     packet_data.data(), 
                                     static_cast<opus_int32>(packet_data.size()),
                                     decode_buffer.data(), 
                                     MAX_FRAME_SIZE, 
                                     0); // 0 = normal decode, 1 = FEC decode
    
    if (samples_decoded < 0) {
        // Handle decoder error
        handleDecoderError_unlocked(samples_decoded);
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
        
        // Apply pre-skip and gain processing
        applyPreSkip_unlocked(frame);
        applyOutputGain_unlocked(frame);
        
        // Update sample counter
        m_samples_decoded.fetch_add(samples_decoded);
        
        // Clear output buffer for next use
        m_output_buffer.clear();
    }
    
    Debug::log("opus", "Returning frame with ", frame.samples.size(), " samples");
    return frame;
}

bool OpusCodec::processHeaderPacket_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "processHeaderPacket_unlocked called, packet_size=", packet_data.size());
    
    if (m_header_packets_received == 0) {
        // Process identification header
        return processIdentificationHeader_unlocked(packet_data);
    } else if (m_header_packets_received == 1) {
        // Process comment header
        return processCommentHeader_unlocked(packet_data);
    }
    
    Debug::log("opus", "Unexpected header packet number: ", m_header_packets_received);
    return false;
}

bool OpusCodec::processIdentificationHeader_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "processIdentificationHeader_unlocked called");
    
    // OpusHead packet must be at least 19 bytes
    if (packet_data.size() < 19) {
        Debug::log("opus", "OpusHead packet too small: ", packet_data.size(), " bytes");
        return false;
    }
    
    // Check for OpusHead signature
    if (std::memcmp(packet_data.data(), "OpusHead", 8) != 0) {
        Debug::log("opus", "Invalid OpusHead signature");
        return false;
    }
    
    // Parse OpusHead packet
    // Byte 8: version (should be 1)
    uint8_t version = packet_data[8];
    if (version != 1) {
        Debug::log("opus", "Unsupported Opus version: ", version);
        return false;
    }
    
    // Byte 9: channel count
    m_channels = packet_data[9];
    if (m_channels < 1 || m_channels > 255) {
        Debug::log("opus", "Invalid channel count: ", m_channels);
        return false;
    }
    
    // Bytes 10-11: pre-skip (little endian)
    m_pre_skip = packet_data[10] | (packet_data[11] << 8);
    
    // Bytes 12-15: input sample rate (little endian, informational only)
    uint32_t input_sample_rate = packet_data[12] | (packet_data[13] << 8) | 
                                (packet_data[14] << 16) | (packet_data[15] << 24);
    
    // Bytes 16-17: output gain (little endian, in Q7.8 format)
    m_output_gain = static_cast<int16_t>(packet_data[16] | (packet_data[17] << 8));
    
    // Byte 18: channel mapping family
    uint8_t channel_mapping_family = packet_data[18];
    
    // Set up pre-skip counter
    m_samples_to_skip.store(m_pre_skip);
    
    Debug::log("opus", "OpusHead parsed successfully:");
    Debug::log("opus", "  Version: ", version);
    Debug::log("opus", "  Channels: ", m_channels);
    Debug::log("opus", "  Pre-skip: ", m_pre_skip);
    Debug::log("opus", "  Input sample rate: ", input_sample_rate);
    Debug::log("opus", "  Output gain: ", m_output_gain);
    Debug::log("opus", "  Channel mapping family: ", channel_mapping_family);
    
    return true;
}

bool OpusCodec::processCommentHeader_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "processCommentHeader_unlocked called");
    
    // OpusTags packet must be at least 8 bytes for signature
    if (packet_data.size() < 8) {
        Debug::log("opus", "OpusTags packet too small: ", packet_data.size(), " bytes");
        return false;
    }
    
    // Check for OpusTags signature
    if (std::memcmp(packet_data.data(), "OpusTags", 8) != 0) {
        Debug::log("opus", "Invalid OpusTags signature");
        return false;
    }
    
    // We don't need to parse the comment data for codec functionality
    // The demuxer will handle metadata extraction
    Debug::log("opus", "OpusTags header validated successfully");
    
    return true;
}

bool OpusCodec::initializeOpusDecoder_unlocked()
{
    Debug::log("opus", "initializeOpusDecoder_unlocked called");
    
    if (m_channels == 0) {
        Debug::log("opus", "Cannot initialize decoder: channels not set");
        return false;
    }
    
    int error;
    m_opus_decoder = opus_decoder_create(m_sample_rate, m_channels, &error);
    
    if (!m_opus_decoder || error != OPUS_OK) {
        Debug::log("opus", "Failed to create Opus decoder: ", opus_strerror(error));
        return false;
    }
    
    m_decoder_initialized = true;
    Debug::log("opus", "Opus decoder initialized successfully - sample_rate=", m_sample_rate, ", channels=", m_channels);
    
    return true;
}

void OpusCodec::applyPreSkip_unlocked(AudioFrame& frame)
{
    uint64_t samples_to_skip = m_samples_to_skip.load();
    if (samples_to_skip == 0 || frame.samples.empty()) {
        return;
    }
    
    size_t frame_samples = frame.getSampleFrameCount();
    
    if (samples_to_skip >= frame_samples) {
        // Skip entire frame
        m_samples_to_skip.fetch_sub(frame_samples);
        frame.samples.clear();
        Debug::log("opus", "Skipped entire frame (", frame_samples, " samples), ", 
                  m_samples_to_skip.load(), " samples remaining to skip");
    } else {
        // Skip partial frame
        size_t samples_to_remove = samples_to_skip * frame.channels;
        frame.samples.erase(frame.samples.begin(), frame.samples.begin() + samples_to_remove);
        m_samples_to_skip.store(0);
        Debug::log("opus", "Skipped ", samples_to_skip, " samples from frame start");
    }
}

void OpusCodec::applyOutputGain_unlocked(AudioFrame& frame)
{
    if (m_output_gain == 0 || frame.samples.empty()) {
        return; // No gain adjustment needed
    }
    
    // Apply Q7.8 format gain (output_gain / 256.0)
    float gain_factor = static_cast<float>(m_output_gain) / 256.0f;
    
    for (int16_t& sample : frame.samples) {
        float adjusted = static_cast<float>(sample) * gain_factor;
        
        // Clamp to 16-bit range
        if (adjusted > 32767.0f) {
            sample = 32767;
        } else if (adjusted < -32768.0f) {
            sample = -32768;
        } else {
            sample = static_cast<int16_t>(adjusted);
        }
    }
    
    Debug::log("opus", "Applied output gain: ", m_output_gain, " (factor: ", gain_factor, ")");
}

bool OpusCodec::validateOpusPacket_unlocked(const std::vector<uint8_t>& packet_data)
{
    // Basic packet validation
    if (packet_data.empty()) {
        return false;
    }
    
    // Opus packets should not be excessively large
    // Maximum theoretical packet size is around 1275 bytes
    if (packet_data.size() > 2000) {
        Debug::log("opus", "Packet suspiciously large: ", packet_data.size(), " bytes");
        return false;
    }
    
    return true;
}

void OpusCodec::handleDecoderError_unlocked(int opus_error)
{
    std::string error_msg = "Opus decode error: " + std::string(opus_strerror(opus_error));
    Debug::log("opus", error_msg);
    
    m_last_error = error_msg;
    
    // For some errors, we can continue; for others, we should stop
    switch (opus_error) {
        case OPUS_BAD_ARG:
        case OPUS_BUFFER_TOO_SMALL:
        case OPUS_INVALID_PACKET:
            // These are recoverable - just skip this packet
            break;
            
        case OPUS_INTERNAL_ERROR:
        case OPUS_UNIMPLEMENTED:
        case OPUS_INVALID_STATE:
        case OPUS_ALLOC_FAIL:
            // These are more serious - set error state
            m_error_state.store(true);
            break;
            
        default:
            // Unknown error - be conservative and set error state
            m_error_state.store(true);
            break;
    }
}

void OpusCodec::resetDecoderState_unlocked()
{
    Debug::log("opus", "resetDecoderState_unlocked called");
    
    // Clean up decoder
    if (m_opus_decoder) {
        opus_decoder_destroy(m_opus_decoder);
        m_opus_decoder = nullptr;
    }
    
    // Reset state
    m_header_packets_received = 0;
    m_decoder_initialized = false;
    m_channels = 0;
    m_pre_skip = 0;
    m_output_gain = 0;
    
    // Clear buffers
    m_output_buffer.clear();
    m_float_buffer.clear();
    
    // Reset atomic variables
    m_samples_decoded.store(0);
    m_samples_to_skip.store(0);
    m_error_state.store(false);
    m_last_error.clear();
    
    Debug::log("opus", "Decoder state reset completed");
}

// ========== OpusHeader Implementation ==========

bool OpusHeader::isValid() const
{
    return version == 1 && 
           channel_count >= 1 && channel_count <= 255 &&
           (channel_mapping_family == 0 || channel_mapping_family == 1 || channel_mapping_family == 255);
}

OpusHeader OpusHeader::parseFromPacket(const std::vector<uint8_t>& packet_data)
{
    OpusHeader header = {};
    
    if (packet_data.size() < 19 || std::memcmp(packet_data.data(), "OpusHead", 8) != 0) {
        return header; // Invalid header
    }
    
    header.version = packet_data[8];
    header.channel_count = packet_data[9];
    header.pre_skip = packet_data[10] | (packet_data[11] << 8);
    header.input_sample_rate = packet_data[12] | (packet_data[13] << 8) | 
                              (packet_data[14] << 16) | (packet_data[15] << 24);
    header.output_gain = static_cast<int16_t>(packet_data[16] | (packet_data[17] << 8));
    header.channel_mapping_family = packet_data[18];
    
    // Parse channel mapping if present
    if (header.channel_mapping_family != 0 && packet_data.size() >= 21) {
        header.stream_count = packet_data[19];
        header.coupled_stream_count = packet_data[20];
        
        if (packet_data.size() >= 21 + static_cast<size_t>(header.channel_count)) {
            header.channel_mapping.resize(header.channel_count);
            for (uint8_t i = 0; i < header.channel_count; i++) {
                header.channel_mapping[i] = packet_data[21 + i];
            }
        }
    }
    
    return header;
}

// ========== OpusComments Implementation ==========

OpusComments OpusComments::parseFromPacket(const std::vector<uint8_t>& packet_data)
{
    OpusComments comments;
    
    if (packet_data.size() < 8 || std::memcmp(packet_data.data(), "OpusTags", 8) != 0) {
        return comments; // Invalid comment header
    }
    
    // Basic parsing - this is primarily for demuxer use
    // For now, just validate the header structure
    if (packet_data.size() >= 12) {
        // Vendor string length (little endian)
        uint32_t vendor_length = packet_data[8] | (packet_data[9] << 8) | 
                                (packet_data[10] << 16) | (packet_data[11] << 24);
        
        if (packet_data.size() >= 12 + vendor_length) {
            comments.vendor_string = std::string(
                reinterpret_cast<const char*>(packet_data.data() + 12), 
                vendor_length
            );
        }
    }
    
    return comments;
}

#endif // HAVE_OGGDEMUXER
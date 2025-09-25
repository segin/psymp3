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
    
    // Initialize decoder state
    m_use_multistream = false;
    
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
    
    // Clean up libopus decoders
    if (m_opus_ms_decoder) {
        opus_multistream_decoder_destroy(m_opus_ms_decoder);
        m_opus_ms_decoder = nullptr;
    }
    if (m_opus_decoder) {
        opus_decoder_destroy(m_opus_decoder);
        m_opus_decoder = nullptr;
    }
    
    Debug::log("opus", "OpusCodec destructor completed");
}

bool OpusCodec::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return initialize_unlocked();
}

bool OpusCodec::initialize_unlocked()
{
    Debug::log("opus", "OpusCodec::initialize_unlocked called");
    
    // Reset all state first
    resetDecoderState_unlocked();
    
    // Extract Opus parameters from StreamInfo
    if (!extractOpusParameters_unlocked()) {
        m_last_error = "Failed to extract valid Opus parameters from StreamInfo";
        Debug::log("opus", m_last_error);
        return false;
    }
    
    // Validate parameters against Opus specification limits
    if (!validateOpusParameters_unlocked()) {
        m_last_error = "Opus parameters validation failed";
        Debug::log("opus", m_last_error);
        return false;
    }
    
    // Set up internal buffers and state variables
    if (!setupInternalBuffers_unlocked()) {
        m_last_error = "Failed to set up internal buffers";
        Debug::log("opus", m_last_error);
        return false;
    }
    
    m_initialized = true;
    
    Debug::log("opus", "OpusCodec::initialize_unlocked completed successfully");
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
    
    Debug::log("opus", "OpusCodec::reset called");
    
    // For seeking support, we can reset the decoder state without destroying it
    // This is more efficient than full reinitialization
    if (m_decoder_initialized) {
        if (m_use_multistream && m_opus_ms_decoder) {
            // Reset multistream decoder state
            int error = opus_multistream_decoder_ctl(m_opus_ms_decoder, OPUS_RESET_STATE);
            if (error != OPUS_OK) {
                Debug::log("opus", "Warning: Failed to reset multistream decoder state: ", opus_strerror(error));
                // Fall back to full reset
                resetDecoderState_unlocked();
                return;
            }
            Debug::log("opus", "Multistream decoder state reset successfully");
            
        } else if (!m_use_multistream && m_opus_decoder) {
            // Reset standard decoder state
            int error = opus_decoder_ctl(m_opus_decoder, OPUS_RESET_STATE);
            if (error != OPUS_OK) {
                Debug::log("opus", "Warning: Failed to reset decoder state: ", opus_strerror(error));
                // Fall back to full reset
                resetDecoderState_unlocked();
                return;
            }
            Debug::log("opus", "Decoder state reset successfully");
        }
        
        // Reset position tracking but keep decoder configuration
        m_samples_decoded.store(0);
        m_samples_to_skip.store(m_pre_skip); // Restore original pre-skip value
        m_error_state.store(false);
        m_last_error.clear();
        
        // Clear output buffers but don't deallocate
        m_output_buffer.clear();
        m_float_buffer.clear();
        
        Debug::log("opus", "Decoder reset completed (seeking mode)");
        
    } else {
        // If decoder not initialized, do full reset
        Debug::log("opus", "Decoder not initialized, performing full reset");
        resetDecoderState_unlocked();
    }
}

// ========== Private Implementation Methods ==========

bool OpusCodec::extractOpusParameters_unlocked()
{
    Debug::log("opus", "extractOpusParameters_unlocked called");
    
    // Verify this is an Opus stream
    if (m_stream_info.codec_name != "opus") {
        Debug::log("opus", "Not an Opus stream: codec_name=", m_stream_info.codec_name);
        return false;
    }
    
    // Extract basic parameters from StreamInfo
    // Note: For Opus, the actual channel count and other parameters come from the
    // identification header packet, not from the container's StreamInfo.
    // However, we can use StreamInfo for initial validation and hints.
    
    if (m_stream_info.channels > 0) {
        // Use StreamInfo channels as a hint, but the actual value will come from OpusHead
        Debug::log("opus", "StreamInfo suggests ", m_stream_info.channels, " channels");
    }
    
    if (m_stream_info.sample_rate > 0) {
        // Opus always outputs at 48kHz regardless of input sample rate
        Debug::log("opus", "StreamInfo suggests ", m_stream_info.sample_rate, " Hz sample rate (will output at 48kHz)");
    }
    
    // Check for codec-specific data (might contain pre-parsed header info)
    if (!m_stream_info.codec_data.empty()) {
        Debug::log("opus", "StreamInfo contains ", m_stream_info.codec_data.size(), " bytes of codec data");
        // For now, we'll process headers from packets rather than codec_data
        // This could be enhanced later to support pre-parsed header information
    }
    
    Debug::log("opus", "Opus parameters extracted successfully from StreamInfo");
    return true;
}

bool OpusCodec::validateOpusParameters_unlocked()
{
    Debug::log("opus", "validateOpusParameters_unlocked called");
    
    // At initialization time, we don't have the actual Opus parameters yet
    // (they come from the identification header packet).
    // We can only validate what we have from StreamInfo.
    
    // Validate codec type
    if (m_stream_info.codec_type != "audio") {
        Debug::log("opus", "Invalid codec type: ", m_stream_info.codec_type, " (expected 'audio')");
        return false;
    }
    
    // Validate codec name
    if (m_stream_info.codec_name != "opus") {
        Debug::log("opus", "Invalid codec name: ", m_stream_info.codec_name, " (expected 'opus')");
        return false;
    }
    
    // If StreamInfo provides channel count, validate it against Opus limits
    if (m_stream_info.channels > 0) {
        if (m_stream_info.channels > 255) {
            Debug::log("opus", "Invalid channel count in StreamInfo: ", m_stream_info.channels, " (max 255)");
            return false;
        }
    }
    
    // If StreamInfo provides sample rate, it should be reasonable
    if (m_stream_info.sample_rate > 0) {
        // Opus supports 8, 12, 16, 24, and 48 kHz input rates
        // But any reasonable rate is acceptable since Opus handles resampling internally
        if (m_stream_info.sample_rate < 8000 || m_stream_info.sample_rate > 192000) {
            Debug::log("opus", "Unusual sample rate in StreamInfo: ", m_stream_info.sample_rate, " Hz");
            // Don't fail for this - just log it
        }
    }
    
    // Validate bitrate if provided
    if (m_stream_info.bitrate > 0) {
        // Opus supports 6 kb/s to 510 kb/s
        if (m_stream_info.bitrate < 6000 || m_stream_info.bitrate > 510000) {
            Debug::log("opus", "Unusual bitrate in StreamInfo: ", m_stream_info.bitrate, " bps");
            // Don't fail for this - just log it
        }
    }
    
    Debug::log("opus", "Opus parameters validation completed successfully");
    return true;
}

bool OpusCodec::setupInternalBuffers_unlocked()
{
    Debug::log("opus", "setupInternalBuffers_unlocked called");
    
    // Set up output buffers for maximum possible Opus frame
    // Maximum frame size for Opus is 5760 samples per channel at 48kHz (120ms at 48kHz)
    // Maximum channels is 255, but we'll be conservative and prepare for 8 channels initially
    constexpr size_t MAX_FRAME_SIZE = 5760;
    constexpr size_t MAX_INITIAL_CHANNELS = 8;
    constexpr size_t INITIAL_BUFFER_SIZE = MAX_FRAME_SIZE * MAX_INITIAL_CHANNELS;
    
    try {
        // Reserve space for output buffers to avoid reallocations during decoding
        m_output_buffer.clear();
        m_output_buffer.reserve(INITIAL_BUFFER_SIZE);
        
        m_float_buffer.clear();
        m_float_buffer.reserve(INITIAL_BUFFER_SIZE);
        
        Debug::log("opus", "Reserved buffer space for ", INITIAL_BUFFER_SIZE, " samples");
        
        // Initialize state variables
        m_sample_rate = 48000;  // Opus always outputs at 48kHz
        m_channels = 0;         // Will be set from identification header
        m_pre_skip = 0;         // Will be set from identification header
        m_output_gain = 0;      // Will be set from identification header
        
        // Reset header processing state
        m_header_packets_received = 0;
        m_decoder_initialized = false;
        
        // Reset atomic counters
        m_samples_decoded.store(0);
        m_samples_to_skip.store(0);
        m_error_state.store(false);
        m_last_error.clear();
        
        Debug::log("opus", "Internal buffers and state variables set up successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("opus", "Failed to set up internal buffers: ", e.what());
        return false;
    }
}

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
    if (!m_decoder_initialized || (!m_opus_decoder && !m_opus_ms_decoder)) {
        Debug::log("opus", "Decoder not initialized, skipping packet");
        return frame;
    }
    
    // Validate decoder state before processing
    if (!validateDecoderState_unlocked()) {
        Debug::log("opus", "Decoder state validation failed, attempting recovery");
        if (!recoverFromError_unlocked()) {
            Debug::log("opus", "Decoder recovery failed, skipping packet");
            return frame;
        }
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
    
    int samples_decoded;
    
    if (m_use_multistream && m_opus_ms_decoder) {
        // Use multistream decoder
        samples_decoded = opus_multistream_decode(m_opus_ms_decoder,
                                                 packet_data.data(),
                                                 static_cast<opus_int32>(packet_data.size()),
                                                 decode_buffer.data(),
                                                 MAX_FRAME_SIZE,
                                                 0); // 0 = normal decode, 1 = FEC decode
    } else if (!m_use_multistream && m_opus_decoder) {
        // Use standard decoder
        samples_decoded = opus_decode(m_opus_decoder, 
                                     packet_data.data(), 
                                     static_cast<opus_int32>(packet_data.size()),
                                     decode_buffer.data(), 
                                     MAX_FRAME_SIZE, 
                                     0); // 0 = normal decode, 1 = FEC decode
    } else {
        Debug::log("opus", "Decoder configuration mismatch");
        m_last_error = "Decoder configuration mismatch";
        return frame;
    }
    
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
    
    // Validate parameters against Opus specification limits
    if (!validateOpusHeaderParameters_unlocked(version, m_channels, channel_mapping_family, input_sample_rate)) {
        return false;
    }
    
    // Resize buffers now that we know the actual channel count
    if (!resizeBuffersForChannels_unlocked(m_channels)) {
        Debug::log("opus", "Failed to resize buffers for ", m_channels, " channels");
        return false;
    }
    
    // Set up pre-skip counter
    m_samples_to_skip.store(m_pre_skip);
    
    Debug::log("opus", "OpusHead parsed successfully:");
    Debug::log("opus", "  Version: ", version);
    Debug::log("opus", "  Channels: ", m_channels);
    Debug::log("opus", "  Pre-skip: ", m_pre_skip);
    Debug::log("opus", "  Input sample rate: ", input_sample_rate);
    Debug::log("opus", "  Output gain: ", m_output_gain);
    Debug::log("opus", "  Channel mapping family: ", channel_mapping_family);
    
    // For multistream configurations, we need to parse the full header
    if (channel_mapping_family != 0 && m_channels > 2) {
        // Parse the complete header for multistream decoder setup
        OpusHeader header = OpusHeader::parseFromPacket(packet_data);
        if (!header.isValid()) {
            Debug::log("opus", "Failed to parse complete Opus header for multistream setup");
            return false;
        }
        
        // Initialize multistream decoder
        if (!initializeMultiStreamDecoder_unlocked(header)) {
            Debug::log("opus", "Failed to initialize multistream decoder");
            return false;
        }
    }
    
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

bool OpusCodec::validateOpusHeaderParameters_unlocked(uint8_t version, uint8_t channels, uint8_t mapping_family, uint32_t input_sample_rate)
{
    Debug::log("opus", "validateOpusHeaderParameters_unlocked called");
    
    // Validate version - RFC 6716 specifies version 1
    if (version != 1) {
        Debug::log("opus", "Unsupported Opus version: ", version, " (expected 1)");
        return false;
    }
    
    // Validate channel count - Opus supports 1-255 channels
    if (channels < 1 || channels > 255) {
        Debug::log("opus", "Invalid channel count: ", channels, " (must be 1-255)");
        return false;
    }
    
    // Validate channel mapping family
    // Family 0: Mono/Stereo (1-2 channels)
    // Family 1: Surround sound (1-8 channels, Vorbis order)
    // Family 255: Custom mapping (application-defined)
    switch (mapping_family) {
        case 0:
            if (channels > 2) {
                Debug::log("opus", "Channel mapping family 0 supports max 2 channels, got ", channels);
                return false;
            }
            break;
        case 1:
            if (channels > 8) {
                Debug::log("opus", "Channel mapping family 1 supports max 8 channels, got ", channels);
                return false;
            }
            break;
        case 255:
            // Custom mapping - accept any channel count up to 255
            break;
        default:
            Debug::log("opus", "Unsupported channel mapping family: ", mapping_family);
            return false;
    }
    
    // Validate input sample rate - should be reasonable
    // Opus internally supports 8, 12, 16, 24, and 48 kHz
    // But the input_sample_rate field is informational only
    if (input_sample_rate > 0) {
        if (input_sample_rate < 8000 || input_sample_rate > 192000) {
            Debug::log("opus", "Unusual input sample rate: ", input_sample_rate, " Hz");
            // Don't fail - this is informational only
        }
    }
    
    // Validate pre-skip - should be reasonable
    if (m_pre_skip > 65535) {
        Debug::log("opus", "Unusually large pre-skip value: ", m_pre_skip);
        // Don't fail - but log it
    }
    
    // Validate output gain - Q7.8 format, so range is -32768 to +32767
    // This represents -128.0 dB to +127.996 dB
    if (m_output_gain < -32768 || m_output_gain > 32767) {
        Debug::log("opus", "Output gain out of range: ", m_output_gain);
        return false;
    }
    
    Debug::log("opus", "Opus header parameters validated successfully");
    return true;
}

bool OpusCodec::resizeBuffersForChannels_unlocked(uint16_t channels)
{
    Debug::log("opus", "resizeBuffersForChannels_unlocked called with ", channels, " channels");
    
    if (channels == 0 || channels > 255) {
        Debug::log("opus", "Invalid channel count for buffer resize: ", channels);
        return false;
    }
    
    try {
        // Maximum frame size for Opus is 5760 samples per channel at 48kHz
        constexpr size_t MAX_FRAME_SIZE = 5760;
        size_t buffer_size = MAX_FRAME_SIZE * channels;
        
        // Resize buffers if needed
        if (m_output_buffer.capacity() < buffer_size) {
            m_output_buffer.clear();
            m_output_buffer.reserve(buffer_size);
            Debug::log("opus", "Resized output buffer to ", buffer_size, " samples");
        }
        
        if (m_float_buffer.capacity() < buffer_size) {
            m_float_buffer.clear();
            m_float_buffer.reserve(buffer_size);
            Debug::log("opus", "Resized float buffer to ", buffer_size, " samples");
        }
        
        Debug::log("opus", "Buffer resize completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("opus", "Failed to resize buffers: ", e.what());
        return false;
    }
}

bool OpusCodec::initializeOpusDecoder_unlocked()
{
    Debug::log("opus", "initializeOpusDecoder_unlocked called");
    
    if (m_channels == 0) {
        Debug::log("opus", "Cannot initialize decoder: channels not set");
        m_last_error = "Cannot initialize decoder: channel count not set";
        return false;
    }
    
    // Validate sample rate - Opus always outputs at 48kHz
    if (m_sample_rate != 48000) {
        Debug::log("opus", "Warning: Opus sample rate should be 48000, got ", m_sample_rate);
        m_sample_rate = 48000; // Force to 48kHz
    }
    
    // For simple mono/stereo configurations (channel mapping family 0),
    // use the standard decoder. For surround sound (family 1) or custom (family 255),
    // we need to use the multistream decoder.
    
    // Since we don't have the full header info here, we'll determine based on channel count
    // and initialize the appropriate decoder type
    
    if (m_channels <= 2) {
        // Use standard decoder for mono/stereo
        int error;
        m_opus_decoder = opus_decoder_create(m_sample_rate, m_channels, &error);
        
        if (!m_opus_decoder || error != OPUS_OK) {
            m_last_error = "Failed to create Opus decoder: " + std::string(opus_strerror(error));
            Debug::log("opus", m_last_error);
            return false;
        }
        
        m_use_multistream = false;
        Debug::log("opus", "Standard Opus decoder initialized successfully - sample_rate=", m_sample_rate, ", channels=", m_channels);
        
    } else {
        // For multi-channel, we need the full header information to set up multistream decoder
        // This will be handled by initializeMultiStreamDecoder_unlocked() when we have the header
        Debug::log("opus", "Multi-channel configuration detected (", m_channels, " channels) - will initialize multistream decoder");
        m_use_multistream = true;
        // Don't set m_decoder_initialized yet - wait for full header processing
        return true;
    }
    
    // Configure decoder parameters for optimal performance
    if (m_opus_decoder) {
        // Note: We're using the integer API (opus_decode) which is the standard approach
        // The decoder is already optimized for the target platform
        Debug::log("opus", "Decoder configured for integer API operation");
    }
    
    m_decoder_initialized = true;
    Debug::log("opus", "Opus decoder configuration completed successfully");
    
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
    
    // For some errors, we can continue; for others, we should attempt recovery
    switch (opus_error) {
        case OPUS_BAD_ARG:
        case OPUS_BUFFER_TOO_SMALL:
        case OPUS_INVALID_PACKET:
            // These are recoverable - just skip this packet
            Debug::log("opus", "Recoverable error, skipping packet");
            break;
            
        case OPUS_INTERNAL_ERROR:
        case OPUS_INVALID_STATE:
            // These indicate decoder state issues - attempt recovery
            Debug::log("opus", "Decoder state error, attempting recovery");
            m_error_state.store(true);
            if (recoverFromError_unlocked()) {
                Debug::log("opus", "Error recovery successful");
            } else {
                Debug::log("opus", "Error recovery failed, decoder disabled");
            }
            break;
            
        case OPUS_UNIMPLEMENTED:
            // Feature not supported - this is permanent
            Debug::log("opus", "Unsupported feature, decoder disabled");
            m_error_state.store(true);
            break;
            
        case OPUS_ALLOC_FAIL:
            // Memory allocation failure - serious but might be temporary
            Debug::log("opus", "Memory allocation failure, attempting recovery");
            m_error_state.store(true);
            if (recoverFromError_unlocked()) {
                Debug::log("opus", "Memory error recovery successful");
            } else {
                Debug::log("opus", "Memory error recovery failed, decoder disabled");
            }
            break;
            
        default:
            // Unknown error - be conservative and attempt recovery
            Debug::log("opus", "Unknown error code ", opus_error, ", attempting recovery");
            m_error_state.store(true);
            if (recoverFromError_unlocked()) {
                Debug::log("opus", "Unknown error recovery successful");
            } else {
                Debug::log("opus", "Unknown error recovery failed, decoder disabled");
            }
            break;
    }
}

bool OpusCodec::initializeMultiStreamDecoder_unlocked(const OpusHeader& header)
{
    Debug::log("opus", "initializeMultiStreamDecoder_unlocked called");
    
    if (!header.isValid()) {
        m_last_error = "Invalid Opus header for multistream decoder initialization";
        Debug::log("opus", m_last_error);
        return false;
    }
    
    // Validate that we need multistream decoder
    if (header.channel_mapping_family == 0 && header.channel_count <= 2) {
        Debug::log("opus", "Warning: Using multistream decoder for simple configuration");
    }
    
    // For channel mapping family 1 (surround sound), we need stream count and coupling info
    if (header.channel_mapping_family == 1) {
        if (header.stream_count == 0 || header.channel_mapping.size() != header.channel_count) {
            m_last_error = "Invalid multistream configuration in Opus header";
            Debug::log("opus", m_last_error);
            return false;
        }
        
        // Create multistream decoder
        int error;
        m_opus_ms_decoder = opus_multistream_decoder_create(
            m_sample_rate,
            header.channel_count,
            header.stream_count,
            header.coupled_stream_count,
            header.channel_mapping.data(),
            &error
        );
        
        if (!m_opus_ms_decoder || error != OPUS_OK) {
            m_last_error = "Failed to create Opus multistream decoder: " + std::string(opus_strerror(error));
            Debug::log("opus", m_last_error);
            return false;
        }
        
        Debug::log("opus", "Multistream decoder created successfully:");
        Debug::log("opus", "  Channels: ", header.channel_count);
        Debug::log("opus", "  Streams: ", header.stream_count);
        Debug::log("opus", "  Coupled streams: ", header.coupled_stream_count);
        
    } else if (header.channel_mapping_family == 255) {
        // Custom channel mapping - we need the mapping table
        if (header.channel_mapping.size() != header.channel_count) {
            m_last_error = "Invalid custom channel mapping in Opus header";
            Debug::log("opus", m_last_error);
            return false;
        }
        
        // For custom mapping, assume all streams are uncoupled for simplicity
        // This may need to be enhanced based on specific use cases
        int error;
        m_opus_ms_decoder = opus_multistream_decoder_create(
            m_sample_rate,
            header.channel_count,
            header.channel_count, // Assume one stream per channel
            0, // No coupled streams
            header.channel_mapping.data(),
            &error
        );
        
        if (!m_opus_ms_decoder || error != OPUS_OK) {
            m_last_error = "Failed to create Opus custom multistream decoder: " + std::string(opus_strerror(error));
            Debug::log("opus", m_last_error);
            return false;
        }
        
        Debug::log("opus", "Custom multistream decoder created successfully for ", header.channel_count, " channels");
        
    } else {
        m_last_error = "Unsupported channel mapping family for multistream decoder: " + std::to_string(header.channel_mapping_family);
        Debug::log("opus", m_last_error);
        return false;
    }
    
    // Configure multistream decoder parameters
    if (m_opus_ms_decoder) {
        // Note: Using integer API for consistency and compatibility
        Debug::log("opus", "Multistream decoder configured for integer API operation");
    }
    
    m_use_multistream = true;
    m_decoder_initialized = true;
    
    Debug::log("opus", "Multistream decoder initialization completed successfully");
    return true;
}

void OpusCodec::resetDecoderState_unlocked()
{
    Debug::log("opus", "resetDecoderState_unlocked called");
    
    // Clean up decoders
    if (m_opus_ms_decoder) {
        opus_multistream_decoder_destroy(m_opus_ms_decoder);
        m_opus_ms_decoder = nullptr;
    }
    if (m_opus_decoder) {
        opus_decoder_destroy(m_opus_decoder);
        m_opus_decoder = nullptr;
    }
    
    // Reset state
    m_header_packets_received = 0;
    m_decoder_initialized = false;
    m_use_multistream = false;
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

bool OpusCodec::validateDecoderState_unlocked() const
{
    // Check basic state consistency
    if (!m_decoder_initialized) {
        return false;
    }
    
    // Check that we have the right decoder type
    if (m_use_multistream) {
        if (!m_opus_ms_decoder) {
            Debug::log("opus", "Decoder state invalid: multistream flag set but no multistream decoder");
            return false;
        }
        if (m_opus_decoder) {
            Debug::log("opus", "Warning: Both standard and multistream decoders present");
        }
    } else {
        if (!m_opus_decoder) {
            Debug::log("opus", "Decoder state invalid: standard decoder expected but not present");
            return false;
        }
        if (m_opus_ms_decoder) {
            Debug::log("opus", "Warning: Both standard and multistream decoders present");
        }
    }
    
    // Check channel configuration
    if (m_channels == 0 || m_channels > 255) {
        Debug::log("opus", "Decoder state invalid: invalid channel count ", m_channels);
        return false;
    }
    
    // Check sample rate
    if (m_sample_rate != 48000) {
        Debug::log("opus", "Decoder state invalid: sample rate should be 48000, got ", m_sample_rate);
        return false;
    }
    
    // Check error state
    if (m_error_state.load()) {
        Debug::log("opus", "Decoder state invalid: error state is set");
        return false;
    }
    
    // Validate decoder state using opus_decoder_ctl
    if (m_use_multistream && m_opus_ms_decoder) {
        opus_int32 lookahead;
        int error = opus_multistream_decoder_ctl(m_opus_ms_decoder, OPUS_GET_LOOKAHEAD(&lookahead));
        if (error != OPUS_OK) {
            Debug::log("opus", "Multistream decoder state query failed: ", opus_strerror(error));
            return false;
        }
    } else if (!m_use_multistream && m_opus_decoder) {
        opus_int32 lookahead;
        int error = opus_decoder_ctl(m_opus_decoder, OPUS_GET_LOOKAHEAD(&lookahead));
        if (error != OPUS_OK) {
            Debug::log("opus", "Decoder state query failed: ", opus_strerror(error));
            return false;
        }
    }
    
    return true;
}

bool OpusCodec::recoverFromError_unlocked()
{
    Debug::log("opus", "Attempting decoder error recovery");
    
    if (!m_error_state.load()) {
        Debug::log("opus", "No error state to recover from");
        return true;
    }
    
    // Try to reset decoder state first
    if (m_decoder_initialized) {
        if (m_use_multistream && m_opus_ms_decoder) {
            int error = opus_multistream_decoder_ctl(m_opus_ms_decoder, OPUS_RESET_STATE);
            if (error == OPUS_OK) {
                Debug::log("opus", "Multistream decoder state reset successful");
                m_error_state.store(false);
                m_last_error.clear();
                return true;
            } else {
                Debug::log("opus", "Multistream decoder state reset failed: ", opus_strerror(error));
            }
        } else if (!m_use_multistream && m_opus_decoder) {
            int error = opus_decoder_ctl(m_opus_decoder, OPUS_RESET_STATE);
            if (error == OPUS_OK) {
                Debug::log("opus", "Decoder state reset successful");
                m_error_state.store(false);
                m_last_error.clear();
                return true;
            } else {
                Debug::log("opus", "Decoder state reset failed: ", opus_strerror(error));
            }
        }
    }
    
    // If state reset failed, try full reinitialization
    Debug::log("opus", "Attempting full decoder reinitialization");
    
    // Save current configuration
    uint16_t saved_channels = m_channels;
    uint16_t saved_pre_skip = m_pre_skip;
    int16_t saved_output_gain = m_output_gain;
    
    // Reset everything
    resetDecoderState_unlocked();
    
    // Restore configuration
    m_channels = saved_channels;
    m_pre_skip = saved_pre_skip;
    m_output_gain = saved_output_gain;
    m_samples_to_skip.store(m_pre_skip);
    
    // Reinitialize decoder
    if (initializeOpusDecoder_unlocked()) {
        Debug::log("opus", "Decoder recovery successful");
        m_error_state.store(false);
        m_last_error.clear();
        return true;
    } else {
        Debug::log("opus", "Decoder recovery failed");
        m_error_state.store(true);
        m_last_error = "Decoder recovery failed";
        return false;
    }
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
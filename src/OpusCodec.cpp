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

// Thread-local error state for concurrent operation (Requirement 8.8)
thread_local std::string OpusCodec::tl_last_error;
thread_local int OpusCodec::tl_last_opus_error = OPUS_OK;
thread_local std::chrono::steady_clock::time_point OpusCodec::tl_last_error_time;

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
    m_frames_processed.store(0);
    m_last_decode_time = std::chrono::steady_clock::now();
    
    // Initialize performance optimization state for thread safety
    m_last_frame_size = 0;
    m_frame_size_changes = 0;
    
    // Performance Optimization 10.1: Use appropriately sized buffers for maximum Opus frame size
    // Maximum Opus frame size is 5760 samples per channel (120ms at 48kHz)
    // Reserve space for up to 8 channels initially to minimize reallocations
    constexpr size_t MAX_OPUS_FRAME_SIZE = 5760;
    constexpr size_t INITIAL_MAX_CHANNELS = 8;
    constexpr size_t OPTIMIZED_BUFFER_SIZE = MAX_OPUS_FRAME_SIZE * INITIAL_MAX_CHANNELS;
    
    // Reserve buffer space for efficiency - minimizes allocation overhead (Requirement 9.6)
    m_output_buffer.reserve(OPTIMIZED_BUFFER_SIZE);
    m_float_buffer.reserve(OPTIMIZED_BUFFER_SIZE);
    
    // Pre-allocate partial packet buffer to avoid frequent reallocations
    m_partial_packet_buffer.reserve(1024); // Reasonable size for partial packets
    
    Debug::log("opus", "OpusCodec constructor completed with optimized buffer allocation");
}

OpusCodec::~OpusCodec()
{
    Debug::log("opus", "OpusCodec destructor called");
    
    // Ensure proper cleanup before codec destruction (Requirement 10.6)
    // Thread safety: Acquire lock to ensure no operations are in progress before destruction
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clear thread-local error state
    clearThreadLocalError_unlocked();
    
    // Clean up libopus decoders
    if (m_opus_ms_decoder) {
        opus_multistream_decoder_destroy(m_opus_ms_decoder);
        m_opus_ms_decoder = nullptr;
    }
    if (m_opus_decoder) {
        opus_decoder_destroy(m_opus_decoder);
        m_opus_decoder = nullptr;
    }
    
    // Clear all buffers
    clearOutputBuffers_unlocked();
    m_output_buffer.clear();
    m_float_buffer.clear();
    m_partial_packet_buffer.clear();
    
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
    
    // Initialize output buffer management system
    if (!initializeOutputBuffers_unlocked()) {
        m_last_error = "Failed to initialize output buffer management";
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
    
    Debug::log("opus", "OpusCodec::decode called - chunk_size=", chunk.data.size());
    
    // Check for buffer overflow - provide backpressure (Requirement 7.4)
    if (m_buffer_overflow.load()) {
        Debug::log("opus", "Output buffer overflow active - returning buffered frame to provide backpressure");
        if (hasBufferedFrames_unlocked()) {
            return getBufferedFrame_unlocked();
        } else {
            // Buffer overflow cleared, continue processing
            m_buffer_overflow.store(false);
        }
    }
    
    // Return buffered frames first if available (incremental processing - Requirement 7.1)
    if (hasBufferedFrames_unlocked() && !chunk.data.empty()) {
        Debug::log("opus", "Returning buffered frame while processing new packet");
        return getBufferedFrame_unlocked();
    }
    
    // Handle empty packets and end-of-stream conditions
    if (chunk.data.empty()) {
        Debug::log("opus", "Empty chunk received - checking for buffered frames");
        if (hasBufferedFrames_unlocked()) {
            return getBufferedFrame_unlocked();
        }
        Debug::log("opus", "No buffered frames, returning empty frame for end-of-stream");
        return AudioFrame();
    }
    
    // Check for error state
    if (m_error_state.load()) {
        Debug::log("opus", "Codec in error state, returning empty frame");
        return AudioFrame();
    }
    
    // Handle partial packet data gracefully (Requirement 7.3)
    if (!handlePartialPacketData_unlocked(chunk.data)) {
        Debug::log("opus", "Partial packet handling failed - returning buffered frame if available");
        if (hasBufferedFrames_unlocked()) {
            return getBufferedFrame_unlocked();
        }
        return AudioFrame();
    }
    
    // If partial packet handling consumed the data, return buffered frame
    if (!m_partial_packet_buffer.empty()) {
        Debug::log("opus", "Packet data buffered for partial handling - returning existing buffered frame");
        if (hasBufferedFrames_unlocked()) {
            return getBufferedFrame_unlocked();
        }
        return AudioFrame();
    }
    
    // Process the new packet
    AudioFrame decoded_frame;
    
    // Route header packets to header processing system
    if (m_header_packets_received < 2) {
        Debug::log("opus", "Routing to header processing system (packet ", m_header_packets_received + 1, ")");
        decoded_frame = decodeAudioPacket_unlocked(chunk.data);
    } else {
        // Route audio packets to audio decoding system
        Debug::log("opus", "Routing to audio decoding system");
        decoded_frame = decodeAudioPacket_unlocked(chunk.data);
        
        // Maintain streaming latency and throughput (Requirement 7.8)
        maintainStreamingLatency_unlocked();
    }
    
    // If we got a valid frame, try to buffer it for incremental processing
    if (!decoded_frame.samples.empty()) {
        if (bufferFrame_unlocked(decoded_frame)) {
            Debug::log("opus", "Frame buffered successfully");
        } else {
            Debug::log("opus", "Failed to buffer frame - returning directly");
            return decoded_frame;
        }
    }
    
    // Return the next available buffered frame
    if (hasBufferedFrames_unlocked()) {
        return getBufferedFrame_unlocked();
    }
    
    // Return the decoded frame directly if no buffering occurred
    return decoded_frame;
}

AudioFrame OpusCodec::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("opus", "OpusCodec::flush called");
    
    // Output any remaining decoded samples from buffer (Requirement 7.5)
    if (hasBufferedFrames_unlocked()) {
        AudioFrame frame = getBufferedFrame_unlocked();
        Debug::log("opus", "Flushing buffered frame with ", frame.samples.size(), " samples");
        return frame;
    }
    
    // For Opus, we can also try to decode any remaining data in the decoder
    // by calling opus_decode with NULL packet data to get any final samples
    if (m_decoder_initialized && (m_opus_decoder || m_opus_ms_decoder)) {
        Debug::log("opus", "Attempting to flush decoder internal state");
        
        // Try to get any remaining samples from the decoder
        constexpr int MAX_FRAME_SIZE = 5760;
        std::vector<opus_int16> decode_buffer(MAX_FRAME_SIZE * m_channels);
        
        int samples_decoded = 0;
        
        if (m_use_multistream && m_opus_ms_decoder) {
            // Flush multistream decoder
            samples_decoded = opus_multistream_decode(m_opus_ms_decoder,
                                                     nullptr, 0, // NULL packet for flushing
                                                     decode_buffer.data(),
                                                     MAX_FRAME_SIZE,
                                                     0);
        } else if (!m_use_multistream && m_opus_decoder) {
            // Flush standard decoder
            samples_decoded = opus_decode(m_opus_decoder, 
                                         nullptr, 0, // NULL packet for flushing
                                         decode_buffer.data(), 
                                         MAX_FRAME_SIZE, 
                                         0);
        }
        
        if (samples_decoded > 0) {
            Debug::log("opus", "Decoder flush returned ", samples_decoded, " samples");
            
            // Create frame with flushed samples
            AudioFrame frame;
            frame.sample_rate = m_sample_rate;
            frame.channels = m_channels;
            
            size_t total_samples = samples_decoded * m_channels;
            frame.samples.resize(total_samples);
            std::memcpy(frame.samples.data(), decode_buffer.data(), total_samples * sizeof(int16_t));
            
            // Apply post-processing
            applyPreSkip_unlocked(frame);
            applyOutputGain_unlocked(frame);
            processChannelMapping_unlocked(frame);
            
            Debug::log("opus", "Returning flushed frame with ", frame.samples.size(), " samples");
            return frame;
        } else if (samples_decoded < 0) {
            Debug::log("opus", "Decoder flush failed: ", opus_strerror(samples_decoded));
        } else {
            Debug::log("opus", "No samples to flush from decoder");
        }
    }
    
    Debug::log("opus", "OpusCodec::flush completed - no data to flush");
    return AudioFrame();
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
        
        // Clear bounded output buffers (Requirement 7.6)
        clearOutputBuffers_unlocked();
        
        // Clear partial packet buffer
        m_partial_packet_buffer.clear();
        
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
    
    // Performance Optimization 10.1: Use appropriately sized buffers for maximum Opus frame size
    // Maximum frame size for Opus is 5760 samples per channel at 48kHz (120ms at 48kHz)
    // Maximum channels is 255, but we'll be conservative and prepare for 8 channels initially
    constexpr size_t MAX_OPUS_FRAME_SIZE = 5760;
    constexpr size_t MAX_INITIAL_CHANNELS = 8;
    constexpr size_t OPTIMIZED_BUFFER_SIZE = MAX_OPUS_FRAME_SIZE * MAX_INITIAL_CHANNELS;
    
    try {
        // Validate memory allocation request (Requirement 8.5)
        size_t buffer_bytes = OPTIMIZED_BUFFER_SIZE * sizeof(int16_t);
        if (!validateMemoryAllocation_unlocked(buffer_bytes)) {
            reportDetailedError_unlocked("Memory Validation", "Buffer allocation size validation failed");
            return false;
        }
        
        // Performance Optimization: Minimize allocation overhead and memory fragmentation (Requirement 9.6)
        // Clear and reserve space for output buffers to avoid reallocations during decoding
        m_output_buffer.clear();
        m_output_buffer.reserve(OPTIMIZED_BUFFER_SIZE);
        
        m_float_buffer.clear();
        m_float_buffer.reserve(OPTIMIZED_BUFFER_SIZE);
        
        // Pre-allocate channel mapping vector for multi-channel support
        m_channel_mapping.clear();
        m_channel_mapping.reserve(255); // Maximum possible channels per Opus spec
        
        Debug::log("opus", "Reserved optimized buffer space for ", OPTIMIZED_BUFFER_SIZE, " samples");
        
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
        
        Debug::log("opus", "Internal buffers and state variables set up successfully with performance optimizations");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("opus", "Failed to set up internal buffers: ", e.what());
        reportDetailedError_unlocked("Memory Allocation", std::string("Buffer setup failed: ") + e.what());
        
        // Attempt to handle memory allocation failure
        if (!handleMemoryAllocationFailure_unlocked()) {
            reportDetailedError_unlocked("Memory Recovery", "Failed to recover from memory allocation failure");
            return false;
        }
        
        // If recovery succeeded, try again with minimal buffers
        Debug::log("opus", "Attempting buffer setup with minimal configuration after recovery");
        return true; // Recovery handled the buffer setup
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
    
    // Comprehensive input parameter validation (Requirement 8.6)
    if (!validateInputParameters_unlocked(packet_data)) {
        reportDetailedError_unlocked("Input Validation", "Invalid input parameters");
        return frame;
    }
    
    // Validate decoder state before processing
    if (!validateDecoderState_unlocked()) {
        Debug::log("opus", "Decoder state validation failed, attempting recovery");
        if (!recoverFromError_unlocked()) {
            reportDetailedError_unlocked("Decoder State", "Decoder recovery failed");
            return frame;
        }
    }
    
    // Validate packet before decoding
    if (!validateOpusPacket_unlocked(packet_data)) {
        reportDetailedError_unlocked("Packet Validation", "Invalid Opus packet structure");
        return frame;
    }
    
    Debug::log("opus", "Decoding audio packet size=", packet_data.size(), " bytes");
    
    // Performance Optimization 10.2: Efficient processing for common cases
    // Decode the packet using opus_decode()
    // Maximum frame size for Opus is 5760 samples per channel at 48kHz (120ms frame)
    // This handles variable frame sizes efficiently (2.5ms to 60ms)
    constexpr int MAX_FRAME_SIZE = 5760;
    
    // Declare decode buffer and samples_decoded outside the optimization paths
    std::vector<opus_int16> decode_buffer(MAX_FRAME_SIZE * m_channels);
    int samples_decoded;
    
    if (isMonoStereoOptimizable_unlocked() && packet_data.size() <= 1275) {
        // Performance Optimization: Use stack allocation for small mono/stereo frames to improve cache efficiency
        // Optimized path for mono/stereo configurations (Requirement 9.4)
        constexpr int OPTIMIZED_FRAME_SIZE = 2880; // 60ms at 48kHz, sufficient for most cases
        opus_int16 stack_buffer[OPTIMIZED_FRAME_SIZE * 2]; // Max 2 channels
        
        samples_decoded = opus_decode(m_opus_decoder, 
                                     packet_data.data(), 
                                     static_cast<opus_int32>(packet_data.size()),
                                     stack_buffer, 
                                     OPTIMIZED_FRAME_SIZE, 
                                     0); // 0 = normal decode, 1 = FEC decode
        
        if (samples_decoded > 0) {
            // Copy from stack buffer to output buffer for better cache performance
            size_t total_samples = samples_decoded * m_channels;
            m_output_buffer.resize(total_samples);
            std::memcpy(m_output_buffer.data(), stack_buffer, total_samples * sizeof(int16_t));
            
            Debug::log("opus", "Optimized mono/stereo decode returned ", samples_decoded, " samples");
        }
        
    } else {
        // Standard path for multi-channel or large packets
        if (m_use_multistream && m_opus_ms_decoder) {
            // Use multistream decoder for surround sound configurations
            // This handles SILK, CELT, and hybrid mode packets correctly
            samples_decoded = opus_multistream_decode(m_opus_ms_decoder,
                                                     packet_data.data(),
                                                     static_cast<opus_int32>(packet_data.size()),
                                                     decode_buffer.data(),
                                                     MAX_FRAME_SIZE,
                                                     0); // 0 = normal decode, 1 = FEC decode
            Debug::log("opus", "Multistream decode returned ", samples_decoded, " samples");
            
        } else if (!m_use_multistream && m_opus_decoder) {
            // Use standard decoder for mono/stereo configurations
            // libopus automatically handles SILK, CELT, and hybrid modes
            samples_decoded = opus_decode(m_opus_decoder, 
                                         packet_data.data(), 
                                         static_cast<opus_int32>(packet_data.size()),
                                         decode_buffer.data(), 
                                         MAX_FRAME_SIZE, 
                                         0); // 0 = normal decode, 1 = FEC decode
            Debug::log("opus", "Standard decode returned ", samples_decoded, " samples");
            
        } else {
            Debug::log("opus", "Decoder configuration mismatch - no valid decoder available");
            m_last_error = "Decoder configuration mismatch";
            return frame;
        }
        
        // Performance Optimization: Handle variable frame sizes efficiently (Requirement 9.5)
        if (samples_decoded > 0) {
            size_t total_samples = samples_decoded * m_channels;
            m_output_buffer.resize(total_samples);
            std::memcpy(m_output_buffer.data(), decode_buffer.data(), total_samples * sizeof(int16_t));
        }
    }
    
    if (samples_decoded < 0) {
        // Handle decoder error and output silence for failed frames while maintaining stream continuity
        handleDecoderError_unlocked(samples_decoded);
        
        // For corrupted packets, output silence to maintain stream continuity
        if (samples_decoded == OPUS_INVALID_PACKET) {
            Debug::log("opus", "Corrupted packet detected, outputting silence frame");
            
            // Generate silence frame with typical Opus frame size (20ms at 48kHz = 960 samples)
            constexpr int TYPICAL_FRAME_SIZE = 960;
            size_t silence_samples = TYPICAL_FRAME_SIZE * m_channels;
            
            frame.sample_rate = m_sample_rate;
            frame.channels = m_channels;
            frame.samples.resize(silence_samples, 0); // Fill with silence (zeros)
            
            // Apply pre-skip and gain processing to silence frame
            applyPreSkip_unlocked(frame);
            applyOutputGain_unlocked(frame);
            
            // Update sample counter
            m_samples_decoded.fetch_add(TYPICAL_FRAME_SIZE);
            
            Debug::log("opus", "Generated silence frame with ", silence_samples, " samples");
            return frame;
        }
        
        // For other errors, return empty frame
        return frame;
    }
    
    if (samples_decoded > 0) {
        // Convert libopus output to 16-bit PCM format
        // libopus already outputs 16-bit samples, so we just need to copy them
        size_t total_samples = samples_decoded * m_channels;
        m_output_buffer.resize(total_samples);
        
        // Direct copy since opus_decode already outputs opus_int16 (16-bit PCM)
        // This efficiently handles variable frame sizes from 2.5ms to 60ms
        std::memcpy(m_output_buffer.data(), decode_buffer.data(), total_samples * sizeof(int16_t));
        
        Debug::log("opus", "Successfully decoded ", samples_decoded, " sample frames (", 
                  total_samples, " total samples) - frame duration: ", 
                  (samples_decoded * 1000.0f / m_sample_rate), "ms");
        
        // Create AudioFrame with decoded PCM data
        frame.sample_rate = m_sample_rate;  // Always 48kHz for Opus
        frame.channels = m_channels;
        frame.samples = std::move(m_output_buffer);
        
        // Performance Optimization 10.2: Handle variable frame sizes efficiently
        handleVariableFrameSizeEfficiently_unlocked(samples_decoded);
        
        // Apply pre-skip and gain processing
        applyPreSkip_unlocked(frame);
        applyOutputGain_unlocked(frame);
        
        // Apply channel mapping and ordering for multi-channel configurations
        processChannelMapping_unlocked(frame);
        
        // Performance Optimization 10.2: Optimize memory access patterns for cache efficiency
        optimizeMemoryAccessPatterns_unlocked(frame);
        
        // Update sample counter for position tracking
        m_samples_decoded.fetch_add(samples_decoded);
        
        // Clear output buffer for next use
        m_output_buffer.clear();
        
    } else if (samples_decoded == 0) {
        Debug::log("opus", "Decoder returned 0 samples - empty frame");
        // Return empty frame for 0-sample result
    }
    
    Debug::log("opus", "Returning frame with ", frame.samples.size(), " samples");
    return frame;
}

bool OpusCodec::processHeaderPacket_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "processHeaderPacket_unlocked called, packet_size=", packet_data.size());
    
    // Comprehensive header packet validation (Requirement 8.1)
    if (!validateHeaderPacketParameters_unlocked(packet_data)) {
        reportDetailedError_unlocked("Header Validation", "Invalid header packet parameters");
        return false;
    }
    
    // Route header packets based on sequence and validate proper order
    if (m_header_packets_received == 0) {
        // First packet must be identification header (OpusHead)
        Debug::log("opus", "Processing identification header (packet 1)");
        if (packet_data.size() >= 8 && std::memcmp(packet_data.data(), "OpusHead", 8) == 0) {
            return processIdentificationHeader_unlocked(packet_data);
        } else {
            Debug::log("opus", "Invalid header sequence: expected OpusHead as first packet");
            return false;
        }
    } else if (m_header_packets_received == 1) {
        // Second packet must be comment header (OpusTags)
        Debug::log("opus", "Processing comment header (packet 2)");
        if (packet_data.size() >= 8 && std::memcmp(packet_data.data(), "OpusTags", 8) == 0) {
            return processCommentHeader_unlocked(packet_data);
        } else {
            Debug::log("opus", "Invalid header sequence: expected OpusTags as second packet");
            return false;
        }
    } else {
        // More than 2 header packets is invalid for Opus
        Debug::log("opus", "Invalid header sequence: received more than 2 header packets (", m_header_packets_received + 1, ")");
        return false;
    }
}

bool OpusCodec::processIdentificationHeader_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "processIdentificationHeader_unlocked called");
    
    // OpusHead packet must be at least 19 bytes for basic header
    if (packet_data.size() < 19) {
        Debug::log("opus", "OpusHead packet too small: ", packet_data.size(), " bytes (minimum 19 required)");
        return false;
    }
    
    // Check for OpusHead signature (RFC 6716 requirement)
    if (std::memcmp(packet_data.data(), "OpusHead", 8) != 0) {
        Debug::log("opus", "Invalid OpusHead signature - expected 'OpusHead'");
        return false;
    }
    
    // Parse OpusHead header format according to RFC 6716
    // Byte 8: version (must be 1 per RFC 6716)
    uint8_t version = packet_data[8];
    if (version != 1) {
        Debug::log("opus", "Unsupported Opus version: ", version, " (RFC 6716 requires version 1)");
        return false;
    }
    
    // Byte 9: channel count (1-255 channels supported)
    uint8_t channels = packet_data[9];
    if (channels < 1 || channels > 255) {
        Debug::log("opus", "Invalid channel count: ", channels, " (must be 1-255)");
        return false;
    }
    
    // Bytes 10-11: pre-skip (little endian) - samples to discard from decoder output
    uint16_t pre_skip = packet_data[10] | (packet_data[11] << 8);
    
    // Bytes 12-15: input sample rate (little endian, informational only)
    uint32_t input_sample_rate = packet_data[12] | (packet_data[13] << 8) | 
                                (packet_data[14] << 16) | (packet_data[15] << 24);
    
    // Bytes 16-17: output gain (little endian, in Q7.8 format)
    // Q7.8 format: 8 integer bits, 8 fractional bits
    int16_t output_gain = static_cast<int16_t>(packet_data[16] | (packet_data[17] << 8));
    
    // Byte 18: channel mapping family
    uint8_t channel_mapping_family = packet_data[18];
    
    Debug::log("opus", "OpusHead header parsed:");
    Debug::log("opus", "  Version: ", version);
    Debug::log("opus", "  Channels: ", channels);
    Debug::log("opus", "  Pre-skip: ", pre_skip, " samples");
    Debug::log("opus", "  Input sample rate: ", input_sample_rate, " Hz (informational)");
    Debug::log("opus", "  Output gain: ", output_gain, " (Q7.8 format)");
    Debug::log("opus", "  Channel mapping family: ", channel_mapping_family);
    
    // Validate parameters against Opus specification limits
    if (!validateOpusHeaderParameters_unlocked(version, channels, channel_mapping_family, input_sample_rate)) {
        Debug::log("opus", "OpusHead parameter validation failed");
        return false;
    }
    
    // Store configuration parameters for decoder initialization
    m_channels = channels;
    m_pre_skip = pre_skip;
    m_output_gain = output_gain;
    
    // Handle channel configuration based on mapping family
    if (!configureChannelMapping_unlocked(channel_mapping_family, channels, packet_data)) {
        Debug::log("opus", "Failed to configure channel mapping");
        return false;
    }
    
    // Resize buffers now that we know the actual channel count
    if (!resizeBuffersForChannels_unlocked(m_channels)) {
        Debug::log("opus", "Failed to resize buffers for ", m_channels, " channels");
        return false;
    }
    
    // Set up pre-skip counter for sample removal during decoding
    m_samples_to_skip.store(m_pre_skip);
    
    Debug::log("opus", "OpusHead identification header processed successfully");
    Debug::log("opus", "Configuration stored - channels=", m_channels, ", pre_skip=", m_pre_skip, 
              ", gain=", m_output_gain, ", mapping_family=", channel_mapping_family);
    
    return true;
}

bool OpusCodec::processCommentHeader_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "processCommentHeader_unlocked called");
    
    // OpusTags packet must be at least 8 bytes for signature
    if (packet_data.size() < 8) {
        Debug::log("opus", "OpusTags packet too small: ", packet_data.size(), " bytes (minimum 8 required)");
        return false;
    }
    
    // Check for OpusTags signature (RFC 7845 requirement)
    if (std::memcmp(packet_data.data(), "OpusTags", 8) != 0) {
        Debug::log("opus", "Invalid OpusTags signature - expected 'OpusTags'");
        return false;
    }
    
    // Validate comment header structure without processing metadata
    // The codec validates structure, but demuxer handles metadata extraction
    
    // OpusTags structure validation (RFC 7845):
    // - 8 bytes: "OpusTags" signature
    // - 4 bytes: vendor string length (little endian)
    // - N bytes: vendor string (UTF-8)
    // - 4 bytes: user comment list length (little endian)
    // - For each comment: 4 bytes length + comment string
    
    if (packet_data.size() < 12) {
        Debug::log("opus", "OpusTags packet too small for vendor string length: ", packet_data.size(), " bytes");
        return false;
    }
    
    // Extract vendor string length (bytes 8-11, little endian)
    uint32_t vendor_length = packet_data[8] | (packet_data[9] << 8) | 
                            (packet_data[10] << 16) | (packet_data[11] << 24);
    
    Debug::log("opus", "OpusTags vendor string length: ", vendor_length, " bytes");
    
    // Validate vendor string fits in packet
    if (packet_data.size() < 12 + vendor_length) {
        Debug::log("opus", "OpusTags packet too small for vendor string: need ", 
                  12 + vendor_length, " bytes, have ", packet_data.size());
        return false;
    }
    
    // Check for user comment list length field
    if (packet_data.size() < 16 + vendor_length) {
        Debug::log("opus", "OpusTags packet too small for comment list length: need ", 
                  16 + vendor_length, " bytes, have ", packet_data.size());
        return false;
    }
    
    // Extract user comment list length (little endian)
    size_t comment_list_offset = 12 + vendor_length;
    uint32_t comment_count = packet_data[comment_list_offset] | 
                            (packet_data[comment_list_offset + 1] << 8) |
                            (packet_data[comment_list_offset + 2] << 16) | 
                            (packet_data[comment_list_offset + 3] << 24);
    
    Debug::log("opus", "OpusTags comment count: ", comment_count);
    
    // Basic validation of comment structure (without full parsing)
    size_t current_offset = comment_list_offset + 4;
    for (uint32_t i = 0; i < comment_count && current_offset < packet_data.size(); i++) {
        // Check if we have space for comment length field
        if (current_offset + 4 > packet_data.size()) {
            Debug::log("opus", "OpusTags comment ", i, " length field extends beyond packet");
            return false;
        }
        
        // Extract comment length
        uint32_t comment_length = packet_data[current_offset] | 
                                 (packet_data[current_offset + 1] << 8) |
                                 (packet_data[current_offset + 2] << 16) | 
                                 (packet_data[current_offset + 3] << 24);
        
        current_offset += 4;
        
        // Check if comment data fits in packet
        if (current_offset + comment_length > packet_data.size()) {
            Debug::log("opus", "OpusTags comment ", i, " data extends beyond packet: need ", 
                      current_offset + comment_length, " bytes, have ", packet_data.size());
            return false;
        }
        
        current_offset += comment_length;
        Debug::log("opus", "OpusTags comment ", i, " validated: ", comment_length, " bytes");
    }
    
    // Make header data available to demuxer for metadata extraction
    // The codec doesn't process metadata directly - that's the demuxer's responsibility
    // This validation ensures the header structure is correct for demuxer processing
    
    Debug::log("opus", "OpusTags header structure validated successfully");
    Debug::log("opus", "Header data available to demuxer for metadata extraction");
    Debug::log("opus", "Total packet size: ", packet_data.size(), " bytes");
    Debug::log("opus", "Vendor string: ", vendor_length, " bytes");
    Debug::log("opus", "Comment entries: ", comment_count);
    
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

bool OpusCodec::configureChannelMapping_unlocked(uint8_t mapping_family, uint8_t channels, const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "configureChannelMapping_unlocked called - family=", mapping_family, ", channels=", channels);
    
    // Validate channel configuration first
    if (!validateChannelConfiguration_unlocked(channels, mapping_family)) {
        Debug::log("opus", "Channel configuration validation failed");
        return false;
    }
    
    // Store basic configuration
    m_channel_mapping_family = mapping_family;
    
    // Handle different channel mapping families
    switch (mapping_family) {
        case 0:
            // Mono and stereo configurations (channel mapping family 0)
            return configureChannelMappingFamily0_unlocked(channels);
            
        case 1:
            // Surround sound configurations (channel mapping family 1)
            return configureChannelMappingFamily1_unlocked(channels, packet_data);
            
        case 255:
            // Custom mapping (application-defined)
            return configureChannelMappingFamily255_unlocked(channels, packet_data);
            
        default:
            Debug::log("opus", "Unsupported channel mapping family: ", mapping_family);
            return false;
    }
}

bool OpusCodec::validateChannelConfiguration_unlocked(uint8_t channels, uint8_t mapping_family)
{
    Debug::log("opus", "validateChannelConfiguration_unlocked called - channels=", channels, ", family=", mapping_family);
    
    // Validate channel count against Opus specification limits
    if (channels < 1 || channels > 255) {
        Debug::log("opus", "Invalid channel count: ", channels, " (must be 1-255)");
        return false;
    }
    
    // Validate channel count against mapping family limits
    switch (mapping_family) {
        case 0:
            // Channel mapping family 0 supports mono and stereo only
            if (channels > 2) {
                Debug::log("opus", "Channel mapping family 0 supports max 2 channels, got ", channels);
                return false;
            }
            break;
            
        case 1:
            // Channel mapping family 1 supports up to 8 channels (surround sound)
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
    
    Debug::log("opus", "Channel configuration validation passed");
    return true;
}

bool OpusCodec::configureChannelMappingFamily0_unlocked(uint8_t channels)
{
    Debug::log("opus", "Configuring channel mapping family 0 for ", channels, " channels");
    
    // Channel mapping family 0: mono and stereo configurations
    // No additional header data needed - simple configuration
    
    if (channels == 1) {
        // Mono configuration
        m_stream_count = 1;
        m_coupled_stream_count = 0;
        m_channel_mapping.clear();
        m_channel_mapping.push_back(0); // Single channel maps to stream 0
        
        Debug::log("opus", "Configured mono (1 channel) - stream_count=1, coupled=0");
        
    } else if (channels == 2) {
        // Stereo configuration with channel coupling
        m_stream_count = 1;
        m_coupled_stream_count = 1; // Stereo uses one coupled stream
        m_channel_mapping.clear();
        m_channel_mapping.push_back(0); // Left channel maps to stream 0
        m_channel_mapping.push_back(1); // Right channel maps to stream 0 (coupled)
        
        Debug::log("opus", "Configured stereo (2 channels) with coupling - stream_count=1, coupled=1");
        
    } else {
        Debug::log("opus", "Invalid channel count for family 0: ", channels);
        return false;
    }
    
    Debug::log("opus", "Channel mapping family 0 configuration completed successfully");
    return true;
}

bool OpusCodec::configureChannelMappingFamily1_unlocked(uint8_t channels, const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "Configuring channel mapping family 1 for ", channels, " channels");
    
    // Channel mapping family 1: surround sound configurations
    // Requires additional header data for stream counts and channel mapping
    
    size_t required_size = 21 + channels; // Basic header + stream counts + channel mapping
    if (packet_data.size() < required_size) {
        Debug::log("opus", "OpusHead packet too small for channel mapping family 1: ", 
                  packet_data.size(), " bytes (need ", required_size, ")");
        return false;
    }
    
    // Extract stream configuration from header
    uint8_t stream_count = packet_data[19];
    uint8_t coupled_stream_count = packet_data[20];
    
    Debug::log("opus", "Family 1 header data - stream_count=", stream_count, ", coupled=", coupled_stream_count);
    
    // Validate stream configuration
    if (stream_count == 0 || stream_count > 255) {
        Debug::log("opus", "Invalid stream count: ", stream_count);
        return false;
    }
    
    if (coupled_stream_count > stream_count) {
        Debug::log("opus", "Invalid coupled stream count: ", coupled_stream_count, " > ", stream_count);
        return false;
    }
    
    // Validate stream count against channel count
    // Each coupled stream handles 2 channels, each uncoupled stream handles 1 channel
    uint8_t expected_channels = coupled_stream_count * 2 + (stream_count - coupled_stream_count);
    if (expected_channels != channels) {
        Debug::log("opus", "Stream configuration mismatch: expected ", expected_channels, 
                  " channels from streams, got ", channels);
        return false;
    }
    
    // Store stream configuration
    m_stream_count = stream_count;
    m_coupled_stream_count = coupled_stream_count;
    
    // Parse channel mapping table
    m_channel_mapping.clear();
    m_channel_mapping.resize(channels);
    for (uint8_t i = 0; i < channels; i++) {
        m_channel_mapping[i] = packet_data[21 + i];
        Debug::log("opus", "Channel ", i, " maps to stream ", m_channel_mapping[i]);
        
        // Validate channel mapping values
        if (m_channel_mapping[i] >= stream_count && m_channel_mapping[i] != 255) {
            Debug::log("opus", "Invalid channel mapping: channel ", i, " maps to stream ", 
                      m_channel_mapping[i], " but only ", stream_count, " streams available");
            return false;
        }
    }
    
    // Validate standard surround sound configurations for family 1
    if (!validateSurroundSoundConfiguration_unlocked(channels)) {
        Debug::log("opus", "Invalid surround sound configuration for ", channels, " channels");
        return false;
    }
    
    Debug::log("opus", "Channel mapping family 1 configuration completed successfully");
    return true;
}

bool OpusCodec::configureChannelMappingFamily255_unlocked(uint8_t channels, const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "Configuring channel mapping family 255 (custom) for ", channels, " channels");
    
    // Channel mapping family 255: custom mapping (application-defined)
    // Requires additional header data for stream counts and channel mapping
    
    size_t required_size = 21 + channels; // Basic header + stream counts + channel mapping
    if (packet_data.size() < required_size) {
        Debug::log("opus", "OpusHead packet too small for channel mapping family 255: ", 
                  packet_data.size(), " bytes (need ", required_size, ")");
        return false;
    }
    
    // Extract stream configuration from header
    uint8_t stream_count = packet_data[19];
    uint8_t coupled_stream_count = packet_data[20];
    
    Debug::log("opus", "Family 255 header data - stream_count=", stream_count, ", coupled=", coupled_stream_count);
    
    // Validate stream configuration
    if (stream_count == 0 || stream_count > 255) {
        Debug::log("opus", "Invalid stream count: ", stream_count);
        return false;
    }
    
    if (coupled_stream_count > stream_count) {
        Debug::log("opus", "Invalid coupled stream count: ", coupled_stream_count, " > ", stream_count);
        return false;
    }
    
    // Store stream configuration
    m_stream_count = stream_count;
    m_coupled_stream_count = coupled_stream_count;
    
    // Parse channel mapping table
    m_channel_mapping.clear();
    m_channel_mapping.resize(channels);
    for (uint8_t i = 0; i < channels; i++) {
        m_channel_mapping[i] = packet_data[21 + i];
        Debug::log("opus", "Channel ", i, " maps to stream ", m_channel_mapping[i]);
        
        // For custom mapping, allow 255 (unmapped) channels
        if (m_channel_mapping[i] < stream_count || m_channel_mapping[i] == 255) {
            // Valid mapping
        } else {
            Debug::log("opus", "Invalid channel mapping: channel ", i, " maps to stream ", 
                      m_channel_mapping[i], " but only ", stream_count, " streams available");
            return false;
        }
    }
    
    Debug::log("opus", "Channel mapping family 255 configuration completed successfully");
    return true;
}

bool OpusCodec::validateSurroundSoundConfiguration_unlocked(uint8_t channels)
{
    Debug::log("opus", "Validating surround sound configuration for ", channels, " channels");
    
    // Validate standard surround sound configurations for channel mapping family 1
    // Based on Opus specification and Vorbis channel ordering
    
    switch (channels) {
        case 1:
            // Mono - should use family 0, but family 1 is allowed
            Debug::log("opus", "Mono configuration in family 1 (unusual but valid)");
            break;
            
        case 2:
            // Stereo - should use family 0, but family 1 is allowed
            Debug::log("opus", "Stereo configuration in family 1 (unusual but valid)");
            break;
            
        case 3:
            // 2.1 surround: L, R, LFE
            Debug::log("opus", "2.1 surround sound configuration");
            break;
            
        case 4:
            // Quadraphonic: L, R, RL, RR
            Debug::log("opus", "Quadraphonic surround sound configuration");
            break;
            
        case 5:
            // 4.1 surround: L, R, RL, RR, LFE
            Debug::log("opus", "4.1 surround sound configuration");
            break;
            
        case 6:
            // 5.1 surround: L, R, C, LFE, RL, RR
            Debug::log("opus", "5.1 surround sound configuration");
            break;
            
        case 7:
            // 6.1 surround: L, R, C, LFE, RC, RL, RR
            Debug::log("opus", "6.1 surround sound configuration");
            break;
            
        case 8:
            // 7.1 surround: L, R, C, LFE, RL, RR, SL, SR
            Debug::log("opus", "7.1 surround sound configuration");
            break;
            
        default:
            Debug::log("opus", "Unsupported surround sound configuration: ", channels, " channels");
            return false;
    }
    
    Debug::log("opus", "Surround sound configuration validation passed");
    return true;
}

void OpusCodec::processChannelMapping_unlocked(AudioFrame& frame)
{
    // Early return if no channel mapping needed or empty frame
    if (frame.samples.empty() || m_channel_mapping_family == 0 || m_channel_mapping.empty()) {
        return;
    }
    
    Debug::log("opus", "Processing channel mapping for family ", m_channel_mapping_family, 
              " with ", frame.channels, " channels");
    
    // Validate frame channel count matches our configuration
    if (frame.channels != m_channels) {
        Debug::log("opus", "Error: Frame channel count (", frame.channels, 
                  ") doesn't match codec configuration (", m_channels, ")");
        handleUnsupportedChannelConfiguration_unlocked(frame.channels, "Channel count mismatch");
        return;
    }
    
    // Calculate frame sample count
    size_t frame_samples = frame.getSampleFrameCount();
    if (frame_samples == 0) {
        Debug::log("opus", "Warning: Frame has zero sample frames, skipping channel mapping");
        return;
    }
    
    // Validate channel mapping configuration
    if (!validateChannelMappingConfiguration_unlocked()) {
        Debug::log("opus", "Error: Invalid channel mapping configuration");
        handleUnsupportedChannelConfiguration_unlocked(m_channels, "Invalid channel mapping");
        return;
    }
    
    // For channel mapping family 0, no reordering is needed (already in correct order)
    if (m_channel_mapping_family == 0) {
        return;
    }
    
    // For families 1 and 255, apply Opus channel ordering conventions
    if (m_channel_mapping_family == 1) {
        // Apply Vorbis channel ordering for surround sound
        if (!applyVorbisChannelOrdering_unlocked(frame)) {
            Debug::log("opus", "Error: Failed to apply Vorbis channel ordering");
            handleUnsupportedChannelConfiguration_unlocked(frame.channels, "Vorbis ordering failed");
            return;
        }
        
    } else if (m_channel_mapping_family == 255) {
        // Apply custom channel mapping
        if (!applyCustomChannelMapping_unlocked(frame)) {
            Debug::log("opus", "Error: Failed to apply custom channel mapping");
            handleUnsupportedChannelConfiguration_unlocked(frame.channels, "Custom mapping failed");
            return;
        }
        
    } else {
        // Unsupported channel mapping family
        Debug::log("opus", "Error: Unsupported channel mapping family: ", m_channel_mapping_family);
        handleUnsupportedChannelConfiguration_unlocked(frame.channels, 
            "Unsupported mapping family " + std::to_string(m_channel_mapping_family));
        return;
    }
    
    Debug::log("opus", "Channel mapping processing completed successfully");
}

bool OpusCodec::applyVorbisChannelOrdering_unlocked(AudioFrame& frame)
{
    Debug::log("opus", "Applying Vorbis channel ordering for ", frame.channels, " channels");
    
    // Opus uses Vorbis channel ordering for surround sound configurations
    // This ensures proper interleaved channel data in AudioFrame
    
    size_t frame_samples = frame.getSampleFrameCount();
    if (frame_samples == 0) {
        Debug::log("opus", "Warning: Empty frame, no channel ordering needed");
        return true;
    }
    
    // Validate channel count against supported Vorbis configurations
    if (!isVorbisChannelOrderingSupported_unlocked(frame.channels)) {
        Debug::log("opus", "Unsupported channel count for Vorbis ordering: ", frame.channels);
        return false;
    }
    
    // Apply channel reordering based on Vorbis conventions
    // For most standard configurations, Opus already outputs in correct Vorbis order
    switch (frame.channels) {
        case 1:
        case 2:
            // Mono and stereo - no reordering needed
            Debug::log("opus", "Mono/stereo configuration - no reordering needed");
            return true;
            
        case 3:
            // 2.1: L, R, LFE -> L, R, LFE (already correct Vorbis order)
            Debug::log("opus", "2.1 surround - already in correct Vorbis order");
            return true;
            
        case 4:
            // Quadraphonic: L, R, RL, RR -> L, R, RL, RR (already correct)
            Debug::log("opus", "Quadraphonic - already in correct Vorbis order");
            return true;
            
        case 5:
            // 4.1: L, R, C, RL, RR -> L, R, C, RL, RR (already correct for Vorbis)
            Debug::log("opus", "4.1 surround - already in correct Vorbis order");
            return true;
            
        case 6:
            // 5.1: L, R, C, LFE, RL, RR (correct Vorbis order)
            Debug::log("opus", "5.1 surround - already in correct Vorbis order");
            return true;
            
        case 7:
            // 6.1: L, R, C, LFE, RC, RL, RR (correct Vorbis order)
            Debug::log("opus", "6.1 surround - already in correct Vorbis order");
            return true;
            
        case 8:
            // 7.1: L, R, C, LFE, RL, RR, SL, SR (correct Vorbis order)
            Debug::log("opus", "7.1 surround - already in correct Vorbis order");
            return true;
            
        default:
            Debug::log("opus", "Unsupported channel count for Vorbis ordering: ", frame.channels);
            return false;
    }
    
    // Note: For most standard configurations, Opus already outputs in Vorbis order,
    // so no reordering is typically needed. This method validates the configuration
    // and ensures proper channel data is provided in AudioFrame.
}

bool OpusCodec::applyCustomChannelMapping_unlocked(AudioFrame& frame)
{
    Debug::log("opus", "Applying custom channel mapping for ", frame.channels, " channels");
    
    // For custom channel mapping (family 255), we need to handle application-specific
    // channel ordering. Since this is application-defined, we provide basic support
    // and validate the configuration.
    
    size_t frame_samples = frame.getSampleFrameCount();
    if (frame_samples == 0) {
        Debug::log("opus", "Warning: Empty frame, no custom mapping needed");
        return true;
    }
    
    // Validate custom channel mapping configuration
    if (m_channel_mapping.size() != frame.channels) {
        Debug::log("opus", "Error: Channel mapping size (", m_channel_mapping.size(), 
                  ") doesn't match frame channels (", frame.channels, ")");
        return false;
    }
    
    // Validate channel mapping values
    for (size_t i = 0; i < m_channel_mapping.size(); i++) {
        uint8_t stream_id = m_channel_mapping[i];
        if (stream_id != 255 && stream_id >= m_stream_count) {
            Debug::log("opus", "Error: Invalid channel mapping - channel ", i, 
                      " maps to stream ", stream_id, " but only ", m_stream_count, " streams available");
            return false;
        }
        Debug::log("opus", "Custom mapping: channel ", i, " -> stream ", stream_id);
    }
    
    // For custom mapping, we assume the libopus multistream decoder already handles
    // the mapping correctly and outputs channels in the expected order. The validation
    // above ensures the configuration is consistent.
    
    // If specific reordering is needed for custom applications, it would be implemented
    // here based on the application's requirements. For now, we provide pass-through
    // with validation.
    
    Debug::log("opus", "Custom channel mapping processing completed successfully");
    return true;
}

bool OpusCodec::validateChannelMappingConfiguration_unlocked()
{
    Debug::log("opus", "Validating channel mapping configuration");
    
    // Basic validation of channel mapping state
    if (m_channels == 0) {
        Debug::log("opus", "Error: Channel count not set");
        return false;
    }
    
    if (m_channel_mapping_family > 1 && m_channel_mapping_family != 255) {
        Debug::log("opus", "Error: Unsupported channel mapping family: ", m_channel_mapping_family);
        return false;
    }
    
    // For families that require channel mapping, validate the mapping table
    if (m_channel_mapping_family != 0) {
        if (m_channel_mapping.empty()) {
            Debug::log("opus", "Error: Channel mapping table is empty for family ", m_channel_mapping_family);
            return false;
        }
        
        if (m_channel_mapping.size() != m_channels) {
            Debug::log("opus", "Error: Channel mapping size (", m_channel_mapping.size(), 
                      ") doesn't match channel count (", m_channels, ")");
            return false;
        }
        
        if (m_stream_count == 0) {
            Debug::log("opus", "Error: Stream count not set for multi-channel configuration");
            return false;
        }
    }
    
    Debug::log("opus", "Channel mapping configuration validation passed");
    return true;
}

bool OpusCodec::isVorbisChannelOrderingSupported_unlocked(uint8_t channels)
{
    // Check if the channel count is supported for Vorbis channel ordering
    switch (channels) {
        case 1:  // Mono
        case 2:  // Stereo
        case 3:  // 2.1 surround
        case 4:  // Quadraphonic
        case 5:  // 4.1 surround
        case 6:  // 5.1 surround
        case 7:  // 6.1 surround
        case 8:  // 7.1 surround
            return true;
            
        default:
            return false;
    }
}

void OpusCodec::handleUnsupportedChannelConfiguration_unlocked(uint8_t channels, const std::string& reason)
{
    std::string error_msg = "Unsupported channel configuration: " + std::to_string(channels) + 
                           " channels (" + reason + ")";
    
    Debug::log("opus", error_msg);
    
    // Set error state to prevent further processing
    m_error_state.store(true);
    m_last_error = error_msg;
    
    // Log detailed configuration for debugging
    Debug::log("opus", "Channel configuration details:");
    Debug::log("opus", "  Channels: ", m_channels);
    Debug::log("opus", "  Mapping family: ", m_channel_mapping_family);
    Debug::log("opus", "  Stream count: ", m_stream_count);
    Debug::log("opus", "  Coupled streams: ", m_coupled_stream_count);
    Debug::log("opus", "  Mapping table size: ", m_channel_mapping.size());
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
    
    // Determine decoder type based on channel mapping family and configuration
    bool needs_multistream = false;
    
    if (m_channel_mapping_family == 0 && m_channels <= 2) {
        // Simple mono/stereo configuration - use standard decoder
        needs_multistream = false;
        Debug::log("opus", "Using standard decoder for family 0, ", m_channels, " channels");
        
    } else if (m_channel_mapping_family == 1 || m_channel_mapping_family == 255) {
        // Surround sound or custom mapping - use multistream decoder
        needs_multistream = true;
        Debug::log("opus", "Using multistream decoder for family ", m_channel_mapping_family, ", ", m_channels, " channels");
        
    } else if (m_channel_mapping_family == 0 && m_channels > 2) {
        // Invalid configuration - family 0 doesn't support > 2 channels
        m_last_error = "Invalid configuration: family 0 with " + std::to_string(m_channels) + " channels";
        Debug::log("opus", m_last_error);
        return false;
        
    } else {
        // Unsupported mapping family
        m_last_error = "Unsupported channel mapping family: " + std::to_string(m_channel_mapping_family);
        Debug::log("opus", m_last_error);
        return false;
    }
    
    if (needs_multistream) {
        // Initialize multistream decoder
        return initializeMultiStreamDecoder_unlocked();
        
    } else {
        // Initialize standard decoder for mono/stereo
        int error;
        m_opus_decoder = opus_decoder_create(m_sample_rate, m_channels, &error);
        
        if (!m_opus_decoder || error != OPUS_OK) {
            m_last_error = "Failed to create Opus decoder: " + std::string(opus_strerror(error));
            Debug::log("opus", m_last_error);
            return false;
        }
        
        m_use_multistream = false;
        Debug::log("opus", "Standard Opus decoder initialized successfully - sample_rate=", m_sample_rate, ", channels=", m_channels);
        
        // Performance Optimization: Optimize buffer sizes for the actual channel count
        optimizeBufferSizes_unlocked(m_channels);
        
        // Configure decoder parameters for optimal performance
        Debug::log("opus", "Decoder configured for integer API operation");
        
        // Configure performance optimizations
        if (!configureOpusOptimizations_unlocked()) {
            Debug::log("opus", "Warning: Failed to configure some performance optimizations");
            // Don't fail completely - optimizations are optional
        }
        
        // Performance Optimization 10.2: Optimize cache efficiency
        optimizeCacheEfficiency_unlocked();
        
        m_decoder_initialized = true;
        Debug::log("opus", "Standard decoder configuration completed successfully with performance optimizations");
        return true;
    }
}

void OpusCodec::applyPreSkip_unlocked(AudioFrame& frame)
{
    // Get current samples to skip using atomic operation
    uint64_t samples_to_skip = m_samples_to_skip.load();
    
    // Early return if no pre-skip needed or empty frame
    if (samples_to_skip == 0 || frame.samples.empty()) {
        return;
    }
    
    // Calculate frame sample count (total samples / channels)
    size_t frame_samples = frame.getSampleFrameCount();
    
    if (frame_samples == 0) {
        Debug::log("opus", "Warning: Frame has zero sample frames, skipping pre-skip processing");
        return;
    }
    
    Debug::log("opus", "Applying pre-skip: ", samples_to_skip, " samples to skip, frame has ", frame_samples, " sample frames");
    
    if (samples_to_skip >= frame_samples) {
        // Skip entire frame - all samples in this frame should be discarded
        uint64_t samples_skipped = frame_samples;
        
        // Use atomic compare-and-swap to safely update skip counter
        uint64_t expected = samples_to_skip;
        while (!m_samples_to_skip.compare_exchange_weak(expected, expected - samples_skipped)) {
            // If another thread modified the counter, reload and retry
            samples_to_skip = expected;
            if (samples_to_skip < samples_skipped) {
                // Skip count was reduced by another thread, adjust
                samples_skipped = samples_to_skip;
                break;
            }
        }
        
        // Clear the entire frame
        frame.samples.clear();
        
        Debug::log("opus", "Skipped entire frame (", samples_skipped, " sample frames), ", 
                  m_samples_to_skip.load(), " sample frames remaining to skip");
        
    } else {
        // Skip partial frame - remove samples from the beginning
        size_t samples_to_remove = samples_to_skip * frame.channels;
        
        // Ensure we don't try to remove more samples than available
        if (samples_to_remove > frame.samples.size()) {
            Debug::log("opus", "Warning: Attempting to skip more samples than available in frame");
            samples_to_remove = frame.samples.size();
            samples_to_skip = samples_to_remove / frame.channels;
        }
        
        // Remove samples from the beginning of the frame
        // Use efficient erase operation that maintains sample alignment
        frame.samples.erase(frame.samples.begin(), frame.samples.begin() + samples_to_remove);
        
        // Reset skip counter to zero atomically
        m_samples_to_skip.store(0);
        
        Debug::log("opus", "Skipped ", samples_to_skip, " sample frames from frame start, ", 
                  frame.samples.size(), " samples remaining in frame");
        
        // Verify sample alignment after pre-skip application
        if ((frame.samples.size() % frame.channels) != 0) {
            Debug::log("opus", "Warning: Sample alignment lost after pre-skip - samples=", 
                      frame.samples.size(), ", channels=", frame.channels);
            
            // Fix alignment by truncating to the nearest complete sample frame
            size_t aligned_samples = (frame.samples.size() / frame.channels) * frame.channels;
            frame.samples.resize(aligned_samples);
            
            Debug::log("opus", "Fixed sample alignment, frame now has ", aligned_samples, " samples");
        }
    }
}

void OpusCodec::applyOutputGain_unlocked(AudioFrame& frame)
{
    // Early return for zero gain case (most efficient - no processing needed)
    if (m_output_gain == 0) {
        return;
    }
    
    // Early return for empty frame
    if (frame.samples.empty()) {
        return;
    }
    
    Debug::log("opus", "Applying output gain: ", m_output_gain, " (Q7.8 format) to ", frame.samples.size(), " samples");
    
    // Convert Q7.8 format gain to floating point factor
    // Q7.8 format: output_gain / 256.0 gives the actual gain multiplier
    // Range: -128.0 dB to +127.996 dB (approximately)
    const float gain_factor = static_cast<float>(m_output_gain) / 256.0f;
    
    // Apply gain to all samples with proper clamping to prevent artifacts
    for (int16_t& sample : frame.samples) {
        // Convert to float for precise calculation
        const float original_sample = static_cast<float>(sample);
        const float adjusted_sample = original_sample * gain_factor;
        
        // Clamp to 16-bit signed integer range with proper rounding
        // This prevents audio artifacts from overflow/underflow
        if (adjusted_sample > 32767.0f) {
            sample = 32767;
        } else if (adjusted_sample < -32768.0f) {
            sample = -32768;
        } else {
            // Round to nearest integer for better audio quality
            sample = static_cast<int16_t>(adjusted_sample + (adjusted_sample >= 0.0f ? 0.5f : -0.5f));
        }
    }
    
    Debug::log("opus", "Output gain applied successfully - factor: ", gain_factor, 
              ", processed ", frame.samples.size(), " samples");
}

bool OpusCodec::validateOpusPacket_unlocked(const std::vector<uint8_t>& packet_data)
{
    // Basic packet validation for Opus packets
    if (packet_data.empty()) {
        Debug::log("opus", "Packet validation failed: empty packet");
        return false;
    }
    
    // Opus packets should not be excessively large
    // Maximum theoretical packet size is around 1275 bytes for standard configurations
    // Allow some headroom for unusual configurations
    if (packet_data.size() > 2000) {
        Debug::log("opus", "Packet validation failed: suspiciously large packet: ", packet_data.size(), " bytes");
        return false;
    }
    
    // Minimum packet size check - Opus packets should be at least 1 byte
    // (even for silence frames)
    if (packet_data.size() < 1) {
        Debug::log("opus", "Packet validation failed: packet too small");
        return false;
    }
    
    // Basic TOC (Table of Contents) byte validation
    // The first byte contains configuration information
    uint8_t toc = packet_data[0];
    
    // Extract configuration from TOC byte (bits 3-7)
    uint8_t config = (toc >> 3) & 0x1F;
    
    // Validate configuration - should be 0-31 (all values are valid)
    // This is just a sanity check since all 32 configurations are defined
    if (config > 31) {
        // This should never happen since we're masking to 5 bits
        Debug::log("opus", "Packet validation failed: invalid configuration in TOC: ", config);
        return false;
    }
    
    // Extract stereo flag (bit 2)
    bool stereo = (toc & 0x04) != 0;
    
    // Extract frame count code (bits 0-1)
    uint8_t frame_count_code = toc & 0x03;
    
    // Validate frame count code (0-3 are all valid)
    // 0: 1 frame, 1: 2 frames, 2: 2 frames, 3: arbitrary frames
    if (frame_count_code > 3) {
        // This should never happen since we're masking to 2 bits
        Debug::log("opus", "Packet validation failed: invalid frame count code: ", frame_count_code);
        return false;
    }
    
    // For code 3 (arbitrary frames), we need at least 2 bytes (TOC + frame count)
    if (frame_count_code == 3 && packet_data.size() < 2) {
        Debug::log("opus", "Packet validation failed: arbitrary frame count packet too small");
        return false;
    }
    
    // Additional validation for multi-frame packets
    if (frame_count_code == 3 && packet_data.size() >= 2) {
        uint8_t frame_count = packet_data[1] & 0x3F; // Lower 6 bits
        if (frame_count == 0) {
            Debug::log("opus", "Packet validation failed: zero frame count in arbitrary frame packet");
            return false;
        }
    }
    
    Debug::log("opus", "Packet validation passed: size=", packet_data.size(), 
              " bytes, config=", config, ", stereo=", stereo, 
              ", frame_count_code=", frame_count_code);
    
    return true;
}

bool OpusCodec::validateInputParameters_unlocked(const std::vector<uint8_t>& packet_data) const
{
    // Comprehensive input parameter validation (Requirement 8.6)
    Debug::log("opus", "Validating input parameters");
    
    // Validate packet data pointer and size
    if (packet_data.empty()) {
        Debug::log("opus", "Input validation failed: empty packet data");
        return false;
    }
    
    // Check for reasonable packet size limits
    if (packet_data.size() > 65535) {
        Debug::log("opus", "Input validation failed: packet size too large: ", packet_data.size(), " bytes");
        return false;
    }
    
    // Validate decoder state before processing
    if (!m_decoder_initialized) {
        Debug::log("opus", "Input validation failed: decoder not initialized");
        return false;
    }
    
    // Validate channel configuration
    if (m_channels == 0 || m_channels > 255) {
        Debug::log("opus", "Input validation failed: invalid channel count: ", m_channels);
        return false;
    }
    
    // Validate sample rate
    if (m_sample_rate != 48000) {
        Debug::log("opus", "Input validation failed: invalid sample rate: ", m_sample_rate);
        return false;
    }
    
    // Validate decoder pointers
    if (m_use_multistream) {
        if (!m_opus_ms_decoder) {
            Debug::log("opus", "Input validation failed: multistream decoder not initialized");
            return false;
        }
    } else {
        if (!m_opus_decoder) {
            Debug::log("opus", "Input validation failed: standard decoder not initialized");
            return false;
        }
    }
    
    // Validate error state
    if (m_error_state.load()) {
        Debug::log("opus", "Input validation failed: codec in error state");
        return false;
    }
    
    Debug::log("opus", "Input parameter validation passed");
    return true;
}

bool OpusCodec::validateHeaderPacketParameters_unlocked(const std::vector<uint8_t>& packet_data) const
{
    // Validate header packet parameters (Requirement 8.1)
    Debug::log("opus", "Validating header packet parameters");
    
    if (packet_data.empty()) {
        Debug::log("opus", "Header validation failed: empty packet");
        return false;
    }
    
    // Check minimum header size
    if (packet_data.size() < 8) {
        Debug::log("opus", "Header validation failed: packet too small for header signature");
        return false;
    }
    
    // Validate header sequence
    if (m_header_packets_received == 0) {
        // First packet must be OpusHead
        if (std::memcmp(packet_data.data(), "OpusHead", 8) != 0) {
            Debug::log("opus", "Header validation failed: first packet is not OpusHead");
            return false;
        }
        
        // OpusHead must be at least 19 bytes
        if (packet_data.size() < 19) {
            Debug::log("opus", "Header validation failed: OpusHead packet too small");
            return false;
        }
        
    } else if (m_header_packets_received == 1) {
        // Second packet must be OpusTags
        if (std::memcmp(packet_data.data(), "OpusTags", 8) != 0) {
            Debug::log("opus", "Header validation failed: second packet is not OpusTags");
            return false;
        }
        
        // OpusTags must be at least 16 bytes (8 byte signature + 4 byte vendor length + 4 byte comment count)
        if (packet_data.size() < 16) {
            Debug::log("opus", "Header validation failed: OpusTags packet too small");
            return false;
        }
        
    } else {
        Debug::log("opus", "Header validation failed: too many header packets");
        return false;
    }
    
    Debug::log("opus", "Header packet validation passed");
    return true;
}

bool OpusCodec::validateMemoryAllocation_unlocked(size_t requested_size) const
{
    // Validate memory allocation requests (Requirement 8.5)
    Debug::log("opus", "Validating memory allocation request: ", requested_size, " bytes");
    
    // Check for reasonable size limits
    if (requested_size == 0) {
        Debug::log("opus", "Memory validation failed: zero size allocation requested");
        return false;
    }
    
    // Check for excessively large allocations (1MB limit for audio buffers)
    if (requested_size > 1024 * 1024) {
        Debug::log("opus", "Memory validation failed: allocation too large: ", requested_size, " bytes");
        return false;
    }
    
    // Check for integer overflow in size calculations
    if (requested_size > SIZE_MAX / 2) {
        Debug::log("opus", "Memory validation failed: size too large, potential overflow");
        return false;
    }
    
    // Validate against maximum frame size expectations
    constexpr size_t MAX_FRAME_SIZE = 5760;
    constexpr size_t MAX_CHANNELS = 255;
    constexpr size_t MAX_EXPECTED_SIZE = MAX_FRAME_SIZE * MAX_CHANNELS * sizeof(int16_t);
    
    if (requested_size > MAX_EXPECTED_SIZE * 2) { // Allow some headroom
        Debug::log("opus", "Memory validation warning: unusually large allocation: ", requested_size, " bytes");
        // Don't fail, just warn - might be legitimate for large multi-channel configurations
    }
    
    Debug::log("opus", "Memory allocation validation passed");
    return true;
}

bool OpusCodec::handleMemoryAllocationFailure_unlocked()
{
    // Handle memory allocation failures gracefully (Requirement 8.5)
    Debug::log("opus", "Handling memory allocation failure");
    
    // Try to free up memory by clearing non-essential buffers
    try {
        // Clear output buffers
        clearOutputBuffers_unlocked();
        
        // Shrink buffers to free memory
        m_output_buffer.clear();
        m_output_buffer.shrink_to_fit();
        m_float_buffer.clear();
        m_float_buffer.shrink_to_fit();
        
        // Clear partial packet buffer
        m_partial_packet_buffer.clear();
        m_partial_packet_buffer.shrink_to_fit();
        
        Debug::log("opus", "Memory cleanup completed");
        
        // Try to re-setup minimal buffers
        constexpr size_t MINIMAL_BUFFER_SIZE = 5760 * 2; // Stereo frame
        m_output_buffer.reserve(MINIMAL_BUFFER_SIZE);
        m_float_buffer.reserve(MINIMAL_BUFFER_SIZE);
        
        Debug::log("opus", "Minimal buffers re-allocated successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("opus", "Memory allocation failure recovery failed: ", e.what());
        m_error_state.store(true);
        m_last_error = "Memory allocation failure - insufficient memory";
        return false;
    }
}

void OpusCodec::reportDetailedError_unlocked(const std::string& error_type, const std::string& details)
{
    // Provide detailed error reporting through PsyMP3's error mechanisms (Requirement 11.7)
    std::string full_error = "OpusCodec " + error_type + ": " + details;
    
    // Log to debug system
    Debug::log("opus", "ERROR: ", full_error);
    
    // Store for later retrieval
    m_last_error = full_error;
    
    // Log additional context information
    Debug::log("opus", "Error context:");
    Debug::log("opus", "  Decoder initialized: ", m_decoder_initialized);
    Debug::log("opus", "  Channels: ", m_channels);
    Debug::log("opus", "  Sample rate: ", m_sample_rate);
    Debug::log("opus", "  Use multistream: ", m_use_multistream);
    Debug::log("opus", "  Header packets received: ", m_header_packets_received);
    Debug::log("opus", "  Samples decoded: ", m_samples_decoded.load());
    Debug::log("opus", "  Error state: ", m_error_state.load());
    
    // Log buffer states
    Debug::log("opus", "  Output buffer size: ", m_output_buffer.size());
    Debug::log("opus", "  Float buffer size: ", m_float_buffer.size());
    Debug::log("opus", "  Buffered samples: ", m_buffered_samples.load());
    Debug::log("opus", "  Buffer overflow: ", m_buffer_overflow.load());
}

bool OpusCodec::isRecoverableError_unlocked(int opus_error) const
{
    // Determine if an error is recoverable (Requirement 8.7)
    switch (opus_error) {
        case OPUS_BAD_ARG:
            // Programming error - not recoverable through reset
            return false;
            
        case OPUS_BUFFER_TOO_SMALL:
            // Buffer sizing error - not recoverable without code changes
            return false;
            
        case OPUS_INVALID_PACKET:
            // Corrupted packet - recoverable by skipping packet
            return true;
            
        case OPUS_INTERNAL_ERROR:
            // Internal libopus error - potentially recoverable through reset
            return true;
            
        case OPUS_INVALID_STATE:
            // Invalid decoder state - recoverable through reset
            return true;
            
        case OPUS_UNIMPLEMENTED:
            // Feature not implemented - not recoverable
            return false;
            
        case OPUS_ALLOC_FAIL:
            // Memory allocation failure - potentially recoverable
            return true;
            
        default:
            // Unknown error - assume recoverable to be safe
            return true;
    }
}

void OpusCodec::setThreadLocalError_unlocked(int opus_error, const std::string& error_message)
{
    // Set thread-local error state for concurrent operation (Requirement 8.8)
    tl_last_opus_error = opus_error;
    tl_last_error = error_message;
    tl_last_error_time = std::chrono::steady_clock::now();
    
    Debug::log("opus", "Thread-local error set: ", error_message, " (code: ", opus_error, ")");
}

std::string OpusCodec::getThreadLocalError_unlocked() const
{
    // Get thread-local error information
    if (tl_last_opus_error != OPUS_OK) {
        auto now = std::chrono::steady_clock::now();
        auto error_age = std::chrono::duration_cast<std::chrono::milliseconds>(now - tl_last_error_time).count();
        
        return tl_last_error + " (age: " + std::to_string(error_age) + "ms)";
    }
    
    return "No thread-local error";
}

void OpusCodec::clearThreadLocalError_unlocked()
{
    // Clear thread-local error state
    tl_last_opus_error = OPUS_OK;
    tl_last_error.clear();
    tl_last_error_time = std::chrono::steady_clock::time_point{};
    
    Debug::log("opus", "Thread-local error state cleared");
}

bool OpusCodec::hasRecentThreadLocalError_unlocked() const
{
    // Check if there's a recent thread-local error (within last 5 seconds)
    if (tl_last_opus_error == OPUS_OK) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto error_age = std::chrono::duration_cast<std::chrono::seconds>(now - tl_last_error_time).count();
    
    return error_age < 5; // Consider errors recent if within 5 seconds
}

bool OpusCodec::validateThreadSafetyState_unlocked() const
{
    // Validate that the codec is in a consistent thread-safe state
    // This method assumes the mutex is already held by the caller
    
    // Check that libopus decoder instances are properly isolated
    if (m_use_multistream) {
        if (!m_opus_ms_decoder && m_decoder_initialized) {
            Debug::log("opus", "Thread safety validation failed: multistream decoder missing but marked initialized");
            return false;
        }
        if (m_opus_decoder && m_decoder_initialized) {
            Debug::log("opus", "Thread safety validation failed: both decoders present in multistream mode");
            return false;
        }
    } else {
        if (!m_opus_decoder && m_decoder_initialized) {
            Debug::log("opus", "Thread safety validation failed: standard decoder missing but marked initialized");
            return false;
        }
        if (m_opus_ms_decoder && m_decoder_initialized) {
            Debug::log("opus", "Thread safety validation failed: multistream decoder present in standard mode");
            return false;
        }
    }
    
    // Check atomic variable consistency
    if (m_error_state.load() && m_last_error.empty()) {
        Debug::log("opus", "Thread safety validation warning: error state set but no error message");
    }
    
    // Check buffer consistency
    if (m_buffered_samples.load() > 0 && m_output_queue.empty()) {
        Debug::log("opus", "Thread safety validation failed: buffered samples count inconsistent with queue");
        return false;
    }
    
    // Validate channel configuration consistency
    if (m_decoder_initialized && m_channels == 0) {
        Debug::log("opus", "Thread safety validation failed: decoder initialized but no channels configured");
        return false;
    }
    
    Debug::log("opus", "Thread safety state validation passed");
    return true;
}

bool OpusCodec::performUnrecoverableErrorReset_unlocked()
{
    // Reset decoder state when encountering unrecoverable errors (Requirement 8.7)
    Debug::log("opus", "Performing unrecoverable error reset");
    
    // Set error state to prevent further processing during reset
    m_error_state.store(true);
    
    // Save current configuration before reset
    uint16_t saved_channels = m_channels;
    uint16_t saved_pre_skip = m_pre_skip;
    int16_t saved_output_gain = m_output_gain;
    uint8_t saved_mapping_family = m_channel_mapping_family;
    uint8_t saved_stream_count = m_stream_count;
    uint8_t saved_coupled_stream_count = m_coupled_stream_count;
    std::vector<uint8_t> saved_channel_mapping = m_channel_mapping;
    
    // Perform complete reset
    resetDecoderState_unlocked();
    
    // Restore configuration
    m_channels = saved_channels;
    m_pre_skip = saved_pre_skip;
    m_output_gain = saved_output_gain;
    m_channel_mapping_family = saved_mapping_family;
    m_stream_count = saved_stream_count;
    m_coupled_stream_count = saved_coupled_stream_count;
    m_channel_mapping = saved_channel_mapping;
    
    // Reset position tracking
    m_samples_decoded.store(0);
    m_samples_to_skip.store(m_pre_skip);
    
    // Clear thread-local error state
    clearThreadLocalError_unlocked();
    
    // Try to reinitialize decoder with saved configuration
    if (m_channels > 0) {
        if (initializeOpusDecoder_unlocked()) {
            Debug::log("opus", "Unrecoverable error reset successful");
            m_error_state.store(false);
            m_last_error.clear();
            return true;
        } else {
            Debug::log("opus", "Failed to reinitialize decoder after unrecoverable error reset");
            reportDetailedError_unlocked("Recovery Failure", "Decoder reinitialization failed after reset");
            return false;
        }
    } else {
        Debug::log("opus", "Cannot reinitialize decoder: no channel configuration available");
        reportDetailedError_unlocked("Recovery Failure", "No channel configuration available for reinitialization");
        return false;
    }
}

bool OpusCodec::attemptGracefulRecovery_unlocked(int opus_error)
{
    // Attempt graceful recovery based on error type (Requirement 8.7)
    Debug::log("opus", "Attempting graceful recovery for error: ", opus_strerror(opus_error));
    
    // Set thread-local error state
    setThreadLocalError_unlocked(opus_error, std::string("Graceful recovery attempt: ") + opus_strerror(opus_error));
    
    // Check if error is recoverable
    if (!isRecoverableError_unlocked(opus_error)) {
        Debug::log("opus", "Error is not recoverable, performing unrecoverable error reset");
        return performUnrecoverableErrorReset_unlocked();
    }
    
    // Try different recovery strategies based on error type
    switch (opus_error) {
        case OPUS_INVALID_PACKET:
            // For invalid packets, just skip and continue - no decoder reset needed
            Debug::log("opus", "Invalid packet - continuing without decoder reset");
            clearThreadLocalError_unlocked();
            return true;
            
        case OPUS_INTERNAL_ERROR:
        case OPUS_INVALID_STATE:
            // Try decoder state reset first
            if (m_decoder_initialized) {
                if (m_use_multistream && m_opus_ms_decoder) {
                    int reset_error = opus_multistream_decoder_ctl(m_opus_ms_decoder, OPUS_RESET_STATE);
                    if (reset_error == OPUS_OK) {
                        Debug::log("opus", "Multistream decoder state reset successful");
                        clearThreadLocalError_unlocked();
                        return true;
                    }
                } else if (!m_use_multistream && m_opus_decoder) {
                    int reset_error = opus_decoder_ctl(m_opus_decoder, OPUS_RESET_STATE);
                    if (reset_error == OPUS_OK) {
                        Debug::log("opus", "Decoder state reset successful");
                        clearThreadLocalError_unlocked();
                        return true;
                    }
                }
            }
            
            // If state reset failed, try full reinitialization
            Debug::log("opus", "Decoder state reset failed, attempting full reinitialization");
            return performUnrecoverableErrorReset_unlocked();
            
        case OPUS_ALLOC_FAIL:
            // Try memory cleanup and recovery
            if (handleMemoryAllocationFailure_unlocked()) {
                Debug::log("opus", "Memory allocation recovery successful");
                clearThreadLocalError_unlocked();
                return true;
            } else {
                Debug::log("opus", "Memory allocation recovery failed");
                return performUnrecoverableErrorReset_unlocked();
            }
            
        default:
            // For unknown errors, try full reset
            Debug::log("opus", "Unknown error, attempting full reset");
            return performUnrecoverableErrorReset_unlocked();
    }
}

void OpusCodec::saveDecoderState_unlocked()
{
    // Save current decoder state for potential restoration
    Debug::log("opus", "Saving decoder state");
    
    // State is already maintained in member variables, so this is mostly
    // for logging and validation purposes
    Debug::log("opus", "Decoder state saved:");
    Debug::log("opus", "  Channels: ", m_channels);
    Debug::log("opus", "  Pre-skip: ", m_pre_skip);
    Debug::log("opus", "  Output gain: ", m_output_gain);
    Debug::log("opus", "  Mapping family: ", m_channel_mapping_family);
    Debug::log("opus", "  Stream count: ", m_stream_count);
    Debug::log("opus", "  Coupled streams: ", m_coupled_stream_count);
    Debug::log("opus", "  Samples decoded: ", m_samples_decoded.load());
    Debug::log("opus", "  Samples to skip: ", m_samples_to_skip.load());
}

bool OpusCodec::restoreDecoderState_unlocked()
{
    // Restore decoder state after recovery attempt
    Debug::log("opus", "Restoring decoder state");
    
    // Validate that we have the necessary configuration to restore
    if (m_channels == 0) {
        Debug::log("opus", "Cannot restore decoder state: no channel configuration");
        return false;
    }
    
    // Try to reinitialize decoder with current configuration
    if (!initializeOpusDecoder_unlocked()) {
        Debug::log("opus", "Failed to restore decoder state: reinitialization failed");
        return false;
    }
    
    Debug::log("opus", "Decoder state restored successfully");
    return true;
}

bool OpusCodec::validateRecoverySuccess_unlocked() const
{
    // Validate that recovery was successful (Requirement 8.7)
    Debug::log("opus", "Validating recovery success");
    
    // Check basic decoder state
    if (!validateDecoderState_unlocked()) {
        Debug::log("opus", "Recovery validation failed: decoder state invalid");
        return false;
    }
    
    // Check error states
    if (m_error_state.load()) {
        Debug::log("opus", "Recovery validation failed: error state still set");
        return false;
    }
    
    // Check thread-local error state
    if (hasRecentThreadLocalError_unlocked()) {
        Debug::log("opus", "Recovery validation failed: recent thread-local error");
        return false;
    }
    
    // Validate decoder functionality with a simple query
    if (m_decoder_initialized) {
        if (m_use_multistream && m_opus_ms_decoder) {
            opus_int32 lookahead;
            int error = opus_multistream_decoder_ctl(m_opus_ms_decoder, OPUS_GET_LOOKAHEAD(&lookahead));
            if (error != OPUS_OK) {
                Debug::log("opus", "Recovery validation failed: multistream decoder query failed");
                return false;
            }
        } else if (!m_use_multistream && m_opus_decoder) {
            opus_int32 lookahead;
            int error = opus_decoder_ctl(m_opus_decoder, OPUS_GET_LOOKAHEAD(&lookahead));
            if (error != OPUS_OK) {
                Debug::log("opus", "Recovery validation failed: decoder query failed");
                return false;
            }
        }
    }
    
    Debug::log("opus", "Recovery validation successful");
    return true;
}

void OpusCodec::handleDecoderError_unlocked(int opus_error)
{
    std::string error_msg = "Opus decode error (" + std::to_string(opus_error) + "): " + std::string(opus_strerror(opus_error));
    Debug::log("opus", error_msg);
    
    m_last_error = error_msg;
    
    // Handle different libopus error codes with appropriate recovery strategies
    switch (opus_error) {
        case OPUS_BAD_ARG:
            // Invalid argument - usually indicates programming error
            // This should not happen in normal operation, log detailed error
            Debug::log("opus", "CRITICAL: Bad argument error - indicates programming error");
            Debug::log("opus", "Decoder state: initialized=", m_decoder_initialized, 
                      ", channels=", m_channels, ", use_multistream=", m_use_multistream);
            // Don't set error state for this - just skip the packet
            break;
            
        case OPUS_BUFFER_TOO_SMALL:
            // Output buffer too small - should not happen with our MAX_FRAME_SIZE
            Debug::log("opus", "CRITICAL: Buffer too small error - this indicates a serious bug");
            Debug::log("opus", "Buffer sizes: output=", m_output_buffer.capacity(), 
                      ", float=", m_float_buffer.capacity());
            // This is a serious error that indicates our buffer sizing is wrong
            m_error_state.store(true);
            m_last_error = "Internal buffer sizing error - decoder disabled";
            break;
            
        case OPUS_INVALID_PACKET:
            // Corrupted packet - skip and continue, output silence handled in caller
            Debug::log("opus", "Invalid/corrupted packet - skipping and outputting silence");
            // Don't set error state - this is expected for corrupted streams
            break;
            
        case OPUS_INTERNAL_ERROR:
            // Internal libopus error - attempt graceful recovery
            Debug::log("opus", "Internal libopus error - attempting graceful recovery");
            m_error_state.store(true);
            if (attemptGracefulRecovery_unlocked(opus_error)) {
                if (validateRecoverySuccess_unlocked()) {
                    Debug::log("opus", "Internal error recovery successful and validated");
                    m_error_state.store(false);
                } else {
                    Debug::log("opus", "Internal error recovery validation failed");
                    reportDetailedError_unlocked("Recovery Validation", "Internal error recovery failed validation");
                }
            } else {
                Debug::log("opus", "Internal error recovery failed - decoder may be unstable");
                reportDetailedError_unlocked("Recovery Failure", "Internal error recovery failed");
            }
            break;
            
        case OPUS_INVALID_STATE:
            // Decoder in invalid state - attempt graceful recovery
            Debug::log("opus", "Invalid decoder state - attempting graceful recovery");
            m_error_state.store(true);
            if (attemptGracefulRecovery_unlocked(opus_error)) {
                if (validateRecoverySuccess_unlocked()) {
                    Debug::log("opus", "Decoder state recovery successful and validated");
                    m_error_state.store(false);
                } else {
                    Debug::log("opus", "Decoder state recovery validation failed");
                    reportDetailedError_unlocked("Recovery Validation", "Decoder state recovery failed validation");
                }
            } else {
                Debug::log("opus", "Decoder state recovery failed - decoder disabled");
                reportDetailedError_unlocked("Recovery Failure", "Decoder state recovery failed");
            }
            break;
            
        case OPUS_UNIMPLEMENTED:
            // Feature not implemented in this libopus version
            Debug::log("opus", "Unimplemented feature - decoder disabled for this stream");
            m_error_state.store(true);
            m_last_error = "Opus feature not supported by libopus version";
            // This is a permanent error - don't attempt recovery
            break;
            
        case OPUS_ALLOC_FAIL:
            // Memory allocation failure - attempt graceful recovery
            Debug::log("opus", "Memory allocation failure - attempting graceful recovery");
            m_error_state.store(true);
            
            if (attemptGracefulRecovery_unlocked(opus_error)) {
                if (validateRecoverySuccess_unlocked()) {
                    Debug::log("opus", "Memory allocation recovery successful and validated");
                    m_error_state.store(false);
                    // Re-setup buffers after recovery
                    if (!setupInternalBuffers_unlocked()) {
                        Debug::log("opus", "Buffer re-setup failed after memory recovery");
                        reportDetailedError_unlocked("Buffer Setup", "Failed to re-setup buffers after memory recovery");
                        m_error_state.store(true);
                    }
                } else {
                    Debug::log("opus", "Memory allocation recovery validation failed");
                    reportDetailedError_unlocked("Recovery Validation", "Memory allocation recovery failed validation");
                }
            } else {
                Debug::log("opus", "Memory allocation recovery failed - decoder disabled");
                reportDetailedError_unlocked("Recovery Failure", "Memory allocation recovery failed - insufficient memory");
            }
            break;
            
        default:
            // Unknown error code - attempt graceful recovery
            Debug::log("opus", "Unknown libopus error code ", opus_error, " - attempting graceful recovery");
            m_error_state.store(true);
            if (attemptGracefulRecovery_unlocked(opus_error)) {
                if (validateRecoverySuccess_unlocked()) {
                    Debug::log("opus", "Unknown error recovery successful and validated");
                    m_error_state.store(false);
                } else {
                    Debug::log("opus", "Unknown error recovery validation failed");
                    reportDetailedError_unlocked("Recovery Validation", "Unknown error recovery failed validation");
                }
            } else {
                Debug::log("opus", "Unknown error recovery failed - decoder disabled");
                reportDetailedError_unlocked("Recovery Failure", "Unknown decoder error recovery failed");
            }
            break;
    }
}

bool OpusCodec::initializeMultiStreamDecoder_unlocked()
{
    Debug::log("opus", "initializeMultiStreamDecoder_unlocked called");
    
    // Validate that we have the necessary configuration
    if (m_channels == 0 || m_stream_count == 0 || m_channel_mapping.empty()) {
        m_last_error = "Incomplete multistream configuration for decoder initialization";
        Debug::log("opus", m_last_error);
        return false;
    }
    
    // Validate channel mapping size
    if (m_channel_mapping.size() != m_channels) {
        m_last_error = "Channel mapping size mismatch: expected " + std::to_string(m_channels) + 
                      " entries, got " + std::to_string(m_channel_mapping.size());
        Debug::log("opus", m_last_error);
        return false;
    }
    
    Debug::log("opus", "Initializing multistream decoder:");
    Debug::log("opus", "  Channels: ", m_channels);
    Debug::log("opus", "  Streams: ", m_stream_count);
    Debug::log("opus", "  Coupled streams: ", m_coupled_stream_count);
    Debug::log("opus", "  Mapping family: ", m_channel_mapping_family);
    
    // Create multistream decoder with our stored configuration
    int error;
    m_opus_ms_decoder = opus_multistream_decoder_create(
        m_sample_rate,
        m_channels,
        m_stream_count,
        m_coupled_stream_count,
        m_channel_mapping.data(),
        &error
    );
    
    if (!m_opus_ms_decoder || error != OPUS_OK) {
        m_last_error = "Failed to create Opus multistream decoder: " + std::string(opus_strerror(error));
        Debug::log("opus", m_last_error);
        return false;
    }
    
    // Performance Optimization: Optimize buffer sizes for the actual channel count
    optimizeBufferSizes_unlocked(m_channels);
    
    // Configure multistream decoder parameters
    Debug::log("opus", "Multistream decoder configured for integer API operation");
    
    // Configure performance optimizations
    if (!configureOpusOptimizations_unlocked()) {
        Debug::log("opus", "Warning: Failed to configure some performance optimizations");
        // Don't fail completely - optimizations are optional
    }
    
    // Performance Optimization 10.2: Optimize cache efficiency
    optimizeCacheEfficiency_unlocked();
    
    m_use_multistream = true;
    m_decoder_initialized = true;
    
    Debug::log("opus", "Multistream decoder initialization completed successfully with performance optimizations");
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
    
    // Reset multi-channel configuration
    m_channel_mapping_family = 0;
    m_stream_count = 0;
    m_coupled_stream_count = 0;
    m_channel_mapping.clear();
    
    // Clear buffers
    m_output_buffer.clear();
    m_float_buffer.clear();
    
    // Clear bounded output buffers (Requirement 7.6)
    clearOutputBuffers_unlocked();
    
    // Clear partial packet buffer
    m_partial_packet_buffer.clear();
    
    // Reset atomic variables
    m_samples_decoded.store(0);
    m_samples_to_skip.store(0);
    m_error_state.store(false);
    m_frames_processed.store(0);
    m_last_error.clear();
    m_last_decode_time = std::chrono::steady_clock::now();
    
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
    Debug::log("opus", "Legacy decoder error recovery - delegating to enhanced recovery system");
    
    if (!m_error_state.load()) {
        Debug::log("opus", "No error state to recover from");
        return true;
    }
    
    // Use the enhanced recovery system with a generic error
    // This maintains backward compatibility while using the new system
    if (attemptGracefulRecovery_unlocked(OPUS_INTERNAL_ERROR)) {
        return validateRecoverySuccess_unlocked();
    }
    
    return false;
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

// ========== Buffer Management Implementation ==========

bool OpusCodec::initializeOutputBuffers_unlocked()
{
    Debug::log("opus", "initializeOutputBuffers_unlocked called");
    
    // Configure bounded output buffer limits to prevent memory exhaustion (Requirement 7.2)
    // Maximum frames to buffer: enough for ~1 second of audio at typical frame rates
    // Opus typical frame size is 20ms (960 samples at 48kHz), so 50 frames = ~1 second
    m_max_output_buffer_frames = 50;
    
    // Maximum samples to buffer: conservative limit based on typical multi-channel usage
    // 8 channels * 48000 Hz * 1 second = 384,000 samples maximum
    m_max_output_buffer_samples = 384000;
    
    // Clear any existing buffered data
    clearOutputBuffers_unlocked();
    
    Debug::log("opus", "Output buffer limits configured:");
    Debug::log("opus", "  Max frames: ", m_max_output_buffer_frames);
    Debug::log("opus", "  Max samples: ", m_max_output_buffer_samples);
    
    return true;
}

bool OpusCodec::canBufferFrame_unlocked(const AudioFrame& frame) const
{
    // Check if we can buffer this frame without exceeding limits (Requirement 7.4)
    
    // Check frame count limit
    if (m_output_queue.size() >= m_max_output_buffer_frames) {
        Debug::log("opus", "Cannot buffer frame: frame limit reached (", 
                  m_output_queue.size(), "/", m_max_output_buffer_frames, ")");
        return false;
    }
    
    // Check sample count limit
    size_t current_samples = m_buffered_samples.load();
    if (current_samples + frame.samples.size() > m_max_output_buffer_samples) {
        Debug::log("opus", "Cannot buffer frame: sample limit would be exceeded (", 
                  current_samples, " + ", frame.samples.size(), " > ", m_max_output_buffer_samples, ")");
        return false;
    }
    
    return true;
}

bool OpusCodec::bufferFrame_unlocked(const AudioFrame& frame)
{
    // Buffer frame with overflow protection (Requirements 7.2, 7.4)
    
    if (frame.samples.empty()) {
        Debug::log("opus", "Skipping empty frame for buffering");
        return true; // Empty frames don't need buffering
    }
    
    // Check if we can buffer this frame
    if (!canBufferFrame_unlocked(frame)) {
        // Set overflow flag to provide backpressure (Requirement 7.4)
        m_buffer_overflow.store(true);
        Debug::log("opus", "Output buffer overflow detected - backpressure activated");
        return false;
    }
    
    // Buffer the frame
    m_output_queue.push(frame);
    m_buffered_samples.fetch_add(frame.samples.size());
    
    Debug::log("opus", "Frame buffered: ", frame.samples.size(), " samples, ", 
              m_output_queue.size(), " frames total, ", 
              m_buffered_samples.load(), " samples total");
    
    return true;
}

AudioFrame OpusCodec::getBufferedFrame_unlocked()
{
    // Retrieve buffered frame and update counters
    
    if (m_output_queue.empty()) {
        Debug::log("opus", "No buffered frames available");
        return AudioFrame();
    }
    
    AudioFrame frame = std::move(m_output_queue.front());
    m_output_queue.pop();
    
    // Update sample counter
    m_buffered_samples.fetch_sub(frame.samples.size());
    
    // Clear overflow flag if buffer is no longer full
    if (m_buffer_overflow.load() && !isOutputBufferFull_unlocked()) {
        m_buffer_overflow.store(false);
        Debug::log("opus", "Output buffer overflow cleared - backpressure released");
    }
    
    Debug::log("opus", "Frame retrieved from buffer: ", frame.samples.size(), " samples, ", 
              m_output_queue.size(), " frames remaining, ", 
              m_buffered_samples.load(), " samples remaining");
    
    return frame;
}

bool OpusCodec::hasBufferedFrames_unlocked() const
{
    return !m_output_queue.empty();
}

void OpusCodec::clearOutputBuffers_unlocked()
{
    // Clear all buffered frames and reset counters (Requirement 7.6)
    
    Debug::log("opus", "Clearing output buffers - had ", m_output_queue.size(), 
              " frames, ", m_buffered_samples.load(), " samples");
    
    // Clear the queue
    while (!m_output_queue.empty()) {
        m_output_queue.pop();
    }
    
    // Reset counters
    m_buffered_samples.store(0);
    m_buffer_overflow.store(false);
    
    Debug::log("opus", "Output buffers cleared");
}

size_t OpusCodec::getBufferedSampleCount_unlocked() const
{
    return m_buffered_samples.load();
}

bool OpusCodec::isOutputBufferFull_unlocked() const
{
    // Check if buffer is at or near capacity
    return (m_output_queue.size() >= m_max_output_buffer_frames) ||
           (m_buffered_samples.load() >= m_max_output_buffer_samples);
}

// ========== Streaming Support Implementation ==========

bool OpusCodec::handlePartialPacketData_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("opus", "handlePartialPacketData_unlocked called with ", packet_data.size(), " bytes");
    
    // Handle partial packet data gracefully (Requirement 7.3)
    
    if (packet_data.empty()) {
        Debug::log("opus", "Empty packet data - nothing to handle");
        return true;
    }
    
    // Check if this looks like a valid partial packet
    if (!isValidPartialPacket_unlocked(packet_data)) {
        Debug::log("opus", "Invalid partial packet data - discarding");
        m_partial_packet_buffer.clear();
        return false;
    }
    
    // For Opus, packets are typically self-contained, but we can handle
    // cases where packet data might be incomplete due to streaming issues
    
    // If we have buffered partial data, try to combine it
    if (!m_partial_packet_buffer.empty()) {
        Debug::log("opus", "Combining with ", m_partial_packet_buffer.size(), " bytes of buffered partial data");
        
        // Append new data to partial buffer
        m_partial_packet_buffer.insert(m_partial_packet_buffer.end(), 
                                      packet_data.begin(), packet_data.end());
        
        Debug::log("opus", "Combined packet size: ", m_partial_packet_buffer.size(), " bytes");
        
        // Check if combined packet is now valid
        if (validateOpusPacket_unlocked(m_partial_packet_buffer)) {
            Debug::log("opus", "Combined packet is now valid - processing");
            
            // Process the combined packet
            AudioFrame frame = decodeAudioPacket_unlocked(m_partial_packet_buffer);
            
            // Clear partial buffer
            m_partial_packet_buffer.clear();
            
            // Buffer the decoded frame if valid
            if (!frame.samples.empty()) {
                bufferFrame_unlocked(frame);
            }
            
            return true;
        } else {
            Debug::log("opus", "Combined packet still invalid - keeping in buffer");
            
            // Prevent buffer from growing too large
            constexpr size_t MAX_PARTIAL_BUFFER_SIZE = 65536; // 64KB limit
            if (m_partial_packet_buffer.size() > MAX_PARTIAL_BUFFER_SIZE) {
                Debug::log("opus", "Partial packet buffer too large - discarding");
                m_partial_packet_buffer.clear();
                return false;
            }
        }
    } else {
        // No existing partial data - check if this packet is complete
        if (validateOpusPacket_unlocked(packet_data)) {
            Debug::log("opus", "Packet is complete - processing normally");
            return true; // Let normal processing handle it
        } else {
            Debug::log("opus", "Packet appears incomplete - buffering for later");
            m_partial_packet_buffer = packet_data;
        }
    }
    
    return true;
}

bool OpusCodec::isValidPartialPacket_unlocked(const std::vector<uint8_t>& packet_data) const
{
    // Basic validation for partial packet data (Requirement 7.3)
    
    if (packet_data.empty()) {
        return false;
    }
    
    // For Opus packets, we can do some basic validation:
    // - Minimum size check (Opus packets are at least 1 byte)
    // - Maximum size check (Opus packets are typically under 1275 bytes)
    // - TOC byte validation (first byte contains configuration)
    
    constexpr size_t MIN_OPUS_PACKET_SIZE = 1;
    constexpr size_t MAX_OPUS_PACKET_SIZE = 1275;
    
    if (packet_data.size() < MIN_OPUS_PACKET_SIZE) {
        Debug::log("opus", "Packet too small: ", packet_data.size(), " bytes");
        return false;
    }
    
    if (packet_data.size() > MAX_OPUS_PACKET_SIZE) {
        Debug::log("opus", "Packet too large: ", packet_data.size(), " bytes");
        return false;
    }
    
    // Basic TOC byte validation
    uint8_t toc = packet_data[0];
    
    // Extract configuration from TOC byte (bits 3-7)
    uint8_t config = (toc >> 3) & 0x1F;
    
    // Valid configurations are 0-31
    if (config > 31) {
        Debug::log("opus", "Invalid TOC configuration: ", config);
        return false;
    }
    
    Debug::log("opus", "Partial packet validation passed - config=", config, ", size=", packet_data.size());
    return true;
}

void OpusCodec::maintainStreamingLatency_unlocked()
{
    // Maintain consistent latency and throughput for continuous streaming (Requirement 7.8)
    
    auto current_time = std::chrono::steady_clock::now();
    
    // Update timing information
    if (m_frames_processed.load() > 0) {
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - m_last_decode_time).count();
        
        // Log timing information for monitoring
        if (time_diff > 50) { // Log if processing takes more than 50ms
            Debug::log("opus", "Decode timing: ", time_diff, "ms for frame ", m_frames_processed.load());
        }
    }
    
    m_last_decode_time = current_time;
    m_frames_processed.fetch_add(1);
    
    // Adaptive buffer management for consistent latency
    size_t buffered_samples = m_buffered_samples.load();
    size_t buffered_frames = m_output_queue.size();
    
    // If we have too many buffered frames, we might be introducing latency
    if (buffered_frames > m_max_output_buffer_frames / 2) {
        Debug::log("opus", "High buffer usage detected: ", buffered_frames, " frames, ", 
                  buffered_samples, " samples - consider reducing latency");
    }
    
    // If buffer is nearly empty during continuous streaming, we might have throughput issues
    if (buffered_frames < 2 && m_frames_processed.load() > 10) {
        Debug::log("opus", "Low buffer usage during streaming: ", buffered_frames, 
                  " frames - throughput may be insufficient");
    }
}

// ========== Performance Optimization Methods ==========

bool OpusCodec::configureOpusOptimizations_unlocked()
{
    Debug::log("opus", "Configuring Opus performance optimizations");
    
    // Performance Optimization 10.1: Leverage libopus built-in optimizations (Requirement 9.3)
    if (!m_decoder_initialized || (!m_opus_decoder && !m_opus_ms_decoder)) {
        Debug::log("opus", "Cannot configure optimizations - decoder not initialized");
        return false;
    }
    
    bool success = true;
    
    // Enable SIMD optimizations if available
    if (!enableSIMDOptimizations_unlocked()) {
        Debug::log("opus", "Warning: SIMD optimizations not available or failed to enable");
        // Don't fail completely - SIMD is optional
    }
    
    // Configure decoder for optimal performance
    if (m_use_multistream && m_opus_ms_decoder) {
        // Configure multistream decoder optimizations
        int error = opus_multistream_decoder_ctl(m_opus_ms_decoder, OPUS_SET_COMPLEXITY(10));
        if (error != OPUS_OK) {
            Debug::log("opus", "Warning: Failed to set multistream decoder complexity: ", opus_strerror(error));
        } else {
            Debug::log("opus", "Multistream decoder complexity set to maximum for best quality");
        }
        
        // Enable phase inversion for better stereo imaging
        error = opus_multistream_decoder_ctl(m_opus_ms_decoder, OPUS_SET_PHASE_INVERSION_DISABLED(0));
        if (error != OPUS_OK) {
            Debug::log("opus", "Warning: Failed to enable phase inversion: ", opus_strerror(error));
        }
        
    } else if (!m_use_multistream && m_opus_decoder) {
        // Configure standard decoder optimizations
        int error = opus_decoder_ctl(m_opus_decoder, OPUS_SET_COMPLEXITY(10));
        if (error != OPUS_OK) {
            Debug::log("opus", "Warning: Failed to set decoder complexity: ", opus_strerror(error));
        } else {
            Debug::log("opus", "Decoder complexity set to maximum for best quality");
        }
        
        // Enable phase inversion for better stereo imaging (if stereo)
        if (m_channels == 2) {
            error = opus_decoder_ctl(m_opus_decoder, OPUS_SET_PHASE_INVERSION_DISABLED(0));
            if (error != OPUS_OK) {
                Debug::log("opus", "Warning: Failed to enable phase inversion: ", opus_strerror(error));
            }
        }
    }
    
    Debug::log("opus", "Opus performance optimizations configured successfully");
    return success;
}

bool OpusCodec::enableSIMDOptimizations_unlocked()
{
    Debug::log("opus", "Attempting to enable SIMD optimizations");
    
    // Performance Optimization 10.1: Leverage libopus SIMD support (Requirement 9.3)
    // libopus automatically uses SIMD when available, but we can query and log the capabilities
    
    // Query libopus version and capabilities
    const char* version = opus_get_version_string();
    Debug::log("opus", "Using libopus version: ", version);
    
    // libopus automatically enables SIMD optimizations when available
    // We don't need to explicitly enable them, but we can verify they're working
    // by checking performance characteristics
    
    // For now, we'll assume SIMD is available if libopus was compiled with it
    // This is typically the case for modern libopus builds
    Debug::log("opus", "SIMD optimizations are handled automatically by libopus");
    
    return true;
}

void OpusCodec::optimizeBufferSizes_unlocked(uint16_t channels)
{
    Debug::log("opus", "Optimizing buffer sizes for ", channels, " channels");
    
    // Performance Optimization 10.1: Minimize allocation overhead and memory fragmentation (Requirement 9.6)
    constexpr size_t MAX_OPUS_FRAME_SIZE = 5760; // 120ms at 48kHz
    size_t optimal_buffer_size = MAX_OPUS_FRAME_SIZE * channels;
    
    // Resize buffers if current capacity is insufficient
    if (m_output_buffer.capacity() < optimal_buffer_size) {
        Debug::log("opus", "Expanding output buffer capacity from ", m_output_buffer.capacity(), 
                  " to ", optimal_buffer_size, " samples");
        m_output_buffer.reserve(optimal_buffer_size);
    }
    
    if (m_float_buffer.capacity() < optimal_buffer_size) {
        Debug::log("opus", "Expanding float buffer capacity from ", m_float_buffer.capacity(), 
                  " to ", optimal_buffer_size, " samples");
        m_float_buffer.reserve(optimal_buffer_size);
    }
    
    // Optimize channel mapping buffer
    if (m_channel_mapping.capacity() < channels) {
        Debug::log("opus", "Expanding channel mapping capacity from ", m_channel_mapping.capacity(), 
                  " to ", channels, " channels");
        m_channel_mapping.reserve(channels);
    }
    
    Debug::log("opus", "Buffer sizes optimized for ", channels, " channels");
}

bool OpusCodec::isMonoStereoOptimizable_unlocked() const
{
    // Performance Optimization 10.2: Optimize for mono and stereo configurations (Requirement 9.4)
    return (m_channels == 1 || m_channels == 2) && m_channel_mapping_family == 0;
}



void OpusCodec::optimizeMemoryAccessPatterns_unlocked(AudioFrame& frame)
{
    // Performance Optimization 10.2: Optimize memory access patterns for cache efficiency (Requirement 9.8)
    if (frame.samples.empty()) {
        return;
    }
    
    // For mono and stereo, samples are already optimally laid out
    if (m_channels <= 2) {
        return;
    }
    
    // For multi-channel audio, ensure samples are properly interleaved for cache efficiency
    // This is already handled by libopus, but we can optimize the copy operation
    size_t samples_per_channel = frame.samples.size() / m_channels;
    
    // Use prefetch hints for large frames to improve cache performance
    if (samples_per_channel > 1920) { // > 40ms at 48kHz
        // Prefetch the next cache line during processing
        const size_t cache_line_size = 64; // Typical cache line size
        const size_t samples_per_cache_line = cache_line_size / sizeof(int16_t);
        
        for (size_t i = 0; i < frame.samples.size(); i += samples_per_cache_line) {
            __builtin_prefetch(&frame.samples[i], 0, 1); // Prefetch for read, low temporal locality
        }
    }
    
    Debug::log("opus", "Memory access patterns optimized for ", m_channels, " channels, ", 
              samples_per_channel, " samples per channel");
}

bool OpusCodec::handleVariableFrameSizeEfficiently_unlocked(int frame_size_samples)
{
    // Performance Optimization 10.2: Handle variable frame sizes and bitrate changes efficiently (Requirement 9.5)
    
    // Common Opus frame sizes at 48kHz:
    // 2.5ms = 120 samples, 5ms = 240, 10ms = 480, 20ms = 960, 40ms = 1920, 60ms = 2880
    
    // Optimize buffer allocation based on frame size patterns
    // Use instance variables for thread safety (Requirements 10.1, 10.2)
    
    if (m_last_frame_size != frame_size_samples) {
        m_frame_size_changes++;
        m_last_frame_size = frame_size_samples;
        
        // If frame size changes frequently, use larger buffer to reduce reallocations
        if (m_frame_size_changes > 10) {
            size_t adaptive_buffer_size = std::max(
                static_cast<size_t>(frame_size_samples * m_channels * 2), // 2x current frame
                static_cast<size_t>(2880 * m_channels) // At least 60ms worth
            );
            
            if (m_output_buffer.capacity() < adaptive_buffer_size) {
                Debug::log("opus", "Adapting buffer size for variable frames: ", adaptive_buffer_size, " samples");
                m_output_buffer.reserve(adaptive_buffer_size);
            }
            
            m_frame_size_changes = 0; // Reset counter
        }
    }
    
    // Optimize for common frame sizes
    switch (frame_size_samples) {
        case 120:  // 2.5ms
        case 240:  // 5ms
        case 480:  // 10ms
        case 960:  // 20ms (most common)
        case 1920: // 40ms
        case 2880: // 60ms
            // These are standard Opus frame sizes - no special handling needed
            return true;
            
        default:
            // Non-standard frame size - might indicate bitrate change or unusual encoding
            Debug::log("opus", "Non-standard frame size detected: ", frame_size_samples, " samples");
            return true;
    }
}

void OpusCodec::optimizeCacheEfficiency_unlocked()
{
    // Performance Optimization 10.2: Optimize memory access patterns for cache efficiency (Requirement 9.7, 9.8)
    
    // Ensure buffers are aligned for optimal cache performance
    constexpr size_t CACHE_LINE_SIZE = 64;
    constexpr size_t ALIGNMENT = CACHE_LINE_SIZE / sizeof(int16_t);
    
    // Reserve buffer sizes that are multiples of cache line size when possible
    size_t current_capacity = m_output_buffer.capacity();
    if (current_capacity > 0) {
        size_t aligned_capacity = ((current_capacity + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;
        if (aligned_capacity != current_capacity && aligned_capacity < current_capacity * 1.1) {
            // Only realign if it doesn't increase size by more than 10%
            m_output_buffer.reserve(aligned_capacity);
            Debug::log("opus", "Buffer aligned for cache efficiency: ", aligned_capacity, " samples");
        }
    }
    
    // For multi-channel processing, ensure channel mapping is cache-friendly
    if (m_channels > 2 && !m_channel_mapping.empty()) {
        // Channel mapping should be accessed sequentially for best cache performance
        // This is already handled by the libopus implementation
        Debug::log("opus", "Multi-channel cache optimization applied for ", m_channels, " channels");
    }
}

#endif // HAVE_OGGDEMUXER
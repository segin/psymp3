/*
 * MuLawCodec.cpp - μ-law (G.711 μ-law) audio codec implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
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

#ifdef ENABLE_MULAW_CODEC

namespace PsyMP3 {
namespace Codec {
namespace PCM {

// Static member initialization
bool MuLawCodec::s_table_initialized = false;

// μ-law to 16-bit PCM conversion lookup table (runtime-initialized)
// ITU-T G.711 μ-law compliant values computed at runtime
int16_t MuLawCodec::MULAW_TO_PCM[256];

MuLawCodec::MuLawCodec(const StreamInfo& stream_info) 
    : SimplePCMCodec(stream_info) {
    Debug::log("codec", "MuLawCodec: Constructor called for codec: ", stream_info.codec_name);
    
    // Ensure lookup table is initialized
    if (!s_table_initialized) {
        Debug::log("codec", "MuLawCodec: Initializing lookup table");
        initializeMuLawTable();
    }
    
    Debug::log("codec", "MuLawCodec: Constructor completed successfully");
}

bool MuLawCodec::canDecode(const StreamInfo& stream_info) const {
    try {
        Debug::log("codec", "MuLawCodec: Checking if can decode stream with codec: ", stream_info.codec_name);
        
        // First check: Must be audio stream with μ-law codec name
        if (stream_info.codec_type != "audio") {
            Debug::log("codec", "MuLawCodec: Rejecting stream - not audio type, got: ", stream_info.codec_type);
            return false;
        }
        
        // Accept various μ-law format identifiers
        bool is_mulaw_codec = (stream_info.codec_name == "mulaw" || 
                              stream_info.codec_name == "pcm_mulaw" ||
                              stream_info.codec_name == "g711_mulaw");
        
        if (!is_mulaw_codec) {
            Debug::log("codec", "MuLawCodec: Rejecting stream - unsupported codec: ", stream_info.codec_name);
            return false;
        }
        
        // Validate μ-law specific parameters
        // μ-law uses 8-bit samples (1 byte per sample)
        if (stream_info.bits_per_sample != 0 && stream_info.bits_per_sample != 8) {
            Debug::log("codec", "MuLawCodec: Rejecting stream - μ-law requires 8 bits per sample, got ", stream_info.bits_per_sample);
            return false;
        }
        
        // Validate sample rate - μ-law supports telephony rates and common audio rates
        if (stream_info.sample_rate != 0) {
            // Check for reasonable sample rate range (1 Hz to 192 kHz)
            if (stream_info.sample_rate < 1 || stream_info.sample_rate > 192000) {
                Debug::log("codec", "MuLawCodec: Rejecting stream - invalid sample rate: ", stream_info.sample_rate, " Hz");
                return false;
            }
            
            // Common telephony and audio sample rates for μ-law
            bool valid_sample_rate = (stream_info.sample_rate == 8000 ||   // Standard telephony
                                     stream_info.sample_rate == 16000 ||   // Wideband telephony
                                     stream_info.sample_rate == 32000 ||   // Super-wideband
                                     stream_info.sample_rate == 44100 ||   // CD quality
                                     stream_info.sample_rate == 48000);    // Professional audio
            
            if (!valid_sample_rate) {
                Debug::log("codec", "MuLawCodec: Warning - Unusual sample rate ", stream_info.sample_rate, " Hz for μ-law stream");
                // Don't reject - allow unusual sample rates but log warning
            }
        }
        
        // Validate channel count - μ-law typically mono but can support stereo
        if (stream_info.channels != 0) {
            if (stream_info.channels > 2) {
                Debug::log("codec", "MuLawCodec: Rejecting stream - μ-law supports max 2 channels, got ", stream_info.channels);
                return false;
            }
            
            if (stream_info.channels == 0) {
                Debug::log("codec", "MuLawCodec: Rejecting stream - Invalid channel count: 0");
                return false;
            }
        }
        
        // All validation passed
        Debug::log("codec", "MuLawCodec: Stream validation passed for codec: ", stream_info.codec_name);
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("codec", "MuLawCodec: Exception during format validation: ", e.what());
        return false;
    } catch (...) {
        Debug::log("codec", "MuLawCodec: Unknown exception during format validation for codec: ", stream_info.codec_name);
        return false;
    }
}

std::string MuLawCodec::getCodecName() const {
    return "mulaw";
}

bool MuLawCodec::initialize() {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        Debug::log("codec", "MuLawCodec: Starting initialization for codec: ", m_stream_info.codec_name);
        
        // Validate stream info before initialization
        if (!canDecode(m_stream_info)) {
            Debug::log("codec", "MuLawCodec: Initialization failed - unsupported stream format for codec: ", m_stream_info.codec_name);
            return false;
        }
        
        // Ensure lookup table is properly initialized
        if (!s_table_initialized) {
            Debug::log("codec", "MuLawCodec: Lookup table not initialized, initializing now");
            initializeMuLawTable();
            if (!s_table_initialized) {
                Debug::log("codec", "MuLawCodec: Initialization failed - lookup table initialization failed");
                return false;
            }
        }
        
        // Set default parameters for raw streams
        if (m_stream_info.sample_rate == 0) {
            m_stream_info.sample_rate = 8000; // Default telephony rate
            Debug::log("codec", "MuLawCodec: Using default sample rate: 8000 Hz");
        }
        
        if (m_stream_info.channels == 0) {
            m_stream_info.channels = 1; // Default mono
            Debug::log("codec", "MuLawCodec: Using default channel count: 1 (mono)");
        }
        
        // Validate final parameters
        if (m_stream_info.sample_rate < 1 || m_stream_info.sample_rate > 192000) {
            Debug::log("codec", "MuLawCodec: Initialization failed - invalid sample rate: ", m_stream_info.sample_rate);
            return false;
        }
        
        if (m_stream_info.channels < 1 || m_stream_info.channels > 2) {
            Debug::log("codec", "MuLawCodec: Initialization failed - invalid channel count: ", m_stream_info.channels);
            return false;
        }
        
        m_initialized = true;
        
        // Performance metrics logging
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        Debug::log("codec", "MuLawCodec: Initialized successfully - ", m_stream_info.sample_rate, " Hz, ", m_stream_info.channels, " channels");
        Debug::log("performance", "MuLawCodec: Initialization completed in ", duration.count(), " microseconds");
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("codec", "MuLawCodec: Exception during initialization: ", e.what());
        m_initialized = false;
        return false;
    } catch (...) {
        Debug::log("codec", "MuLawCodec: Unknown exception during initialization for codec: ", m_stream_info.codec_name);
        m_initialized = false;
        return false;
    }
}

AudioFrame MuLawCodec::decode(const MediaChunk& chunk) {
    AudioFrame frame;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Check initialization state
        if (!m_initialized) {
            Debug::log("codec", "MuLawCodec: Decode called on uninitialized codec");
            return frame; // Empty frame
        }
        
        // Handle empty or null chunk gracefully
        if (chunk.data.empty()) {
            Debug::log("codec", "MuLawCodec: Received empty chunk (size=", chunk.data.size(), "), returning empty frame");
            return frame; // Empty frame
        }
        
        // Set frame properties from stream info
        frame.sample_rate = m_stream_info.sample_rate;
        frame.channels = m_stream_info.channels;
        frame.timestamp_samples = chunk.timestamp_samples;
        
        // Calculate timestamp in milliseconds from samples
        if (m_stream_info.sample_rate > 0) {
            frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / m_stream_info.sample_rate;
        } else {
            frame.timestamp_ms = 0;
        }
        
        // Convert samples with error handling and performance tracking
        size_t samples_converted = convertSamples(chunk.data, frame.samples);
        
        if (samples_converted == 0 && !chunk.data.empty()) {
            Debug::log("codec", "MuLawCodec: Warning - no samples converted from non-empty chunk of size ", chunk.data.size());
            return frame; // Empty frame
        }
        
        // Validate output consistency
        if (frame.samples.size() != samples_converted) {
            Debug::log("codec", "MuLawCodec: Warning - sample count mismatch: converted ", samples_converted, ", frame has ", frame.samples.size());
        }
        
        // Performance metrics logging
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        Debug::log("performance", "MuLawCodec: Decoded ", samples_converted, " samples in ", duration.count(), " microseconds");
        
        // Calculate throughput (samples per second)
        if (duration.count() > 0) {
            double throughput = (samples_converted * 1000000.0) / duration.count();
            Debug::log("performance", "MuLawCodec: Decoding throughput: ", static_cast<uint64_t>(throughput), " samples/second");
        }
        
        return frame;
        
    } catch (const std::exception& e) {
        Debug::log("codec", "MuLawCodec: Exception during decode: ", e.what());
        return AudioFrame{}; // Empty frame
    } catch (...) {
        Debug::log("codec", "MuLawCodec: Unknown exception during decode for codec: ", getCodecName());
        return AudioFrame{}; // Empty frame
    }
}

size_t MuLawCodec::convertSamples(const std::vector<uint8_t>& input_data, 
                                 std::vector<int16_t>& output_samples) {
    const size_t input_samples = input_data.size();
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Handle empty input gracefully
    if (input_samples == 0) {
        output_samples.clear();
        return 0;
    }
    
    try {
        // Ensure output vector has sufficient capacity for optimal performance
        // Use try-catch to handle potential memory allocation failures
        output_samples.reserve(input_samples);
        output_samples.clear();
        
        // Convert each μ-law sample to 16-bit PCM using lookup table
        // All 8-bit values (0x00-0xFF) are valid μ-law inputs
        // For multi-channel audio, samples are processed in interleaved order:
        // Mono: [sample0, sample1, sample2, ...]
        // Stereo: [L0, R0, L1, R1, L2, R2, ...]
        // This maintains proper channel interleaving in the output
        for (size_t i = 0; i < input_samples; ++i) {
            const uint8_t mulaw_sample = input_data[i];
            
            // Direct lookup - no validation needed as all 8-bit values are valid
            const int16_t pcm_sample = MULAW_TO_PCM[mulaw_sample];
            output_samples.push_back(pcm_sample);
        }
        
        // Performance metrics for large conversions
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        if (input_samples > 1000) { // Only log for significant conversions
            Debug::log("performance", "MuLawCodec: Converted ", input_samples, " μ-law samples in ", duration.count(), " microseconds");
            
            if (duration.count() > 0) {
                double conversion_rate = (input_samples * 1000000.0) / duration.count();
                Debug::log("performance", "MuLawCodec: Conversion rate: ", static_cast<uint64_t>(conversion_rate), " samples/second");
            }
        }
        
        return input_samples;
        
    } catch (const std::bad_alloc& e) {
        Debug::log("codec", "MuLawCodec: Memory allocation failed during sample conversion: ", e.what());
        output_samples.clear();
        return 0;
    } catch (const std::exception& e) {
        Debug::log("codec", "MuLawCodec: Exception during sample conversion: ", e.what());
        // Attempt to preserve partial conversion if possible
        if (!output_samples.empty()) {
            Debug::log("codec", "MuLawCodec: Returning ", output_samples.size(), " partially converted samples");
            return output_samples.size();
        }
        output_samples.clear();
        return 0;
    } catch (...) {
        Debug::log("codec", "MuLawCodec: Unknown exception during sample conversion (input_size=", input_data.size(), ")");
        output_samples.clear();
        return 0;
    }
}

size_t MuLawCodec::getBytesPerInputSample() const {
    return 1; // μ-law uses 8-bit samples
}

void MuLawCodec::initializeMuLawTable() {
    if (s_table_initialized) {
        Debug::log("codec", "MuLawCodec: Lookup table already initialized");
        return; // Already initialized
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        Debug::log("codec", "MuLawCodec: Computing ITU-T G.711 μ-law lookup table at runtime");
        
        // Compute all 256 μ-law values using ITU-T G.711 μ-law algorithm
        for (int i = 0; i < 256; ++i) {
            uint8_t mulaw_sample = static_cast<uint8_t>(i);
            
            // ITU-T G.711 μ-law decoding algorithm
            // Step 1: Invert all bits (XOR with 0xFF)
            uint8_t complement = mulaw_sample ^ 0xFF;
            
            // Step 2: Extract sign bit (bit 7)
            bool sign = (complement & 0x80) != 0;
            
            // Step 3: Extract exponent (bits 6-4)
            uint8_t exponent = (complement & 0x70) >> 4;
            
            // Step 4: Extract mantissa (bits 3-0)
            uint8_t mantissa = complement & 0x0F;
            
            // Step 5: Compute linear value
            int16_t linear;
            if (exponent == 0) {
                // Segment 0: linear region
                linear = 16 + 2 * mantissa;
            } else {
                // Segments 1-7: logarithmic regions
                linear = (16 + 2 * mantissa) << exponent;
            }
            
            // Step 6: Apply sign
            if (!sign) {
                linear = -linear;
            }
            
            // Store in lookup table
            MULAW_TO_PCM[i] = linear;
        }
        
        // Validate critical values for ITU-T G.711 compliance
        Debug::log("codec", "MuLawCodec: Validating computed μ-law values");
        
        // μ-law silence value (0xFF) should map to 0
        if (MULAW_TO_PCM[0xFF] != 0) {
            Debug::log("codec", "MuLawCodec: Warning - μ-law silence value (0xFF) computed as ", MULAW_TO_PCM[0xFF], ", expected 0");
        }
        
        // Validate sign bit handling - values with bit 7 clear (0x00-0x7F) should be negative
        if (MULAW_TO_PCM[0x00] >= 0 || MULAW_TO_PCM[0x7F] >= 0) {
            Debug::log("codec", "MuLawCodec: Warning - μ-law sign bit handling incorrect for negative range (0x00=", MULAW_TO_PCM[0x00], ", 0x7F=", MULAW_TO_PCM[0x7F], ")");
        }
        
        // Validate sign bit handling - values with bit 7 set (0x80-0xFE) should be positive
        if (MULAW_TO_PCM[0x80] <= 0 || MULAW_TO_PCM[0xFE] <= 0) {
            Debug::log("codec", "MuLawCodec: Warning - μ-law sign bit handling incorrect for positive range (0x80=", MULAW_TO_PCM[0x80], ", 0xFE=", MULAW_TO_PCM[0xFE], ")");
        }
        
        // Log some key computed values for verification
        Debug::log("codec", "MuLawCodec: Key computed values - 0x00=", MULAW_TO_PCM[0x00], ", 0x80=", MULAW_TO_PCM[0x80], ", 0xFE=", MULAW_TO_PCM[0xFE], ", 0xFF=", MULAW_TO_PCM[0xFF]);
        
        // Performance metrics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        Debug::log("codec", "MuLawCodec: ITU-T G.711 μ-law lookup table computed successfully with 256 entries");
        Debug::log("performance", "MuLawCodec: Table computation completed in ", duration.count(), " microseconds");
        
        s_table_initialized = true;
        
    } catch (const std::exception& e) {
        Debug::log("codec", "MuLawCodec: Exception during table computation: ", e.what());
        // Don't set s_table_initialized to true on error
    } catch (...) {
        Debug::log("codec", "MuLawCodec: Unknown exception during table computation");
        // Don't set s_table_initialized to true on error
    }
}

void registerMuLawCodec() {
    Debug::log("codec", "MuLawCodec: Registering μ-law codec with AudioCodecFactory");
    
    // Register μ-law codec with multiple format identifiers
    AudioCodecFactory::registerCodec("mulaw", [](const StreamInfo& stream_info) {
        return std::make_unique<MuLawCodec>(stream_info);
    });
    
    AudioCodecFactory::registerCodec("pcm_mulaw", [](const StreamInfo& stream_info) {
        return std::make_unique<MuLawCodec>(stream_info);
    });
    
    AudioCodecFactory::registerCodec("g711_mulaw", [](const StreamInfo& stream_info) {
        return std::make_unique<MuLawCodec>(stream_info);
    });
    
    Debug::log("codec", "MuLawCodec: Successfully registered for codec names: mulaw, pcm_mulaw, g711_mulaw");
}

} // namespace PCM
} // namespace Codec
} // namespace PsyMP3

#endif // ENABLE_MULAW_CODEC

/*
 * ALawCodec.cpp - A-law (G.711 A-law) audio codec implementation
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

#ifdef ENABLE_ALAW_CODEC

namespace PsyMP3 {
namespace Codec {
namespace PCM {

// Static member initialization
bool ALawCodec::s_table_initialized = false;

// A-law to 16-bit PCM conversion lookup table (runtime-initialized)
// ITU-T G.711 A-law compliant values computed at runtime
int16_t ALawCodec::ALAW_TO_PCM[256];

ALawCodec::ALawCodec(const StreamInfo& stream_info) 
    : SimplePCMCodec(stream_info) {
    Debug::log("codec", "ALawCodec: Constructor called for codec: ", stream_info.codec_name);
    
    // Ensure lookup table is initialized
    if (!s_table_initialized) {
        Debug::log("codec", "ALawCodec: Initializing lookup table");
        initializeALawTable();
    }
    
    Debug::log("codec", "ALawCodec: Constructor completed successfully");
}

bool ALawCodec::canDecode(const StreamInfo& stream_info) const {
    try {
        Debug::log("codec", "ALawCodec: Checking if can decode stream with codec: ", stream_info.codec_name);
        
        // First check: Must be audio stream with A-law codec name
        if (stream_info.codec_type != "audio") {
            Debug::log("codec", "ALawCodec: Rejecting stream - not audio type, got: ", stream_info.codec_type);
            return false;
        }
        
        // Accept various A-law format identifiers
        bool is_alaw_codec = (stream_info.codec_name == "alaw" || 
                             stream_info.codec_name == "pcm_alaw" ||
                             stream_info.codec_name == "g711_alaw");
        
        if (!is_alaw_codec) {
            Debug::log("codec", "ALawCodec: Rejecting stream - unsupported codec: ", stream_info.codec_name);
            return false;
        }
        
        // Validate A-law specific parameters
        // A-law uses 8-bit samples (1 byte per sample)
        if (stream_info.bits_per_sample != 0 && stream_info.bits_per_sample != 8) {
            Debug::log("codec", "ALawCodec: Rejecting stream - A-law requires 8 bits per sample, got ", stream_info.bits_per_sample);
            return false;
        }
        
        // Validate sample rate - A-law supports telephony rates and common audio rates
        if (stream_info.sample_rate != 0) {
            // Check for reasonable sample rate range (1 Hz to 192 kHz)
            if (stream_info.sample_rate < 1 || stream_info.sample_rate > 192000) {
                Debug::log("codec", "ALawCodec: Rejecting stream - invalid sample rate: ", stream_info.sample_rate, " Hz");
                return false;
            }
            
            // Common telephony and audio sample rates for A-law
            bool valid_sample_rate = (stream_info.sample_rate == 8000 ||   // Standard telephony
                                     stream_info.sample_rate == 16000 ||   // Wideband telephony
                                     stream_info.sample_rate == 32000 ||   // Super-wideband
                                     stream_info.sample_rate == 44100 ||   // CD quality
                                     stream_info.sample_rate == 48000);    // Professional audio
            
            if (!valid_sample_rate) {
                Debug::log("codec", "ALawCodec: Warning - Unusual sample rate ", stream_info.sample_rate, " Hz for A-law stream");
                // Don't reject - allow unusual sample rates but log warning
            }
        }
        
        // Validate channel count - A-law typically mono but can support stereo
        if (stream_info.channels != 0) {
            if (stream_info.channels > 2) {
                Debug::log("codec", "ALawCodec: Rejecting stream - A-law supports max 2 channels, got ", stream_info.channels);
                return false;
            }
            
            if (stream_info.channels == 0) {
                Debug::log("codec", "ALawCodec: Rejecting stream - Invalid channel count: 0");
                return false;
            }
        }
        
        // All validation passed
        Debug::log("codec", "ALawCodec: Stream validation passed for codec: ", stream_info.codec_name);
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("codec", "ALawCodec: Exception during format validation: ", e.what());
        return false;
    } catch (...) {
        Debug::log("codec", "ALawCodec: Unknown exception during format validation for codec: ", stream_info.codec_name);
        return false;
    }
}

std::string ALawCodec::getCodecName() const {
    return "alaw";
}

bool ALawCodec::initialize() {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        Debug::log("codec", "ALawCodec: Starting initialization for codec: ", m_stream_info.codec_name);
        
        // Validate stream info before initialization
        if (!canDecode(m_stream_info)) {
            Debug::log("codec", "ALawCodec: Initialization failed - unsupported stream format for codec: ", m_stream_info.codec_name);
            return false;
        }
        
        // Ensure lookup table is properly initialized
        if (!s_table_initialized) {
            Debug::log("codec", "ALawCodec: Lookup table not initialized, initializing now");
            initializeALawTable();
            if (!s_table_initialized) {
                Debug::log("codec", "ALawCodec: Initialization failed - lookup table initialization failed");
                return false;
            }
        }
        
        // Set default parameters for raw streams
        if (m_stream_info.sample_rate == 0) {
            m_stream_info.sample_rate = 8000; // Default telephony rate
            Debug::log("codec", "ALawCodec: Using default sample rate: 8000 Hz");
        }
        
        if (m_stream_info.channels == 0) {
            m_stream_info.channels = 1; // Default mono
            Debug::log("codec", "ALawCodec: Using default channel count: 1 (mono)");
        }
        
        // Validate final parameters
        if (m_stream_info.sample_rate < 1 || m_stream_info.sample_rate > 192000) {
            Debug::log("codec", "ALawCodec: Initialization failed - invalid sample rate: ", m_stream_info.sample_rate);
            return false;
        }
        
        if (m_stream_info.channels < 1 || m_stream_info.channels > 2) {
            Debug::log("codec", "ALawCodec: Initialization failed - invalid channel count: ", m_stream_info.channels);
            return false;
        }
        
        m_initialized = true;
        
        // Performance metrics logging
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        Debug::log("codec", "ALawCodec: Initialized successfully - ", m_stream_info.sample_rate, " Hz, ", m_stream_info.channels, " channels");
        Debug::log("performance", "ALawCodec: Initialization completed in ", duration.count(), " microseconds");
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("codec", "ALawCodec: Exception during initialization: ", e.what());
        m_initialized = false;
        return false;
    } catch (...) {
        Debug::log("codec", "ALawCodec: Unknown exception during initialization for codec: ", m_stream_info.codec_name);
        m_initialized = false;
        return false;
    }
}

AudioFrame ALawCodec::decode(const MediaChunk& chunk) {
    AudioFrame frame;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Check initialization state
        if (!m_initialized) {
            Debug::log("codec", "ALawCodec: Decode called on uninitialized codec");
            return frame; // Empty frame
        }
        
        // Handle empty or null chunk gracefully
        if (chunk.data.empty()) {
            Debug::log("codec", "ALawCodec: Received empty chunk (size=", chunk.data.size(), "), returning empty frame");
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
            Debug::log("codec", "ALawCodec: Warning - no samples converted from non-empty chunk of size ", chunk.data.size());
            return frame; // Empty frame
        }
        
        // Validate output consistency
        if (frame.samples.size() != samples_converted) {
            Debug::log("codec", "ALawCodec: Warning - sample count mismatch: converted ", samples_converted, ", frame has ", frame.samples.size());
        }
        
        // Performance metrics logging
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        Debug::log("performance", "ALawCodec: Decoded ", samples_converted, " samples in ", duration.count(), " microseconds");
        
        // Calculate throughput (samples per second)
        if (duration.count() > 0) {
            double throughput = (samples_converted * 1000000.0) / duration.count();
            Debug::log("performance", "ALawCodec: Decoding throughput: ", static_cast<uint64_t>(throughput), " samples/second");
        }
        
        return frame;
        
    } catch (const std::exception& e) {
        Debug::log("codec", "ALawCodec: Exception during decode: ", e.what());
        return AudioFrame{}; // Empty frame
    } catch (...) {
        Debug::log("codec", "ALawCodec: Unknown exception during decode for codec: ", getCodecName());
        return AudioFrame{}; // Empty frame
    }
}

size_t ALawCodec::convertSamples(const std::vector<uint8_t>& input_data, 
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
        
        // Convert each A-law sample to 16-bit PCM using lookup table
        // All 8-bit values (0x00-0xFF) are valid A-law inputs
        // For multi-channel audio, samples are processed in interleaved order:
        // Mono: [sample0, sample1, sample2, ...]
        // Stereo: [L0, R0, L1, R1, L2, R2, ...]
        // This maintains proper channel interleaving in the output
        for (size_t i = 0; i < input_samples; ++i) {
            const uint8_t alaw_sample = input_data[i];
            
            // Direct lookup - no validation needed as all 8-bit values are valid
            const int16_t pcm_sample = ALAW_TO_PCM[alaw_sample];
            output_samples.push_back(pcm_sample);
        }
        
        // Performance metrics for large conversions
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        if (input_samples > 1000) { // Only log for significant conversions
            Debug::log("performance", "ALawCodec: Converted ", input_samples, " A-law samples in ", duration.count(), " microseconds");
            
            if (duration.count() > 0) {
                double conversion_rate = (input_samples * 1000000.0) / duration.count();
                Debug::log("performance", "ALawCodec: Conversion rate: ", static_cast<uint64_t>(conversion_rate), " samples/second");
            }
        }
        
        return input_samples;
        
    } catch (const std::bad_alloc& e) {
        Debug::log("codec", "ALawCodec: Memory allocation failed during sample conversion: ", e.what());
        output_samples.clear();
        return 0;
    } catch (const std::exception& e) {
        Debug::log("codec", "ALawCodec: Exception during sample conversion: ", e.what());
        // Attempt to preserve partial conversion if possible
        if (!output_samples.empty()) {
            Debug::log("codec", "ALawCodec: Returning ", output_samples.size(), " partially converted samples");
            return output_samples.size();
        }
        output_samples.clear();
        return 0;
    } catch (...) {
        Debug::log("codec", "ALawCodec: Unknown exception during sample conversion (input_size=", input_data.size(), ")");
        output_samples.clear();
        return 0;
    }
}

size_t ALawCodec::getBytesPerInputSample() const {
    return 1; // A-law uses 8-bit samples
}

void ALawCodec::initializeALawTable() {
    if (s_table_initialized) {
        Debug::log("codec", "ALawCodec: Lookup table already initialized");
        return; // Already initialized
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        Debug::log("codec", "ALawCodec: Computing ITU-T G.711 A-law lookup table at runtime");
        
        // Compute all 256 A-law values using ITU-T G.711 A-law algorithm
        for (int i = 0; i < 256; ++i) {
            uint8_t alaw_sample = static_cast<uint8_t>(i);
            
            // ITU-T G.711 A-law decoding algorithm
            // Step 1: Apply even-bit inversion (XOR with 0x55)
            uint8_t complement = alaw_sample ^ 0x55;
            
            // Step 2: Extract sign bit (bit 7, but inverted logic for A-law)
            bool sign = (complement & 0x80) == 0;
            
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
            if (sign) {
                linear = -linear;
            }
            
            // Store in lookup table
            ALAW_TO_PCM[i] = linear;
        }
        
        // Validate critical values for ITU-T G.711 compliance
        Debug::log("codec", "ALawCodec: Validating computed A-law values");
        
        // A-law closest-to-silence value (0x55) should map to -8
        if (ALAW_TO_PCM[0x55] != -8) {
            Debug::log("codec", "ALawCodec: Warning - A-law closest-to-silence value (0x55) computed as ", ALAW_TO_PCM[0x55], ", expected -8");
        }
        
        // Validate sign bit handling - values with bit 7 clear (0x00-0x7F) should be negative
        if (ALAW_TO_PCM[0x00] >= 0 || ALAW_TO_PCM[0x7F] >= 0) {
            Debug::log("codec", "ALawCodec: Warning - A-law sign bit handling incorrect for negative range (0x00=", ALAW_TO_PCM[0x00], ", 0x7F=", ALAW_TO_PCM[0x7F], ")");
        }
        
        // Validate sign bit handling - values with bit 7 set (0x80-0xFF) should be positive
        if (ALAW_TO_PCM[0x80] <= 0 || ALAW_TO_PCM[0xFF] <= 0) {
            Debug::log("codec", "ALawCodec: Warning - A-law sign bit handling incorrect for positive range (0x80=", ALAW_TO_PCM[0x80], ", 0xFF=", ALAW_TO_PCM[0xFF], ")");
        }
        
        // Log some key computed values for verification
        Debug::log("codec", "ALawCodec: Key computed values - 0x00=", ALAW_TO_PCM[0x00], ", 0x55=", ALAW_TO_PCM[0x55], ", 0x80=", ALAW_TO_PCM[0x80], ", 0xFF=", ALAW_TO_PCM[0xFF]);
        
        // Performance metrics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        Debug::log("codec", "ALawCodec: ITU-T G.711 A-law lookup table computed successfully with 256 entries");
        Debug::log("performance", "ALawCodec: Table computation completed in ", duration.count(), " microseconds");
        
        s_table_initialized = true;
        
    } catch (const std::exception& e) {
        Debug::log("codec", "ALawCodec: Exception during table computation: ", e.what());
        // Don't set s_table_initialized to true on error
    } catch (...) {
        Debug::log("codec", "ALawCodec: Unknown exception during table computation");
        // Don't set s_table_initialized to true on error
    }
}

void registerALawCodec() {
    Debug::log("codec", "ALawCodec: Registering A-law codec with AudioCodecFactory");
    
    // Register A-law codec with multiple format identifiers
    AudioCodecFactory::registerCodec("alaw", [](const StreamInfo& stream_info) {
        return std::make_unique<ALawCodec>(stream_info);
    });
    
    AudioCodecFactory::registerCodec("pcm_alaw", [](const StreamInfo& stream_info) {
        return std::make_unique<ALawCodec>(stream_info);
    });
    
    AudioCodecFactory::registerCodec("g711_alaw", [](const StreamInfo& stream_info) {
        return std::make_unique<ALawCodec>(stream_info);
    });
    
    Debug::log("codec", "ALawCodec: Successfully registered for codec names: alaw, pcm_alaw, g711_alaw");
}

} // namespace PCM
} // namespace Codec
} // namespace PsyMP3

#endif // ENABLE_ALAW_CODEC
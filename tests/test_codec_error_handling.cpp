/*
 * test_codec_error_handling.cpp - Test comprehensive error handling for μ-law and A-law codecs
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <cassert>
#include <iostream>

// Test error handling for both codecs
int main() {
    std::cout << "Testing comprehensive error handling for μ-law and A-law codecs..." << std::endl;
    
    // Test 1: Invalid stream info handling
    {
        StreamInfo invalid_stream;
        invalid_stream.codec_type = "video"; // Wrong type
        invalid_stream.codec_name = "mulaw";
        
        MuLawCodec mulaw_codec(invalid_stream);
        assert(!mulaw_codec.canDecode(invalid_stream));
        std::cout << "✓ MuLawCodec correctly rejects invalid stream type" << std::endl;
        
        ALawCodec alaw_codec(invalid_stream);
        assert(!alaw_codec.canDecode(invalid_stream));
        std::cout << "✓ ALawCodec correctly rejects invalid stream type" << std::endl;
    }
    
    // Test 2: Invalid bits per sample
    {
        StreamInfo invalid_bits;
        invalid_bits.codec_type = "audio";
        invalid_bits.codec_name = "mulaw";
        invalid_bits.bits_per_sample = 16; // Should be 8
        
        MuLawCodec mulaw_codec(invalid_bits);
        assert(!mulaw_codec.canDecode(invalid_bits));
        std::cout << "✓ MuLawCodec correctly rejects invalid bits per sample" << std::endl;
        
        invalid_bits.codec_name = "alaw";
        ALawCodec alaw_codec(invalid_bits);
        assert(!alaw_codec.canDecode(invalid_bits));
        std::cout << "✓ ALawCodec correctly rejects invalid bits per sample" << std::endl;
    }
    
    // Test 3: Invalid channel count
    {
        StreamInfo invalid_channels;
        invalid_channels.codec_type = "audio";
        invalid_channels.codec_name = "mulaw";
        invalid_channels.channels = 8; // Too many channels
        
        MuLawCodec mulaw_codec(invalid_channels);
        assert(!mulaw_codec.canDecode(invalid_channels));
        std::cout << "✓ MuLawCodec correctly rejects too many channels" << std::endl;
        
        invalid_channels.codec_name = "alaw";
        ALawCodec alaw_codec(invalid_channels);
        assert(!alaw_codec.canDecode(invalid_channels));
        std::cout << "✓ ALawCodec correctly rejects too many channels" << std::endl;
    }
    
    // Test 4: Valid initialization
    {
        StreamInfo valid_stream;
        valid_stream.codec_type = "audio";
        valid_stream.codec_name = "mulaw";
        valid_stream.bits_per_sample = 8;
        valid_stream.channels = 1;
        valid_stream.sample_rate = 8000;
        
        MuLawCodec mulaw_codec(valid_stream);
        assert(mulaw_codec.canDecode(valid_stream));
        assert(mulaw_codec.initialize());
        std::cout << "✓ MuLawCodec initializes correctly with valid stream" << std::endl;
        
        valid_stream.codec_name = "alaw";
        ALawCodec alaw_codec(valid_stream);
        assert(alaw_codec.canDecode(valid_stream));
        assert(alaw_codec.initialize());
        std::cout << "✓ ALawCodec initializes correctly with valid stream" << std::endl;
    }
    
    // Test 5: Empty data handling
    {
        StreamInfo valid_stream;
        valid_stream.codec_type = "audio";
        valid_stream.codec_name = "mulaw";
        valid_stream.bits_per_sample = 8;
        valid_stream.channels = 1;
        valid_stream.sample_rate = 8000;
        
        MuLawCodec mulaw_codec(valid_stream);
        mulaw_codec.initialize();
        
        MediaChunk empty_chunk;
        empty_chunk.data.clear(); // Empty data
        
        AudioFrame frame = mulaw_codec.decode(empty_chunk);
        assert(frame.samples.empty());
        std::cout << "✓ MuLawCodec handles empty chunks gracefully" << std::endl;
        
        valid_stream.codec_name = "alaw";
        ALawCodec alaw_codec(valid_stream);
        alaw_codec.initialize();
        
        frame = alaw_codec.decode(empty_chunk);
        assert(frame.samples.empty());
        std::cout << "✓ ALawCodec handles empty chunks gracefully" << std::endl;
    }
    
    // Test 6: Sample conversion with all 8-bit values
    {
        StreamInfo valid_stream;
        valid_stream.codec_type = "audio";
        valid_stream.codec_name = "mulaw";
        valid_stream.bits_per_sample = 8;
        valid_stream.channels = 1;
        valid_stream.sample_rate = 8000;
        
        MuLawCodec mulaw_codec(valid_stream);
        mulaw_codec.initialize();
        
        // Test all 8-bit values (0x00-0xFF)
        MediaChunk test_chunk;
        for (int i = 0; i < 256; ++i) {
            test_chunk.data.push_back(static_cast<uint8_t>(i));
        }
        
        AudioFrame frame = mulaw_codec.decode(test_chunk);
        assert(frame.samples.size() == 256);
        std::cout << "✓ MuLawCodec accepts all 8-bit values as valid input" << std::endl;
        
        valid_stream.codec_name = "alaw";
        ALawCodec alaw_codec(valid_stream);
        alaw_codec.initialize();
        
        frame = alaw_codec.decode(test_chunk);
        assert(frame.samples.size() == 256);
        std::cout << "✓ ALawCodec accepts all 8-bit values as valid input" << std::endl;
    }
    
    std::cout << "\nAll error handling tests passed! ✓" << std::endl;
    return 0;
}
/*
 * test_flac_audioframe_creation.cpp - Test FLAC codec AudioFrame creation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_FLAC

/**
 * @brief Test FLAC codec AudioFrame creation and validation
 */
class FLACCodecAudioFrameTest {
public:
    static bool runTests() {
        std::cout << "Running FLAC AudioFrame creation tests..." << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testBasicAudioFrameCreation();
        all_passed &= testTimestampCalculation();
        all_passed &= testSampleValidation();
        all_passed &= testSilenceFrameCreation();
        
        if (all_passed) {
            std::cout << "All FLAC AudioFrame tests passed!" << std::endl;
        } else {
            std::cout << "Some FLAC AudioFrame tests failed!" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool testBasicAudioFrameCreation() {
        std::cout << "  Testing basic AudioFrame creation..." << std::endl;
        
        try {
            // Create a test StreamInfo for FLAC
            StreamInfo stream_info;
            stream_info.codec_name = "flac";
            stream_info.sample_rate = 44100;
            stream_info.channels = 2;
            stream_info.bits_per_sample = 16;
            stream_info.duration_samples = 1000000;
            
            // Create FLAC codec
            FLACCodec codec(stream_info);
            
            if (!codec.initialize()) {
                std::cout << "    ERROR: Failed to initialize FLAC codec" << std::endl;
                return false;
            }
            
            // Test canDecode
            if (!codec.canDecode(stream_info)) {
                std::cout << "    ERROR: Codec should be able to decode FLAC stream" << std::endl;
                return false;
            }
            
            // Test getCodecName
            if (codec.getCodecName() != "flac") {
                std::cout << "    ERROR: Codec name should be 'flac', got: " << codec.getCodecName() << std::endl;
                return false;
            }
            
            std::cout << "    Basic AudioFrame creation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "    ERROR: Exception in basic AudioFrame creation test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testTimestampCalculation() {
        std::cout << "  Testing timestamp calculation..." << std::endl;
        
        try {
            // Create a test StreamInfo for FLAC
            StreamInfo stream_info;
            stream_info.codec_name = "flac";
            stream_info.sample_rate = 48000;
            stream_info.channels = 2;
            stream_info.bits_per_sample = 24;
            
            // Create FLAC codec
            FLACCodec codec(stream_info);
            
            if (!codec.initialize()) {
                std::cout << "    ERROR: Failed to initialize FLAC codec" << std::endl;
                return false;
            }
            
            // Test getCurrentSample
            uint64_t initial_sample = codec.getCurrentSample();
            if (initial_sample != 0) {
                std::cout << "    ERROR: Initial sample position should be 0, got: " << initial_sample << std::endl;
                return false;
            }
            
            // Test supportsSeekReset
            if (!codec.supportsSeekReset()) {
                std::cout << "    ERROR: FLAC codec should support seek reset" << std::endl;
                return false;
            }
            
            std::cout << "    Timestamp calculation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "    ERROR: Exception in timestamp calculation test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testSampleValidation() {
        std::cout << "  Testing sample validation..." << std::endl;
        
        try {
            // Test with various invalid configurations
            StreamInfo invalid_stream;
            invalid_stream.codec_name = "flac";
            invalid_stream.sample_rate = 0; // Invalid
            invalid_stream.channels = 0; // Invalid
            invalid_stream.bits_per_sample = 0; // Invalid
            
            FLACCodec codec(invalid_stream);
            
            // Should not be able to decode invalid stream
            if (codec.canDecode(invalid_stream)) {
                std::cout << "    ERROR: Codec should reject invalid stream configuration" << std::endl;
                return false;
            }
            
            // Test with valid configuration
            StreamInfo valid_stream;
            valid_stream.codec_name = "flac";
            valid_stream.sample_rate = 44100;
            valid_stream.channels = 2;
            valid_stream.bits_per_sample = 16;
            
            if (!codec.canDecode(valid_stream)) {
                std::cout << "    ERROR: Codec should accept valid stream configuration" << std::endl;
                return false;
            }
            
            std::cout << "    Sample validation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "    ERROR: Exception in sample validation test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testSilenceFrameCreation() {
        std::cout << "  Testing silence frame creation..." << std::endl;
        
        try {
            // Create a test StreamInfo for FLAC
            StreamInfo stream_info;
            stream_info.codec_name = "flac";
            stream_info.sample_rate = 44100;
            stream_info.channels = 2;
            stream_info.bits_per_sample = 16;
            
            // Create FLAC codec
            FLACCodec codec(stream_info);
            
            if (!codec.initialize()) {
                std::cout << "    ERROR: Failed to initialize FLAC codec" << std::endl;
                return false;
            }
            
            // Test flush (should return empty frame initially)
            AudioFrame flush_frame = codec.flush();
            if (flush_frame.getSampleFrameCount() != 0) {
                std::cout << "    ERROR: Initial flush should return empty frame" << std::endl;
                return false;
            }
            
            // Test reset
            codec.reset();
            
            // Verify position is reset
            if (codec.getCurrentSample() != 0) {
                std::cout << "    ERROR: Position should be reset to 0 after reset()" << std::endl;
                return false;
            }
            
            std::cout << "    Silence frame creation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "    ERROR: Exception in silence frame creation test: " << e.what() << std::endl;
            return false;
        }
    }
};

int main() {
    std::cout << "FLAC AudioFrame Creation Test Suite" << std::endl;
    std::cout << "====================================" << std::endl;
    
    bool success = FLACCodecAudioFrameTest::runTests();
    
    return success ? 0 : 1;
}

#else

int main() {
    std::cout << "FLAC support not available - skipping tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC
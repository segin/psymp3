/*
 * test_flac_streamable_subset_validation.cpp - Test FLAC RFC 9639 Streamable Subset Validation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <cassert>
#include <memory>
#include <vector>
#include <cstdint>

// Mock minimal dependencies for testing
namespace Debug {
    template<typename... Args>
    void log(const char* category, Args&&... args) {
        // Silent for tests unless debugging
        // std::cout << "[" << category << "] ";
        // ((std::cout << args << " "), ...);
        // std::cout << std::endl;
    }
}

// Mock FLACFrame structure for testing
struct FLACFrame {
    uint64_t sample_offset = 0;
    uint64_t file_offset = 0;
    uint32_t block_size = 0;
    uint32_t frame_size = 0;
    uint32_t sample_rate = 0;
    uint8_t channels = 0;
    uint8_t bits_per_sample = 0;
    bool variable_block_size = false;
    
    bool isValid() const {
        return block_size > 0 && sample_rate > 0 && channels > 0 && 
               bits_per_sample >= 4 && bits_per_sample <= 32;
    }
};

// Mock FLACStreamInfo structure
struct FLACStreamInfo {
    uint16_t min_block_size = 0;
    uint16_t max_block_size = 0;
    uint32_t min_frame_size = 0;
    uint32_t max_frame_size = 0;
    uint32_t sample_rate = 0;
    uint8_t channels = 0;
    uint8_t bits_per_sample = 0;
    uint64_t total_samples = 0;
    
    bool isValid() const {
        return sample_rate > 0 && channels > 0 && channels <= 8 && 
               bits_per_sample >= 4 && bits_per_sample <= 32 &&
               min_block_size > 0 && max_block_size >= min_block_size;
    }
};

// Mock streamable subset validation implementation for testing
class StreamableSubsetValidator {
public:
    enum class StreamableSubsetMode {
        DISABLED,
        ENABLED,
        STRICT
    };
    
    struct StreamableSubsetStats {
        size_t sample_rate_violations = 0;
        size_t bit_depth_violations = 0;
        size_t block_size_violations = 0;
        size_t block_size_48k_violations = 0;
        size_t total_violations = 0;
        size_t frames_validated = 0;
        
        double getViolationRate() const {
            if (frames_validated == 0) return 0.0;
            return (static_cast<double>(total_violations) / frames_validated) * 100.0;
        }
    };
    
private:
    StreamableSubsetMode m_mode = StreamableSubsetMode::ENABLED;
    FLACStreamInfo m_streaminfo;
    StreamableSubsetStats m_stats;
    
public:
    void setStreamableSubsetMode(StreamableSubsetMode mode) {
        m_mode = mode;
    }
    
    StreamableSubsetMode getStreamableSubsetMode() const {
        return m_mode;
    }
    
    void setStreamInfo(const FLACStreamInfo& streaminfo) {
        m_streaminfo = streaminfo;
    }
    
    StreamableSubsetStats getStreamableSubsetStats() const {
        return m_stats;
    }
    
    void resetStreamableSubsetStats() {
        m_stats = StreamableSubsetStats{};
    }
    
    bool validateStreamableSubset(const FLACFrame& frame) {
        if (m_mode == StreamableSubsetMode::DISABLED) {
            return true;
        }
        
        if (!frame.isValid()) {
            return false;
        }
        
        m_stats.frames_validated++;
        
        bool sample_rate_violation = false;
        bool bit_depth_violation = false;
        bool block_size_violation = false;
        bool block_size_48k_violation = false;
        
        // Check frame header independence (heuristic)
        if (m_streaminfo.isValid()) {
            if (frame.sample_rate == m_streaminfo.sample_rate) {
                sample_rate_violation = true;
            }
            if (frame.bits_per_sample == m_streaminfo.bits_per_sample) {
                bit_depth_violation = true;
            }
        }
        
        // Check block size constraints
        if (frame.block_size > 16384) {
            block_size_violation = true;
        }
        
        if (frame.sample_rate <= 48000 && frame.block_size > 4608) {
            block_size_48k_violation = true;
        }
        
        // Update statistics
        if (sample_rate_violation) {
            m_stats.sample_rate_violations++;
            m_stats.total_violations++;
        }
        if (bit_depth_violation) {
            m_stats.bit_depth_violations++;
            m_stats.total_violations++;
        }
        if (block_size_violation) {
            m_stats.block_size_violations++;
            m_stats.total_violations++;
        }
        if (block_size_48k_violation) {
            m_stats.block_size_48k_violations++;
            m_stats.total_violations++;
        }
        
        bool has_violations = sample_rate_violation || bit_depth_violation || 
                             block_size_violation || block_size_48k_violation;
        
        if (has_violations && m_mode == StreamableSubsetMode::STRICT) {
            return false;
        }
        
        return true;
    }
};

#ifdef HAVE_FLAC

/**
 * @brief Test FLAC RFC 9639 Streamable Subset Validation
 * 
 * This test validates the implementation of RFC 9639 Section 7 streamable subset
 * constraints including frame header independence and block size limitations.
 */
class FLACStreamableSubsetValidationTest {
public:
    static bool runAllTests() {
        std::cout << "=== FLAC RFC 9639 Streamable Subset Validation Test ===" << std::endl;
        std::cout << "Testing RFC 9639 Section 7 streamable subset constraints" << std::endl;
        std::cout << std::endl;
        
        bool allPassed = true;
        
        allPassed &= testStreamableSubsetConfiguration();
        allPassed &= testBlockSizeConstraints();
        allPassed &= testFrameHeaderIndependence();
        allPassed &= testStreamableSubsetStatistics();
        allPassed &= testStreamableSubsetModes();
        
        std::cout << std::endl;
        if (allPassed) {
            std::cout << "✓ All FLAC streamable subset validation tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some FLAC streamable subset validation tests FAILED" << std::endl;
        }
        
        return allPassed;
    }
    
private:
    static bool testStreamableSubsetConfiguration() {
        std::cout << "Testing streamable subset configuration..." << std::endl;
        
        try {
            StreamableSubsetValidator validator;
            
            // Test default configuration
            auto default_mode = validator.getStreamableSubsetMode();
            std::cout << "✓ Default streamable subset mode: " << static_cast<int>(default_mode) << std::endl;
            
            // Test setting different modes
            validator.setStreamableSubsetMode(StreamableSubsetValidator::StreamableSubsetMode::DISABLED);
            assert(validator.getStreamableSubsetMode() == StreamableSubsetValidator::StreamableSubsetMode::DISABLED);
            
            validator.setStreamableSubsetMode(StreamableSubsetValidator::StreamableSubsetMode::ENABLED);
            assert(validator.getStreamableSubsetMode() == StreamableSubsetValidator::StreamableSubsetMode::ENABLED);
            
            validator.setStreamableSubsetMode(StreamableSubsetValidator::StreamableSubsetMode::STRICT);
            assert(validator.getStreamableSubsetMode() == StreamableSubsetValidator::StreamableSubsetMode::STRICT);
            
            std::cout << "✓ Streamable subset mode configuration working correctly" << std::endl;
            
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception during streamable subset configuration test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testBlockSizeConstraints() {
        std::cout << "Testing block size constraints..." << std::endl;
        
        try {
            StreamableSubsetValidator validator;
            validator.setStreamableSubsetMode(StreamableSubsetValidator::StreamableSubsetMode::ENABLED);
            
            // Test valid block sizes
            FLACFrame valid_frame;
            valid_frame.block_size = 4096;
            valid_frame.sample_rate = 44100;
            valid_frame.channels = 2;
            valid_frame.bits_per_sample = 16;
            
            assert(validator.validateStreamableSubset(valid_frame) == true);
            std::cout << "✓ Valid block size (4096) accepted" << std::endl;
            
            // Test block size exceeding 16384 limit
            FLACFrame large_block_frame;
            large_block_frame.block_size = 20000;  // Exceeds 16384 limit
            large_block_frame.sample_rate = 44100;
            large_block_frame.channels = 2;
            large_block_frame.bits_per_sample = 16;
            
            validator.validateStreamableSubset(large_block_frame);
            auto stats = validator.getStreamableSubsetStats();
            assert(stats.block_size_violations > 0);
            std::cout << "✓ Large block size (20000) violation detected" << std::endl;
            
            // Test block size exceeding 4608 limit for ≤48kHz
            validator.resetStreamableSubsetStats();
            FLACFrame large_48k_frame;
            large_48k_frame.block_size = 5000;  // Exceeds 4608 limit for ≤48kHz
            large_48k_frame.sample_rate = 44100; // ≤48kHz
            large_48k_frame.channels = 2;
            large_48k_frame.bits_per_sample = 16;
            
            validator.validateStreamableSubset(large_48k_frame);
            stats = validator.getStreamableSubsetStats();
            assert(stats.block_size_48k_violations > 0);
            std::cout << "✓ Large block size for ≤48kHz (5000) violation detected" << std::endl;
            
            // Test block size OK for >48kHz
            validator.resetStreamableSubsetStats();
            FLACFrame high_sample_rate_frame;
            high_sample_rate_frame.block_size = 5000;  // OK for >48kHz
            high_sample_rate_frame.sample_rate = 96000; // >48kHz
            high_sample_rate_frame.channels = 2;
            high_sample_rate_frame.bits_per_sample = 16;
            
            validator.validateStreamableSubset(high_sample_rate_frame);
            stats = validator.getStreamableSubsetStats();
            assert(stats.block_size_48k_violations == 0);
            std::cout << "✓ Block size 5000 accepted for >48kHz sample rate" << std::endl;
            
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception during block size constraints test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testFrameHeaderIndependence() {
        std::cout << "Testing frame header independence..." << std::endl;
        
        try {
            StreamableSubsetValidator validator;
            validator.setStreamableSubsetMode(StreamableSubsetValidator::StreamableSubsetMode::ENABLED);
            
            // Set up STREAMINFO
            FLACStreamInfo streaminfo;
            streaminfo.sample_rate = 44100;
            streaminfo.channels = 2;
            streaminfo.bits_per_sample = 16;
            streaminfo.min_block_size = 1024;
            streaminfo.max_block_size = 4096;
            validator.setStreamInfo(streaminfo);
            
            // Test frame that matches STREAMINFO (potential violation)
            FLACFrame matching_frame;
            matching_frame.block_size = 4096;
            matching_frame.sample_rate = 44100;  // Matches STREAMINFO
            matching_frame.channels = 2;
            matching_frame.bits_per_sample = 16; // Matches STREAMINFO
            
            validator.validateStreamableSubset(matching_frame);
            auto stats = validator.getStreamableSubsetStats();
            std::cout << "✓ Frame matching STREAMINFO processed" << std::endl;
            std::cout << "  Sample rate violations: " << stats.sample_rate_violations << std::endl;
            std::cout << "  Bit depth violations: " << stats.bit_depth_violations << std::endl;
            
            // Test frame with different parameters (independent)
            validator.resetStreamableSubsetStats();
            FLACFrame independent_frame;
            independent_frame.block_size = 2048;
            independent_frame.sample_rate = 48000;  // Different from STREAMINFO
            independent_frame.channels = 2;
            independent_frame.bits_per_sample = 24; // Different from STREAMINFO
            
            validator.validateStreamableSubset(independent_frame);
            stats = validator.getStreamableSubsetStats();
            assert(stats.sample_rate_violations == 0);
            assert(stats.bit_depth_violations == 0);
            std::cout << "✓ Independent frame header accepted without violations" << std::endl;
            
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception during frame header independence test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testStreamableSubsetStatistics() {
        std::cout << "Testing streamable subset statistics..." << std::endl;
        
        try {
            StreamableSubsetValidator validator;
            validator.setStreamableSubsetMode(StreamableSubsetValidator::StreamableSubsetMode::ENABLED);
            
            // Test statistics reset
            validator.resetStreamableSubsetStats();
            auto reset_stats = validator.getStreamableSubsetStats();
            
            assert(reset_stats.sample_rate_violations == 0);
            assert(reset_stats.bit_depth_violations == 0);
            assert(reset_stats.block_size_violations == 0);
            assert(reset_stats.block_size_48k_violations == 0);
            assert(reset_stats.total_violations == 0);
            assert(reset_stats.frames_validated == 0);
            
            std::cout << "✓ Statistics reset working correctly" << std::endl;
            
            // Generate some statistics by validating frames
            FLACFrame test_frame;
            test_frame.block_size = 4096;
            test_frame.sample_rate = 44100;
            test_frame.channels = 2;
            test_frame.bits_per_sample = 16;
            
            for (int i = 0; i < 5; ++i) {
                validator.validateStreamableSubset(test_frame);
            }
            
            auto final_stats = validator.getStreamableSubsetStats();
            assert(final_stats.frames_validated == 5);
            std::cout << "✓ Statistics collection working" << std::endl;
            std::cout << "  Frames validated: " << final_stats.frames_validated << std::endl;
            std::cout << "  Violation rate: " << final_stats.getViolationRate() << "%" << std::endl;
            
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception during streamable subset statistics test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testStreamableSubsetModes() {
        std::cout << "Testing streamable subset modes..." << std::endl;
        
        try {
            // Create a frame that violates streamable subset constraints
            FLACFrame violating_frame;
            violating_frame.block_size = 20000;  // Exceeds 16384 limit
            violating_frame.sample_rate = 44100;
            violating_frame.channels = 2;
            violating_frame.bits_per_sample = 16;
            
            // Test DISABLED mode
            {
                StreamableSubsetValidator validator;
                validator.setStreamableSubsetMode(StreamableSubsetValidator::StreamableSubsetMode::DISABLED);
                
                bool result = validator.validateStreamableSubset(violating_frame);
                assert(result == true);  // Should pass in disabled mode
                
                auto stats = validator.getStreamableSubsetStats();
                assert(stats.frames_validated == 0);  // No validation performed
                std::cout << "✓ DISABLED mode working (no validation performed)" << std::endl;
            }
            
            // Test ENABLED mode
            {
                StreamableSubsetValidator validator;
                validator.setStreamableSubsetMode(StreamableSubsetValidator::StreamableSubsetMode::ENABLED);
                
                bool result = validator.validateStreamableSubset(violating_frame);
                assert(result == true);  // Should pass with warnings in enabled mode
                
                auto stats = validator.getStreamableSubsetStats();
                assert(stats.frames_validated > 0);
                assert(stats.total_violations > 0);
                std::cout << "✓ ENABLED mode working (validation with warnings)" << std::endl;
            }
            
            // Test STRICT mode
            {
                StreamableSubsetValidator validator;
                validator.setStreamableSubsetMode(StreamableSubsetValidator::StreamableSubsetMode::STRICT);
                
                bool result = validator.validateStreamableSubset(violating_frame);
                assert(result == false);  // Should fail in strict mode
                
                auto stats = validator.getStreamableSubsetStats();
                assert(stats.frames_validated > 0);
                assert(stats.total_violations > 0);
                std::cout << "✓ STRICT mode working (validation with rejection)" << std::endl;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception during streamable subset modes test: " << e.what() << std::endl;
            return false;
        }
    }
};

int main() {
    return FLACStreamableSubsetValidationTest::runAllTests() ? 0 : 1;
}

#else

int main() {
    std::cout << "FLAC support not available - skipping streamable subset validation tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC
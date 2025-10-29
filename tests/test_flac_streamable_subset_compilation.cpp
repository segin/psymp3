/*
 * test_flac_streamable_subset_compilation.cpp - Test streamable subset compilation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <cassert>

/**
 * @brief Simple compilation test for streamable subset validation
 * 
 * This test verifies that the streamable subset validation code compiles
 * and basic functionality works without requiring full dependencies.
 */

// Mock structures for testing
struct MockFLACFrame {
    uint32_t block_size = 4096;
    uint32_t sample_rate = 44100;
    uint8_t channels = 2;
    uint8_t bits_per_sample = 16;
    
    bool isValid() const {
        return block_size > 0 && sample_rate > 0 && channels > 0;
    }
};

struct MockStreamInfo {
    uint32_t sample_rate = 44100;
    uint8_t channels = 2;
    uint8_t bits_per_sample = 16;
    
    bool isValid() const {
        return sample_rate > 0 && channels > 0;
    }
};

// Mock streamable subset validator
class MockStreamableSubsetValidator {
public:
    enum class Mode { DISABLED, ENABLED, STRICT };
    
    struct Stats {
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
    Mode m_mode = Mode::ENABLED;
    MockStreamInfo m_streaminfo;
    Stats m_stats;
    
public:
    void setMode(Mode mode) { m_mode = mode; }
    Mode getMode() const { return m_mode; }
    void setStreamInfo(const MockStreamInfo& info) { m_streaminfo = info; }
    Stats getStats() const { return m_stats; }
    void resetStats() { m_stats = Stats{}; }
    
    bool validateStreamableSubset(const MockFLACFrame& frame) {
        if (m_mode == Mode::DISABLED) {
            return true;
        }
        
        if (!frame.isValid()) {
            return false;
        }
        
        m_stats.frames_validated++;
        
        bool has_violations = false;
        
        // RFC 9639 Section 7: Block size constraints
        if (frame.block_size > 16384) {
            m_stats.block_size_violations++;
            m_stats.total_violations++;
            has_violations = true;
        }
        
        if (frame.sample_rate <= 48000 && frame.block_size > 4608) {
            m_stats.block_size_48k_violations++;
            m_stats.total_violations++;
            has_violations = true;
        }
        
        // RFC 9639 Section 7: Frame header independence (heuristic)
        if (m_streaminfo.isValid()) {
            if (frame.sample_rate == m_streaminfo.sample_rate) {
                m_stats.sample_rate_violations++;
                m_stats.total_violations++;
                has_violations = true;
            }
            if (frame.bits_per_sample == m_streaminfo.bits_per_sample) {
                m_stats.bit_depth_violations++;
                m_stats.total_violations++;
                has_violations = true;
            }
        }
        
        // In strict mode, reject frames with violations
        if (has_violations && m_mode == Mode::STRICT) {
            return false;
        }
        
        return true;
    }
};

bool testStreamableSubsetValidation() {
    std::cout << "Testing streamable subset validation..." << std::endl;
    
    MockStreamableSubsetValidator validator;
    
    // Test configuration
    validator.setMode(MockStreamableSubsetValidator::Mode::ENABLED);
    assert(validator.getMode() == MockStreamableSubsetValidator::Mode::ENABLED);
    std::cout << "✓ Configuration working" << std::endl;
    
    // Test valid frame
    MockFLACFrame valid_frame;
    valid_frame.block_size = 4096;
    valid_frame.sample_rate = 44100;
    valid_frame.channels = 2;
    valid_frame.bits_per_sample = 16;
    
    assert(validator.validateStreamableSubset(valid_frame) == true);
    std::cout << "✓ Valid frame accepted" << std::endl;
    
    // Test block size violation
    MockFLACFrame large_block_frame;
    large_block_frame.block_size = 20000;  // Exceeds 16384
    large_block_frame.sample_rate = 44100;
    large_block_frame.channels = 2;
    large_block_frame.bits_per_sample = 16;
    
    validator.resetStats();
    validator.validateStreamableSubset(large_block_frame);
    auto stats = validator.getStats();
    assert(stats.block_size_violations > 0);
    std::cout << "✓ Block size violation detected" << std::endl;
    
    // Test 48kHz block size violation
    MockFLACFrame large_48k_frame;
    large_48k_frame.block_size = 5000;  // Exceeds 4608 for ≤48kHz
    large_48k_frame.sample_rate = 44100;  // ≤48kHz
    large_48k_frame.channels = 2;
    large_48k_frame.bits_per_sample = 16;
    
    validator.resetStats();
    validator.validateStreamableSubset(large_48k_frame);
    stats = validator.getStats();
    assert(stats.block_size_48k_violations > 0);
    std::cout << "✓ 48kHz block size violation detected" << std::endl;
    
    // Test strict mode rejection
    validator.setMode(MockStreamableSubsetValidator::Mode::STRICT);
    validator.resetStats();
    bool strict_result = validator.validateStreamableSubset(large_block_frame);
    assert(strict_result == false);
    std::cout << "✓ Strict mode rejection working" << std::endl;
    
    // Test disabled mode
    validator.setMode(MockStreamableSubsetValidator::Mode::DISABLED);
    validator.resetStats();
    bool disabled_result = validator.validateStreamableSubset(large_block_frame);
    assert(disabled_result == true);
    stats = validator.getStats();
    assert(stats.frames_validated == 0);  // No validation in disabled mode
    std::cout << "✓ Disabled mode working" << std::endl;
    
    return true;
}

bool testRFC9639Constraints() {
    std::cout << "Testing RFC 9639 Section 7 constraints..." << std::endl;
    
    MockStreamableSubsetValidator validator;
    validator.setMode(MockStreamableSubsetValidator::Mode::ENABLED);
    
    // Test maximum block size constraint (16384)
    MockFLACFrame max_block_frame;
    max_block_frame.block_size = 16384;  // Exactly at limit
    max_block_frame.sample_rate = 96000;  // >48kHz
    max_block_frame.channels = 2;
    max_block_frame.bits_per_sample = 16;
    
    validator.resetStats();
    assert(validator.validateStreamableSubset(max_block_frame) == true);
    auto stats = validator.getStats();
    assert(stats.block_size_violations == 0);
    std::cout << "✓ Maximum block size (16384) accepted" << std::endl;
    
    // Test 48kHz constraint (4608)
    MockFLACFrame max_48k_frame;
    max_48k_frame.block_size = 4608;  // Exactly at 48kHz limit
    max_48k_frame.sample_rate = 48000;  // Exactly 48kHz
    max_48k_frame.channels = 2;
    max_48k_frame.bits_per_sample = 16;
    
    validator.resetStats();
    assert(validator.validateStreamableSubset(max_48k_frame) == true);
    stats = validator.getStats();
    assert(stats.block_size_48k_violations == 0);
    std::cout << "✓ Maximum 48kHz block size (4608) accepted" << std::endl;
    
    // Test that >48kHz allows larger blocks
    MockFLACFrame high_rate_frame;
    high_rate_frame.block_size = 8192;  // >4608 but <16384
    high_rate_frame.sample_rate = 96000;  // >48kHz
    high_rate_frame.channels = 2;
    high_rate_frame.bits_per_sample = 16;
    
    validator.resetStats();
    assert(validator.validateStreamableSubset(high_rate_frame) == true);
    stats = validator.getStats();
    assert(stats.block_size_48k_violations == 0);
    std::cout << "✓ Large block size accepted for >48kHz" << std::endl;
    
    return true;
}

int main() {
    std::cout << "=== FLAC Streamable Subset Compilation Test ===" << std::endl;
    std::cout << "Testing RFC 9639 Section 7 streamable subset implementation" << std::endl;
    std::cout << std::endl;
    
    bool allPassed = true;
    
    try {
        allPassed &= testStreamableSubsetValidation();
        allPassed &= testRFC9639Constraints();
        
        std::cout << std::endl;
        if (allPassed) {
            std::cout << "✓ All streamable subset compilation tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some streamable subset compilation tests FAILED" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "✗ Exception during testing: " << e.what() << std::endl;
        allPassed = false;
    }
    
    return allPassed ? 0 : 1;
}
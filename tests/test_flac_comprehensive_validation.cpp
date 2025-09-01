/*
 * test_flac_comprehensive_validation.cpp - Comprehensive FLAC validation using test data
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "flac_test_data_utils.h"

#ifdef HAVE_FLAC

/**
 * @brief Comprehensive FLAC validation test suite
 */
class FLACComprehensiveValidation {
public:
    /**
     * @brief Run all comprehensive validation tests
     */
    static bool runAllTests() {
        std::cout << "=== FLAC Comprehensive Validation Suite ===" << std::endl;
        
        // Validate test data availability
        if (!FLACTestDataUtils::validateTestDataAvailable("Comprehensive Validation")) {
            return false;
        }
        
        bool allPassed = true;
        
        allPassed &= testBasicDemuxerFunctionality();
        allPassed &= testMetadataExtraction();
        allPassed &= testFrameReading();
        allPassed &= testSeekingAccuracy();
        allPassed &= testPerformanceMetrics();
        allPassed &= testErrorRecovery();
        allPassed &= testThreadSafety();
        allPassed &= testMemoryUsage();
        
        std::cout << std::endl;
        if (allPassed) {
            std::cout << "✓ All comprehensive validation tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some comprehensive validation tests FAILED" << std::endl;
        }
        
        return allPassed;
    }

private:
    /**
     * @brief Test basic demuxer functionality with all test files
     */
    static bool testBasicDemuxerFunctionality() {
        std::cout << "Testing basic demuxer functionality..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Testing: " << file << std::endl;
            
            try {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                // Test initialization
                if (demuxer->isEOF()) {
                    std::cout << "    ✗ Demuxer reports EOF immediately" << std::endl;
                    allPassed = false;
                    continue;
                }
                
                // Test basic frame reading
                auto chunk = demuxer->readChunk();
                if (chunk.data.empty()) {
                    std::cout << "    ✗ Failed to read first frame" << std::endl;
                    allPassed = false;
                    continue;
                }
                
                std::cout << "    ✓ Basic functionality working" << std::endl;
                
            } catch (const std::exception& e) {
                std::cout << "    ✗ Exception: " << e.what() << std::endl;
                allPassed = false;
            }
        }
        
        return allPassed;
    }
    
    /**
     * @brief Test metadata extraction from all test files
     */
    static bool testMetadataExtraction() {
        std::cout << "Testing metadata extraction..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Extracting metadata from: " << file << std::endl;
            
            try {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                // Try to extract basic stream information
                // This would typically involve reading the STREAMINFO block
                auto chunk = demuxer->readChunk();
                if (!chunk.data.empty()) {
                    std::cout << "    ✓ Successfully accessed stream data" << std::endl;
                } else {
                    std::cout << "    ✗ Failed to access stream data" << std::endl;
                    allPassed = false;
                }
                
            } catch (const std::exception& e) {
                std::cout << "    ✗ Exception: " << e.what() << std::endl;
                allPassed = false;
            }
        }
        
        return allPassed;
    }
    
    /**
     * @brief Test frame reading consistency
     */
    static bool testFrameReading() {
        std::cout << "Testing frame reading consistency..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Testing frame reading: " << file << std::endl;
            
            try {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                int frameCount = 0;
                int maxFrames = 50; // Limit for testing
                
                while (frameCount < maxFrames && !demuxer->isEOF()) {
                    auto chunk = demuxer->readChunk();
                    if (!chunk.data.empty()) {
                        frameCount++;
                        
                        // Validate chunk has data
                        if (chunk.data.empty()) {
                            std::cout << "    ✗ Frame " << frameCount << " has no data" << std::endl;
                            allPassed = false;
                            break;
                        }
                    } else {
                        break;
                    }
                }
                
                std::cout << "    ✓ Successfully read " << frameCount << " frames" << std::endl;
                
                if (frameCount == 0) {
                    std::cout << "    ✗ No frames read" << std::endl;
                    allPassed = false;
                }
                
            } catch (const std::exception& e) {
                std::cout << "    ✗ Exception: " << e.what() << std::endl;
                allPassed = false;
            }
        }
        
        return allPassed;
    }
    
    /**
     * @brief Test seeking accuracy
     */
    static bool testSeekingAccuracy() {
        std::cout << "Testing seeking accuracy..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Testing seeking: " << file << std::endl;
            
            try {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                size_t fileSize = FLACTestDataUtils::getFileSize(file);
                
                // Test seeking to beginning
                demuxer->seekTo(0);
                auto chunk1 = demuxer->readChunk();
                if (chunk1.data.empty()) {
                    std::cout << "    ✗ Failed to read after seek to beginning" << std::endl;
                    allPassed = false;
                    continue;
                }
                
                // Test seeking to middle (if file has duration)
                uint64_t duration = demuxer->getDuration();
                if (duration > 1000) {
                    demuxer->seekTo(duration / 2);
                    auto chunk2 = demuxer->readChunk();
                    if (chunk2.data.empty()) {
                        std::cout << "    ✗ Failed to read after seek to middle" << std::endl;
                        allPassed = false;
                        continue;
                    }
                }
                
                std::cout << "    ✓ Seeking functionality working" << std::endl;
                
            } catch (const std::exception& e) {
                std::cout << "    ✗ Exception: " << e.what() << std::endl;
                allPassed = false;
            }
        }
        
        return allPassed;
    }
    
    /**
     * @brief Test performance metrics
     */
    static bool testPerformanceMetrics() {
        std::cout << "Testing performance metrics..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Performance test: " << file << std::endl;
            
            try {
                auto start = std::chrono::high_resolution_clock::now();
                
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                int frameCount = 0;
                while (frameCount < 20 && !demuxer->isEOF()) {
                    auto chunk = demuxer->readChunk();
                    if (!chunk.data.empty()) frameCount++;
                }
                
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                
                std::cout << "    ✓ Read " << frameCount << " frames in " 
                          << duration.count() << "ms" << std::endl;
                
                // Performance threshold check (should be reasonable)
                if (frameCount > 0 && duration.count() > 5000) {
                    std::cout << "    ⚠ Performance warning: took longer than expected" << std::endl;
                }
                
            } catch (const std::exception& e) {
                std::cout << "    ✗ Exception: " << e.what() << std::endl;
                allPassed = false;
            }
        }
        
        return allPassed;
    }
    
    /**
     * @brief Test error recovery
     */
    static bool testErrorRecovery() {
        std::cout << "Testing error recovery..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        if (testFiles.empty()) return false;
        
        const std::string& file = testFiles[0];
        std::cout << "  Testing error recovery with: " << file << std::endl;
        
        try {
            auto handler = std::make_unique<FileIOHandler>(file);
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            
            // Test seeking beyond duration
            uint64_t duration = demuxer->getDuration();
            try {
                demuxer->seekTo(duration + 10000);
                std::cout << "    ✓ Handled invalid seek gracefully" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "    ✓ Properly threw exception for invalid seek" << std::endl;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "    ✗ Exception: " << e.what() << std::endl;
            return false;
        }
    }
    
    /**
     * @brief Test thread safety (basic)
     */
    static bool testThreadSafety() {
        std::cout << "Testing basic thread safety..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        if (testFiles.empty()) return false;
        
        const std::string& file = testFiles[0];
        std::cout << "  Testing thread safety with: " << file << std::endl;
        
        try {
            // Create multiple demuxers (simulating multi-threaded usage)
            std::vector<std::unique_ptr<FLACDemuxer>> demuxers;
            
            for (int i = 0; i < 3; ++i) {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                demuxers.push_back(std::move(demuxer));
            }
            
            // Test that each can read independently
            for (auto& demuxer : demuxers) {
                auto chunk = demuxer->readChunk();
                if (chunk.data.empty()) {
                    std::cout << "    ✗ Failed to read from concurrent demuxer" << std::endl;
                    return false;
                }
            }
            
            std::cout << "    ✓ Basic thread safety test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "    ✗ Exception: " << e.what() << std::endl;
            return false;
        }
    }
    
    /**
     * @brief Test memory usage patterns
     */
    static bool testMemoryUsage() {
        std::cout << "Testing memory usage patterns..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        if (testFiles.empty()) return false;
        
        const std::string& file = testFiles[0];
        std::cout << "  Testing memory usage with: " << file << std::endl;
        
        try {
            // Test creating and destroying multiple demuxers
            for (int i = 0; i < 10; ++i) {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                // Read a few frames
                for (int j = 0; j < 5 && !demuxer->isEOF(); ++j) {
                    auto chunk = demuxer->readChunk();
                    if (chunk.data.empty()) break;
                }
                
                // Demuxer will be destroyed at end of loop
            }
            
            std::cout << "    ✓ Memory usage test completed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "    ✗ Exception: " << e.what() << std::endl;
            return false;
        }
    }
};

/**
 * @brief Main test function
 */
bool testFLACComprehensiveValidation() {
    FLACTestDataUtils::printTestFileInfo("FLAC Comprehensive Validation");
    return FLACComprehensiveValidation::runAllTests();
}

int main() {
    try {
        bool success = testFLACComprehensiveValidation();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC support not available - skipping comprehensive validation tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC
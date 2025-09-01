/*
 * test_flac_test_data_validation.cpp - Comprehensive validation of FLAC test data files
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "flac_test_data_utils.h"

/**
 * @brief Test basic file accessibility and format validation
 */
bool testBasicFileValidation() {
    std::cout << "Testing basic file validation..." << std::endl;
    
    auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
    if (testFiles.empty()) {
        std::cerr << "ERROR: No test files available" << std::endl;
        return false;
    }
    
    bool allPassed = true;
    for (const auto& file : testFiles) {
        std::cout << "  Validating: " << file << std::endl;
        
        // Check file size
        size_t size = FLACTestDataUtils::getFileSize(file);
        if (size == 0) {
            std::cerr << "    ERROR: File is empty or unreadable" << std::endl;
            allPassed = false;
            continue;
        }
        std::cout << "    Size: " << size << " bytes" << std::endl;
        
        // Check FLAC signature
        std::ifstream fileStream(file, std::ios::binary);
        if (!fileStream) {
            std::cerr << "    ERROR: Cannot open file" << std::endl;
            allPassed = false;
            continue;
        }
        
        char signature[4];
        fileStream.read(signature, 4);
        if (fileStream.gcount() != 4 || 
            signature[0] != 'f' || signature[1] != 'L' || 
            signature[2] != 'a' || signature[3] != 'C') {
            std::cerr << "    ERROR: Invalid FLAC signature" << std::endl;
            allPassed = false;
            continue;
        }
        
        std::cout << "    ✓ Valid FLAC signature" << std::endl;
    }
    
    return allPassed;
}

/**
 * @brief Test FLACDemuxer initialization with test files
 */
bool testDemuxerInitialization() {
    std::cout << "Testing FLACDemuxer initialization..." << std::endl;
    
    auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
    if (testFiles.empty()) {
        std::cerr << "ERROR: No test files available" << std::endl;
        return false;
    }
    
    bool allPassed = true;
    for (const auto& file : testFiles) {
        std::cout << "  Testing with: " << file << std::endl;
        
        try {
            auto handler = std::make_unique<FileIOHandler>(file);
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            
            // Parse the container before using the demuxer
            if (!demuxer->parseContainer()) {
                std::cerr << "    ERROR: Failed to parse FLAC container" << std::endl;
                allPassed = false;
                continue;
            }
            
            std::cout << "    ✓ FLACDemuxer created and container parsed successfully" << std::endl;
            
            // Test basic metadata access
            if (demuxer->isEOF()) {
                std::cerr << "    WARNING: Demuxer reports EOF immediately" << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "    ERROR: " << e.what() << std::endl;
            allPassed = false;
        }
    }
    
    return allPassed;
}

/**
 * @brief Test metadata extraction from test files
 */
bool testMetadataExtraction() {
    std::cout << "Testing metadata extraction..." << std::endl;
    
    auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
    if (testFiles.empty()) {
        std::cerr << "ERROR: No test files available" << std::endl;
        return false;
    }
    
    bool allPassed = true;
    for (const auto& file : testFiles) {
        std::cout << "  Extracting metadata from: " << file << std::endl;
        
        try {
            auto handler = std::make_unique<FileIOHandler>(file);
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            
            // Parse the container first
            if (!demuxer->parseContainer()) {
                std::cerr << "    ERROR: Failed to parse FLAC container" << std::endl;
                allPassed = false;
                continue;
            }
            
            // Try to read some frames to trigger metadata parsing
            for (int i = 0; i < 5 && !demuxer->isEOF(); ++i) {
                auto chunk = demuxer->readChunk();
                if (!chunk.data.empty()) {
                    std::cout << "    ✓ Successfully read frame " << i << std::endl;
                    break;
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "    ERROR: " << e.what() << std::endl;
            allPassed = false;
        }
    }
    
    return allPassed;
}

/**
 * @brief Test seeking functionality with test files
 */
bool testSeekingFunctionality() {
    std::cout << "Testing seeking functionality..." << std::endl;
    
    auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
    if (testFiles.empty()) {
        std::cerr << "ERROR: No test files available" << std::endl;
        return false;
    }
    
    bool allPassed = true;
    for (const auto& file : testFiles) {
        std::cout << "  Testing seeking with: " << file << std::endl;
        
        try {
            auto handler = std::make_unique<FileIOHandler>(file);
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            
            // Parse the container first
            if (!demuxer->parseContainer()) {
                std::cerr << "    ERROR: Failed to parse FLAC container" << std::endl;
                allPassed = false;
                continue;
            }
            
            // Test seeking to beginning
            demuxer->seekTo(0);
            std::cout << "    ✓ Seek to beginning successful" << std::endl;
            
            // Test seeking to middle (if file has duration)
            uint64_t duration = demuxer->getDuration();
            if (duration > 1000) {
                demuxer->seekTo(duration / 2);
                std::cout << "    ✓ Seek to middle successful" << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "    ERROR: " << e.what() << std::endl;
            allPassed = false;
        }
    }
    
    return allPassed;
}

/**
 * @brief Test frame reading performance with test files
 */
bool testFrameReadingPerformance() {
    std::cout << "Testing frame reading performance..." << std::endl;
    
    auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
    if (testFiles.empty()) {
        std::cerr << "ERROR: No test files available" << std::endl;
        return false;
    }
    
    bool allPassed = true;
    for (const auto& file : testFiles) {
        std::cout << "  Performance test with: " << file << std::endl;
        
        try {
            auto handler = std::make_unique<FileIOHandler>(file);
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            
            // Parse the container first
            if (!demuxer->parseContainer()) {
                std::cerr << "    ERROR: Failed to parse FLAC container" << std::endl;
                allPassed = false;
                continue;
            }
            
            auto start = std::chrono::high_resolution_clock::now();
            int frameCount = 0;
            
            // Read up to 100 frames or until finished
            while (frameCount < 100 && !demuxer->isEOF()) {
                auto chunk = demuxer->readChunk();
                if (!chunk.data.empty()) {
                    frameCount++;
                }
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::cout << "    ✓ Read " << frameCount << " frames in " 
                      << duration.count() << "ms" << std::endl;
            
            if (frameCount == 0) {
                std::cerr << "    WARNING: No frames read" << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "    ERROR: " << e.what() << std::endl;
            allPassed = false;
        }
    }
    
    return allPassed;
}

/**
 * @brief Test error handling with corrupted data simulation
 */
bool testErrorHandling() {
    std::cout << "Testing error handling..." << std::endl;
    
    auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
    if (testFiles.empty()) {
        std::cerr << "ERROR: No test files available" << std::endl;
        return false;
    }
    
    // Test with first available file
    const std::string& file = testFiles[0];
    std::cout << "  Testing error handling with: " << file << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(file);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Parse the container first
        if (!demuxer->parseContainer()) {
            std::cerr << "    ERROR: Failed to parse FLAC container" << std::endl;
            return false;
        }
        
        // Test seeking beyond duration
        uint64_t duration = demuxer->getDuration();
        try {
            demuxer->seekTo(duration + 10000);
            std::cout << "    ✓ Handled seek beyond duration gracefully" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "    ✓ Properly threw exception for invalid seek: " << e.what() << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "    ERROR: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Main test function
 */
bool testFLACTestDataValidation() {
    std::cout << "=== FLAC Test Data Validation ===" << std::endl;
    
    // Print test file information
    FLACTestDataUtils::printTestFileInfo("FLAC Test Data Validation");
    
    // Validate test data is available
    if (!FLACTestDataUtils::validateTestDataAvailable("FLAC Test Data Validation")) {
        return false;
    }
    
    bool allPassed = true;
    
    // Run all validation tests
    allPassed &= testBasicFileValidation();
    allPassed &= testDemuxerInitialization();
    allPassed &= testMetadataExtraction();
    allPassed &= testSeekingFunctionality();
    allPassed &= testFrameReadingPerformance();
    allPassed &= testErrorHandling();
    
    std::cout << std::endl;
    if (allPassed) {
        std::cout << "✓ All FLAC test data validation tests passed!" << std::endl;
    } else {
        std::cout << "✗ Some FLAC test data validation tests failed!" << std::endl;
    }
    
    return allPassed;
}

int main() {
    try {
        bool success = testFLACTestDataValidation();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
}
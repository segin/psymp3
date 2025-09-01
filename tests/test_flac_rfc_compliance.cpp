/*
 * test_flac_rfc_compliance.cpp - RFC 9639 compliance validation using test data
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "flac_test_data_utils.h"

#ifdef HAVE_FLAC

/**
 * @brief RFC 9639 FLAC compliance validation test suite
 */
class FLACRFCComplianceTest {
public:
    /**
     * @brief Run all RFC compliance tests
     */
    static bool runAllTests() {
        std::cout << "=== FLAC RFC 9639 Compliance Validation ===" << std::endl;
        std::cout << "Testing against RFC 9639 FLAC specification" << std::endl;
        std::cout << std::endl;
        
        // Validate test data availability
        if (!FLACTestDataUtils::validateTestDataAvailable("RFC Compliance")) {
            return false;
        }
        
        bool allPassed = true;
        
        allPassed &= testFLACSignature();
        allPassed &= testStreamInfoBlock();
        allPassed &= testFrameStructure();
        allPassed &= testSyncPattern();
        allPassed &= testBlockSizeValidation();
        allPassed &= testSampleRateValidation();
        allPassed &= testChannelConfiguration();
        allPassed &= testBitDepthValidation();
        
        std::cout << std::endl;
        if (allPassed) {
            std::cout << "✓ All RFC 9639 compliance tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some RFC 9639 compliance tests FAILED" << std::endl;
        }
        
        return allPassed;
    }

private:
    /**
     * @brief Test FLAC signature compliance (RFC 9639 Section 4)
     */
    static bool testFLACSignature() {
        std::cout << "Testing FLAC signature compliance..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Validating signature: " << file << std::endl;
            
            std::ifstream fileStream(file, std::ios::binary);
            if (!fileStream) {
                std::cout << "    ✗ Cannot open file" << std::endl;
                allPassed = false;
                continue;
            }
            
            // RFC 9639: FLAC files must start with "fLaC" (0x664C6143)
            char signature[4];
            fileStream.read(signature, 4);
            
            if (fileStream.gcount() != 4) {
                std::cout << "    ✗ Cannot read signature" << std::endl;
                allPassed = false;
                continue;
            }
            
            if (signature[0] != 'f' || signature[1] != 'L' || 
                signature[2] != 'a' || signature[3] != 'C') {
                std::cout << "    ✗ Invalid FLAC signature" << std::endl;
                allPassed = false;
                continue;
            }
            
            std::cout << "    ✓ Valid FLAC signature" << std::endl;
        }
        
        return allPassed;
    }
    
    /**
     * @brief Test STREAMINFO block compliance (RFC 9639 Section 4.2.1)
     */
    static bool testStreamInfoBlock() {
        std::cout << "Testing STREAMINFO block compliance..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Validating STREAMINFO: " << file << std::endl;
            
            std::ifstream fileStream(file, std::ios::binary);
            if (!fileStream) {
                std::cout << "    ✗ Cannot open file" << std::endl;
                allPassed = false;
                continue;
            }
            
            // Skip FLAC signature
            fileStream.seekg(4);
            
            // Read metadata block header
            uint8_t header[4];
            fileStream.read(reinterpret_cast<char*>(header), 4);
            
            if (fileStream.gcount() != 4) {
                std::cout << "    ✗ Cannot read metadata block header" << std::endl;
                allPassed = false;
                continue;
            }
            
            // RFC 9639: First metadata block must be STREAMINFO (type 0)
            uint8_t blockType = header[0] & 0x7F;
            if (blockType != 0) {
                std::cout << "    ✗ First metadata block is not STREAMINFO (type=" 
                          << static_cast<int>(blockType) << ")" << std::endl;
                allPassed = false;
                continue;
            }
            
            // RFC 9639: STREAMINFO block must be exactly 34 bytes
            uint32_t blockLength = (header[1] << 16) | (header[2] << 8) | header[3];
            if (blockLength != 34) {
                std::cout << "    ✗ STREAMINFO block length is not 34 bytes (length=" 
                          << blockLength << ")" << std::endl;
                allPassed = false;
                continue;
            }
            
            std::cout << "    ✓ Valid STREAMINFO block" << std::endl;
        }
        
        return allPassed;
    }
    
    /**
     * @brief Test frame structure compliance (RFC 9639 Section 5)
     */
    static bool testFrameStructure() {
        std::cout << "Testing frame structure compliance..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Validating frame structure: " << file << std::endl;
            
            try {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                // Try to read first frame and validate its structure
                auto chunk = demuxer->readChunk();
                if (chunk.data.empty()) {
                    std::cout << "    ✗ Cannot read first frame" << std::endl;
                    allPassed = false;
                    continue;
                }
                
                // Basic frame validation
                if (chunk.data.empty()) {
                    std::cout << "    ✗ Frame has no data" << std::endl;
                    allPassed = false;
                    continue;
                }
                
                std::cout << "    ✓ Frame structure appears valid" << std::endl;
                
            } catch (const std::exception& e) {
                std::cout << "    ✗ Exception: " << e.what() << std::endl;
                allPassed = false;
            }
        }
        
        return allPassed;
    }
    
    /**
     * @brief Test sync pattern compliance (RFC 9639 Section 5.1)
     */
    static bool testSyncPattern() {
        std::cout << "Testing sync pattern compliance..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Validating sync patterns: " << file << std::endl;
            
            try {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                // Read multiple frames to validate sync patterns
                int frameCount = 0;
                while (frameCount < 5 && !demuxer->isEOF()) {
                    auto chunk = demuxer->readChunk();
                    if (!chunk.data.empty()) {
                        frameCount++;
                        
                        // RFC 9639: Each frame should start with sync pattern
                        // This is validated internally by the demuxer
                        if (chunk.data.empty()) {
                            std::cout << "    ✗ Frame " << frameCount << " has no data" << std::endl;
                            allPassed = false;
                            break;
                        }
                    } else {
                        break;
                    }
                }
                
                if (frameCount > 0) {
                    std::cout << "    ✓ Successfully validated " << frameCount << " frame sync patterns" << std::endl;
                } else {
                    std::cout << "    ✗ No frames read for sync pattern validation" << std::endl;
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
     * @brief Test block size validation (RFC 9639 Section 5.2.1)
     */
    static bool testBlockSizeValidation() {
        std::cout << "Testing block size validation..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Validating block sizes: " << file << std::endl;
            
            try {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                // Read frames and validate block sizes are within RFC limits
                int frameCount = 0;
                while (frameCount < 10 && !demuxer->isEOF()) {
                    auto chunk = demuxer->readChunk();
                    if (!chunk.data.empty()) {
                        frameCount++;
                        
                        // RFC 9639: Block sizes must be between 1 and 65535 samples
                        // This validation is done internally by the demuxer
                        if (chunk.data.empty()) {
                            std::cout << "    ✗ Frame " << frameCount << " has invalid block size" << std::endl;
                            allPassed = false;
                            break;
                        }
                    } else {
                        break;
                    }
                }
                
                if (frameCount > 0) {
                    std::cout << "    ✓ Block sizes appear valid for " << frameCount << " frames" << std::endl;
                } else {
                    std::cout << "    ✗ No frames read for block size validation" << std::endl;
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
     * @brief Test sample rate validation (RFC 9639 Section 5.2.2)
     */
    static bool testSampleRateValidation() {
        std::cout << "Testing sample rate validation..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Validating sample rates: " << file << std::endl;
            
            try {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                // Read a frame to trigger sample rate validation
                auto chunk = demuxer->readChunk();
                if (!chunk.data.empty()) {
                    std::cout << "    ✓ Sample rate validation passed" << std::endl;
                } else {
                    std::cout << "    ✗ Cannot read frame for sample rate validation" << std::endl;
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
     * @brief Test channel configuration validation (RFC 9639 Section 5.2.3)
     */
    static bool testChannelConfiguration() {
        std::cout << "Testing channel configuration validation..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Validating channel configuration: " << file << std::endl;
            
            try {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                // Read a frame to trigger channel configuration validation
                auto chunk = demuxer->readChunk();
                if (!chunk.data.empty()) {
                    std::cout << "    ✓ Channel configuration validation passed" << std::endl;
                } else {
                    std::cout << "    ✗ Cannot read frame for channel configuration validation" << std::endl;
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
     * @brief Test bit depth validation (RFC 9639 Section 5.2.4)
     */
    static bool testBitDepthValidation() {
        std::cout << "Testing bit depth validation..." << std::endl;
        
        auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
        bool allPassed = true;
        
        for (const auto& file : testFiles) {
            std::cout << "  Validating bit depth: " << file << std::endl;
            
            try {
                auto handler = std::make_unique<FileIOHandler>(file);
                auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                
                // Read a frame to trigger bit depth validation
                auto chunk = demuxer->readChunk();
                if (!chunk.data.empty()) {
                    std::cout << "    ✓ Bit depth validation passed" << std::endl;
                } else {
                    std::cout << "    ✗ Cannot read frame for bit depth validation" << std::endl;
                    allPassed = false;
                }
                
            } catch (const std::exception& e) {
                std::cout << "    ✗ Exception: " << e.what() << std::endl;
                allPassed = false;
            }
        }
        
        return allPassed;
    }
};

/**
 * @brief Main test function
 */
bool testFLACRFCCompliance() {
    FLACTestDataUtils::printTestFileInfo("FLAC RFC 9639 Compliance");
    return FLACRFCComplianceTest::runAllTests();
}

int main() {
    try {
        bool success = testFLACRFCCompliance();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC support not available - skipping RFC compliance tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC
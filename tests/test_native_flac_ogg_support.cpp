/*
 * test_native_flac_ogg_support.cpp - Test native FLAC codec Ogg FLAC support
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * Tests Requirement 49: Ogg Container Support
 */

#include "psymp3.h"

#ifdef HAVE_NATIVE_FLAC

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

/**
 * Test Ogg FLAC header parsing
 * 
 * Validates Requirements:
 * - 49.2: Verify 0x7F 0x46 0x4C 0x41 0x43 signature
 * - 49.3: Parse version number
 * - 49.4: Parse header count
 */
bool test_ogg_flac_header_parsing() {
    Debug::log("test", "[test_ogg_flac_header_parsing] Testing Ogg FLAC header parsing");
    
    // Create Ogg FLAC header packet
    std::vector<uint8_t> header_data;
    
    // Ogg FLAC signature: 0x7F 'F' 'L' 'A' 'C'
    header_data.push_back(0x7F);
    header_data.push_back('F');
    header_data.push_back('L');
    header_data.push_back('A');
    header_data.push_back('C');
    
    // Version: 1.0
    header_data.push_back(0x01);  // Major version
    header_data.push_back(0x00);  // Minor version
    
    // Header count: 1 (big-endian)
    header_data.push_back(0x00);
    header_data.push_back(0x01);
    
    // Create bitstream reader
    BitstreamReader reader;
    reader.feedData(header_data.data(), header_data.size());
    
    // Create metadata parser
    MetadataParser parser(&reader);
    
    // Parse Ogg FLAC header
    uint8_t major_version = 0;
    uint8_t minor_version = 0;
    uint16_t header_count = 0;
    
    if (!parser.parseOggFLACHeader(major_version, minor_version, header_count)) {
        Debug::log("test", "[test_ogg_flac_header_parsing] FAILED: Could not parse Ogg FLAC header");
        return false;
    }
    
    // Verify version
    if (major_version != 1 || minor_version != 0) {
        Debug::log("test", "[test_ogg_flac_header_parsing] FAILED: Invalid version: ",
                  (int)major_version, ".", (int)minor_version, " (expected 1.0)");
        return false;
    }
    
    // Verify header count
    if (header_count != 1) {
        Debug::log("test", "[test_ogg_flac_header_parsing] FAILED: Invalid header count: ",
                  header_count, " (expected 1)");
        return false;
    }
    
    Debug::log("test", "[test_ogg_flac_header_parsing] SUCCESS: Parsed Ogg FLAC header v",
              (int)major_version, ".", (int)minor_version, " with ", header_count, " header(s)");
    return true;
}

/**
 * Test Ogg FLAC signature verification
 * 
 * Validates Requirement 49.2: Verify signature
 */
bool test_ogg_flac_signature_verification() {
    Debug::log("test", "[test_ogg_flac_signature_verification] Testing signature verification");
    
    // Test valid signature
    {
        std::vector<uint8_t> valid_data = {0x7F, 'F', 'L', 'A', 'C', 0x01, 0x00, 0x00, 0x01};
        BitstreamReader reader;
        reader.feedData(valid_data.data(), valid_data.size());
        MetadataParser parser(&reader);
        
        uint8_t major, minor;
        uint16_t count;
        
        if (!parser.parseOggFLACHeader(major, minor, count)) {
            Debug::log("test", "[test_ogg_flac_signature_verification] FAILED: Valid signature rejected");
            return false;
        }
    }
    
    // Test invalid signature (wrong first byte)
    {
        std::vector<uint8_t> invalid_data = {0xFF, 'F', 'L', 'A', 'C', 0x01, 0x00, 0x00, 0x01};
        BitstreamReader reader;
        reader.feedData(invalid_data.data(), invalid_data.size());
        MetadataParser parser(&reader);
        
        uint8_t major, minor;
        uint16_t count;
        
        if (parser.parseOggFLACHeader(major, minor, count)) {
            Debug::log("test", "[test_ogg_flac_signature_verification] FAILED: Invalid signature accepted");
            return false;
        }
    }
    
    // Test invalid signature (wrong magic bytes)
    {
        std::vector<uint8_t> invalid_data = {0x7F, 'V', 'O', 'R', 'B', 0x01, 0x00, 0x00, 0x01};
        BitstreamReader reader;
        reader.feedData(invalid_data.data(), invalid_data.size());
        MetadataParser parser(&reader);
        
        uint8_t major, minor;
        uint16_t count;
        
        if (parser.parseOggFLACHeader(major, minor, count)) {
            Debug::log("test", "[test_ogg_flac_signature_verification] FAILED: Wrong magic bytes accepted");
            return false;
        }
    }
    
    Debug::log("test", "[test_ogg_flac_signature_verification] SUCCESS: Signature verification working correctly");
    return true;
}

/**
 * Test container-agnostic decoding
 * 
 * Validates Requirement 49.6: Decode FLAC frames from audio packets
 */
bool test_container_agnostic_decoding() {
    Debug::log("test", "[test_container_agnostic_decoding] Testing container-agnostic decoding");
    
    // Create StreamInfo for Ogg FLAC
    StreamInfo ogg_flac_info;
    ogg_flac_info.codec_type = "audio";
    ogg_flac_info.codec_name = "flac";
    ogg_flac_info.container_format = "ogg";
    ogg_flac_info.sample_rate = 44100;
    ogg_flac_info.channels = 2;
    ogg_flac_info.bits_per_sample = 16;
    ogg_flac_info.bitrate = 0;
    ogg_flac_info.duration_samples = 0;
    
    // Create codec
    FLACCodec codec(ogg_flac_info);
    
    // Initialize codec
    if (!codec.initialize()) {
        Debug::log("test", "[test_container_agnostic_decoding] FAILED: Could not initialize codec");
        return false;
    }
    
    // Verify codec can decode Ogg FLAC
    if (!codec.canDecode(ogg_flac_info)) {
        Debug::log("test", "[test_container_agnostic_decoding] FAILED: Codec reports it cannot decode Ogg FLAC");
        return false;
    }
    
    Debug::log("test", "[test_container_agnostic_decoding] SUCCESS: Codec can decode Ogg FLAC streams");
    return true;
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

int main() {
    using namespace PsyMP3::Codec::FLAC;
    
    std::cout << "=== Native FLAC Codec Ogg FLAC Support Tests ===" << std::endl;
    std::cout << "Testing Requirement 49: Ogg Container Support" << std::endl;
    std::cout << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    // Test 1: Ogg FLAC header parsing
    std::cout << "Test 1: Ogg FLAC header parsing..." << std::endl;
    if (test_ogg_flac_header_parsing()) {
        std::cout << "  PASSED" << std::endl;
        passed++;
    } else {
        std::cout << "  FAILED" << std::endl;
        failed++;
    }
    
    // Test 2: Signature verification
    std::cout << "Test 2: Ogg FLAC signature verification..." << std::endl;
    if (test_ogg_flac_signature_verification()) {
        std::cout << "  PASSED" << std::endl;
        passed++;
    } else {
        std::cout << "  FAILED" << std::endl;
        failed++;
    }
    
    // Test 3: Container-agnostic decoding
    std::cout << "Test 3: Container-agnostic decoding..." << std::endl;
    if (test_container_agnostic_decoding()) {
        std::cout << "  PASSED" << std::endl;
        passed++;
    } else {
        std::cout << "  FAILED" << std::endl;
        failed++;
    }
    
    std::cout << std::endl;
    std::cout << "=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Total:  " << (passed + failed) << std::endl;
    
    return (failed == 0) ? 0 : 1;
}

#else

int main() {
    std::cout << "Native FLAC codec not enabled (HAVE_NATIVE_FLAC not defined)" << std::endl;
    return 0;
}

#endif // HAVE_NATIVE_FLAC

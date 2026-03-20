/*
 * test_tag_error_handling_manual.cpp - Manual test for tag error handling
 * 
 * This test verifies that tag parsers handle corrupted/malformed input gracefully
 * without crashing or throwing exceptions.
 */

#include "psymp3.h"
#include <iostream>
#include <vector>
#include <cstring>

using namespace PsyMP3::Tag;

bool test_id3v1_corrupted_data() {
    std::cout << "Testing ID3v1 with corrupted data..." << std::endl;
    
    // Test 1: Random data
    std::vector<uint8_t> random_data(128);
    for (size_t i = 0; i < 128; ++i) {
        random_data[i] = static_cast<uint8_t>(rand() % 256);
    }
    
    auto tag1 = ID3v1Tag::parse(random_data.data());
    std::cout << "  Random data: " << (tag1 ? "parsed" : "rejected") << std::endl;
    
    // Test 2: Truncated data (only 50 bytes)
    std::vector<uint8_t> truncated_data(50, 0xFF);
    auto tag2 = ID3v1Tag::parse(truncated_data.data());
    std::cout << "  Truncated data: " << (tag2 ? "parsed" : "rejected") << std::endl;
    
    // Test 3: Null pointer
    auto tag3 = ID3v1Tag::parse(nullptr);
    std::cout << "  Null pointer: " << (tag3 ? "parsed" : "rejected") << std::endl;
    
    // Test 4: Valid header but corrupted content
    std::vector<uint8_t> corrupted_header(128, 0xFF);
    corrupted_header[0] = 'T';
    corrupted_header[1] = 'A';
    corrupted_header[2] = 'G';
    auto tag4 = ID3v1Tag::parse(corrupted_header.data());
    std::cout << "  Valid header, corrupted content: " << (tag4 ? "parsed" : "rejected") << std::endl;
    
    return true;
}

bool test_id3v2_corrupted_data() {
    std::cout << "\nTesting ID3v2 with corrupted data..." << std::endl;
    
    // Test 1: Random data
    std::vector<uint8_t> random_data(100);
    for (size_t i = 0; i < 100; ++i) {
        random_data[i] = static_cast<uint8_t>(rand() % 256);
    }
    
    auto tag1 = ID3v2Tag::parse(random_data.data(), random_data.size());
    std::cout << "  Random data: " << (tag1 ? "parsed" : "rejected") << std::endl;
    
    // Test 2: Truncated header (only 5 bytes)
    std::vector<uint8_t> truncated_data(5, 0xFF);
    auto tag2 = ID3v2Tag::parse(truncated_data.data(), truncated_data.size());
    std::cout << "  Truncated header: " << (tag2 ? "parsed" : "rejected") << std::endl;
    
    // Test 3: Null pointer
    auto tag3 = ID3v2Tag::parse(nullptr, 0);
    std::cout << "  Null pointer: " << (tag3 ? "parsed" : "rejected") << std::endl;
    
    // Test 4: Valid header but invalid size
    std::vector<uint8_t> invalid_size(10);
    invalid_size[0] = 'I'; invalid_size[1] = 'D'; invalid_size[2] = '3';
    invalid_size[3] = 0x03; // Version 2.3
    invalid_size[4] = 0x00; // Minor version
    invalid_size[5] = 0x00; // Flags
    // Invalid synchsafe size (all 0xFF)
    invalid_size[6] = 0xFF;
    invalid_size[7] = 0xFF;
    invalid_size[8] = 0xFF;
    invalid_size[9] = 0xFF;
    auto tag4 = ID3v2Tag::parse(invalid_size.data(), invalid_size.size());
    std::cout << "  Valid header, invalid size: " << (tag4 ? "parsed" : "rejected") << std::endl;
    
    return true;
}

bool test_vorbiscomment_corrupted_data() {
    std::cout << "\nTesting VorbisComment with corrupted data..." << std::endl;
    
    // Test 1: Random data
    std::vector<uint8_t> random_data(100);
    for (size_t i = 0; i < 100; ++i) {
        random_data[i] = static_cast<uint8_t>(rand() % 256);
    }
    
    auto tag1 = VorbisCommentTag::parse(random_data.data(), random_data.size());
    std::cout << "  Random data: " << (tag1 ? "parsed" : "rejected") << std::endl;
    
    // Test 2: Truncated data (only 2 bytes)
    std::vector<uint8_t> truncated_data(2, 0xFF);
    auto tag2 = VorbisCommentTag::parse(truncated_data.data(), truncated_data.size());
    std::cout << "  Truncated data: " << (tag2 ? "parsed" : "rejected") << std::endl;
    
    // Test 3: Null pointer
    auto tag3 = VorbisCommentTag::parse(nullptr, 0);
    std::cout << "  Null pointer: " << (tag3 ? "parsed" : "rejected") << std::endl;
    
    // Test 4: Oversized vendor string length
    std::vector<uint8_t> oversized(10);
    // Vendor string length: 0xFFFFFFFF (4GB)
    oversized[0] = 0xFF;
    oversized[1] = 0xFF;
    oversized[2] = 0xFF;
    oversized[3] = 0xFF;
    // Some random data
    for (size_t i = 4; i < 10; ++i) {
        oversized[i] = static_cast<uint8_t>(rand() % 256);
    }
    auto tag4 = VorbisCommentTag::parse(oversized.data(), oversized.size());
    std::cout << "  Oversized vendor length: " << (tag4 ? "parsed" : "rejected") << std::endl;
    
    return true;
}

bool test_tagfactory_corrupted_data() {
    std::cout << "\nTesting TagFactory with corrupted data..." << std::endl;
    
    // Test 1: Random data
    std::vector<uint8_t> random_data(200);
    for (size_t i = 0; i < 200; ++i) {
        random_data[i] = static_cast<uint8_t>(rand() % 256);
    }
    
    auto tag1 = createTagReaderFromData(random_data.data(), random_data.size());
    std::cout << "  Random data: " << (tag1 ? tag1->formatName() : "null") << std::endl;
    
    // Test 2: Empty data
    auto tag2 = createTagReaderFromData(nullptr, 0);
    std::cout << "  Empty data: " << (tag2 ? tag2->formatName() : "null") << std::endl;
    
    // Test 3: Very small data
    std::vector<uint8_t> small_data(3, 0xFF);
    auto tag3 = createTagReaderFromData(small_data.data(), small_data.size());
    std::cout << "  Very small data: " << (tag3 ? tag3->formatName() : "null") << std::endl;
    
    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Tag Error Handling Manual Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    bool all_passed = true;
    
    try {
        all_passed &= test_id3v1_corrupted_data();
        all_passed &= test_id3v2_corrupted_data();
        all_passed &= test_vorbiscomment_corrupted_data();
        all_passed &= test_tagfactory_corrupted_data();
        
        std::cout << "\n========================================" << std::endl;
        if (all_passed) {
            std::cout << "All error handling tests PASSED" << std::endl;
            std::cout << "No crashes or exceptions occurred" << std::endl;
        } else {
            std::cout << "Some tests FAILED" << std::endl;
        }
        std::cout << "========================================" << std::endl;
        
        return all_passed ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "\nFATAL: Exception thrown: " << e.what() << std::endl;
        std::cerr << "Error handling tests FAILED - parsers should not throw exceptions" << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\nFATAL: Unknown exception thrown" << std::endl;
        std::cerr << "Error handling tests FAILED - parsers should not throw exceptions" << std::endl;
        return 1;
    }
}

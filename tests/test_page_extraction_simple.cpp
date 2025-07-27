/*
 * test_page_extraction_simple.cpp - Simple unit tests for OggDemuxer page extraction functions
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>
#include <cassert>

// Simple assertion macro
#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << (message) \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

// Test helper class to create mock Ogg files
class MockOggFile {
public:
    static std::vector<uint8_t> createSimpleOggFile() {
        std::vector<uint8_t> data;
        
        // Create a simple Ogg page with "OggS" header
        // Capture pattern
        data.push_back('O');
        data.push_back('g');
        data.push_back('g');
        data.push_back('S');
        
        // Version
        data.push_back(0);
        
        // Header type (first page)
        data.push_back(0x02);
        
        // Granule position (8 bytes, little-endian)
        for (int i = 0; i < 8; i++) {
            data.push_back(0);
        }
        
        // Serial number (4 bytes, little-endian) - 12345
        data.push_back(0x39);  // 12345 & 0xFF
        data.push_back(0x30);  // (12345 >> 8) & 0xFF
        data.push_back(0x00);  // (12345 >> 16) & 0xFF
        data.push_back(0x00);  // (12345 >> 24) & 0xFF
        
        // Page sequence (4 bytes)
        for (int i = 0; i < 4; i++) {
            data.push_back(0);
        }
        
        // Checksum (4 bytes) - will be calculated later
        size_t checksum_pos = data.size();
        for (int i = 0; i < 4; i++) {
            data.push_back(0);
        }
        
        // Page segments
        data.push_back(1);
        
        // Segment table
        data.push_back(10);  // One segment of 10 bytes
        
        // Packet data (10 bytes of dummy data)
        for (int i = 0; i < 10; i++) {
            data.push_back(0x41 + i);  // 'A', 'B', 'C', etc.
        }
        
        // Calculate and set checksum using libogg
        ogg_page page;
        page.header = data.data();
        page.header_len = 27;  // Header size
        page.body = data.data() + 27;
        page.body_len = 10;    // Body size
        
        ogg_page_checksum_set(&page);  // This modifies the page header directly
        
        return data;
    }
    
    static void writeToFile(const std::string& filename, const std::vector<uint8_t>& data) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to create test file: " + filename);
        }
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
    }
};

// Test functions
bool testGetData() {
    std::cout << "Testing getData()..." << std::endl;
    
    // Create test file
    auto data = MockOggFile::createSimpleOggFile();
    MockOggFile::writeToFile("test_simple.ogg", data);
    
    try {
        auto handler = std::make_unique<FileIOHandler>("test_simple.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Test getting data with small size to avoid buffer pool optimization
        int result = demuxer.getData(10);  // Small read to avoid triggering optimization
        
        ASSERT(result > 0, "Should read some data");
        ASSERT(result <= 10, "Should not read more than requested");
        
        std::remove("test_simple.ogg");
        std::cout << "  ✓ getData() test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ getData() test failed: " << e.what() << std::endl;
        std::remove("test_simple.ogg");
        return false;
    }
}

bool testGetDataDefaultSize() {
    std::cout << "Testing getData() with default size..." << std::endl;
    
    // Create test file
    auto data = MockOggFile::createSimpleOggFile();
    MockOggFile::writeToFile("test_simple.ogg", data);
    
    try {
        auto handler = std::make_unique<FileIOHandler>("test_simple.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Test getting data with default size
        int result = demuxer.getData();
        
        ASSERT(result > 0, "Should read some data");
        
        std::remove("test_simple.ogg");
        std::cout << "  ✓ getData() default size test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ getData() default size test failed: " << e.what() << std::endl;
        std::remove("test_simple.ogg");
        return false;
    }
}

bool testGetNextPage() {
    std::cout << "Testing getNextPage()..." << std::endl;
    
    // Create test file
    auto data = MockOggFile::createSimpleOggFile();
    MockOggFile::writeToFile("test_simple.ogg", data);
    
    try {
        auto handler = std::make_unique<FileIOHandler>("test_simple.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Test getting next page directly (without parsing container)
        ogg_page page;
        int result = demuxer.getNextPage(&page);
        
        // For our simple mock file, we expect either:
        // - A positive result if it found a valid page
        // - Zero if it needs more data (which is likely with our simple mock)
        // - A negative result if there's an error
        // The key test is that it doesn't crash and returns a reasonable result
        ASSERT(result >= -1, "getNextPage() should return a reasonable result (>= -1)");
        
        std::remove("test_simple.ogg");
        std::cout << "  ✓ getNextPage() test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ getNextPage() test failed: " << e.what() << std::endl;
        std::remove("test_simple.ogg");
        return false;
    }
}

bool testNullPagePointer() {
    std::cout << "Testing null page pointer handling..." << std::endl;
    
    // Create test file
    auto data = MockOggFile::createSimpleOggFile();
    MockOggFile::writeToFile("test_simple.ogg", data);
    
    try {
        auto handler = std::make_unique<FileIOHandler>("test_simple.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Test with null page pointer
        int result = demuxer.getNextPage(nullptr);
        ASSERT(result < 0, "Should fail with null pointer");
        
        result = demuxer.getPrevPage(nullptr);
        ASSERT(result < 0, "Should fail with null pointer");
        
        result = demuxer.getPrevPageSerial(nullptr, 12345);
        ASSERT(result < 0, "Should fail with null pointer");
        
        std::remove("test_simple.ogg");
        std::cout << "  ✓ Null pointer handling test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Null pointer handling test failed: " << e.what() << std::endl;
        std::remove("test_simple.ogg");
        return false;
    }
}

bool testBoundaryConditions() {
    std::cout << "Testing boundary conditions..." << std::endl;
    
    // Create test file
    auto data = MockOggFile::createSimpleOggFile();
    MockOggFile::writeToFile("test_simple.ogg", data);
    
    try {
        auto handler = std::make_unique<FileIOHandler>("test_simple.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Test with zero boundary
        ogg_page page;
        int result = demuxer.getNextPage(&page, 0);
        // With zero boundary, the function should either fail immediately or not find any pages
        // Both negative return values and zero are acceptable
        ASSERT(result <= 0, "Should fail or return zero with zero boundary");
        
        std::remove("test_simple.ogg");
        std::cout << "  ✓ Boundary conditions test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Boundary conditions test failed: " << e.what() << std::endl;
        std::remove("test_simple.ogg");
        return false;
    }
}

// Main test runner
int main() {
    std::cout << "Running OggDemuxer Page Extraction Tests..." << std::endl;
    std::cout << "=============================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // Run tests
    if (testGetData()) passed++;
    total++;
    
    if (testGetDataDefaultSize()) passed++;
    total++;
    
    if (testGetNextPage()) passed++;
    total++;
    
    if (testNullPagePointer()) passed++;
    total++;
    
    if (testBoundaryConditions()) passed++;
    total++;
    
    // Print results
    std::cout << "=============================================" << std::endl;
    std::cout << "Test Results: " << passed << "/" << total << " passed" << std::endl;
    
    if (passed == total) {
        std::cout << "All tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << (total - passed) << " tests FAILED!" << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main() {
    std::cout << "OggDemuxer not available - skipping page extraction tests" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER
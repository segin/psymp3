/*
 * test_page_extraction.cpp - Unit tests for OggDemuxer page extraction functions
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include "test_framework.h"
#include <fstream>
#include <vector>
#include <cstring>

// Test helper class to create mock Ogg files
class MockOggFile {
public:
    static std::vector<uint8_t> createSimpleOggFile() {
        std::vector<uint8_t> data;
        
        // Create a simple Ogg page with "OggS" header
        // Page header structure:
        // - capture_pattern[4]: "OggS"
        // - version: 0
        // - header_type: 0x02 (first page)
        // - granule_position: 0 (8 bytes)
        // - serial_number: 12345 (4 bytes)
        // - page_sequence: 0 (4 bytes)
        // - checksum: will be calculated by libogg
        // - page_segments: 1
        // - segment_table[1]: 10 (packet size)
        // - packet_data[10]: dummy data
        
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
    
    static std::vector<uint8_t> createMultiPageOggFile() {
        std::vector<uint8_t> data;
        
        // Create first page
        auto page1 = createSimpleOggFile();
        data.insert(data.end(), page1.begin(), page1.end());
        
        // Create second page with different serial number and granule position
        std::vector<uint8_t> page2;
        
        // Capture pattern
        page2.push_back('O');
        page2.push_back('g');
        page2.push_back('g');
        page2.push_back('S');
        
        // Version
        page2.push_back(0);
        
        // Header type (normal page)
        page2.push_back(0x00);
        
        // Granule position (8 bytes, little-endian) - 1000
        page2.push_back(0xE8);  // 1000 & 0xFF
        page2.push_back(0x03);  // (1000 >> 8) & 0xFF
        for (int i = 2; i < 8; i++) {
            page2.push_back(0);
        }
        
        // Serial number (4 bytes, little-endian) - 54321
        page2.push_back(0x31);  // 54321 & 0xFF
        page2.push_back(0xD4);  // (54321 >> 8) & 0xFF
        page2.push_back(0x00);  // (54321 >> 16) & 0xFF
        page2.push_back(0x00);  // (54321 >> 24) & 0xFF
        
        // Page sequence (4 bytes) - 1
        page2.push_back(1);
        for (int i = 1; i < 4; i++) {
            page2.push_back(0);
        }
        
        // Checksum (4 bytes) - will be calculated later
        size_t checksum_pos = page2.size();
        for (int i = 0; i < 4; i++) {
            page2.push_back(0);
        }
        
        // Page segments
        page2.push_back(1);
        
        // Segment table
        page2.push_back(15);  // One segment of 15 bytes
        
        // Packet data (15 bytes of dummy data)
        for (int i = 0; i < 15; i++) {
            page2.push_back(0x61 + i);  // 'a', 'b', 'c', etc.
        }
        
        // Calculate and set checksum
        ogg_page page;
        page.header = page2.data();
        page.header_len = 27;
        page.body = page2.data() + 27;
        page.body_len = 15;
        
        ogg_page_checksum_set(&page);  // This modifies the page header directly
        
        data.insert(data.end(), page2.begin(), page2.end());
        
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

// Test fixture for page extraction tests
class PageExtractionTest : public TestFramework::TestCase {
public:
    PageExtractionTest() : TestFramework::TestCase("PageExtractionTest") {}
    
protected:
    void setUp() override {
        // Create test files
        auto simple_data = MockOggFile::createSimpleOggFile();
        MockOggFile::writeToFile("test_simple.ogg", simple_data);
        
        auto multi_data = MockOggFile::createMultiPageOggFile();
        MockOggFile::writeToFile("test_multi.ogg", multi_data);
    }
    
    void tearDown() override {
        // Clean up test files
        std::remove("test_simple.ogg");
        std::remove("test_multi.ogg");
    }
    
    void runTest() override {
        testGetNextPage();
        testGetNextPageWithBoundary();
        testGetNextPageWithRestrictiveBoundary();
        testGetPrevPage();
        testGetPrevPageSerial();
        testGetPrevPageSerialNotFound();
        testGetData();
        testGetDataDefaultSize();
        testNullPagePointer();
        testBoundaryConditions();
        testCorruptedData();
    }
    
private:
    void testGetNextPage();
    void testGetNextPageWithBoundary();
    void testGetNextPageWithRestrictiveBoundary();
    void testGetPrevPage();
    void testGetPrevPageSerial();
    void testGetPrevPageSerialNotFound();
    void testGetData();
    void testGetDataDefaultSize();
    void testNullPagePointer();
    void testBoundaryConditions();
    void testCorruptedData();
};

void PageExtractionTest::testGetNextPage() {
    auto handler = std::make_unique<FileIOHandler>("test_simple.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    // Initialize demuxer
    ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse container");
    
    // Test getting next page
    ogg_page page;
    int result = demuxer.getNextPage(&page);
    
    ASSERT_TRUE(result > 0, "Should return positive value for successful page read");
    ASSERT_EQUALS(ogg_page_serialno(&page), 12345U, "Check serial number");
    ASSERT_EQUALS(ogg_page_granulepos(&page), 0LL, "Check granule position");
    ASSERT_TRUE(ogg_page_bos(&page), "Should be beginning of stream");
}

void PageExtractionTest::testGetNextPageWithBoundary() {
    auto handler = std::make_unique<FileIOHandler>("test_multi.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    // Initialize demuxer
    ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse container");
    
    // Test getting next page with boundary that should allow first page
    ogg_page page;
    int result = demuxer.getNextPage(&page, 100);  // Boundary at 100 bytes
    
    ASSERT_TRUE(result > 0, "Should succeed");
    ASSERT_EQUALS(ogg_page_serialno(&page), 12345U, "Check serial number");
}

void PageExtractionTest::testGetNextPageWithRestrictiveBoundary() {
    auto handler = std::make_unique<FileIOHandler>("test_multi.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    // Initialize demuxer
    ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse container");
    
    // Test getting next page with very restrictive boundary
    ogg_page page;
    int result = demuxer.getNextPage(&page, 10);  // Very small boundary
    
    ASSERT_TRUE(result < 0, "Should fail due to boundary restriction");
}

void PageExtractionTest::testGetPrevPage() {
    auto handler = std::make_unique<FileIOHandler>("test_multi.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    // Initialize demuxer
    ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse container");
    
    // First, advance to second page
    ogg_page page1, page2;
    int result1 = demuxer.getNextPage(&page1);
    ASSERT_TRUE(result1 > 0, "Failed to get first page");
    
    int result2 = demuxer.getNextPage(&page2);
    ASSERT_TRUE(result2 > 0, "Failed to get second page");
    ASSERT_EQUALS(ogg_page_serialno(&page2), 54321U, "Second page serial");
    
    // Now test getting previous page
    ogg_page prev_page;
    int prev_result = demuxer.getPrevPage(&prev_page);
    
    ASSERT_TRUE(prev_result > 0, "Should succeed");
    ASSERT_EQUALS(ogg_page_serialno(&prev_page), 12345U, "Should be first page");
}

void PageExtractionTest::testGetPrevPageSerial() {
    auto handler = std::make_unique<FileIOHandler>("test_multi.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    // Initialize demuxer
    ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse container");
    
    // Advance past both pages
    ogg_page page1, page2;
    demuxer.getNextPage(&page1);
    demuxer.getNextPage(&page2);
    
    // Test getting previous page with specific serial number
    ogg_page prev_page;
    int result = demuxer.getPrevPageSerial(&prev_page, 12345);
    
    ASSERT_TRUE(result > 0, "Should succeed");
    ASSERT_EQUALS(ogg_page_serialno(&prev_page), 12345U, "Should match requested serial");
}

void PageExtractionTest::testGetPrevPageSerialNotFound() {
    auto handler = std::make_unique<FileIOHandler>("test_multi.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    // Initialize demuxer
    ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse container");
    
    // Advance past first page
    ogg_page page;
    demuxer.getNextPage(&page);
    
    // Test getting previous page with non-existent serial number
    ogg_page prev_page;
    int result = demuxer.getPrevPageSerial(&prev_page, 99999);
    
    ASSERT_TRUE(result < 0, "Should fail - serial not found");
}

void PageExtractionTest::testGetData() {
    auto handler = std::make_unique<FileIOHandler>("test_simple.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    // Test getting data
    int result = demuxer.getData(100);
    
    ASSERT_TRUE(result > 0, "Should read some data");
    ASSERT_TRUE(result <= 100, "Should not read more than requested");
}

void PageExtractionTest::testGetDataDefaultSize() {
    auto handler = std::make_unique<FileIOHandler>("test_simple.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    // Test getting data with default size
    int result = demuxer.getData();
    
    ASSERT_TRUE(result > 0, "Should read some data");
}

void PageExtractionTest::testNullPagePointer() {
    auto handler = std::make_unique<FileIOHandler>("test_simple.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    // Test with null page pointer
    int result = demuxer.getNextPage(nullptr);
    ASSERT_TRUE(result < 0, "Should fail");
    
    result = demuxer.getPrevPage(nullptr);
    ASSERT_TRUE(result < 0, "Should fail");
    
    result = demuxer.getPrevPageSerial(nullptr, 12345);
    ASSERT_TRUE(result < 0, "Should fail");
}

void PageExtractionTest::testBoundaryConditions() {
    auto handler = std::make_unique<FileIOHandler>("test_simple.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    // Test with zero boundary
    ogg_page page;
    int result = demuxer.getNextPage(&page, 0);
    ASSERT_TRUE(result < 0, "Should fail immediately");
}

void PageExtractionTest::testCorruptedData() {
    // Create a file with invalid Ogg data
    std::vector<uint8_t> corrupt_data = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    MockOggFile::writeToFile("test_corrupt.ogg", corrupt_data);
    
    auto handler = std::make_unique<FileIOHandler>("test_corrupt.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    // Test getting page from corrupted data
    ogg_page page;
    int result = demuxer.getNextPage(&page);
    
    // Should handle corruption gracefully (either skip or return error)
    // The exact behavior depends on the corruption, but it shouldn't crash
    // We don't assert a specific result since corruption handling can vary
    
    std::remove("test_corrupt.ogg");
}

// Main test runner
int main() {
    TestFramework::TestSuite suite("Page Extraction Tests");
    
    // Register test cases
    suite.addTest(std::make_unique<PageExtractionTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}

#else // !HAVE_OGGDEMUXER

int main() {
    std::cout << "OggDemuxer not available - skipping page extraction tests" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER
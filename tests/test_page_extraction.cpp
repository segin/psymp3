/*
 * test_page_extraction.cpp - Unit tests for OggSyncManager page extraction functions
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include "demuxer/ogg/OggSyncManager.h"
#include "io/file/FileIOHandler.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>
#include <cassert>

using namespace PsyMP3::Demuxer::Ogg;
using namespace PsyMP3::IO;

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
        data.push_back('O'); data.push_back('g'); data.push_back('g'); data.push_back('S');
        data.push_back(0); // Version
        data.push_back(0x02); // Header type (BOS)
        
        // Granule position (8 bytes)
        for (int i = 0; i < 8; i++) data.push_back(0);
        
        // Serial number (4 bytes) - 12345
        data.push_back(0x39); data.push_back(0x30); data.push_back(0x00); data.push_back(0x00);
        
        // Page sequence (4 bytes)
        for (int i = 0; i < 4; i++) data.push_back(0);
        
        // Checksum (4 bytes) - placeholder
        for (int i = 0; i < 4; i++) data.push_back(0);
        
        // Page segments
        data.push_back(1);
        // Segment table
        data.push_back(10);
        
        // Packet data
        for (int i = 0; i < 10; i++) data.push_back(0x41 + i);
        
        // Checksum calculation
        ogg_page page;
        page.header = data.data();
        page.header_len = 28; // 27 fixed + 1 segment table
        page.body = data.data() + 28;
        page.body_len = 10;
        ogg_page_checksum_set(&page);
        
        return data;
    }
    
    static std::vector<uint8_t> createMultiPageOggFile() {
        std::vector<uint8_t> data;
        
        // Page 1
        auto page1 = createSimpleOggFile();
        data.insert(data.end(), page1.begin(), page1.end());
        
        // Page 2
        std::vector<uint8_t> page2;
        page2.push_back('O'); page2.push_back('g'); page2.push_back('g'); page2.push_back('S');
        page2.push_back(0);
        page2.push_back(0x00); // Normal page
        
        // Granule 1000
        page2.push_back(0xE8); page2.push_back(0x03);
        for(int i=2; i<8; i++) page2.push_back(0);
        
        // Serial 54321, Seq 1
        page2.push_back(0x31); page2.push_back(0xD4); page2.push_back(0x00); page2.push_back(0x00);
        page2.push_back(1); for(int i=1; i<4; i++) page2.push_back(0);
        
        // Checksum placeholder
        for(int i=0; i<4; i++) page2.push_back(0);
        
        page2.push_back(1); // 1 segment
        page2.push_back(15); // 15 bytes
        for(int i=0; i<15; i++) page2.push_back(0x61 + i);
        
        ogg_page page;
        page.header = page2.data();
        page.header_len = 28; // 27 fixed + 1 segment table
        page.body = page2.data() + 28;
        page.body_len = 15;
        ogg_page_checksum_set(&page);
        
        data.insert(data.end(), page2.begin(), page2.end());
        return data;
    }
    
    static void writeToFile(const std::string& filename, const std::vector<uint8_t>& data) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) throw std::runtime_error("Failed to create test file: " + filename);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
};

bool testGetData() {
    std::cout << "Testing OggSyncManager::getData()..." << std::endl;
    auto data = MockOggFile::createSimpleOggFile();
    MockOggFile::writeToFile("test_sync_simple.ogg", data);
    
    try {
        auto handler = std::make_unique<FileIOHandler>("test_sync_simple.ogg");
        OggSyncManager sync(handler.get());
        
        long result = sync.getData(10);
        ASSERT(result > 0, "Should read data");
        ASSERT(result <= 10, "Should not exceed requested");
        
        std::remove("test_sync_simple.ogg");
        std::cout << "  ✓ Passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed: " << e.what() << std::endl;
        std::remove("test_sync_simple.ogg");
        return false;
    }
}

bool testGetNextPage() {
    std::cout << "Testing OggSyncManager::getNextPage()..." << std::endl;
    auto data = MockOggFile::createSimpleOggFile();
    MockOggFile::writeToFile("test_sync_simple.ogg", data);
    
    try {
        auto handler = std::make_unique<FileIOHandler>("test_sync_simple.ogg");
        OggSyncManager sync(handler.get());
        
        ogg_page page;
        int result = sync.getNextPage(&page);
        
        ASSERT(result == 1, "Should return 1 (Got Page)");
        ASSERT(ogg_page_serialno(&page) == 12345U, "Serial mismatch");
        ASSERT(ogg_page_bos(&page), "BOS flag missing");
        
        std::remove("test_sync_simple.ogg");
        std::cout << "  ✓ Passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed: " << e.what() << std::endl;
        std::remove("test_sync_simple.ogg");
        return false;
    }
}

bool testMultiPage() {
    std::cout << "Testing OggSyncManager multiple pages..." << std::endl;
    auto data = MockOggFile::createMultiPageOggFile();
    MockOggFile::writeToFile("test_sync_multi.ogg", data);
    
    try {
        auto handler = std::make_unique<FileIOHandler>("test_sync_multi.ogg");
        OggSyncManager sync(handler.get());
        
        ogg_page page;
        // Page 1
        int result = sync.getNextPage(&page);
        ASSERT(result == 1, "Should get first page");
        ASSERT(ogg_page_serialno(&page) == 12345U, "First page serial mismatch");
        
        // Page 2
        result = sync.getNextPage(&page);
        ASSERT(result == 1, "Should get second page");
        ASSERT(ogg_page_serialno(&page) == 54321U, "Second page serial mismatch");
        ASSERT(ogg_page_pageno(&page) == 1, "Second page sequence mismatch");
        
        // EOF
        result = sync.getNextPage(&page);
        ASSERT(result == 0, "Should return 0 (EOF)");
        
        std::remove("test_sync_multi.ogg");
        std::cout << "  ✓ Passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed: " << e.what() << std::endl;
        std::remove("test_sync_multi.ogg");
        return false;
    }
}

bool testGetPrevPage() {
    std::cout << "Testing OggSyncManager::getPrevPage()..." << std::endl;
    auto data = MockOggFile::createMultiPageOggFile();
    MockOggFile::writeToFile("test_sync_multi.ogg", data);
    
    try {
        auto handler = std::make_unique<FileIOHandler>("test_sync_multi.ogg");
        OggSyncManager sync(handler.get());
        
        ogg_page page;
        // Advance to second page
        int result = sync.getNextPage(&page);
        if (result == 1) result = sync.getNextPage(&page);
        
        ASSERT(result == 1, "Should be at second page");
        ASSERT(ogg_page_serialno(&page) == 54321U, "Should be at second page serial");
        
        // Go back
        ogg_page prev_page;
        int prev_result = sync.getPrevPage(&prev_page);
        
        ASSERT(prev_result == 1, "Should find previous page");
        ASSERT(ogg_page_serialno(&prev_page) == 12345U, "Previous page serial mismatch");
        
        std::remove("test_sync_multi.ogg");
        std::cout << "  ✓ Passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed: " << e.what() << std::endl;
        std::remove("test_sync_multi.ogg");
        return false;
    }
}

bool testGetPrevPageSerial() {
    std::cout << "Testing OggSyncManager::getPrevPageSerial()..." << std::endl;
    auto data = MockOggFile::createMultiPageOggFile();
    MockOggFile::writeToFile("test_sync_multi.ogg", data);
    
    try {
        auto handler = std::make_unique<FileIOHandler>("test_sync_multi.ogg");
        OggSyncManager sync(handler.get());
        
        // Directly look for previous page with serial 12345 (assuming we are effectively after it or seeking finds it)
        // Since we start at 0, backward seek from 0 is 0.
        // Then we scan forward. We should find page with serial 12345 (first page).
        // Wait, getPrevPageSerial logic starts by seeking BACK from current pos.
        // If current pos is 0, it seeks to 0. It reads forward.
        // It should find first page.
        
        // But let's advance first to make it a real "prev" test.
        ogg_page page;
        sync.getNextPage(&page); // Page 1
        sync.getNextPage(&page); // Page 2
        
        // Now at end of Page 2 (or EOF).
        
        ogg_page prev_page;
        int result = sync.getPrevPageSerial(&prev_page, 12345);
        
        ASSERT(result == 1, "Should find page with serial 12345");
        ASSERT(ogg_page_serialno(&prev_page) == 12345U, "Serial check");
        
        std::remove("test_sync_multi.ogg");
        std::cout << "  ✓ Passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed: " << e.what() << std::endl;
        std::remove("test_sync_multi.ogg");
        return false;
    }
}

bool testGetPrevPageSerialNotFound() {
    std::cout << "Testing OggSyncManager::getPrevPageSerial() not found..." << std::endl;
    auto data = MockOggFile::createMultiPageOggFile();
    MockOggFile::writeToFile("test_sync_multi.ogg", data);
    
    try {
        auto handler = std::make_unique<FileIOHandler>("test_sync_multi.ogg");
        OggSyncManager sync(handler.get());
        
        ogg_page prev_page;
        int result = sync.getPrevPageSerial(&prev_page, 99999);
        
        ASSERT(result == 0, "Should NOT find page with serial 99999");
        
        std::remove("test_sync_multi.ogg");
        std::cout << "  ✓ Passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed: " << e.what() << std::endl;
        std::remove("test_sync_multi.ogg");
        return false;
    }
}

// Main test runner
int main() {
    std::cout << "Running OggSyncManager Tests..." << std::endl;
    std::cout << "=============================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // Run all tests
    if (testGetData()) passed++; total++;
    if (testGetNextPage()) passed++; total++;
    if (testMultiPage()) passed++; total++;
    if (testGetPrevPage()) passed++; total++;
    if (testGetPrevPageSerial()) passed++; total++;
    if (testGetPrevPageSerialNotFound()) passed++; total++;
    
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
    std::cout << "OggSyncManager not available - skipping page extraction tests" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER
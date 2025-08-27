/*
 * Debug test for IOHandler position tracking issue
 */

#include <iostream>
#include <fstream>
#include <memory>
#include <cstdio>
#include <stdexcept>

// Include the master header which has all dependencies
#include "../include/psymp3.h"

class DebugIOHandlerPositionTest {
public:
    DebugIOHandlerPositionTest() {}
    
    void run() {
        testPositionTracking();
    }
    
private:
    void testPositionTracking() {
        std::cout << "=== Debug IOHandler Position Tracking ===" << std::endl;
        
        // Create test file with known content
        std::string test_file = "debug_position_test.dat";
        createTestFile(test_file);
        
        try {
            auto handler = std::make_unique<FileIOHandler>(TagLib::String(test_file.c_str()));
            
            // Test 1: Read fLaC marker
            std::cout << "\n1. Reading fLaC marker (4 bytes at position 0)" << std::endl;
            uint8_t marker[4];
            off_t pos_before = handler->tell();
            std::cout << "Position before read: " << pos_before << std::endl;
            
            size_t bytes_read = handler->read(marker, 1, 4);
            std::cout << "Bytes read: " << bytes_read << std::endl;
            std::cout << "Marker: " << marker[0] << marker[1] << marker[2] << marker[3] << std::endl;
            
            off_t pos_after = handler->tell();
            std::cout << "Position after read: " << pos_after << std::endl;
            
            // Test 2: Read metadata header
            std::cout << "\n2. Reading metadata header (4 bytes at current position)" << std::endl;
            uint8_t metadata[4];
            pos_before = handler->tell();
            std::cout << "Position before read: " << pos_before << std::endl;
            
            bytes_read = handler->read(metadata, 1, 4);
            std::cout << "Bytes read: " << bytes_read << std::endl;
            
            pos_after = handler->tell();
            std::cout << "Position after read: " << pos_after << std::endl;
            
            // Test 3: Check if position jumped unexpectedly
            if (pos_after == 131072) {
                std::cout << "*** ISSUE REPRODUCED: Position jumped to 131072! ***" << std::endl;
                std::cout << "Expected position: 8, Actual position: " << pos_after << std::endl;
            } else if (pos_after == 8) {
                std::cout << "Position is correct (8)" << std::endl;
            } else {
                std::cout << "Position is unexpected: " << pos_after << " (expected 8)" << std::endl;
            }
            
            // Test 4: Force buffer invalidation and try again
            std::cout << "\n4. Testing with forced buffer operations" << std::endl;
            handler->seek(0, SEEK_SET);
            
            // Read a large chunk to force buffering
            std::vector<uint8_t> large_buffer(1024);
            bytes_read = handler->read(large_buffer.data(), 1, 1024);
            std::cout << "Read large chunk: " << bytes_read << " bytes" << std::endl;
            
            off_t pos_after_large = handler->tell();
            std::cout << "Position after large read: " << pos_after_large << std::endl;
            
            // Now seek back and read small amounts
            handler->seek(4, SEEK_SET);
            pos_before = handler->tell();
            std::cout << "Position after seek to 4: " << pos_before << std::endl;
            
            bytes_read = handler->read(metadata, 1, 4);
            pos_after = handler->tell();
            std::cout << "Position after reading 4 bytes from position 4: " << pos_after << std::endl;
            
            if (pos_after == 131072) {
                std::cout << "*** ISSUE REPRODUCED: Position jumped to 131072 after buffer operations! ***" << std::endl;
            } else if (pos_after == 8) {
                std::cout << "Position is correct (8) after buffer operations" << std::endl;
            } else {
                std::cout << "Position is unexpected after buffer operations: " << pos_after << " (expected 8)" << std::endl;
            }
            
            // Test 5: Try to trigger the specific issue mentioned in the task
            std::cout << "\n5. Testing specific FLAC demuxer pattern" << std::endl;
            handler->seek(0, SEEK_SET);
            
            // Read exactly like FLAC demuxer would
            uint8_t flac_marker[4];
            bytes_read = handler->read(flac_marker, 1, 4);
            std::cout << "FLAC marker read: " << bytes_read << " bytes, position: " << handler->tell() << std::endl;
            
            // This is where the issue might occur - reading the next 4 bytes
            uint8_t metadata_header[4];
            bytes_read = handler->read(metadata_header, 1, 4);
            off_t final_pos = handler->tell();
            std::cout << "Metadata header read: " << bytes_read << " bytes, position: " << final_pos << std::endl;
            
            if (final_pos == 131072) {
                std::cout << "*** CRITICAL ISSUE REPRODUCED: Position jumped from 4 to 131072! ***" << std::endl;
                std::cout << "This matches the issue described in the task!" << std::endl;
            } else if (final_pos == 8) {
                std::cout << "Position tracking is working correctly" << std::endl;
            } else {
                std::cout << "Unexpected position: " << final_pos << std::endl;
            }
            
            // Test 6: Try to trigger the bug by forcing a seek operation that calls tell_internal
            std::cout << "\n6. Testing seek operation that might trigger tell_internal bug" << std::endl;
            handler->seek(0, SEEK_SET);
            
            // Read to fill buffer
            uint8_t buffer[4];
            handler->read(buffer, 1, 4);
            std::cout << "After reading 4 bytes, position: " << handler->tell() << std::endl;
            
            // Now do a SEEK_CUR operation which calls tell_internal() in the seek logic
            int seek_result = handler->seek(0, SEEK_CUR);  // This should be a no-op but calls tell_internal
            std::cout << "After SEEK_CUR(0), position: " << handler->tell() << std::endl;
            
            off_t pos_after_seek_cur = handler->tell();
            if (pos_after_seek_cur == 131072) {
                std::cout << "*** BUG REPRODUCED: SEEK_CUR corrupted position to 131072! ***" << std::endl;
            } else if (pos_after_seek_cur == 4) {
                std::cout << "Position correctly maintained after SEEK_CUR" << std::endl;
            } else {
                std::cout << "Unexpected position after SEEK_CUR: " << pos_after_seek_cur << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "Exception: " << e.what() << std::endl;
        }
        
        // Clean up
        std::remove(test_file.c_str());
    }
    
    void createTestFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to create test file");
        }
        
        // Write "fLaC" at the beginning
        file.write("fLaC", 4);
        
        // Write some metadata header pattern
        uint8_t metadata_header[4] = {0x00, 0x00, 0x00, 0x22}; // STREAMINFO block, 34 bytes
        file.write(reinterpret_cast<char*>(metadata_header), 4);
        
        // Write 34 bytes of STREAMINFO data
        for (int i = 0; i < 34; i++) {
            file.put(static_cast<char>(i));
        }
        
        // Fill rest with pattern up to 200KB to trigger buffering
        for (size_t i = 42; i < 200 * 1024; i++) {
            file.put(static_cast<char>(i % 256));
        }
    }
};

int main() {
    DebugIOHandlerPositionTest test;
    test.run();
    return 0;
}
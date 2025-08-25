#include "psymp3.h"
#include <fstream>
#include <vector>
#include <random>

/**
 * Comprehensive IOHandler seek behavior tests
 * Tests the FileIOHandler seek/tell/read position tracking
 */

class IOHandlerSeekTester {
private:
    std::string m_test_file;
    std::vector<uint8_t> m_test_data;
    
public:
    IOHandlerSeekTester() : m_test_file("test_seek_file.dat") {
        // Create test data with known patterns
        generateTestData();
        writeTestFile();
    }
    
    ~IOHandlerSeekTester() {
        // Clean up test file
        std::remove(m_test_file.c_str());
    }
    
    void generateTestData() {
        // Create 1MB of test data with predictable patterns
        m_test_data.resize(1024 * 1024);
        
        for (size_t i = 0; i < m_test_data.size(); i++) {
            // Pattern: position % 256, so we can verify exact positions
            m_test_data[i] = static_cast<uint8_t>(i % 256);
        }
        
        // Add some special markers at key positions
        // Marker at position 0: "STRT"
        if (m_test_data.size() >= 4) {
            m_test_data[0] = 'S';
            m_test_data[1] = 'T';
            m_test_data[2] = 'R';
            m_test_data[3] = 'T';
        }
        
        // Marker at position 128KB: "128K"
        size_t pos_128k = 128 * 1024;
        if (m_test_data.size() >= pos_128k + 4) {
            m_test_data[pos_128k] = '1';
            m_test_data[pos_128k + 1] = '2';
            m_test_data[pos_128k + 2] = '8';
            m_test_data[pos_128k + 3] = 'K';
        }
        
        // Marker at position 256KB: "256K"
        size_t pos_256k = 256 * 1024;
        if (m_test_data.size() >= pos_256k + 4) {
            m_test_data[pos_256k] = '2';
            m_test_data[pos_256k + 1] = '5';
            m_test_data[pos_256k + 2] = '6';
            m_test_data[pos_256k + 3] = 'K';
        }
    }
    
    void writeTestFile() {
        std::ofstream file(m_test_file, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to create test file");
        }
        file.write(reinterpret_cast<const char*>(m_test_data.data()), m_test_data.size());
        file.close();
    }
    
    bool testBasicSeekTell() {
        std::cout << "\n=== Basic Seek/Tell Test ===" << std::endl;
        
        auto handler = std::make_unique<FileIOHandler>(TagLib::String(m_test_file.c_str()));
        
        // Test 1: Initial position should be 0
        off_t pos = handler->tell();
        std::cout << "Initial position: " << pos << std::endl;
        if (pos != 0) {
            std::cout << "FAIL: Initial position should be 0, got " << pos << std::endl;
            return false;
        }
        
        // Test 2: Seek to various positions and verify tell()
        std::vector<off_t> test_positions = {0, 1, 4, 100, 1024, 4096, 65536, 131072, 262144};
        
        for (off_t target_pos : test_positions) {
            if (target_pos >= static_cast<off_t>(m_test_data.size())) continue;
            
            std::cout << "Seeking to position " << target_pos << "..." << std::endl;
            
            int seek_result = handler->seek(target_pos, SEEK_SET);
            if (seek_result != 0) {
                std::cout << "FAIL: Seek to " << target_pos << " failed with result " << seek_result << std::endl;
                return false;
            }
            
            off_t actual_pos = handler->tell();
            if (actual_pos != target_pos) {
                std::cout << "FAIL: After seeking to " << target_pos << ", tell() returned " << actual_pos << std::endl;
                return false;
            }
            
            std::cout << "  OK: Seek to " << target_pos << " successful" << std::endl;
        }
        
        std::cout << "Basic seek/tell test PASSED" << std::endl;
        return true;
    }
    
    bool testSeekReadPositionTracking() {
        std::cout << "\n=== Seek/Read Position Tracking Test ===" << std::endl;
        
        auto handler = std::make_unique<FileIOHandler>(TagLib::String(m_test_file.c_str()));
        
        // Test reading small amounts and checking position tracking
        std::vector<std::pair<off_t, size_t>> test_cases = {
            {0, 1},      // Read 1 byte from start
            {0, 4},      // Read 4 bytes from start (like fLaC marker)
            {4, 4},      // Read 4 bytes from position 4 (like metadata header)
            {8, 34},     // Read 34 bytes from position 8 (like STREAMINFO)
            {100, 10},   // Random position
            {1000, 50},  // Another random position
            {65536, 1},  // 64KB boundary
            {131072, 4}, // 128KB boundary (problematic position)
            {262144, 8}  // 256KB boundary
        };
        
        for (auto& test_case : test_cases) {
            off_t start_pos = test_case.first;
            size_t read_size = test_case.second;
            
            if (start_pos + static_cast<off_t>(read_size) > static_cast<off_t>(m_test_data.size())) {
                continue;
            }
            
            std::cout << "Testing: seek to " << start_pos << ", read " << read_size << " bytes" << std::endl;
            
            // Seek to start position
            int seek_result = handler->seek(start_pos, SEEK_SET);
            if (seek_result != 0) {
                std::cout << "FAIL: Seek to " << start_pos << " failed" << std::endl;
                return false;
            }
            
            // Verify position before read
            off_t pos_before_read = handler->tell();
            if (pos_before_read != start_pos) {
                std::cout << "FAIL: Position before read should be " << start_pos << ", got " << pos_before_read << std::endl;
                return false;
            }
            
            // Read data
            std::vector<uint8_t> buffer(read_size);
            size_t bytes_read = handler->read(buffer.data(), 1, read_size);
            if (bytes_read != read_size) {
                std::cout << "FAIL: Expected to read " << read_size << " bytes, got " << bytes_read << std::endl;
                return false;
            }
            
            // Verify position after read
            off_t pos_after_read = handler->tell();
            off_t expected_pos = start_pos + static_cast<off_t>(read_size);
            if (pos_after_read != expected_pos) {
                std::cout << "FAIL: Position after read should be " << expected_pos << ", got " << pos_after_read << std::endl;
                std::cout << "  This indicates a buffering position tracking issue!" << std::endl;
                return false;
            }
            
            // Verify data correctness
            for (size_t i = 0; i < read_size; i++) {
                size_t file_offset = start_pos + i;
                uint8_t expected = m_test_data[file_offset];
                uint8_t actual = buffer[i];
                
                if (actual != expected) {
                    std::cout << "FAIL: Data mismatch at file offset " << file_offset 
                              << " (read offset " << i << "): expected 0x" << std::hex 
                              << static_cast<int>(expected) << ", got 0x" 
                              << static_cast<int>(actual) << std::dec << std::endl;
                    return false;
                }
            }
            
            std::cout << "  OK: Position tracking and data correct" << std::endl;
        }
        
        std::cout << "Seek/read position tracking test PASSED" << std::endl;
        return true;
    }
    
    bool testSequentialReadAfterSeek() {
        std::cout << "\n=== Sequential Read After Seek Test ===" << std::endl;
        
        auto handler = std::make_unique<FileIOHandler>(TagLib::String(m_test_file.c_str()));
        
        // Test the specific FLAC demuxer pattern:
        // 1. Seek to 0
        // 2. Read 4 bytes (fLaC marker)
        // 3. Read 4 bytes (metadata header)
        // 4. Read 34 bytes (STREAMINFO data)
        
        std::cout << "Simulating FLAC demuxer read pattern..." << std::endl;
        
        // Step 1: Seek to beginning
        std::cout << "Step 1: Seek to position 0" << std::endl;
        int seek_result = handler->seek(0, SEEK_SET);
        if (seek_result != 0) {
            std::cout << "FAIL: Initial seek failed" << std::endl;
            return false;
        }
        
        off_t pos = handler->tell();
        std::cout << "  Position after seek: " << pos << std::endl;
        if (pos != 0) {
            std::cout << "FAIL: Position should be 0" << std::endl;
            return false;
        }
        
        // Step 2: Read fLaC marker (4 bytes)
        std::cout << "Step 2: Read fLaC marker (4 bytes)" << std::endl;
        uint8_t flac_marker[4];
        size_t bytes_read = handler->read(flac_marker, 1, 4);
        if (bytes_read != 4) {
            std::cout << "FAIL: Failed to read fLaC marker" << std::endl;
            return false;
        }
        
        pos = handler->tell();
        std::cout << "  Position after reading fLaC marker: " << pos << std::endl;
        if (pos != 4) {
            std::cout << "FAIL: Position should be 4, got " << pos << std::endl;
            std::cout << "  This is the buffering issue we're trying to fix!" << std::endl;
            return false;
        }
        
        // Verify fLaC marker data
        if (flac_marker[0] != 'S' || flac_marker[1] != 'T' || 
            flac_marker[2] != 'R' || flac_marker[3] != 'T') {
            std::cout << "FAIL: fLaC marker data incorrect" << std::endl;
            return false;
        }
        
        // Step 3: Read metadata header (4 bytes)
        std::cout << "Step 3: Read metadata header (4 bytes)" << std::endl;
        uint8_t metadata_header[4];
        bytes_read = handler->read(metadata_header, 1, 4);
        if (bytes_read != 4) {
            std::cout << "FAIL: Failed to read metadata header" << std::endl;
            return false;
        }
        
        pos = handler->tell();
        std::cout << "  Position after reading metadata header: " << pos << std::endl;
        if (pos != 8) {
            std::cout << "FAIL: Position should be 8, got " << pos << std::endl;
            return false;
        }
        
        // Step 4: Read STREAMINFO data (34 bytes)
        std::cout << "Step 4: Read STREAMINFO data (34 bytes)" << std::endl;
        uint8_t streaminfo_data[34];
        bytes_read = handler->read(streaminfo_data, 1, 34);
        if (bytes_read != 34) {
            std::cout << "FAIL: Failed to read STREAMINFO data" << std::endl;
            return false;
        }
        
        pos = handler->tell();
        std::cout << "  Position after reading STREAMINFO: " << pos << std::endl;
        if (pos != 42) {
            std::cout << "FAIL: Position should be 42, got " << pos << std::endl;
            return false;
        }
        
        std::cout << "Sequential read after seek test PASSED" << std::endl;
        return true;
    }
    
    bool testBufferBoundarySeeks() {
        std::cout << "\n=== Buffer Boundary Seek Test ===" << std::endl;
        
        auto handler = std::make_unique<FileIOHandler>(TagLib::String(m_test_file.c_str()));
        
        // Test seeks around buffer boundaries (64KB, 128KB, 256KB)
        std::vector<off_t> boundary_positions = {
            65535, 65536, 65537,    // 64KB boundary
            131071, 131072, 131073, // 128KB boundary  
            262143, 262144, 262145  // 256KB boundary
        };
        
        for (off_t pos : boundary_positions) {
            if (pos >= static_cast<off_t>(m_test_data.size())) continue;
            
            std::cout << "Testing boundary position " << pos << std::endl;
            
            // Seek to position
            int seek_result = handler->seek(pos, SEEK_SET);
            if (seek_result != 0) {
                std::cout << "FAIL: Seek to " << pos << " failed" << std::endl;
                return false;
            }
            
            // Verify tell()
            off_t actual_pos = handler->tell();
            if (actual_pos != pos) {
                std::cout << "FAIL: tell() returned " << actual_pos << " instead of " << pos << std::endl;
                return false;
            }
            
            // Read 1 byte and verify position
            uint8_t byte;
            size_t bytes_read = handler->read(&byte, 1, 1);
            if (bytes_read != 1) {
                std::cout << "FAIL: Failed to read byte at position " << pos << std::endl;
                return false;
            }
            
            off_t pos_after_read = handler->tell();
            if (pos_after_read != pos + 1) {
                std::cout << "FAIL: Position after read should be " << (pos + 1) 
                          << ", got " << pos_after_read << std::endl;
                return false;
            }
            
            // Verify data
            uint8_t expected = m_test_data[pos];
            if (byte != expected) {
                std::cout << "FAIL: Data at position " << pos << " should be 0x" 
                          << std::hex << static_cast<int>(expected) << ", got 0x" 
                          << static_cast<int>(byte) << std::dec << std::endl;
                return false;
            }
            
            std::cout << "  OK: Position " << pos << " correct" << std::endl;
        }
        
        std::cout << "Buffer boundary seek test PASSED" << std::endl;
        return true;
    }
    
    bool testRandomSeekPattern() {
        std::cout << "\n=== Random Seek Pattern Test ===" << std::endl;
        
        auto handler = std::make_unique<FileIOHandler>(TagLib::String(m_test_file.c_str()));
        
        // Generate random seek pattern
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<off_t> pos_dist(0, m_test_data.size() - 100);
        std::uniform_int_distribution<size_t> size_dist(1, 50);
        
        const int num_tests = 50;
        
        for (int i = 0; i < num_tests; i++) {
            off_t seek_pos = pos_dist(gen);
            size_t read_size = size_dist(gen);
            
            // Ensure we don't read past end of file
            if (seek_pos + static_cast<off_t>(read_size) > static_cast<off_t>(m_test_data.size())) {
                read_size = m_test_data.size() - seek_pos;
            }
            
            std::cout << "Random test " << (i + 1) << ": seek to " << seek_pos 
                      << ", read " << read_size << " bytes" << std::endl;
            
            // Seek
            int seek_result = handler->seek(seek_pos, SEEK_SET);
            if (seek_result != 0) {
                std::cout << "FAIL: Seek to " << seek_pos << " failed" << std::endl;
                return false;
            }
            
            // Verify position
            off_t pos_before_read = handler->tell();
            if (pos_before_read != seek_pos) {
                std::cout << "FAIL: Position before read should be " << seek_pos 
                          << ", got " << pos_before_read << std::endl;
                return false;
            }
            
            // Read
            std::vector<uint8_t> buffer(read_size);
            size_t bytes_read = handler->read(buffer.data(), 1, read_size);
            if (bytes_read != read_size) {
                std::cout << "FAIL: Read " << bytes_read << " bytes instead of " << read_size << std::endl;
                return false;
            }
            
            // Verify position after read
            off_t pos_after_read = handler->tell();
            off_t expected_pos = seek_pos + static_cast<off_t>(read_size);
            if (pos_after_read != expected_pos) {
                std::cout << "FAIL: Position after read should be " << expected_pos 
                          << ", got " << pos_after_read << std::endl;
                return false;
            }
            
            // Verify data
            for (size_t j = 0; j < read_size; j++) {
                uint8_t expected = m_test_data[seek_pos + j];
                if (buffer[j] != expected) {
                    std::cout << "FAIL: Data mismatch at offset " << (seek_pos + j) << std::endl;
                    return false;
                }
            }
        }
        
        std::cout << "Random seek pattern test PASSED" << std::endl;
        return true;
    }
    
    bool runAllTests() {
        std::cout << "Running comprehensive IOHandler seek tests..." << std::endl;
        std::cout << "Test file: " << m_test_file << " (" << m_test_data.size() << " bytes)" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testBasicSeekTell();
        all_passed &= testSeekReadPositionTracking();
        all_passed &= testSequentialReadAfterSeek();
        all_passed &= testBufferBoundarySeeks();
        all_passed &= testRandomSeekPattern();
        
        std::cout << "\n=== FINAL RESULTS ===" << std::endl;
        if (all_passed) {
            std::cout << "✅ ALL TESTS PASSED - IOHandler seek behavior is correct" << std::endl;
        } else {
            std::cout << "❌ SOME TESTS FAILED - IOHandler has seek/buffering issues" << std::endl;
        }
        
        return all_passed;
    }
};

int main() {
    try {
        IOHandlerSeekTester tester;
        bool success = tester.runAllTests();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
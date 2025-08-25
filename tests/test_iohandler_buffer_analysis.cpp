#include "psymp3.h"
#include <fstream>

/**
 * Analyze FileIOHandler buffer behavior to understand the exact issue
 */

int main() {
    std::cout << "=== FileIOHandler Buffer Behavior Analysis ===" << std::endl;
    
    // Create a small test file
    const std::string test_file = "buffer_test.dat";
    const size_t file_size = 1024 * 1024; // 1MB
    
    // Create test file with known pattern
    {
        std::ofstream file(test_file, std::ios::binary);
        for (size_t i = 0; i < file_size; i++) {
            uint8_t byte = static_cast<uint8_t>(i % 256);
            file.write(reinterpret_cast<const char*>(&byte), 1);
        }
    }
    
    auto handler = std::make_unique<FileIOHandler>(TagLib::String(test_file.c_str()));
    
    std::cout << "File size: " << file_size << " bytes" << std::endl;
    
    // Test 1: Read 1 byte and see position jump
    std::cout << "\nTest 1: Read 1 byte from position 0" << std::endl;
    handler->seek(0, SEEK_SET);
    std::cout << "Position after seek to 0: " << handler->tell() << std::endl;
    
    uint8_t byte;
    size_t bytes_read = handler->read(&byte, 1, 1);
    std::cout << "Bytes read: " << bytes_read << std::endl;
    std::cout << "Position after reading 1 byte: " << handler->tell() << std::endl;
    std::cout << "Data read: 0x" << std::hex << static_cast<int>(byte) << std::dec << std::endl;
    
    // Test 2: Read different amounts and see position jumps
    std::vector<size_t> read_sizes = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
    
    for (size_t read_size : read_sizes) {
        std::cout << "\nTest: Read " << read_size << " bytes from position 0" << std::endl;
        
        handler->seek(0, SEEK_SET);
        off_t pos_before = handler->tell();
        
        std::vector<uint8_t> buffer(read_size);
        bytes_read = handler->read(buffer.data(), 1, read_size);
        
        off_t pos_after = handler->tell();
        
        std::cout << "  Position before: " << pos_before << std::endl;
        std::cout << "  Bytes read: " << bytes_read << std::endl;
        std::cout << "  Position after: " << pos_after << std::endl;
        std::cout << "  Expected position: " << read_size << std::endl;
        std::cout << "  Position jump: " << (pos_after - read_size) << " bytes" << std::endl;
        
        if (pos_after != static_cast<off_t>(read_size)) {
            std::cout << "  ❌ POSITION TRACKING ERROR!" << std::endl;
        } else {
            std::cout << "  ✅ Position tracking correct" << std::endl;
        }
    }
    
    // Test 3: Multiple small reads to see cumulative effect
    std::cout << "\nTest 3: Multiple small reads (simulating FLAC parsing)" << std::endl;
    handler->seek(0, SEEK_SET);
    
    // Read fLaC marker (4 bytes)
    uint8_t flac_marker[4];
    bytes_read = handler->read(flac_marker, 1, 4);
    off_t pos_after_flac = handler->tell();
    std::cout << "After reading fLaC marker (4 bytes): position = " << pos_after_flac << std::endl;
    
    // Read metadata header (4 bytes) - this should be at position 4
    uint8_t metadata_header[4];
    bytes_read = handler->read(metadata_header, 1, 4);
    off_t pos_after_header = handler->tell();
    std::cout << "After reading metadata header (4 bytes): position = " << pos_after_header << std::endl;
    
    // Read STREAMINFO (34 bytes) - this should be at position 8
    uint8_t streaminfo[34];
    bytes_read = handler->read(streaminfo, 1, 34);
    off_t pos_after_streaminfo = handler->tell();
    std::cout << "After reading STREAMINFO (34 bytes): position = " << pos_after_streaminfo << std::endl;
    
    std::cout << "Expected final position: 42" << std::endl;
    std::cout << "Actual final position: " << pos_after_streaminfo << std::endl;
    
    // Test 4: Check if seeking fixes the position
    std::cout << "\nTest 4: Position correction with seeks" << std::endl;
    
    handler->seek(0, SEEK_SET);
    std::cout << "Seek to 0, position: " << handler->tell() << std::endl;
    
    handler->read(flac_marker, 1, 4);
    std::cout << "After reading 4 bytes, position: " << handler->tell() << std::endl;
    
    // Force seek to correct position
    handler->seek(4, SEEK_SET);
    std::cout << "After corrective seek to 4, position: " << handler->tell() << std::endl;
    
    handler->read(metadata_header, 1, 4);
    std::cout << "After reading 4 more bytes, position: " << handler->tell() << std::endl;
    
    // Clean up
    std::remove(test_file.c_str());
    
    return 0;
}
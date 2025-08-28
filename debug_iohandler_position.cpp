#include "psymp3.h"

int main() {
    try {
        // Create a test file with known content
        std::string test_file = "debug_position_test.dat";
        
        // Create test file with specific pattern
        {
            std::ofstream file(test_file, std::ios::binary);
            if (!file) {
                std::cerr << "Failed to create test file" << std::endl;
                return 1;
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
            
            // Fill rest with pattern up to 200KB
            for (size_t i = 42; i < 200 * 1024; i++) {
                file.put(static_cast<char>(i % 256));
            }
        }
        
        // Now test FileIOHandler behavior
        auto handler = std::make_unique<FileIOHandler>(TagLib::String(test_file.c_str()));
        
        std::cout << "=== Testing FileIOHandler Position Tracking ===" << std::endl;
        
        // Test 1: Read fLaC marker
        std::cout << "\n1. Reading fLaC marker (4 bytes at position 0)" << std::endl;
        uint8_t marker[4];
        off_t pos_before = handler->tell();
        std::cout << "Position before read: " << pos_before << std::endl;
        
        size_t bytes_read = handler->read(marker, 1, 4);
        std::cout << "Bytes read: " << bytes_read << std::endl;
        
        off_t pos_after = handler->tell();
        std::cout << "Position after read: " << pos_after << std::endl;
        
        // Test 2: Read metadata header
        std::cout << "\n2. Reading metadata header (4 bytes at position 4)" << std::endl;
        uint8_t metadata[4];
        pos_before = handler->tell();
        std::cout << "Position before read: " << pos_before << std::endl;
        
        bytes_read = handler->read(metadata, 1, 4);
        std::cout << "Bytes read: " << bytes_read << std::endl;
        
        pos_after = handler->tell();
        std::cout << "Position after read: " << pos_after << std::endl;
        
        // Test 3: Seek to position 4 and read
        std::cout << "\n3. Seeking to position 4 and reading" << std::endl;
        int seek_result = handler->seek(4, SEEK_SET);
        std::cout << "Seek result: " << seek_result << std::endl;
        
        pos_before = handler->tell();
        std::cout << "Position after seek: " << pos_before << std::endl;
        
        bytes_read = handler->read(metadata, 1, 4);
        std::cout << "Bytes read: " << bytes_read << std::endl;
        
        pos_after = handler->tell();
        std::cout << "Position after read: " << pos_after << std::endl;
        
        // Test 4: Check if position jumps to 131072
        std::cout << "\n4. Checking for position jump to 131072" << std::endl;
        if (pos_after == 131072) {
            std::cout << "*** ISSUE REPRODUCED: Position jumped to 131072! ***" << std::endl;
        } else if (pos_after == 8) {
            std::cout << "Position is correct (8)" << std::endl;
        } else {
            std::cout << "Position is unexpected: " << pos_after << std::endl;
        }
        
        // Clean up
        handler.reset();
        std::remove(test_file.c_str());
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
#include <iostream>
#include <vector>
#include <cstdint>

int main() {
    // Test the minimal frame data generation
    std::vector<uint8_t> frame_data;
    
    // Minimal FLAC frame header (from test)
    frame_data.push_back(0xFF); // Sync code start
    frame_data.push_back(0xF8); // Sync code end + reserved + blocking strategy
    frame_data.push_back(0x69); // Block size + sample rate
    frame_data.push_back(0x04); // Channel assignment + sample size + reserved
    frame_data.push_back(0x00); // Frame number (UTF-8 coded, single byte)
    frame_data.push_back(0x8A); // CRC-8 (dummy value)
    
    std::cout << "Generated frame header bytes:" << std::endl;
    for (size_t i = 0; i < frame_data.size(); i++) {
        std::cout << "  [" << i << "] = 0x" << std::hex << (int)frame_data[i] << std::dec << std::endl;
    }
    
    // Check sync code
    uint16_t sync_code = (static_cast<uint16_t>(frame_data[0]) << 8) | 
                        static_cast<uint16_t>(frame_data[1]);
    std::cout << "\nSync code: 0x" << std::hex << sync_code << std::dec << std::endl;
    std::cout << "Sync code & 0xFFFC = 0x" << std::hex << (sync_code & 0xFFFC) << std::dec << std::endl;
    std::cout << "Expected: 0xFFF8" << std::endl;
    std::cout << "Match: " << ((sync_code & 0xFFFC) == 0xFFF8 ? "YES" : "NO") << std::endl;
    
    // Check reserved bit
    std::cout << "\nReserved bit check:" << std::endl;
    std::cout << "frame_data[1] & 0x02 = 0x" << std::hex << (frame_data[1] & 0x02) << std::dec << std::endl;
    std::cout << "Should be 0: " << ((frame_data[1] & 0x02) == 0 ? "YES" : "NO") << std::endl;
    
    return 0;
}
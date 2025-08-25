#include <iostream>
#include <iomanip>
#include <cstdint>

void analyzeBitExtraction(const uint8_t* data, const std::string& label) {
    std::cout << "\n=== " << label << " ===" << std::endl;
    
    // Print raw bytes
    std::cout << "Raw bytes 10-13: ";
    for (int i = 10; i <= 13; i++) {
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
    
    // According to RFC 9639, the layout is:
    // Bytes 10-12 + 4 bits of byte 13: Sample rate (20 bits)
    // Next 3 bits: Channels - 1
    // Next 5 bits: Bits per sample - 1
    // Remaining bits + bytes 14-17: Total samples (36 bits)
    
    // Let's build a 32-bit value from bytes 10-13
    uint32_t full_packed = (static_cast<uint32_t>(data[10]) << 24) |
                           (static_cast<uint32_t>(data[11]) << 16) |
                           (static_cast<uint32_t>(data[12]) << 8) |
                           static_cast<uint32_t>(data[13]);
    
    std::cout << "Full 32-bit packed: 0x" << std::hex << full_packed << std::dec << std::endl;
    
    // Sample rate: top 20 bits
    uint32_t sample_rate = (full_packed >> 12) & 0xFFFFF;
    std::cout << "Sample rate (top 20 bits): " << sample_rate << " Hz" << std::endl;
    
    // Channels: next 3 bits + 1
    uint8_t channels = ((full_packed >> 9) & 0x07) + 1;
    std::cout << "Channels (next 3 bits + 1): " << static_cast<int>(channels) << std::endl;
    
    // Bits per sample: next 5 bits + 1
    uint8_t bits_per_sample = ((full_packed >> 4) & 0x1F) + 1;
    std::cout << "Bits per sample (next 5 bits + 1): " << static_cast<int>(bits_per_sample) << std::endl;
    
    // Total samples: bottom 4 bits + bytes 14-17
    uint64_t total_samples = (static_cast<uint64_t>(full_packed & 0x0F) << 32) |
                             (static_cast<uint64_t>(data[14]) << 24) |
                             (static_cast<uint64_t>(data[15]) << 16) |
                             (static_cast<uint64_t>(data[16]) << 8) |
                             static_cast<uint64_t>(data[17]);
    std::cout << "Total samples: " << total_samples << std::endl;
    
    if (sample_rate > 0) {
        uint64_t duration_ms = (total_samples * 1000ULL) / sample_rate;
        std::cout << "Calculated duration: " << duration_ms << " ms" << std::endl;
    }
}

int main() {
    // File 1 data (almost monday)
    uint8_t file1_data[] = {
        0x66, 0x4c, 0x61, 0x43, 0x00, 0x00, 0x00, 0x22, 0x10, 0x00, 
        0x10, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x36, 0x2e, 0x0a, 0xc4, 
        0x42, 0xf0, 0x00, 0x67, 0x9e, 0x32, 0x70, 0x97, 0xe6, 0xd8, 
        0x56, 0x42, 0x3c, 0xe0, 0x83, 0x52, 0xd7, 0x7f, 0x24, 0xd6, 
        0xa0, 0x4a
    };
    
    // File 2 data (RADIO GA GA)
    uint8_t file2_data[] = {
        0x66, 0x4c, 0x61, 0x43, 0x00, 0x00, 0x00, 0x22, 0x04, 0x80, 
        0x04, 0x80, 0x00, 0x09, 0x81, 0x00, 0x13, 0x60, 0x2e, 0xe0, 
        0x03, 0x70, 0x03, 0xec, 0xe2, 0x00, 0x24, 0xc7, 0x71, 0x39, 
        0xf7, 0x3d, 0x47, 0x7d, 0x0c, 0x3d, 0x13, 0x22, 0x82, 0x3f, 
        0x0f, 0x46
    };
    
    analyzeBitExtraction(&file1_data[8], "File 1 (almost monday) - STREAMINFO");
    analyzeBitExtraction(&file2_data[8], "File 2 (RADIO GA GA) - STREAMINFO");
    
    return 0;
}
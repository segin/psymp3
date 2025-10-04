#include <iostream>
#include <iomanip>
#include <cstdint>

int main() {
    // Test data from the failing test: 0xc4 0x42 0x0f 0x00
    uint8_t data[4] = {0xc4, 0x42, 0x0f, 0x00};
    
    std::cout << "Raw bytes: ";
    for (int i = 0; i < 4; i++) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Try different interpretations
    std::cout << "\n=== Interpretation 1: Original (wrong) ===" << std::endl;
    uint32_t full_packed = (static_cast<uint32_t>(data[0]) << 24) |
                           (static_cast<uint32_t>(data[1]) << 16) |
                           (static_cast<uint32_t>(data[2]) << 8) |
                           static_cast<uint32_t>(data[3]);
    std::cout << "Full packed: 0x" << std::hex << full_packed << std::dec << std::endl;
    uint32_t sample_rate1 = (full_packed >> 12) & 0xFFFFF;
    uint8_t channels1 = ((full_packed >> 9) & 0x07) + 1;
    uint8_t bits_per_sample1 = ((full_packed >> 4) & 0x1F) + 1;
    std::cout << "Sample rate: " << sample_rate1 << std::endl;
    std::cout << "Channels: " << static_cast<int>(channels1) << std::endl;
    std::cout << "Bits per sample: " << static_cast<int>(bits_per_sample1) << std::endl;
    
    std::cout << "\n=== Interpretation 2: Corrected ===" << std::endl;
    uint32_t sample_rate2 = (static_cast<uint32_t>(data[0]) << 12) |
                            (static_cast<uint32_t>(data[1]) << 4) |
                            ((static_cast<uint32_t>(data[2]) >> 4) & 0x0F);
    uint8_t channels2 = ((data[2] >> 1) & 0x07) + 1;
    uint8_t bits_per_sample2 = (((data[2] & 0x01) << 4) | ((data[3] >> 4) & 0x0F)) + 1;
    std::cout << "Sample rate: " << sample_rate2 << std::endl;
    std::cout << "Channels: " << static_cast<int>(channels2) << std::endl;
    std::cout << "Bits per sample: " << static_cast<int>(bits_per_sample2) << std::endl;
    
    std::cout << "\n=== What would be reasonable values? ===" << std::endl;
    std::cout << "Expected sample rate: 44100 (CD quality)" << std::endl;
    std::cout << "Expected channels: 2 (stereo)" << std::endl;
    std::cout << "Expected bits per sample: 16 (CD quality)" << std::endl;
    
    // Let's see what bytes would give us 44100 Hz, 2 channels, 16 bits
    std::cout << "\n=== Reverse engineering for 44100 Hz, 2 ch, 16 bits ===" << std::endl;
    uint32_t target_rate = 44100;
    uint8_t target_channels = 2;
    uint8_t target_bits = 16;
    
    // Pack the values according to FLAC spec
    // Sample rate (20 bits) + channels-1 (3 bits) + bits_per_sample-1 (5 bits) + total_samples_high (4 bits)
    uint32_t packed = (target_rate << 12) | 
                      ((target_channels - 1) << 9) | 
                      ((target_bits - 1) << 4) | 
                      0; // total_samples_high = 0 for now
    
    std::cout << "Target packed value: 0x" << std::hex << packed << std::dec << std::endl;
    std::cout << "Target bytes: ";
    for (int i = 3; i >= 0; i--) {
        uint8_t byte = (packed >> (i * 8)) & 0xFF;
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
    
    return 0;
}
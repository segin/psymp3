#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>

// Simple test to extract STREAMINFO from a real FLAC file
struct StreamInfo {
    uint16_t min_block_size;
    uint16_t max_block_size;
    uint32_t min_frame_size;
    uint32_t max_frame_size;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint64_t total_samples;
    uint8_t md5_signature[16];
};

bool parseStreamInfo(const uint8_t* data, StreamInfo& info) {
    // Parse STREAMINFO fields according to RFC 9639 bit layout
    
    // Minimum block size (16 bits) - bytes 0-1
    info.min_block_size = (static_cast<uint16_t>(data[0]) << 8) | 
                          static_cast<uint16_t>(data[1]);
    
    // Maximum block size (16 bits) - bytes 2-3
    info.max_block_size = (static_cast<uint16_t>(data[2]) << 8) | 
                          static_cast<uint16_t>(data[3]);
    
    // Minimum frame size (24 bits) - bytes 4-6
    info.min_frame_size = (static_cast<uint32_t>(data[4]) << 16) |
                          (static_cast<uint32_t>(data[5]) << 8) |
                          static_cast<uint32_t>(data[6]);
    
    // Maximum frame size (24 bits) - bytes 7-9
    info.max_frame_size = (static_cast<uint32_t>(data[7]) << 16) |
                          (static_cast<uint32_t>(data[8]) << 8) |
                          static_cast<uint32_t>(data[9]);
    
    // Sample rate (20 bits) - bytes 10-12 + upper 4 bits of byte 13
    info.sample_rate = (static_cast<uint32_t>(data[10]) << 12) |
                       (static_cast<uint32_t>(data[11]) << 4) |
                       ((static_cast<uint32_t>(data[12]) >> 4) & 0x0F);
    
    // Channels (3 bits) - bits 1-3 of byte 12, then add 1
    info.channels = ((data[12] >> 1) & 0x07) + 1;
    
    // Bits per sample (5 bits) - bit 0 of byte 12 + upper 4 bits of byte 13, then add 1
    info.bits_per_sample = (((data[12] & 0x01) << 4) | ((data[13] >> 4) & 0x0F)) + 1;
    
    // Total samples (36 bits) - lower 4 bits of byte 13 + bytes 14-17
    info.total_samples = (static_cast<uint64_t>(data[13] & 0x0F) << 32) |
                         (static_cast<uint64_t>(data[14]) << 24) |
                         (static_cast<uint64_t>(data[15]) << 16) |
                         (static_cast<uint64_t>(data[16]) << 8) |
                         static_cast<uint64_t>(data[17]);
    
    // MD5 signature (16 bytes) - bytes 18-33
    std::memcpy(info.md5_signature, &data[18], 16);
    
    return true;
}

bool extractStreamInfoFromFile(const std::string& filename, StreamInfo& info) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return false;
    }
    
    // Read fLaC marker
    char marker[4];
    file.read(marker, 4);
    if (!file || marker[0] != 'f' || marker[1] != 'L' || marker[2] != 'a' || marker[3] != 'C') {
        std::cerr << "Invalid FLAC file (no fLaC marker)" << std::endl;
        return false;
    }
    
    // Read metadata block header
    uint8_t header[4];
    file.read(reinterpret_cast<char*>(header), 4);
    if (!file) {
        std::cerr << "Cannot read metadata block header" << std::endl;
        return false;
    }
    
    // Check if this is STREAMINFO (type 0)
    uint8_t block_type = header[0] & 0x7F;
    if (block_type != 0) {
        std::cerr << "First metadata block is not STREAMINFO (type " << static_cast<int>(block_type) << ")" << std::endl;
        return false;
    }
    
    // Get block length
    uint32_t block_length = (static_cast<uint32_t>(header[1]) << 16) |
                           (static_cast<uint32_t>(header[2]) << 8) |
                           static_cast<uint32_t>(header[3]);
    
    if (block_length != 34) {
        std::cerr << "Invalid STREAMINFO length: " << block_length << " (expected 34)" << std::endl;
        return false;
    }
    
    // Read STREAMINFO data
    uint8_t data[34];
    file.read(reinterpret_cast<char*>(data), 34);
    if (!file) {
        std::cerr << "Cannot read STREAMINFO data" << std::endl;
        return false;
    }
    
    return parseStreamInfo(data, info);
}

void printStreamInfo(const StreamInfo& info) {
    std::cout << "STREAMINFO:" << std::endl;
    std::cout << "  Min block size: " << info.min_block_size << std::endl;
    std::cout << "  Max block size: " << info.max_block_size << std::endl;
    std::cout << "  Min frame size: " << info.min_frame_size << std::endl;
    std::cout << "  Max frame size: " << info.max_frame_size << std::endl;
    std::cout << "  Sample rate: " << info.sample_rate << " Hz" << std::endl;
    std::cout << "  Channels: " << static_cast<int>(info.channels) << std::endl;
    std::cout << "  Bits per sample: " << static_cast<int>(info.bits_per_sample) << std::endl;
    std::cout << "  Total samples: " << info.total_samples << std::endl;
    
    if (info.sample_rate > 0) {
        double duration = static_cast<double>(info.total_samples) / info.sample_rate;
        std::cout << "  Duration: " << duration << " seconds" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <flac_file>" << std::endl;
        return 1;
    }
    
    StreamInfo info;
    if (extractStreamInfoFromFile(argv[1], info)) {
        printStreamInfo(info);
        return 0;
    } else {
        return 1;
    }
}
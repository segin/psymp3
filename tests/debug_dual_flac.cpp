#include "psymp3.h"

void testFlacFile(const std::string& filename, const std::string& label) {
    std::cout << "\n=== Testing " << label << " ===" << std::endl;
    std::cout << "File: " << filename << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(TagLib::String(filename.c_str()));
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        std::cout << "Created demuxer, calling parseContainer()..." << std::endl;
        bool result = demuxer->parseContainer();
        
        std::cout << "parseContainer() returned: " << (result ? "true" : "false") << std::endl;
        
        if (!result) {
            if (demuxer->hasError()) {
                auto error = demuxer->getLastError();
                std::cout << "Error: [" << error.category << "] " << error.message << std::endl;
            } else {
                std::cout << "No error information available" << std::endl;
            }
        } else {
            std::cout << "Parse succeeded!" << std::endl;
            
            auto streams = demuxer->getStreams();
            std::cout << "Found " << streams.size() << " streams" << std::endl;
            
            if (!streams.empty()) {
                const auto& stream = streams[0];
                std::cout << "Stream info:" << std::endl;
                std::cout << "  Codec: " << stream.codec_name << std::endl;
                std::cout << "  Sample rate: " << stream.sample_rate << " Hz" << std::endl;
                std::cout << "  Channels: " << stream.channels << std::endl;
                std::cout << "  Bits per sample: " << stream.bits_per_sample << std::endl;
                std::cout << "  Duration: " << stream.duration_ms << " ms" << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
}

void analyzeFlacHeader(const std::string& filename, const std::string& label) {
    std::cout << "\n=== Raw Header Analysis for " << label << " ===" << std::endl;
    
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cout << "Cannot open file for raw analysis" << std::endl;
        return;
    }
    
    // Read first 50 bytes to analyze header structure
    uint8_t header[50];
    file.read(reinterpret_cast<char*>(header), 50);
    size_t bytes_read = file.gcount();
    
    std::cout << "First " << bytes_read << " bytes (hex):" << std::endl;
    for (size_t i = 0; i < bytes_read; i++) {
        if (i % 16 == 0) std::cout << std::endl << std::setfill('0') << std::setw(4) << std::hex << i << ": ";
        std::cout << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(header[i]) << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Check for fLaC marker
    if (bytes_read >= 4) {
        std::cout << "Stream marker: ";
        for (int i = 0; i < 4; i++) {
            std::cout << static_cast<char>(header[i]);
        }
        std::cout << std::endl;
        
        if (header[0] == 'f' && header[1] == 'L' && header[2] == 'a' && header[3] == 'C') {
            std::cout << "Valid fLaC marker found" << std::endl;
            
            // Analyze STREAMINFO block header (bytes 4-7)
            if (bytes_read >= 8) {
                uint8_t block_type = header[4] & 0x7F;
                bool is_last = (header[4] & 0x80) != 0;
                uint32_t block_length = (static_cast<uint32_t>(header[5]) << 16) |
                                       (static_cast<uint32_t>(header[6]) << 8) |
                                       static_cast<uint32_t>(header[7]);
                
                std::cout << "First metadata block:" << std::endl;
                std::cout << "  Type: " << static_cast<int>(block_type) << (block_type == 0 ? " (STREAMINFO)" : "") << std::endl;
                std::cout << "  Is last: " << (is_last ? "yes" : "no") << std::endl;
                std::cout << "  Length: " << block_length << " bytes" << std::endl;
                
                // Analyze STREAMINFO data if we have enough bytes
                if (block_type == 0 && bytes_read >= 42) { // 8 header + 34 STREAMINFO
                    std::cout << "STREAMINFO data analysis:" << std::endl;
                    
                    uint8_t* data = &header[8]; // STREAMINFO starts at byte 8
                    
                    // Min/max block sizes
                    uint16_t min_block = (static_cast<uint16_t>(data[0]) << 8) | data[1];
                    uint16_t max_block = (static_cast<uint16_t>(data[2]) << 8) | data[3];
                    std::cout << "  Block size range: " << min_block << " - " << max_block << std::endl;
                    
                    // Sample rate, channels, bits per sample (packed in bytes 10-13)
                    uint32_t packed = (static_cast<uint32_t>(data[10]) << 16) |
                                     (static_cast<uint32_t>(data[11]) << 8) |
                                     static_cast<uint32_t>(data[12]);
                    
                    uint32_t sample_rate = (packed >> 4) & 0xFFFFF;
                    uint8_t channels = ((packed >> 1) & 0x07) + 1;
                    uint8_t bits_per_sample = (((packed & 0x01) << 4) | ((data[13] & 0xF0) >> 4)) + 1;
                    
                    std::cout << "  Raw packed data (bytes 10-12): 0x" << std::hex << packed << std::dec << std::endl;
                    std::cout << "  Byte 13: 0x" << std::hex << static_cast<int>(data[13]) << std::dec << std::endl;
                    std::cout << "  Sample rate: " << sample_rate << " Hz" << std::endl;
                    std::cout << "  Channels: " << static_cast<int>(channels) << std::endl;
                    std::cout << "  Bits per sample: " << static_cast<int>(bits_per_sample) << std::endl;
                    
                    // Total samples
                    uint64_t total_samples = (static_cast<uint64_t>(data[13] & 0x0F) << 32) |
                                           (static_cast<uint64_t>(data[14]) << 24) |
                                           (static_cast<uint64_t>(data[15]) << 16) |
                                           (static_cast<uint64_t>(data[16]) << 8) |
                                           static_cast<uint64_t>(data[17]);
                    
                    std::cout << "  Total samples: " << total_samples << std::endl;
                    
                    if (sample_rate > 0) {
                        uint64_t duration_ms = (total_samples * 1000ULL) / sample_rate;
                        std::cout << "  Calculated duration: " << duration_ms << " ms" << std::endl;
                    }
                }
            }
        } else {
            std::cout << "Invalid fLaC marker" << std::endl;
        }
    }
}

int main() {
    const std::string file1 = "/mnt/8TB-3/music/almost monday/DIVE/11 life goes by.flac";
    const std::string file2 = "/mnt/c/Users/segin/Downloads/38833FF26BA1D.UnigramPreview_g9c9v27vpyspw!App/RADIO GA GA.flac";
    
    // Test both files with our demuxer
    testFlacFile(file1, "File 1 (almost monday)");
    testFlacFile(file2, "File 2 (RADIO GA GA)");
    
    // Analyze raw headers
    analyzeFlacHeader(file1, "File 1 (almost monday)");
    analyzeFlacHeader(file2, "File 2 (RADIO GA GA)");
    
    return 0;
}
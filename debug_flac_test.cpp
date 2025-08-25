#include "psymp3.h"
#include <iostream>
#include <vector>
#include <memory>

class DebugIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    
public:
    explicit DebugIOHandler(const std::vector<uint8_t>& data) 
        : m_data(data), m_position(0) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t total_bytes = size * count;
        size_t available = m_data.size() - m_position;
        size_t to_read = std::min(total_bytes, available);
        
        std::cout << "READ: requested " << total_bytes << " bytes, available " << available << ", reading " << to_read << std::endl;
        
        if (to_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, to_read);
            m_position += to_read;
        }
        
        return to_read / size;
    }
    
    int seek(off_t offset, int whence) override {
        size_t new_pos;
        switch (whence) {
            case SEEK_SET:
                new_pos = static_cast<size_t>(offset);
                break;
            case SEEK_CUR:
                new_pos = m_position + static_cast<size_t>(offset);
                break;
            case SEEK_END:
                new_pos = m_data.size() + static_cast<size_t>(offset);
                break;
            default:
                return -1;
        }
        
        if (new_pos > m_data.size()) return -1;
        
        std::cout << "SEEK: from " << m_position << " to " << new_pos << std::endl;
        m_position = new_pos;
        return 0;
    }
    
    off_t tell() override {
        return static_cast<off_t>(m_position);
    }
    
    off_t getFileSize() override {
        return static_cast<off_t>(m_data.size());
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    int close() override {
        return 0;
    }
    
    int getLastError() const override {
        return 0;
    }
};

std::vector<uint8_t> generateMinimalFLAC() {
    std::vector<uint8_t> data;
    
    // fLaC stream marker (0x664C6143)
    data.push_back(0x66); // 'f'
    data.push_back(0x4C); // 'L'
    data.push_back(0x61); // 'a'
    data.push_back(0x43); // 'C'
    
    // STREAMINFO metadata block (mandatory, last block)
    data.push_back(0x80); // Last block flag (1) + STREAMINFO type (0)
    data.push_back(0x00); // Length high byte
    data.push_back(0x00); // Length middle byte
    data.push_back(0x22); // Length low byte = 34 bytes
    
    // STREAMINFO data (34 bytes)
    // Min block size (16 bits) = 4096
    data.push_back(0x10);
    data.push_back(0x00);
    
    // Max block size (16 bits) = 4096
    data.push_back(0x10);
    data.push_back(0x00);
    
    // Min frame size (24 bits) = 0 (unknown)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    
    // Max frame size (24 bits) = 0 (unknown)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    
    // Sample rate (20 bits) = 44100, channels (3 bits) = 2, bits per sample (5 bits) = 16
    uint32_t sr_ch_bps = (44100 << 12) | (1 << 9) | (15); // 2 channels = 1, 16 bits = 15
    data.push_back((sr_ch_bps >> 16) & 0xFF);
    data.push_back((sr_ch_bps >> 8) & 0xFF);
    data.push_back(sr_ch_bps & 0xFF);
    
    // Total samples (36 bits) = 1000000
    uint64_t total_samples = 1000000;
    data.push_back((total_samples >> 28) & 0xFF);
    data.push_back((total_samples >> 20) & 0xFF);
    data.push_back((total_samples >> 12) & 0xFF);
    data.push_back((total_samples >> 4) & 0xFF);
    data.push_back((total_samples << 4) & 0xF0);
    
    // MD5 signature (16 bytes) - all zeros for test
    for (int i = 0; i < 16; i++) {
        data.push_back(0x00);
    }
    
    std::cout << "Generated FLAC data: " << data.size() << " bytes" << std::endl;
    std::cout << "First 8 bytes: ";
    for (int i = 0; i < 8 && i < data.size(); i++) {
        std::cout << std::hex << (int)data[i] << " ";
    }
    std::cout << std::endl;
    
    return data;
}

int main() {
    std::cout << "Testing FLAC demuxer parsing..." << std::endl;
    
    auto data = generateMinimalFLAC();
    auto handler = std::make_unique<DebugIOHandler>(data);
    auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
    
    std::cout << "Calling parseContainer()..." << std::endl;
    bool result = demuxer->parseContainer();
    
    std::cout << "parseContainer() returned: " << (result ? "true" : "false") << std::endl;
    
    if (!result) {
        std::cout << "Parse failed. Checking error..." << std::endl;
        if (demuxer->hasError()) {
            auto error = demuxer->getLastError();
            std::cout << "Error: [" << error.category << "] " << error.message << std::endl;
        }
    } else {
        std::cout << "Parse succeeded!" << std::endl;
        auto streams = demuxer->getStreams();
        std::cout << "Found " << streams.size() << " streams" << std::endl;
    }
    
    return 0;
}
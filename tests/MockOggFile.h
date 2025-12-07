/*
 * MockOggFile.h - Helper for Ogg unit tests
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#ifndef MOCK_OGG_FILE_H
#define MOCK_OGG_FILE_H

#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <ogg/ogg.h>

class MockOggFile {
public:
    static std::vector<uint8_t> createSimpleOggFile(long serial = 12345, int packet_len = 10) {
        std::vector<uint8_t> data;
        
        // Create a simple Ogg page with "OggS" header
        // Capture pattern
        data.push_back('O'); data.push_back('g'); data.push_back('g'); data.push_back('S');
        data.push_back(0); // Version
        data.push_back(0x02); // Header type (BOS)
        
        // Granule position (8 bytes)
        for (int i = 0; i < 8; i++) data.push_back(0);
        
        // Serial number (4 bytes)
        data.push_back(serial & 0xFF);
        data.push_back((serial >> 8) & 0xFF);
        data.push_back((serial >> 16) & 0xFF);
        data.push_back((serial >> 24) & 0xFF);
        
        // Page sequence (4 bytes)
        for (int i = 0; i < 4; i++) data.push_back(0);
        
        // Checksum (4 bytes) - placeholder
        for (int i = 0; i < 4; i++) data.push_back(0);
        
        // Page segments
        int segments = (packet_len + 254) / 255;
        data.push_back(segments);
        
        // Segment table
        int remaining = packet_len;
        for (int i = 0; i < segments; i++) {
            int len = (remaining > 255) ? 255 : remaining;
            data.push_back(len);
            remaining -= len;
        }
        
        int header_len = 27 + segments + 1; // +1 for segments count byte itself? No, segment table IS segments count bytes?
        // Wait, byte 26 is segment count (N). Then N bytes follow.
        // So header size is 27 + N.
        
        header_len = 27 + segments;
        
        // Packet data
        for (int i = 0; i < packet_len; i++) data.push_back(0x41 + (i % 26));
        
        // Checksum calculation
        ogg_page page;
        page.header = data.data();
        page.header_len = header_len;
        page.body = data.data() + header_len;
        page.body_len = packet_len;
        ogg_page_checksum_set(&page);
        
        return data;
    }
    
    static std::vector<uint8_t> createMultiPageOggFile() {
        std::vector<uint8_t> data;
        
        // Page 1
        auto page1 = createSimpleOggFile(12345, 10);
        data.insert(data.end(), page1.begin(), page1.end());
        
        // Page 2
        std::vector<uint8_t> page2;
        page2.push_back('O'); page2.push_back('g'); page2.push_back('g'); page2.push_back('S');
        page2.push_back(0);
        page2.push_back(0x00); // Normal page
        
        // Granule 1000
        page2.push_back(0xE8); page2.push_back(0x03);
        for(int i=2; i<8; i++) page2.push_back(0);
        
        // Serial 54321, Seq 1
        page2.push_back(0x31); page2.push_back(0xD4); page2.push_back(0x00); page2.push_back(0x00);
        page2.push_back(1); for(int i=1; i<4; i++) page2.push_back(0);
        
        // Checksum placeholder
        for(int i=0; i<4; i++) page2.push_back(0);
        
        page2.push_back(1); // 1 segment
        page2.push_back(15); // 15 bytes
        for(int i=0; i<15; i++) page2.push_back(0x61 + i);
        
        ogg_page page;
        page.header = page2.data();
        page.header_len = 28; // 27 fixed + 1 segment table
        page.body = page2.data() + 28;
        page.body_len = 15;
        ogg_page_checksum_set(&page);
        
        data.insert(data.end(), page2.begin(), page2.end());
        return data;
    }
    
    static void writeToFile(const std::string& filename, const std::vector<uint8_t>& data) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) throw std::runtime_error("Failed to create test file: " + filename);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
};

#endif // MOCK_OGG_FILE_H

/*
 * test_data_streaming.cpp - Unit tests for OggDemuxer data streaming functionality
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include "test_framework.h"
#include "demuxer/ogg/OggDemuxer.h"
#include <fstream>
#include <memory>
#include <cstring>
#include <vector>

using namespace PsyMP3;
using namespace PsyMP3::IO;
using namespace PsyMP3::Demuxer;
using namespace PsyMP3::Demuxer::Ogg;

using namespace PsyMP3;
using namespace PsyMP3::Demuxer;
using namespace PsyMP3::Demuxer::Ogg;
using namespace PsyMP3::IO;

// Memory-based IOHandler for testing
class LocalMemoryIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position;
    
public:
    LocalMemoryIOHandler(const std::vector<uint8_t>& data) : m_data(data), m_position(0) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = size * count;
        size_t available = m_data.size() - m_position;
        size_t actual_bytes = std::min(bytes_to_read, available);
        
        if (actual_bytes > 0) {
            std::memcpy(buffer, m_data.data() + m_position, actual_bytes);
            m_position += actual_bytes;
        }
        
        return actual_bytes / size;
    }
    
    int seek(off_t offset, int whence) override {
        switch (whence) {
            case SEEK_SET:
                m_position = offset;
                break;
            case SEEK_CUR:
                m_position += offset;
                break;
            case SEEK_END:
                m_position = m_data.size() + offset;
                break;
        }
        
        if (m_position > m_data.size()) {
            m_position = m_data.size();
        }
        
        return 0;
    }
    
    off_t tell() override {
        return m_position;
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    off_t getFileSize() override {
        return m_data.size();
    }
};

class DataStreamingTest : public TestFramework::TestCase {
public:
    DataStreamingTest() : TestFramework::TestCase("DataStreamingTest") {}
    
protected:
    void runTest() override {
        testFetchAndProcessPacket();
        testFillPacketQueue();
        testHeaderPacketHandling();
        testBoundedPacketQueues();
        testPacketHoleHandling();
        testPageBoundaryHandling();
        testPositionTracking();
        testDataStreamingIntegration();
    }

private:
    void testFetchAndProcessPacket() {
        try {
            // Create a simple test Ogg file with Vorbis data
            std::vector<uint8_t> test_data = createTestOggVorbisData();
            auto handler = std::unique_ptr<LocalMemoryIOHandler>(new LocalMemoryIOHandler(test_data));
            OggDemuxer demuxer(std::move(handler));
            
            // Parse container first
            ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse test container");
            
            // Test fetchAndProcessPacket method
            int result = demuxer.fetchAndProcessPacket();
            ASSERT_TRUE(result >= 0, "fetchAndProcessPacket should not return error");
            
            // Verify that packets were processed
            auto& streams = demuxer.getStreamsForTesting();
            ASSERT_FALSE(streams.empty(), "Should have at least one stream");
            
            // Check that header packets were processed
            for (const auto& [stream_id, stream] : streams) {
                if (stream.codec_name == "vorbis") {
                    ASSERT_TRUE(stream.header_packets.size() > 0, "Should have header packets");
                    ASSERT_TRUE(stream.headers_complete, "Headers should be complete");
                }
            }
            
        } catch (const std::exception& e) {
            addFailure("Exception in fetchAndProcessPacket test: " + std::string(e.what()));
        }
    }
    
    void testFillPacketQueue() {
        try {
            // Create test data with multiple packets
            std::vector<uint8_t> test_data = createTestOggVorbisDataWithMultiplePackets();
            auto handler = std::unique_ptr<LocalMemoryIOHandler>(new LocalMemoryIOHandler(test_data));
            OggDemuxer demuxer(std::move(handler));
            
            // Parse container
            ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse test container");
            
            // Get the first audio stream
            auto streams = demuxer.getStreams();
            ASSERT_FALSE(streams.empty(), "Should have at least one stream");
            
            uint32_t stream_id = streams[0].stream_id;
            
            // Test fillPacketQueue method
            demuxer.fillPacketQueue(stream_id);
            
            // Verify that packets were queued
            auto& test_streams = demuxer.getStreamsForTesting();
            auto stream_it = test_streams.find(stream_id);
            ASSERT_TRUE(stream_it != test_streams.end(), "Stream should exist");
            
            // Should have some packets in queue after filling
            ASSERT_TRUE(stream_it->second.m_packet_queue.size() > 0, "Should have packets in queue");
            
        } catch (const std::exception& e) {
            addFailure("Exception in fillPacketQueue test: " + std::string(e.what()));
        }
    }
    
    void testHeaderPacketHandling() {
        try {
            // Create test Ogg Vorbis data
            std::vector<uint8_t> test_data = createTestOggVorbisData();
            auto handler = std::unique_ptr<LocalMemoryIOHandler>(new LocalMemoryIOHandler(test_data));
            OggDemuxer demuxer(std::move(handler));
            
            // Parse container
            ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse test container");
            
            auto streams = demuxer.getStreams();
            ASSERT_FALSE(streams.empty(), "Should have at least one stream");
            
            uint32_t stream_id = streams[0].stream_id;
            
            // Read chunks - first few should be header packets
            std::vector<MediaChunk> header_chunks;
            for (int i = 0; i < 5; i++) {
                MediaChunk chunk = demuxer.readChunk(stream_id);
                if (chunk.data.empty()) break;
                header_chunks.push_back(chunk);
            }
            
            ASSERT_TRUE(header_chunks.size() >= 3, "Should have at least 3 header packets for Vorbis");
            
            // Verify header packets have granule_position = 0 and are keyframes
            for (size_t i = 0; i < std::min(size_t(3), header_chunks.size()); i++) {
                ASSERT_EQUALS(header_chunks[i].granule_position, 0ULL, "Header packets should have granule_position = 0");
                ASSERT_TRUE(header_chunks[i].is_keyframe, "Header packets should be keyframes");
            }
            
            // Test that headers are sent only once (never resent after seeks)
            auto& test_streams = demuxer.getStreamsForTesting();
            auto stream_it = test_streams.find(stream_id);
            ASSERT_TRUE(stream_it != test_streams.end(), "Stream should exist");
            ASSERT_TRUE(stream_it->second.headers_sent, "Headers should be marked as sent");
            
            // Simulate seek and verify headers are not resent
            demuxer.seekTo(1000); // Seek to 1 second
            
            MediaChunk chunk_after_seek = demuxer.readChunk(stream_id);
            if (!chunk_after_seek.data.empty()) {
                // Should not be a header packet (granule_position should not be 0)
                ASSERT_NOT_EQUALS(chunk_after_seek.granule_position, 0ULL, "Should not resend header packets after seek");
            }
            
        } catch (const std::exception& e) {
            addFailure("Exception in headerPacketHandling test: " + std::string(e.what()));
        }
    }
    
    void testBoundedPacketQueues() {
        try {
            // Create test data with many packets
            std::vector<uint8_t> test_data = createTestOggVorbisDataWithManyPackets();
            auto handler = std::unique_ptr<LocalMemoryIOHandler>(new LocalMemoryIOHandler(test_data));
            OggDemuxer demuxer(std::move(handler));
            
            // Parse container
            ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse test container");
            
            auto streams = demuxer.getStreams();
            ASSERT_FALSE(streams.empty(), "Should have at least one stream");
            
            uint32_t stream_id = streams[0].stream_id;
            
            // Fill packet queue multiple times
            for (int i = 0; i < 20; i++) {
                demuxer.fillPacketQueue(stream_id);
            }
            
            // Verify queue size is bounded (should not exceed MAX_QUEUE_SIZE = 100)
            auto& test_streams = demuxer.getStreamsForTesting();
            auto stream_it = test_streams.find(stream_id);
            ASSERT_TRUE(stream_it != test_streams.end(), "Stream should exist");
            
            size_t queue_size = stream_it->second.m_packet_queue.size();
            ASSERT_TRUE(queue_size <= 100, "Queue size should be bounded to prevent memory exhaustion");
            
        } catch (const std::exception& e) {
            addFailure("Exception in boundedPacketQueues test: " + std::string(e.what()));
        }
    }
    
    void testPacketHoleHandling() {
        try {
            // Create test data with simulated packet holes
            std::vector<uint8_t> test_data = createTestOggDataWithHoles();
            auto handler = std::unique_ptr<LocalMemoryIOHandler>(new LocalMemoryIOHandler(test_data));
            OggDemuxer demuxer(std::move(handler));
            
            // Parse container
            ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse test container");
            
            // Test fetchAndProcessPacket with holes
            int result = demuxer.fetchAndProcessPacket();
            
            // Should handle holes gracefully (return 0 to continue, not -1 for error)
            ASSERT_TRUE(result >= 0, "Should handle packet holes gracefully like reference implementations");
            
        } catch (const std::exception& e) {
            addFailure("Exception in packetHoleHandling test: " + std::string(e.what()));
        }
    }
    
    void testPageBoundaryHandling() {
        try {
            // Create test data with packets spanning multiple pages
            std::vector<uint8_t> test_data = createTestOggDataWithSpanningPackets();
            auto handler = std::unique_ptr<LocalMemoryIOHandler>(new LocalMemoryIOHandler(test_data));
            OggDemuxer demuxer(std::move(handler));
            
            // Parse container
            ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse test container");
            
            auto streams = demuxer.getStreams();
            ASSERT_FALSE(streams.empty(), "Should have at least one stream");
            
            uint32_t stream_id = streams[0].stream_id;
            
            // Read packets that span page boundaries
            std::vector<MediaChunk> chunks;
            for (int i = 0; i < 10; i++) {
                MediaChunk chunk = demuxer.readChunk(stream_id);
                if (chunk.data.empty()) break;
                chunks.push_back(chunk);
            }
            
            // Should successfully reconstruct packets that span pages
            ASSERT_TRUE(chunks.size() > 0, "Should successfully read packets spanning page boundaries");
            
            // Verify packet integrity
            for (const auto& chunk : chunks) {
                ASSERT_TRUE(chunk.data.size() > 0, "Packets should have valid data");
                ASSERT_EQUALS(chunk.stream_id, stream_id, "Packets should have correct stream ID");
            }
            
        } catch (const std::exception& e) {
            addFailure("Exception in pageBoundaryHandling test: " + std::string(e.what()));
        }
    }
    
    void testPositionTracking() {
        try {
            // Create test data with known granule positions
            std::vector<uint8_t> test_data = createTestOggVorbisDataWithGranules();
            auto handler = std::unique_ptr<LocalMemoryIOHandler>(new LocalMemoryIOHandler(test_data));
            OggDemuxer demuxer(std::move(handler));
            
            // Parse container
            ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse test container");
            
            auto streams = demuxer.getStreams();
            ASSERT_FALSE(streams.empty(), "Should have at least one stream");
            
            uint32_t stream_id = streams[0].stream_id;
            
            uint64_t initial_position = demuxer.getPosition();
            
            // Read some packets
            for (int i = 0; i < 5; i++) {
                MediaChunk chunk = demuxer.readChunk(stream_id);
                if (chunk.data.empty()) break;
                
                // Position should be updated from granule positions
                if (chunk.granule_position != static_cast<uint64_t>(-1)) {
                    uint64_t current_position = demuxer.getPosition();
                    // Position should advance (or stay the same for header packets)
                    ASSERT_TRUE(current_position >= initial_position, "Position should advance or stay the same");
                }
            }
            
        } catch (const std::exception& e) {
            addFailure("Exception in positionTracking test: " + std::string(e.what()));
        }
    }
    
    void testDataStreamingIntegration() {
        try {
            // Create comprehensive test data
            std::vector<uint8_t> test_data = createTestOggVorbisData();
            auto handler = std::unique_ptr<LocalMemoryIOHandler>(new LocalMemoryIOHandler(test_data));
            OggDemuxer demuxer(std::move(handler));
            
            // Parse container
            ASSERT_TRUE(demuxer.parseContainer(), "Failed to parse test container");
            
            auto streams = demuxer.getStreams();
            ASSERT_FALSE(streams.empty(), "Should have at least one stream");
            
            uint32_t stream_id = streams[0].stream_id;
            
            // Test complete streaming workflow
            std::vector<MediaChunk> all_chunks;
            
            // Read all available chunks
            while (!demuxer.isEOF()) {
                MediaChunk chunk = demuxer.readChunk(stream_id);
                if (chunk.data.empty()) break;
                all_chunks.push_back(chunk);
            }
            
            ASSERT_TRUE(all_chunks.size() > 0, "Should read at least some chunks");
            
            // Verify header packets come first
            if (all_chunks.size() >= 3) {
                for (int i = 0; i < 3; i++) {
                    ASSERT_EQUALS(all_chunks[i].granule_position, 0ULL, "First 3 chunks should be header packets");
                    ASSERT_TRUE(all_chunks[i].is_keyframe, "Header packets should be keyframes");
                }
            }
            
            // Verify data packets have valid granule positions (after headers)
            bool found_data_packet = false;
            for (size_t i = 3; i < all_chunks.size(); i++) {
                if (all_chunks[i].granule_position != 0 && all_chunks[i].granule_position != static_cast<uint64_t>(-1)) {
                    found_data_packet = true;
                    break;
                }
            }
            
            if (all_chunks.size() > 3) {
                ASSERT_TRUE(found_data_packet, "Should have data packets with valid granule positions");
            }
            
        } catch (const std::exception& e) {
            addFailure("Exception in dataStreamingIntegration test: " + std::string(e.what()));
        }
    }
    
    // Helper methods to create test data
    std::vector<uint8_t> createTestOggVorbisData() {
        // Create minimal valid Ogg Vorbis data for testing
        std::vector<uint8_t> data;
        
        // This is a simplified test - in a real implementation, you would
        // create proper Ogg pages with valid Vorbis headers
        // For now, create minimal structure that can be parsed
        
        // Ogg page header: "OggS" + version + flags + granule + serial + sequence + checksum + segments
        data.insert(data.end(), {'O', 'g', 'g', 'S'}); // Capture pattern
        data.push_back(0); // Version
        data.push_back(0x02); // First page flag
        
        // Granule position (8 bytes, little-endian)
        for (int i = 0; i < 8; i++) data.push_back(0);
        
        // Serial number (4 bytes, little-endian) - use 12345
        data.push_back(0x39); data.push_back(0x30); data.push_back(0x00); data.push_back(0x00);
        
        // Page sequence (4 bytes, little-endian)
        for (int i = 0; i < 4; i++) data.push_back(0);
        
        // Checksum (4 bytes) - simplified, not real CRC
        for (int i = 0; i < 4; i++) data.push_back(0);
        
        // Number of segments
        data.push_back(1);
        
        // Segment length
        data.push_back(30); // Length of Vorbis ID header
        
        // Vorbis identification header
        data.insert(data.end(), {0x01, 'v', 'o', 'r', 'b', 'i', 's'});
        
        // Vorbis version (4 bytes, little-endian)
        for (int i = 0; i < 4; i++) data.push_back(0);
        
        // Channels (1 byte)
        data.push_back(2); // Stereo
        
        // Sample rate (4 bytes, little-endian) - 44100 Hz
        data.push_back(0x44); data.push_back(0xAC); data.push_back(0x00); data.push_back(0x00);
        
        // Bitrate max/nominal/min (12 bytes)
        for (int i = 0; i < 12; i++) data.push_back(0);
        
        // Blocksize info
        data.push_back(0xB8); // 8/11 blocksize
        
        // Framing bit
        data.push_back(0x01);
        
        return data;
    }
    
    std::vector<uint8_t> createTestOggVorbisDataWithMultiplePackets() {
        // Create test data with multiple packets for queue testing
        auto data = createTestOggVorbisData();
        
        // Add more pages with additional packets
        // This is simplified - real implementation would need proper Ogg structure
        for (int page = 0; page < 3; page++) {
            // Add another page
            data.insert(data.end(), {'O', 'g', 'g', 'S'}); // Capture pattern
            data.push_back(0); // Version
            data.push_back(0x00); // No special flags
            
            // Granule position
            for (int i = 0; i < 8; i++) data.push_back(page + 1);
            
            // Serial number (same as first page)
            data.push_back(0x39); data.push_back(0x30); data.push_back(0x00); data.push_back(0x00);
            
            // Page sequence
            data.push_back(page + 1); data.push_back(0); data.push_back(0); data.push_back(0);
            
            // Checksum
            for (int i = 0; i < 4; i++) data.push_back(0);
            
            // Segments
            data.push_back(1);
            data.push_back(50); // Packet length
            
            // Packet data (simplified)
            for (int i = 0; i < 50; i++) data.push_back(0xAA + page);
        }
        
        return data;
    }
    
    std::vector<uint8_t> createTestOggVorbisDataWithManyPackets() {
        auto data = createTestOggVorbisDataWithMultiplePackets();
        
        // Add many more pages to test bounded queues
        for (int page = 3; page < 150; page++) {
            data.insert(data.end(), {'O', 'g', 'g', 'S'});
            data.push_back(0);
            data.push_back(0x00);
            
            for (int i = 0; i < 8; i++) data.push_back(page);
            
            data.push_back(0x39); data.push_back(0x30); data.push_back(0x00); data.push_back(0x00);
            
            data.push_back(page & 0xFF); data.push_back((page >> 8) & 0xFF); 
            data.push_back(0); data.push_back(0);
            
            for (int i = 0; i < 4; i++) data.push_back(0);
            
            data.push_back(1);
            data.push_back(30);
            
            for (int i = 0; i < 30; i++) data.push_back(0xBB + (page % 10));
        }
        
        return data;
    }
    
    std::vector<uint8_t> createTestOggDataWithHoles() {
        // Create test data with simulated packet holes
        auto data = createTestOggVorbisData();
        
        // Add some corrupted/incomplete pages to simulate holes
        data.insert(data.end(), {'O', 'g', 'g', 'S'});
        data.push_back(0);
        data.push_back(0x00);
        
        // Incomplete page - will cause ogg_stream_packetout to return < 0
        for (int i = 0; i < 10; i++) data.push_back(0xFF);
        
        return data;
    }
    
    std::vector<uint8_t> createTestOggDataWithSpanningPackets() {
        // Create test data with packets that span multiple pages
        auto data = createTestOggVorbisData();
        
        // Add page with continued packet
        data.insert(data.end(), {'O', 'g', 'g', 'S'});
        data.push_back(0);
        data.push_back(0x01); // Continued packet flag
        
        for (int i = 0; i < 8; i++) data.push_back(1);
        
        data.push_back(0x39); data.push_back(0x30); data.push_back(0x00); data.push_back(0x00);
        data.push_back(1); data.push_back(0); data.push_back(0); data.push_back(0);
        
        for (int i = 0; i < 4; i++) data.push_back(0);
        
        data.push_back(2); // 2 segments
        data.push_back(255); // First segment (max size, indicates continuation)
        data.push_back(100); // Second segment (end of packet)
        
        // Packet data
        for (int i = 0; i < 355; i++) data.push_back(0xCC);
        
        return data;
    }
    
    std::vector<uint8_t> createTestOggVorbisDataWithGranules() {
        auto data = createTestOggVorbisData();
        
        // Add pages with specific granule positions for position tracking tests
        for (int page = 0; page < 5; page++) {
            data.insert(data.end(), {'O', 'g', 'g', 'S'});
            data.push_back(0);
            data.push_back(0x00);
            
            // Granule position (simulate 1024 samples per packet)
            uint64_t granule = (page + 1) * 1024;
            for (int i = 0; i < 8; i++) {
                data.push_back((granule >> (i * 8)) & 0xFF);
            }
            
            data.push_back(0x39); data.push_back(0x30); data.push_back(0x00); data.push_back(0x00);
            data.push_back(page + 1); data.push_back(0); data.push_back(0); data.push_back(0);
            
            for (int i = 0; i < 4; i++) data.push_back(0);
            
            data.push_back(1);
            data.push_back(40);
            
            for (int i = 0; i < 40; i++) data.push_back(0xDD + page);
        }
        
        return data;
    }
};

int main() {
    DataStreamingTest test;
    TestFramework::TestCaseInfo result = test.run();
    
    if (result.result == TestFramework::TestResult::PASSED) {
        std::cout << "All data streaming tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "Data streaming tests failed: " << result.failure_message << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main() {
    std::cout << "OggDemuxer not available - skipping data streaming tests" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER
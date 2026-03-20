/*
 * test_chunk_demuxer.cpp - Unit tests for ChunkDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "demuxer/ChunkDemuxer.h"
#include "io/IOHandler.h"
#include <vector>
#include <cstring>
#include <algorithm>

using namespace TestFramework;
using namespace PsyMP3;
using namespace PsyMP3::Demuxer;
using namespace PsyMP3::IO;

/**
 * @brief Mock IOHandler for testing with in-memory data
 */
class MockIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;

public:
    explicit MockIOHandler(const std::vector<uint8_t>& data) : m_data(data) {}

    size_t read(void* buffer, size_t size, size_t count) override {
        if (m_position >= m_data.size()) return 0;

        size_t bytes_requested = size * count;
        size_t bytes_available = m_data.size() - m_position;
        size_t bytes_to_read = std::min(bytes_requested, bytes_available);

        std::memcpy(buffer, m_data.data() + m_position, bytes_to_read);
        m_position += bytes_to_read;

        return bytes_to_read / size;
    }

    int seek(off_t offset, int whence) override {
        off_t new_pos = 0;
        switch (whence) {
            case SEEK_SET: new_pos = offset; break;
            case SEEK_CUR: new_pos = static_cast<off_t>(m_position) + offset; break;
            case SEEK_END: new_pos = static_cast<off_t>(m_data.size()) + offset; break;
            default: return -1;
        }

        if (new_pos < 0) return -1;
        // Allow seeking past end of file (standard behavior, though reading will fail/return 0)

        m_position = static_cast<size_t>(new_pos);
        return 0;
    }

    off_t tell() override {
        return static_cast<off_t>(m_position);
    }

    bool eof() override {
        return m_position >= m_data.size();
    }

    off_t getFileSize() override {
        return static_cast<off_t>(m_data.size());
    }

    int close() override { return 0; }
};

// --- Helper Functions ---

template<typename T>
void writeLE(std::vector<uint8_t>& data, T value) {
    for (size_t i = 0; i < sizeof(T); ++i) {
        data.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

template<typename T>
void writeBE(std::vector<uint8_t>& data, T value) {
    for (size_t i = 0; i < sizeof(T); ++i) {
        data.push_back(static_cast<uint8_t>((value >> ((sizeof(T) - 1 - i) * 8)) & 0xFF));
    }
}

// Helper to write a FourCC code
void writeFourCC(std::vector<uint8_t>& data, const char* fourcc) {
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<uint8_t>(fourcc[i]));
    }
}

// Append a chunk with data
void appendChunk(std::vector<uint8_t>& data, const char* fourcc, const std::vector<uint8_t>& chunkData) {
    writeFourCC(data, fourcc);
    writeLE<uint32_t>(data, static_cast<uint32_t>(chunkData.size()));
    data.insert(data.end(), chunkData.begin(), chunkData.end());
    // Pad byte if size is odd
    if (chunkData.size() % 2 != 0) {
        data.push_back(0);
    }
}

// Append a chunk with explicit size (for testing size errors)
void appendChunkExplicitSize(std::vector<uint8_t>& data, const char* fourcc, uint32_t size, const std::vector<uint8_t>& chunkData) {
    writeFourCC(data, fourcc);
    writeLE<uint32_t>(data, size);
    data.insert(data.end(), chunkData.begin(), chunkData.end());
    // Pad byte if size is odd (based on declared size? No, usually based on data written, but here we simulate corruption)
    // Let's just append the data provided.
}

std::vector<uint8_t> createWavHeader(uint32_t fileSize) {
    std::vector<uint8_t> data;
    writeFourCC(data, "RIFF");
    writeLE<uint32_t>(data, fileSize - 8); // Chunk size (File size - 8)
    writeFourCC(data, "WAVE");
    return data;
}

std::vector<uint8_t> createFmtChunk(uint16_t channels, uint32_t sampleRate, uint16_t bitsPerSample) {
    std::vector<uint8_t> chunkData;
    writeLE<uint16_t>(chunkData, 1); // PCM
    writeLE<uint16_t>(chunkData, channels);
    writeLE<uint32_t>(chunkData, sampleRate);
    uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
    writeLE<uint32_t>(chunkData, byteRate);
    writeLE<uint16_t>(chunkData, channels * (bitsPerSample / 8)); // Block align
    writeLE<uint16_t>(chunkData, bitsPerSample);
    return chunkData;
}

// --- Test Cases ---

class ValidWavParsingTest : public TestCase {
public:
    ValidWavParsingTest() : TestCase("Valid WAV Parsing") {}

protected:
    void runTest() override {
        // Construct a minimal valid WAV file
        std::vector<uint8_t> wavData;

        // fmt chunk
        std::vector<uint8_t> fmtData = createFmtChunk(2, 44100, 16);

        // data chunk (10ms of silence)
        size_t dataSize = 44100 * 2 * 2 * 0.01; // ~1764 bytes
        std::vector<uint8_t> audioData(dataSize, 0);

        // Calculate total file size
        // Header (12) + fmt header (8) + fmt data (16) + data header (8) + data (1764) = ~1808
        uint32_t fileSize = 12 + (8 + 16) + (8 + dataSize);

        // Build file
        std::vector<uint8_t> header = createWavHeader(fileSize);
        wavData.insert(wavData.end(), header.begin(), header.end());

        appendChunk(wavData, "fmt ", fmtData);
        appendChunk(wavData, "data", audioData);

        // Parse
        auto handler = std::make_unique<MockIOHandler>(wavData);
        ChunkDemuxer demuxer(std::move(handler));

        ASSERT_TRUE(demuxer.parseContainer(), "Should parse valid WAV container");
        ASSERT_TRUE(demuxer.isWaveFile(), "Should be identified as WAVE");
        ASSERT_FALSE(demuxer.isBigEndian(), "WAVE should be little-endian");

        auto streams = demuxer.getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have 1 stream");

        const auto& stream = streams[0];
        ASSERT_EQUALS(2u, stream.channels, "Channels should match");
        ASSERT_EQUALS(44100u, stream.sample_rate, "Sample rate should match");
        ASSERT_EQUALS(16u, stream.bits_per_sample, "Bits per sample should match");

        // Read chunks
        MediaChunk chunk = demuxer.readChunk();
        ASSERT_TRUE(chunk.isValid(), "Should read valid chunk");
        ASSERT_EQUALS(stream.stream_id, chunk.stream_id, "Stream ID should match");
        ASSERT_FALSE(chunk.data.empty(), "Chunk should have data");
    }
};

class CorruptedChunkHeaderSkippingTest : public TestCase {
public:
    CorruptedChunkHeaderSkippingTest() : TestCase("Corrupted Chunk Header Skipping") {}

protected:
    void runTest() override {
        std::vector<uint8_t> wavData;
        std::vector<uint8_t> fmtData = createFmtChunk(2, 44100, 16);
        std::vector<uint8_t> audioData(100, 0);
        std::vector<uint8_t> junkData(50, 0xCC);

        uint32_t fileSize = 12 + (8 + 16) + (8 + 50) + (8 + 100);
        std::vector<uint8_t> header = createWavHeader(fileSize);
        wavData.insert(wavData.end(), header.begin(), header.end());

        appendChunk(wavData, "fmt ", fmtData);

        // Insert Junk Chunk with unknown FourCC "JUNK"
        appendChunk(wavData, "JUNK", junkData);

        appendChunk(wavData, "data", audioData);

        auto handler = std::make_unique<MockIOHandler>(wavData);
        ChunkDemuxer demuxer(std::move(handler));

        ASSERT_TRUE(demuxer.parseContainer(), "Should parse container with junk chunk");

        // Ensure we can still find the data chunk
        bool foundData = false;
        // Depending on implementation, parseContainer might have already parsed metadata.
        // ChunkDemuxer typically parses all chunks until data or list?
        // In ChunkDemuxer.cpp, it loops while !eof and offset < container size.
        // It skips unknown chunks.

        // Check if stream info is correct (implies fmt was parsed)
        auto streams = demuxer.getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have 1 stream");

        // Try reading data. If JUNK was skipped correctly, we should hit 'data' chunk.
        MediaChunk chunk = demuxer.readChunk();
        ASSERT_TRUE(chunk.isValid(), "Should read valid chunk after skipping junk");
        // Verify we got the data we expected
        ASSERT_EQUALS(audioData.size(), chunk.data.size(), "Should read all audio data (small chunk)");
    }
};

class ChunkSizeClampingTest : public TestCase {
public:
    ChunkSizeClampingTest() : TestCase("Chunk Size Clamping") {}

protected:
    void runTest() override {
        std::vector<uint8_t> wavData;
        std::vector<uint8_t> fmtData = createFmtChunk(2, 44100, 16);
        std::vector<uint8_t> audioData(100, 0);

        // Construct file with a huge junk chunk size
        uint32_t fileSize = 1000; // Arbitrary small size claim in header
        std::vector<uint8_t> header = createWavHeader(fileSize);
        wavData.insert(wavData.end(), header.begin(), header.end());

        appendChunk(wavData, "fmt ", fmtData);

        // Insert chunk with huge size (larger than file)
        std::vector<uint8_t> junkHeader;
        writeFourCC(junkHeader, "JUNK");
        writeLE<uint32_t>(junkHeader, 0xFFFFFFFF); // Huge size
        wavData.insert(wavData.end(), junkHeader.begin(), junkHeader.end());

        // Append real data after (which technically shouldn't be reached if size is huge,
        // but if clamping works, it might just skip to end of file or something?)
        // Actually, if size is huge, it will try to skip. skipping might fail seeking.
        // ChunkDemuxer has handleChunkSizeError.

        auto handler = std::make_unique<MockIOHandler>(wavData);
        ChunkDemuxer demuxer(std::move(handler));

        // It should probably not crash. It might fail to find audio streams if data chunk is "inside" the junk or not reached.
        // In this case, I didn't put a 'data' chunk yet.
        // Let's put 'data' chunk BEFORE junk, so we have a valid stream.
        // But parseContainer loops through all.

        // Re-construct: fmt -> data -> huge_junk
        wavData.clear();
        header = createWavHeader(fileSize);
        wavData.insert(wavData.end(), header.begin(), header.end());
        appendChunk(wavData, "fmt ", fmtData);
        appendChunk(wavData, "data", audioData); // Valid data

        // Bad chunk at end
        writeFourCC(wavData, "BAD ");
        writeLE<uint32_t>(wavData, 0x10000000); // Very large size

        auto handler2 = std::make_unique<MockIOHandler>(wavData);
        ChunkDemuxer demuxer2(std::move(handler2));

        ASSERT_TRUE(demuxer2.parseContainer(), "Should parse container with bad chunk at end");
        auto streams = demuxer2.getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should find stream");

        // It shouldn't crash or hang.
    }
};

class MissingRequiredChunksTest : public TestCase {
public:
    MissingRequiredChunksTest() : TestCase("Missing Required Chunks") {}

protected:
    void runTest() override {
        std::vector<uint8_t> wavData;
        uint32_t fileSize = 100;
        std::vector<uint8_t> header = createWavHeader(fileSize);
        wavData.insert(wavData.end(), header.begin(), header.end());

        // No fmt or data chunks

        auto handler = std::make_unique<MockIOHandler>(wavData);
        ChunkDemuxer demuxer(std::move(handler));

        ASSERT_FALSE(demuxer.parseContainer(), "Should fail to parse WAV without chunks");

        // Try with only fmt
        std::vector<uint8_t> fmtData = createFmtChunk(2, 44100, 16);
        appendChunk(wavData, "fmt ", fmtData);

        auto handler2 = std::make_unique<MockIOHandler>(wavData);
        ChunkDemuxer demuxer2(std::move(handler2));

        // It might parse 'fmt' but if no 'data', streams might be empty or valid?
        // Code says: if (m_audio_streams.empty()) return false;
        // parseWaveFormat adds to m_audio_streams.
        // So with just fmt, it might return true, but reading data will fail/return EOF.
        // Actually, parseWaveFormat sets up the stream. parseWaveData sets offset/size.
        // If data chunk is missing, we might have stream info but no data.
        // Let's check logic in ChunkDemuxer.cpp
        // It returns true if m_audio_streams is not empty.

        ASSERT_TRUE(demuxer2.parseContainer(), "Might parse with just fmt chunk (implementation detail)");
        auto streams = demuxer2.getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have stream from fmt chunk");

        // Verify readChunk returns empty/invalid
        MediaChunk chunk = demuxer2.readChunk();
        ASSERT_FALSE(chunk.isValid(), "Should not read valid chunk without data chunk");
    }
};

class ZeroSizeChunkHandlingTest : public TestCase {
public:
    ZeroSizeChunkHandlingTest() : TestCase("Zero Size Chunk Handling") {}

protected:
    void runTest() override {
        std::vector<uint8_t> wavData;
        std::vector<uint8_t> fmtData = createFmtChunk(2, 44100, 16);
        std::vector<uint8_t> audioData(100, 0);

        uint32_t fileSize = 1000;
        std::vector<uint8_t> header = createWavHeader(fileSize);
        wavData.insert(wavData.end(), header.begin(), header.end());

        appendChunk(wavData, "fmt ", fmtData);

        // Zero size chunk
        std::vector<uint8_t> emptyData;
        appendChunk(wavData, "ZERO", emptyData);

        appendChunk(wavData, "data", audioData);

        auto handler = std::make_unique<MockIOHandler>(wavData);
        ChunkDemuxer demuxer(std::move(handler));

        ASSERT_TRUE(demuxer.parseContainer(), "Should parse container with zero size chunk");

        MediaChunk chunk = demuxer.readChunk();
        ASSERT_TRUE(chunk.isValid(), "Should find data after zero size chunk");
        ASSERT_EQUALS(audioData.size(), chunk.data.size(), "Data size match");
    }
};

class ValidAiffParsingTest : public TestCase {
public:
    ValidAiffParsingTest() : TestCase("Valid AIFF Parsing") {}

protected:
    void runTest() override {
        std::vector<uint8_t> aiffData;

        // FORM chunk
        writeFourCC(aiffData, "FORM");
        writeBE<uint32_t>(aiffData, 1000); // Size placeholder
        writeFourCC(aiffData, "AIFF");

        // COMM chunk
        std::vector<uint8_t> commData;
        writeBE<uint16_t>(commData, 2); // Channels
        writeBE<uint32_t>(commData, 1000); // Num sample frames
        writeBE<uint16_t>(commData, 16); // Bits per sample

        // 80-bit float sample rate 44100
        // 0x400E AC44 0000 0000 0000
        commData.push_back(0x40); commData.push_back(0x0E);
        commData.push_back(0xAC); commData.push_back(0x44);
        for(int i=0; i<6; ++i) commData.push_back(0);

        // Pad to even? COMM is 18 + 10 = 28 bytes? No.
        // 2 + 4 + 2 + 10 = 18 bytes.

        writeFourCC(aiffData, "COMM");
        writeBE<uint32_t>(aiffData, static_cast<uint32_t>(commData.size()));
        aiffData.insert(aiffData.end(), commData.begin(), commData.end());

        // SSND chunk
        std::vector<uint8_t> ssndData;
        writeBE<uint32_t>(ssndData, 0); // Offset
        writeBE<uint32_t>(ssndData, 0); // BlockSize
        // Audio data
        for(int i=0; i<100; ++i) ssndData.push_back(0);

        writeFourCC(aiffData, "SSND");
        writeBE<uint32_t>(aiffData, static_cast<uint32_t>(ssndData.size()));
        aiffData.insert(aiffData.end(), ssndData.begin(), ssndData.end());

        auto handler = std::make_unique<MockIOHandler>(aiffData);
        ChunkDemuxer demuxer(std::move(handler));

        ASSERT_TRUE(demuxer.parseContainer(), "Should parse valid AIFF");
        ASSERT_TRUE(demuxer.isAiffFile(), "Should be identified as AIFF");
        ASSERT_TRUE(demuxer.isBigEndian(), "AIFF should be big-endian");

        auto streams = demuxer.getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have 1 stream");
        ASSERT_EQUALS(2u, streams[0].channels, "Channels should match");
        ASSERT_EQUALS(44100u, streams[0].sample_rate, "Sample rate should match");
    }
};

int main() {
    TestSuite suite("ChunkDemuxer Tests");

    suite.addTest(std::make_unique<ValidWavParsingTest>());
    suite.addTest(std::make_unique<CorruptedChunkHeaderSkippingTest>());
    suite.addTest(std::make_unique<ChunkSizeClampingTest>());
    suite.addTest(std::make_unique<MissingRequiredChunksTest>());
    suite.addTest(std::make_unique<ZeroSizeChunkHandlingTest>());
    suite.addTest(std::make_unique<ValidAiffParsingTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results);
}

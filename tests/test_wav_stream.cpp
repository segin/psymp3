/*
 * test_wav_stream.cpp - Unit tests for WaveStream
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "demuxer/riff/wav.h"
#include "io/MemoryIOHandler.h"
#include <vector>
#include <cstring>
#include <cmath>
#include <iostream>

using namespace PsyMP3::Demuxer::RIFF;
using namespace PsyMP3::IO;
using namespace TestFramework;

// Helper to write little-endian values
template <typename T>
void write_le(std::vector<uint8_t>& buffer, T value) {
    size_t size = sizeof(T);
    for (size_t i = 0; i < size; ++i) {
        buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

// Helper to create WAV data
std::vector<uint8_t> createWavData(uint16_t format, uint16_t channels, uint32_t rate, uint16_t bits, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> wav;

    // RIFF Header
    write_le<uint32_t>(wav, 0x46464952); // "RIFF"
    uint32_t subchunk2Size = data.size();
    uint32_t chunkSize = 36 + subchunk2Size;
    write_le<uint32_t>(wav, chunkSize);
    write_le<uint32_t>(wav, 0x45564157); // "WAVE"

    // fmt Chunk
    write_le<uint32_t>(wav, 0x20746d66); // "fmt "
    write_le<uint32_t>(wav, 16); // Subchunk1Size (16 for PCM)
    write_le<uint16_t>(wav, format);
    write_le<uint16_t>(wav, channels);
    write_le<uint32_t>(wav, rate);
    uint32_t byteRate = rate * channels * bits / 8;
    write_le<uint32_t>(wav, byteRate);
    uint16_t blockAlign = channels * bits / 8;
    write_le<uint16_t>(wav, blockAlign);
    write_le<uint16_t>(wav, bits);

    // data Chunk
    write_le<uint32_t>(wav, 0x61746164); // "data"
    write_le<uint32_t>(wav, subchunk2Size);
    wav.insert(wav.end(), data.begin(), data.end());

    return wav;
}

void testPCM16() {
    std::cout << "Testing PCM 16-bit..." << std::endl;
    // 16-bit signed PCM (Little Endian)
    // 2 samples: 0x1000, 0xF000 (-4096)
    std::vector<uint8_t> pcmData = {0x00, 0x10, 0x00, 0xF0};

    auto wavData = createWavData(1, 1, 44100, 16, pcmData);
    auto handler = std::make_unique<MemoryIOHandler>(wavData.data(), wavData.size());
    WaveStream stream(std::move(handler));

    ASSERT_EQUALS(1, stream.getChannels(), "Channels should be 1");
    ASSERT_EQUALS(44100, stream.getRate(), "Rate should be 44100");

    int16_t buffer[2];
    size_t bytesRead = stream.getData(4, buffer);

    ASSERT_EQUALS(4, bytesRead, "Should read 4 bytes");
    ASSERT_EQUALS(0x1000, buffer[0], "First sample mismatch");
    ASSERT_EQUALS((int16_t)0xF000, buffer[1], "Second sample mismatch");
}

void testPCM8() {
    std::cout << "Testing PCM 8-bit..." << std::endl;
    // 8-bit unsigned PCM
    // 0x80 (128) -> 0 (silence)
    // 0xFF (255) -> 32512 (approx max positive)
    // 0x00 (0)   -> -32768 (min negative)
    std::vector<uint8_t> pcmData = {0x80, 0xFF, 0x00};

    auto wavData = createWavData(1, 1, 44100, 8, pcmData);
    auto handler = std::make_unique<MemoryIOHandler>(wavData.data(), wavData.size());
    WaveStream stream(std::move(handler));

    int16_t buffer[3];
    size_t bytesRead = stream.getData(6, buffer);

    ASSERT_EQUALS(6, bytesRead, "Should read 6 bytes");
    ASSERT_EQUALS(0, buffer[0], "0x80 should be 0");
    // (255 - 128) << 8 = 127 * 256 = 32512
    ASSERT_EQUALS(32512, buffer[1], "0xFF conversion mismatch");
    // (0 - 128) << 8 = -128 * 256 = -32768
    ASSERT_EQUALS(-32768, buffer[2], "0x00 conversion mismatch");
}

void testFloat32() {
    std::cout << "Testing Float 32-bit..." << std::endl;
    // 32-bit float
    // 1.0f, -0.5f
    float samples[] = {1.0f, -0.5f};
    std::vector<uint8_t> pcmData(sizeof(samples));
    std::memcpy(pcmData.data(), samples, sizeof(samples));

    auto wavData = createWavData(3, 1, 44100, 32, pcmData); // Format 3 = IEEE Float
    auto handler = std::make_unique<MemoryIOHandler>(wavData.data(), wavData.size());
    WaveStream stream(std::move(handler));

    int16_t buffer[2];
    size_t bytesRead = stream.getData(4, buffer);

    ASSERT_EQUALS(4, bytesRead, "Should read 4 bytes");
    // 1.0 * 32767 = 32767
    ASSERT_EQUALS(32767, buffer[0], "1.0f conversion mismatch");
    // -0.5 * 32767 = -16383.5 -> -16383
    ASSERT_EQUALS(-16383, buffer[1], "-0.5f conversion mismatch");
}

void testALaw() {
    std::cout << "Testing A-Law..." << std::endl;
    // A-law
    // 0xD5 (silence) -> 0
    std::vector<uint8_t> data = {0xD5};
    auto wavData = createWavData(6, 1, 8000, 8, data); // Format 6 = ALAW
    auto handler = std::make_unique<MemoryIOHandler>(wavData.data(), wavData.size());
    WaveStream stream(std::move(handler));

    int16_t buffer[1];
    stream.getData(2, buffer);
    ASSERT_EQUALS(0, buffer[0], "A-Law silence mismatch");
}

void testMuLaw() {
    std::cout << "Testing Mu-Law..." << std::endl;
    // Mu-law
    // 0xFF (silence) -> 0
    std::vector<uint8_t> data = {0xFF};
    auto wavData = createWavData(7, 1, 8000, 8, data); // Format 7 = MULAW
    auto handler = std::make_unique<MemoryIOHandler>(wavData.data(), wavData.size());
    WaveStream stream(std::move(handler));

    int16_t buffer[1];
    stream.getData(2, buffer);
    ASSERT_EQUALS(0, buffer[0], "Mu-Law silence mismatch");
}

void testInvalidHeader() {
    std::cout << "Testing Invalid Header..." << std::endl;
    std::vector<uint8_t> data = {0x00}; // Too short
    auto handler = std::make_unique<MemoryIOHandler>(data.data(), data.size());

    bool exceptionThrown = false;
    try {
        WaveStream stream(std::move(handler));
    } catch (const std::exception&) {
        exceptionThrown = true;
    }
    ASSERT_TRUE(exceptionThrown, "Should throw on invalid header");
}

void testSeek() {
     std::cout << "Testing Seek..." << std::endl;
     // 16-bit stereo, 44100Hz
     // 1 second of data = 44100 * 2 (channels) * 2 (bytes) = 176400 bytes
     std::vector<uint8_t> pcmData(176400, 0);
     // Mark a sample at 500ms
     // 500ms = 22050 samples
     // Offset = 22050 * 4 bytes = 88200
     pcmData[88200] = 0xAA;
     pcmData[88201] = 0xBB; // 0xBBAA sample for Left channel

     auto wavData = createWavData(1, 2, 44100, 16, pcmData);
     auto handler = std::make_unique<MemoryIOHandler>(wavData.data(), wavData.size());
     WaveStream stream(std::move(handler));

     stream.seekTo(500); // Seek to 500ms

     int16_t buffer[2]; // L, R
     stream.getData(4, buffer);

     ASSERT_EQUALS((int16_t)0xBBAA, buffer[0], "Seek failed or data mismatch");
}

int main() {
    try {
        testPCM16();
        testPCM8();
        testFloat32();
        testALaw();
        testMuLaw();
        testInvalidHeader();
        testSeek();

        std::cout << "All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}

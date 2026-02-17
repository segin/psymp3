/*
 * test_wav_stream.cpp - Test WaveStream implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "demuxer/riff/wav.h"
#include "core/utility/G711.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <unistd.h>
#include <sys/stat.h>

using namespace PsyMP3::Demuxer::RIFF;
using namespace PsyMP3::Core::Utility::G711;

// Helper to write little-endian values
void write_le_u32(std::ofstream& out, uint32_t value) {
    uint8_t buf[4];
    buf[0] = value & 0xFF;
    buf[1] = (value >> 8) & 0xFF;
    buf[2] = (value >> 16) & 0xFF;
    buf[3] = (value >> 24) & 0xFF;
    out.write(reinterpret_cast<char*>(buf), 4);
}

void write_le_u16(std::ofstream& out, uint16_t value) {
    uint8_t buf[2];
    buf[0] = value & 0xFF;
    buf[1] = (value >> 8) & 0xFF;
    out.write(reinterpret_cast<char*>(buf), 2);
}

// RIFF constants
constexpr uint32_t RIFF_ID = 0x46464952; // "RIFF"
constexpr uint32_t WAVE_ID = 0x45564157; // "WAVE"
constexpr uint32_t FMT_ID  = 0x20746d66; // "fmt "
constexpr uint32_t DATA_ID = 0x61746164; // "data"

// WAVE format tags
constexpr uint16_t FORMAT_PCM = 0x0001;
constexpr uint16_t FORMAT_FLOAT = 0x0003;
constexpr uint16_t FORMAT_ALAW = 0x0006;
constexpr uint16_t FORMAT_MULAW = 0x0007;

struct WavHeader {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

void create_wav_file(const std::string& filename, const WavHeader& header, const std::vector<uint8_t>& data) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open " << filename << " for writing." << std::endl;
        exit(1);
    }

    uint32_t data_chunk_size = data.size();
    // 4 (WAVE) + 8 (fmt header) + 16 (fmt body) + 8 (data header) + data size
    uint32_t riff_chunk_size = 4 + (8 + 16) + (8 + data_chunk_size);
    if (data_chunk_size % 2 != 0) riff_chunk_size++; // padding

    write_le_u32(out, RIFF_ID);
    write_le_u32(out, riff_chunk_size);
    write_le_u32(out, WAVE_ID);

    write_le_u32(out, FMT_ID);
    write_le_u32(out, 16); // fmt chunk size
    write_le_u16(out, header.audio_format);
    write_le_u16(out, header.num_channels);
    write_le_u32(out, header.sample_rate);
    write_le_u32(out, header.byte_rate);
    write_le_u16(out, header.block_align);
    write_le_u16(out, header.bits_per_sample);

    write_le_u32(out, DATA_ID);
    write_le_u32(out, data_chunk_size);
    out.write(reinterpret_cast<const char*>(data.data()), data.size());

    if (data_chunk_size % 2 != 0) {
        char pad = 0;
        out.write(&pad, 1);
    }
}

class FileDeleter {
    std::string m_path;
public:
    FileDeleter(std::string path) : m_path(std::move(path)) {}
    ~FileDeleter() { unlink(m_path.c_str()); }
};

#define ASSERT_TRUE(cond, msg) \
    if (!(cond)) { \
        std::cerr << "FAIL: " << msg << std::endl; \
        return 1; \
    }

int test_pcm_8bit() {
    std::cout << "Testing PCM 8-bit..." << std::endl;
    std::string filename = "test_pcm_8.wav";
    FileDeleter deleter(filename);

    // 8-bit PCM is unsigned 0..255, center at 128
    WavHeader header = {FORMAT_PCM, 1, 44100, 44100, 1, 8};
    std::vector<uint8_t> data = {128, 0, 255};
    create_wav_file(filename, header, data);

    try {
        WaveStream stream((TagLib::String(filename)));
        ASSERT_TRUE(stream.getChannels() == 1, "Channels mismatch");
        ASSERT_TRUE(stream.getRate() == 44100, "Rate mismatch");

        int16_t buffer[3];
        size_t read = stream.getData(6, buffer); // 3 samples * 2 bytes
        ASSERT_TRUE(read == 6, "Read size mismatch");

        // 128 -> 0
        // 0 -> -32768
        // 255 -> 32512 ( (255-128)<<8 = 127<<8 = 32512 )

        ASSERT_TRUE(buffer[0] == 0, "Sample 0 mismatch (128 -> 0)");
        ASSERT_TRUE(buffer[1] == -32768, "Sample 1 mismatch (0 -> -32768)");
        ASSERT_TRUE(buffer[2] == 32512, "Sample 2 mismatch (255 -> 32512)");

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int test_pcm_16bit() {
    std::cout << "Testing PCM 16-bit..." << std::endl;
    std::string filename = "test_pcm_16.wav";
    FileDeleter deleter(filename);

    WavHeader header = {FORMAT_PCM, 2, 48000, 48000 * 4, 4, 16};
    std::vector<uint8_t> data(4 * 2); // 2 samples, stereo (4 bytes per frame)
    int16_t* ptr = reinterpret_cast<int16_t*>(data.data());
    ptr[0] = 0;      // L
    ptr[1] = -32768; // R
    ptr[2] = 32767;  // L
    ptr[3] = -1;     // R

    create_wav_file(filename, header, data);

    try {
        WaveStream stream((TagLib::String(filename)));
        ASSERT_TRUE(stream.getChannels() == 2, "Channels mismatch");
        ASSERT_TRUE(stream.getRate() == 48000, "Rate mismatch");

        int16_t buffer[4];
        size_t read = stream.getData(8, buffer);
        ASSERT_TRUE(read == 8, "Read size mismatch");

        ASSERT_TRUE(buffer[0] == 0, "Sample 0 mismatch");
        ASSERT_TRUE(buffer[1] == -32768, "Sample 1 mismatch");
        ASSERT_TRUE(buffer[2] == 32767, "Sample 2 mismatch");
        ASSERT_TRUE(buffer[3] == -1, "Sample 3 mismatch");

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int test_float_32bit() {
    std::cout << "Testing IEEE Float 32-bit..." << std::endl;
    std::string filename = "test_float_32.wav";
    FileDeleter deleter(filename);

    WavHeader header = {FORMAT_FLOAT, 1, 44100, 44100 * 4, 4, 32};
    std::vector<uint8_t> data(4 * 2); // 2 samples
    float* ptr = reinterpret_cast<float*>(data.data());
    ptr[0] = 0.0f;
    ptr[1] = 1.0f;

    create_wav_file(filename, header, data);

    try {
        WaveStream stream((TagLib::String(filename)));

        int16_t buffer[2];
        size_t read = stream.getData(4, buffer);
        ASSERT_TRUE(read == 4, "Read size mismatch");

        // 0.0f -> 0
        // 1.0f -> 32767

        ASSERT_TRUE(buffer[0] == 0, "Sample 0 mismatch");
        ASSERT_TRUE(buffer[1] == 32767, "Sample 1 mismatch");

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int test_alaw() {
    std::cout << "Testing ALAW..." << std::endl;
    std::string filename = "test_alaw.wav";
    FileDeleter deleter(filename);

    WavHeader header = {FORMAT_ALAW, 1, 8000, 8000, 1, 8};
    // 0x55 -> +0, 0xD5 -> -0 in ALAW
    std::vector<uint8_t> data = {0x55, 0xD5};
    create_wav_file(filename, header, data);

    try {
        WaveStream stream((TagLib::String(filename)));

        int16_t buffer[2];
        size_t read = stream.getData(4, buffer);
        ASSERT_TRUE(read == 4, "Read size mismatch");

        ASSERT_TRUE(buffer[0] == 0, "Sample 0 mismatch (0x55 -> 0)");
        ASSERT_TRUE(buffer[1] == 0, "Sample 1 mismatch (0xD5 -> 0)");

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int test_mulaw() {
    std::cout << "Testing MULAW..." << std::endl;
    std::string filename = "test_mulaw.wav";
    FileDeleter deleter(filename);

    WavHeader header = {FORMAT_MULAW, 1, 8000, 8000, 1, 8};
    // 0xFF and 0x7F decode to 0 in mu-law
    std::vector<uint8_t> data = {0xFF, 0x7F};
    create_wav_file(filename, header, data);

    try {
        WaveStream stream((TagLib::String(filename)));

        int16_t buffer[2];
        size_t read = stream.getData(4, buffer);
        ASSERT_TRUE(read == 4, "Read size mismatch");

        ASSERT_TRUE(buffer[0] == 0, "Sample 0 mismatch (0xFF -> 0)");
        ASSERT_TRUE(buffer[1] == 0, "Sample 1 mismatch (0x7F -> 0)");

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int test_malformed_header() {
    std::cout << "Testing Malformed Header (missing data chunk)..." << std::endl;
    std::string filename = "test_malformed.wav";
    FileDeleter deleter(filename);

    std::ofstream out(filename, std::ios::binary);

    // Header without data chunk
    write_le_u32(out, RIFF_ID);
    write_le_u32(out, 4 + 8 + 16); // size
    write_le_u32(out, WAVE_ID);
    write_le_u32(out, FMT_ID);
    write_le_u32(out, 16);
    write_le_u16(out, FORMAT_PCM);
    write_le_u16(out, 1);
    write_le_u32(out, 44100);
    write_le_u32(out, 44100);
    write_le_u16(out, 1);
    write_le_u16(out, 8);
    out.close();

    bool caught = false;
    try {
        WaveStream stream((TagLib::String(filename)));
    } catch (const BadFormatException& e) {
        caught = true;
        std::cout << "Caught expected exception: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught unexpected exception: " << e.what() << std::endl;
    }

    ASSERT_TRUE(caught, "Should catch BadFormatException for missing data chunk");
    return 0;
}

int main() {
    int failed = 0;

    if (test_pcm_8bit() != 0) failed++;
    if (test_pcm_16bit() != 0) failed++;
    if (test_float_32bit() != 0) failed++;
    if (test_alaw() != 0) failed++;
    if (test_mulaw() != 0) failed++;
    if (test_malformed_header() != 0) failed++;

    if (failed == 0) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << failed << " tests failed." << std::endl;
        return 1;
    }
}

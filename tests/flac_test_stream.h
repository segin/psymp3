/*
 * flac_test_stream.h - FLAC frames for tests, built after RFC 9639
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Every frame holds 4096 16-bit samples at 44.1 kHz on two independent
 * channels, coded verbatim, and each sample is a running counter, so any
 * lost or repeated audio shows as a break in the count.
 */

#ifndef FLAC_TEST_STREAM_H
#define FLAC_TEST_STREAM_H

#include <cstdint>
#include <vector>

namespace FlacTestStream {

using Bytes = std::vector<uint8_t>;

constexpr uint32_t kBlock = 4096;
constexpr uint32_t kRate = 44100;

/// RFC 9639's CRC-8 (9.1.8) and CRC-16 (9.3): polynomials 0x07 and 0x8005,
/// most significant bit first, starting from zero.
inline uint32_t crc(const Bytes& data, uint32_t polynomial, int width)
{
    const uint32_t top = 1u << (width - 1);
    const uint32_t mask = (1u << width) - 1;
    uint32_t value = 0;
    for (uint8_t byte : data) {
        value ^= static_cast<uint32_t>(byte) << (width - 8);
        for (int bit = 0; bit < 8; ++bit) {
            value = (value & top) ? ((value << 1) ^ polynomial) : (value << 1);
        }
        value &= mask;
    }
    return value;
}

/// Frame @p index (RFC 9639 9), below 128 so its number fits one byte.
inline Bytes frame(uint32_t index)
{
    // Sync code, fixed block size; 4096 samples, 44.1 kHz; two channels,
    // 16 bits; the frame number.
    Bytes out{0xFF, 0xF8, 0xC9, 0x18, static_cast<uint8_t>(index)};
    out.push_back(static_cast<uint8_t>(crc(out, 0x07, 8)));
    for (int channel = 0; channel < 2; ++channel) {
        out.push_back(0x02); // verbatim, no wasted bits
        for (uint32_t i = 0; i < kBlock; ++i) {
            const auto counter = static_cast<uint16_t>(index * kBlock + i);
            out.push_back(static_cast<uint8_t>(counter >> 8));
            out.push_back(static_cast<uint8_t>(counter));
        }
    }
    const uint32_t check = crc(out, 0x8005, 16);
    out.push_back(static_cast<uint8_t>(check >> 8));
    out.push_back(static_cast<uint8_t>(check));
    return out;
}

/// A STREAMINFO metadata block, marked last, for @p total_samples.
inline Bytes streamInfo(uint64_t total_samples)
{
    Bytes block{0x80, 0x00, 0x00, 34,
                static_cast<uint8_t>(kBlock >> 8), static_cast<uint8_t>(kBlock),
                static_cast<uint8_t>(kBlock >> 8), static_cast<uint8_t>(kBlock),
                0, 0, 0, 0, 0, 0};
    const uint64_t packed = (uint64_t{kRate} << 44) | (uint64_t{1} << 41) | (uint64_t{15} << 36)
                          | total_samples;
    for (int shift = 56; shift >= 0; shift -= 8) {
        block.push_back(static_cast<uint8_t>(packed >> shift));
    }
    block.insert(block.end(), 16, 0); // MD5 unknown
    return block;
}

/// The counter value sample @p n holds, as a signed 16-bit sample.
inline int32_t counterAt(uint64_t n)
{
    return static_cast<int32_t>(static_cast<int16_t>(static_cast<uint16_t>(n)));
}

} // namespace FlacTestStream

#endif // FLAC_TEST_STREAM_H

/*
 * test_flac_demuxer_rfc9639_regressions.cpp - Focused FLACDemuxer RFC 9639 regressions
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifdef HAVE_FLAC

namespace {

class MemoryIOHandler : public IOHandler {
public:
    explicit MemoryIOHandler(std::vector<uint8_t> data)
        : m_data(std::move(data))
    {
    }

    size_t read(void* buffer, size_t size, size_t count) override
    {
        if (size == 0 || count == 0 || m_position >= m_data.size()) {
            return 0;
        }

        const size_t bytes_requested = size * count;
        const size_t bytes_available = m_data.size() - m_position;
        const size_t bytes_to_read = std::min(bytes_requested, bytes_available);

        std::memcpy(buffer, m_data.data() + m_position, bytes_to_read);
        m_position += bytes_to_read;
        return bytes_to_read / size;
    }

    int seek(PsyMP3::IO::filesize_t offset, int whence) override
    {
        PsyMP3::IO::filesize_t new_position = 0;
        switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position = static_cast<PsyMP3::IO::filesize_t>(m_position) + offset;
            break;
        case SEEK_END:
            new_position = static_cast<PsyMP3::IO::filesize_t>(m_data.size()) + offset;
            break;
        default:
            return -1;
        }

        if (new_position < 0 || new_position > static_cast<PsyMP3::IO::filesize_t>(m_data.size())) {
            return -1;
        }

        m_position = static_cast<size_t>(new_position);
        return 0;
    }

    PsyMP3::IO::filesize_t tell() override
    {
        return static_cast<PsyMP3::IO::filesize_t>(m_position);
    }

    bool eof() override
    {
        return m_position >= m_data.size();
    }

    int close() override
    {
        return 0;
    }

    PsyMP3::IO::filesize_t getFileSize() override
    {
        return static_cast<PsyMP3::IO::filesize_t>(m_data.size());
    }

private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
};

class LimitedReadMemoryIOHandler : public IOHandler {
public:
    LimitedReadMemoryIOHandler(std::vector<uint8_t> data, size_t max_single_read)
        : m_data(std::move(data))
        , m_max_single_read(max_single_read)
    {
    }

    size_t read(void* buffer, size_t size, size_t count) override
    {
        if (size == 0 || count == 0 || m_position >= m_data.size()) {
            return 0;
        }

        const size_t bytes_requested = size * count;
        if (bytes_requested > m_max_single_read) {
            return 0;
        }

        const size_t bytes_available = m_data.size() - m_position;
        const size_t bytes_to_read = std::min(bytes_requested, bytes_available);

        std::memcpy(buffer, m_data.data() + m_position, bytes_to_read);
        m_position += bytes_to_read;
        return bytes_to_read / size;
    }

    int seek(PsyMP3::IO::filesize_t offset, int whence) override
    {
        PsyMP3::IO::filesize_t new_position = 0;
        switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position = static_cast<PsyMP3::IO::filesize_t>(m_position) + offset;
            break;
        case SEEK_END:
            new_position = static_cast<PsyMP3::IO::filesize_t>(m_data.size()) + offset;
            break;
        default:
            return -1;
        }

        if (new_position < 0 || new_position > static_cast<PsyMP3::IO::filesize_t>(m_data.size())) {
            return -1;
        }

        m_position = static_cast<size_t>(new_position);
        return 0;
    }

    PsyMP3::IO::filesize_t tell() override
    {
        return static_cast<PsyMP3::IO::filesize_t>(m_position);
    }

    bool eof() override
    {
        return m_position >= m_data.size();
    }

    int close() override
    {
        return 0;
    }

    PsyMP3::IO::filesize_t getFileSize() override
    {
        return static_cast<PsyMP3::IO::filesize_t>(m_data.size());
    }

private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    size_t m_max_single_read = 0;
};

struct FrameOptions {
    bool variable_block_size = false;
    bool reserved_bit = false;
    uint8_t block_size_bits = 0x01;
    uint8_t sample_rate_bits = 0x09;
    uint8_t channel_bits = 0x00;
    uint8_t bit_depth_bits = 0x04;
    uint64_t coded_number = 0;
    std::vector<uint8_t> uncommon_block_size_bytes;
    std::vector<uint8_t> uncommon_sample_rate_bytes;
    std::vector<uint8_t> payload {0x00};
    bool corrupt_footer_crc = false;
};

bool assertTrue(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        return false;
    }
    return true;
}

uint8_t calculateCRC8(const uint8_t* data, size_t length)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x80) != 0) {
                crc = static_cast<uint8_t>((crc << 1) ^ 0x07);
            } else {
                crc = static_cast<uint8_t>(crc << 1);
            }
        }
    }
    return crc;
}

uint16_t calculateCRC16(const uint8_t* data, size_t length)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000) != 0) {
                crc = static_cast<uint16_t>((crc << 1) ^ 0x8005);
            } else {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

std::vector<uint8_t> encodeCodedNumber(uint64_t value)
{
    if (value <= 0x7F) {
        return {static_cast<uint8_t>(value)};
    }

    if (value <= 0x7FF) {
        return {
            static_cast<uint8_t>(0xC0 | ((value >> 6) & 0x1F)),
            static_cast<uint8_t>(0x80 | (value & 0x3F))
        };
    }

    if (value <= 0xFFFF) {
        return {
            static_cast<uint8_t>(0xE0 | ((value >> 12) & 0x0F)),
            static_cast<uint8_t>(0x80 | ((value >> 6) & 0x3F)),
            static_cast<uint8_t>(0x80 | (value & 0x3F))
        };
    }

    return {};
}

std::vector<uint8_t> makeStreamInfoBlock()
{
    std::vector<uint8_t> block;
    block.push_back(0x80);
    block.push_back(0x00);
    block.push_back(0x00);
    block.push_back(0x22);

    block.push_back(0x00);
    block.push_back(0xC0);
    block.push_back(0x00);
    block.push_back(0xC0);

    block.push_back(0x00);
    block.push_back(0x00);
    block.push_back(0x00);
    block.push_back(0x00);
    block.push_back(0x00);
    block.push_back(0x00);

    const uint32_t sample_rate = 44100;
    const uint8_t channels_minus_one = 0;
    const uint8_t bits_per_sample_minus_one = 15;
    block.push_back(static_cast<uint8_t>((sample_rate >> 12) & 0xFF));
    block.push_back(static_cast<uint8_t>((sample_rate >> 4) & 0xFF));
    block.push_back(static_cast<uint8_t>(((sample_rate & 0x0F) << 4) | (channels_minus_one << 1) |
                                         ((bits_per_sample_minus_one >> 4) & 0x01)));
    block.push_back(static_cast<uint8_t>((bits_per_sample_minus_one & 0x0F) << 4));

    block.push_back(0x00);
    block.push_back(0x00);
    block.push_back(0x00);
    block.push_back(0x00);

    for (int i = 0; i < 16; ++i) {
        block.push_back(0x00);
    }

    return block;
}

std::vector<uint8_t> makePaddingBlock(uint32_t length, bool is_last)
{
    std::vector<uint8_t> block;
    block.push_back(static_cast<uint8_t>((is_last ? 0x80 : 0x00) | 0x01));
    block.push_back(static_cast<uint8_t>((length >> 16) & 0xFF));
    block.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    block.push_back(static_cast<uint8_t>(length & 0xFF));
    block.insert(block.end(), length, 0x00);
    return block;
}

std::vector<uint8_t> makePictureBlock(uint32_t picture_bytes, bool is_last)
{
    std::vector<uint8_t> payload;

    auto appendU32 = [&payload](uint32_t value) {
        payload.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        payload.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(value & 0xFF));
    };

    appendU32(3);  // Front cover

    const std::string mime = "image/jpeg";
    appendU32(static_cast<uint32_t>(mime.size()));
    payload.insert(payload.end(), mime.begin(), mime.end());

    const std::string description = "Large artwork";
    appendU32(static_cast<uint32_t>(description.size()));
    payload.insert(payload.end(), description.begin(), description.end());

    appendU32(1200);
    appendU32(1200);
    appendU32(24);
    appendU32(0);
    appendU32(picture_bytes);
    payload.insert(payload.end(), picture_bytes, 0x5A);

    std::vector<uint8_t> block;
    block.push_back(static_cast<uint8_t>((is_last ? 0x80 : 0x00) | 0x06));
    block.push_back(static_cast<uint8_t>((payload.size() >> 16) & 0xFF));
    block.push_back(static_cast<uint8_t>((payload.size() >> 8) & 0xFF));
    block.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
    block.insert(block.end(), payload.begin(), payload.end());
    return block;
}

std::vector<uint8_t> makeFrame(const FrameOptions& options)
{
    std::vector<uint8_t> header;
    header.push_back(0xFF);
    header.push_back(options.variable_block_size ? 0xF9 : 0xF8);
    header.push_back(static_cast<uint8_t>((options.block_size_bits << 4) | (options.sample_rate_bits & 0x0F)));
    header.push_back(static_cast<uint8_t>((options.channel_bits << 4) |
                                          ((options.bit_depth_bits & 0x07) << 1) |
                                          (options.reserved_bit ? 0x01 : 0x00)));

    std::vector<uint8_t> coded_number = encodeCodedNumber(options.coded_number);
    header.insert(header.end(), coded_number.begin(), coded_number.end());
    header.insert(header.end(), options.uncommon_block_size_bytes.begin(), options.uncommon_block_size_bytes.end());
    header.insert(header.end(), options.uncommon_sample_rate_bytes.begin(), options.uncommon_sample_rate_bytes.end());
    header.push_back(calculateCRC8(header.data(), header.size()));

    std::vector<uint8_t> frame = header;
    frame.insert(frame.end(), options.payload.begin(), options.payload.end());

    uint16_t footer_crc = calculateCRC16(frame.data(), frame.size());
    if (options.corrupt_footer_crc) {
        footer_crc ^= 0x00FF;
    }

    frame.push_back(static_cast<uint8_t>((footer_crc >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(footer_crc & 0xFF));
    return frame;
}

std::vector<uint8_t> makeFlacFile(const std::vector<std::vector<uint8_t>>& metadata_blocks,
                                  const std::vector<std::vector<uint8_t>>& frames)
{
    std::vector<uint8_t> data {'f', 'L', 'a', 'C'};
    for (const auto& block : metadata_blocks) {
        data.insert(data.end(), block.begin(), block.end());
    }
    for (const auto& frame : frames) {
        data.insert(data.end(), frame.begin(), frame.end());
    }
    return data;
}

std::unique_ptr<FLACDemuxer> makeDemuxer(std::vector<uint8_t> data)
{
    return std::make_unique<FLACDemuxer>(std::make_unique<MemoryIOHandler>(std::move(data)));
}

std::unique_ptr<FLACDemuxer> makeLimitedReadDemuxer(std::vector<uint8_t> data, size_t max_single_read)
{
    return std::make_unique<FLACDemuxer>(
        std::make_unique<LimitedReadMemoryIOHandler>(std::move(data), max_single_read));
}

bool testRejectsMissingLeadingStreamInfo()
{
    auto demuxer = makeDemuxer(makeFlacFile({makePaddingBlock(4, false), makeStreamInfoBlock()}, {}));
    return assertTrue(!demuxer->parseContainer(),
                      "FLACDemuxer should reject files whose first metadata block is not STREAMINFO");
}

bool testRejectsDuplicateStreamInfo()
{
    std::vector<uint8_t> first = makeStreamInfoBlock();
    first[0] = 0x00;
    auto demuxer = makeDemuxer(makeFlacFile({first, makeStreamInfoBlock()}, {}));
    return assertTrue(!demuxer->parseContainer(),
                      "FLACDemuxer should reject files with duplicate STREAMINFO blocks");
}

bool testRejectsReservedFrameHeaderBit()
{
    FrameOptions invalid_frame;
    invalid_frame.reserved_bit = true;

    auto demuxer = makeDemuxer(makeFlacFile({makeStreamInfoBlock()}, {makeFrame(invalid_frame)}));
    if (!assertTrue(demuxer->parseContainer(), "Container with valid metadata should parse")) {
        return false;
    }

    MediaChunk chunk = demuxer->readChunk();
    return assertTrue(!chunk.isValid(),
                      "FLACDemuxer should reject frames whose reserved header bit is set");
}

bool testRejectsBadFrameFooterCRC()
{
    FrameOptions invalid_frame;
    invalid_frame.corrupt_footer_crc = true;

    auto demuxer = makeDemuxer(makeFlacFile({makeStreamInfoBlock()}, {makeFrame(invalid_frame)}));
    if (!assertTrue(demuxer->parseContainer(), "Container with valid metadata should parse")) {
        return false;
    }

    MediaChunk chunk = demuxer->readChunk();
    return assertTrue(!chunk.isValid(),
                      "FLACDemuxer should reject frames whose footer CRC-16 is invalid");
}

bool testSkipsNonFinalSmallUncommonBlockSize()
{
    std::vector<uint8_t> streaminfo = makeStreamInfoBlock();
    streaminfo[11] = 0x00;
    streaminfo[12] = 0x00;
    streaminfo[13] = 0x80;

    FrameOptions invalid_first;
    invalid_first.block_size_bits = 0x06;
    invalid_first.uncommon_block_size_bytes = {0x0E};
    invalid_first.payload.assign(32, 0x00);

    FrameOptions valid_second;
    valid_second.coded_number = 1;
    valid_second.payload.assign(20, 0x00);

    std::vector<uint8_t> invalid_frame = makeFrame(invalid_first);
    const uint64_t expected_second_offset = 4 + streaminfo.size() + invalid_frame.size();

    auto demuxer = makeDemuxer(makeFlacFile({streaminfo}, {invalid_frame, makeFrame(valid_second)}));
    if (!assertTrue(demuxer->parseContainer(), "Container with valid metadata should parse")) {
        return false;
    }

    MediaChunk chunk = demuxer->readChunk();
    if (!assertTrue(chunk.isValid(), "FLACDemuxer should recover to the next valid frame")) {
        return false;
    }

    if (chunk.file_offset != expected_second_offset) {
        std::cerr << "Observed file_offset=" << chunk.file_offset
                  << ", expected=" << expected_second_offset << std::endl;
        return false;
    }

    return true;
}

bool testReadsLargePictureBlocksWithoutOversizedSingleRead()
{
    FrameOptions frame;
    frame.payload.assign(24, 0x11);
    std::vector<uint8_t> streaminfo = makeStreamInfoBlock();
    streaminfo[0] = 0x00;

    auto demuxer = makeLimitedReadDemuxer(
        makeFlacFile({streaminfo, makePictureBlock(200000, true)}, {makeFrame(frame)}),
        65536);

    if (!assertTrue(demuxer->parseContainer(),
                    "FLACDemuxer should parse files whose PICTURE block requires chunked reads")) {
        return false;
    }

    MediaChunk chunk = demuxer->readChunk();
    return assertTrue(chunk.isValid(),
                      "FLACDemuxer should still reach audio frames after a large PICTURE block");
}

} // namespace

int main()
{
    std::cout << "FLACDemuxer RFC 9639 regression tests" << std::endl;

    bool ok = true;
    ok &= testRejectsMissingLeadingStreamInfo();
    ok &= testRejectsDuplicateStreamInfo();
    ok &= testRejectsReservedFrameHeaderBit();
    ok &= testRejectsBadFrameFooterCRC();
    ok &= testSkipsNonFinalSmallUncommonBlockSize();
    ok &= testReadsLargePictureBlocksWithoutOversizedSingleRead();

    if (!ok) {
        return 1;
    }

    std::cout << "All FLACDemuxer RFC 9639 regression tests passed" << std::endl;
    return 0;
}

#else

int main()
{
    std::cout << "FLAC support not available - skipping FLACDemuxer RFC 9639 regression tests" << std::endl;
    return 0;
}

#endif

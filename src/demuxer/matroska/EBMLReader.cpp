/*
 * EBMLReader.cpp - EBML element reader, the substrate Matroska is built on.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Demuxer {
namespace Matroska {

EBMLReader::EBMLReader(PsyMP3::IO::IOHandler* handler)
    : m_handler(handler)
{
    if (!m_handler) {
        throw std::invalid_argument("EBMLReader requires an IOHandler");
    }
}

int EBMLReader::vintLength(uint8_t first_byte)
{
    // The width is the position of the highest set bit: 1xxxxxxx is one byte,
    // 01xxxxxx two, down to 00000001 at eight. All zeroes would be a ninth
    // byte or beyond, which EBML does not define.
    for (int length = 1; length <= 8; ++length) {
        if (first_byte & (0x80 >> (length - 1))) {
            return length;
        }
    }
    return 0;
}

size_t EBMLReader::decodeVInt(const uint8_t* data, size_t avail, uint64_t& value,
                              bool keep_marker, bool* unknown)
{
    if (unknown) {
        *unknown = false;
    }
    if (!data || avail == 0) {
        return 0;
    }

    const int length = vintLength(data[0]);
    if (length == 0 || static_cast<size_t>(length) > avail) {
        return 0;
    }

    // Accumulate big-endian. The marker bit is part of the number for an ID and
    // not for a size, so the first byte is masked in one case and not the other.
    uint64_t raw = keep_marker
                 ? data[0]
                 : static_cast<uint64_t>(data[0] & (0xFF >> length));
    for (int i = 1; i < length; ++i) {
        raw = (raw << 8) | data[i];
    }

    if (unknown) {
        // Every value bit set. The value bits are 7 per byte for the first and
        // 8 for each after it, so the all-ones pattern depends on the width:
        // 0xFF at one byte, 0x7FFF at two, and so on.
        const uint64_t value_bits = static_cast<uint64_t>(length) * 8 - length;
        const uint64_t all_ones = (value_bits >= 64)
                                ? ~0ULL
                                : ((1ULL << value_bits) - 1);
        const uint64_t without_marker = keep_marker
            ? (raw & ~(1ULL << (value_bits)))
            : raw;
        *unknown = (without_marker == all_ones);
    }

    value = raw;
    return static_cast<size_t>(length);
}

bool EBMLReader::readVInt(uint64_t& value, int max_length, bool keep_marker,
                          bool* unknown, int& consumed)
{
    uint8_t buffer[8];
    if (m_handler->read(buffer, 1, 1) != 1) {
        return false; // clean end of input
    }

    const int length = vintLength(buffer[0]);
    if (length == 0 || length > max_length) {
        throw std::runtime_error("EBML: invalid variable-length integer");
    }
    if (length > 1
        && m_handler->read(buffer + 1, 1, length - 1)
               != static_cast<size_t>(length - 1)) {
        throw std::runtime_error("EBML: truncated variable-length integer");
    }

    if (decodeVInt(buffer, static_cast<size_t>(length), value, keep_marker,
                   unknown)
        != static_cast<size_t>(length)) {
        throw std::runtime_error("EBML: invalid variable-length integer");
    }
    consumed = length;
    return true;
}

bool EBMLReader::readElementHeader(EBMLElement& out)
{
    const uint64_t start = tell();

    uint64_t id = 0;
    int id_length = 0;
    if (!readVInt(id, kMaxIdLength, /*keep_marker=*/true, nullptr, id_length)) {
        return false; // no more elements; the caller's loop ends here
    }

    uint64_t size = 0;
    bool unknown = false;
    int size_length = 0;
    if (!readVInt(size, kMaxSizeLength, /*keep_marker=*/false, &unknown,
                  size_length)) {
        throw std::runtime_error("EBML: element ID with no size");
    }

    out.id = static_cast<uint32_t>(id);
    out.size = unknown ? 0 : size;
    out.unknown_size = unknown;
    out.header_offset = start;
    out.data_offset = start + id_length + size_length;
    return true;
}

void EBMLReader::readPayload(const EBMLElement& element, uint8_t* out,
                             size_t length)
{
    seek(element.data_offset);
    if (length > 0 && m_handler->read(out, 1, length) != length) {
        throw std::runtime_error("EBML: truncated element payload");
    }
}

uint64_t EBMLReader::readUInt(const EBMLElement& element)
{
    if (element.size > 8) {
        throw std::runtime_error("EBML: integer wider than 8 bytes");
    }
    uint8_t buffer[8] = {0};
    readPayload(element, buffer, static_cast<size_t>(element.size));

    // A zero-length integer is legal and is zero; the loop yields that.
    uint64_t value = 0;
    for (uint64_t i = 0; i < element.size; ++i) {
        value = (value << 8) | buffer[i];
    }
    return value;
}

int64_t EBMLReader::readInt(const EBMLElement& element)
{
    if (element.size > 8) {
        throw std::runtime_error("EBML: integer wider than 8 bytes");
    }
    if (element.size == 0) {
        seek(element.data_offset);
        return 0;
    }
    uint8_t buffer[8] = {0};
    readPayload(element, buffer, static_cast<size_t>(element.size));

    uint64_t value = 0;
    for (uint64_t i = 0; i < element.size; ++i) {
        value = (value << 8) | buffer[i];
    }
    // Sign-extend from the encoded width rather than from 64 bits: a one-byte
    // -1 arrives as 0x000000FF and has to become -1, not 255.
    const unsigned bits = static_cast<unsigned>(element.size) * 8;
    if (bits < 64 && (value & (1ULL << (bits - 1)))) {
        value |= ~((1ULL << bits) - 1);
    }
    return static_cast<int64_t>(value);
}

double EBMLReader::readFloat(const EBMLElement& element)
{
    if (element.size == 0) {
        seek(element.data_offset);
        return 0.0;
    }
    if (element.size == 4) {
        uint8_t buffer[4];
        readPayload(element, buffer, 4);
        uint32_t bits = (static_cast<uint32_t>(buffer[0]) << 24)
                      | (static_cast<uint32_t>(buffer[1]) << 16)
                      | (static_cast<uint32_t>(buffer[2]) << 8)
                      | static_cast<uint32_t>(buffer[3]);
        float out = 0.0f;
        std::memcpy(&out, &bits, sizeof(out));
        return static_cast<double>(out);
    }
    if (element.size == 8) {
        uint8_t buffer[8];
        readPayload(element, buffer, 8);
        uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) {
            bits = (bits << 8) | buffer[i];
        }
        double out = 0.0;
        std::memcpy(&out, &bits, sizeof(out));
        return out;
    }
    throw std::runtime_error("EBML: float is neither 4 nor 8 bytes");
}

std::string EBMLReader::readString(const EBMLElement& element)
{
    if (element.size > kMaxBinarySize) {
        throw std::runtime_error("EBML: implausible string length");
    }
    std::string value(static_cast<size_t>(element.size), '\0');
    readPayload(element, reinterpret_cast<uint8_t*>(value.data()),
                static_cast<size_t>(element.size));

    // EBML allows a string to be padded with NULs to a fixed width. Left on,
    // they make every comparison against a CodecID fail.
    const std::size_t end = value.find('\0');
    if (end != std::string::npos) {
        value.resize(end);
    }
    return value;
}

std::vector<uint8_t> EBMLReader::readBinary(const EBMLElement& element)
{
    if (element.size > kMaxBinarySize) {
        // The size came out of the file. Refuse rather than trust it: an
        // 8-byte size field reaches into petabytes, and a truncated file is
        // enough to produce one by accident.
        throw std::runtime_error("EBML: element payload too large to read");
    }
    std::vector<uint8_t> data(static_cast<size_t>(element.size));
    readPayload(element, data.data(), data.size());
    return data;
}

int64_t EBMLReader::readDate(const EBMLElement& element)
{
    if (element.size != 0 && element.size != 8) {
        throw std::runtime_error("EBML: date is neither absent nor 8 bytes");
    }
    // Nanoseconds since 2001-01-01T00:00:00 UTC. Callers converting to Unix
    // time have to add 978307200 seconds; nothing here does that, because
    // nothing in Matroska audio playback needs a wall clock.
    return readInt(element);
}

void EBMLReader::skip(const EBMLElement& element)
{
    if (element.unknown_size) {
        throw std::runtime_error("EBML: cannot skip an element of unknown size");
    }
    seek(element.end());
}

uint64_t EBMLReader::tell()
{
    const auto position = m_handler->tell();
    if (position < 0) {
        throw std::runtime_error("EBML: cannot determine stream position");
    }
    return static_cast<uint64_t>(position);
}

void EBMLReader::seek(uint64_t offset)
{
    if (m_handler->seek(static_cast<PsyMP3::IO::filesize_t>(offset), SEEK_SET)
        != 0) {
        throw std::runtime_error("EBML: seek failed");
    }
}

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3

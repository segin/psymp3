/*
 * matroska_ebml_builder.h - Builds EBML byte sequences for the Matroska tests.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef MATROSKA_EBML_BUILDER_H
#define MATROSKA_EBML_BUILDER_H

// Building the bytes rather than shipping a .mkv keeps every value in the
// Matroska tests visible at the call site, and reaches cases no muxer would
// produce: a zero TimestampScale, a Cues element naming only another track,
// a Cluster whose timestamp is not its first child.
//
// tests/data carries no committed binaries -- the FLAC fixture is generated and
// the G.722 one is an embedded header -- so this is also the house pattern.

namespace MatroskaBuilder {

using namespace PsyMP3::Demuxer::Matroska;

inline std::vector<uint8_t> idBytes(uint32_t id)
{
    std::vector<uint8_t> out;
    bool started = false;
    for (int shift = 24; shift >= 0; shift -= 8) {
        const uint8_t byte = static_cast<uint8_t>((id >> shift) & 0xFF);
        if (byte != 0 || started) {
            out.push_back(byte);
            started = true;
        }
    }
    if (out.empty()) {
        out.push_back(0);
    }
    return out;
}

inline std::vector<uint8_t> sizeBytes(uint64_t size)
{
    // Narrowest width that holds the value, which is what a real muxer writes.
    for (int length = 1; length <= 8; ++length) {
        const uint64_t capacity = (1ULL << (7 * length)) - 1;
        if (size < capacity) {
            std::vector<uint8_t> out(static_cast<size_t>(length));
            uint64_t value = size | (1ULL << (7 * length));
            for (int i = length - 1; i >= 0; --i) {
                out[static_cast<size_t>(i)] = static_cast<uint8_t>(value & 0xFF);
                value >>= 8;
            }
            return out;
        }
    }
    throw std::runtime_error("size too large for a VINT");
}

inline std::vector<uint8_t> element(uint32_t id, const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> out = idBytes(id);
    const std::vector<uint8_t> size = sizeBytes(payload.size());
    out.insert(out.end(), size.begin(), size.end());
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

inline std::vector<uint8_t> operator+(std::vector<uint8_t> a, const std::vector<uint8_t>& b)
{
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

inline std::vector<uint8_t> uintPayload(uint64_t value)
{
    std::vector<uint8_t> out;
    bool started = false;
    for (int shift = 56; shift >= 0; shift -= 8) {
        const uint8_t byte = static_cast<uint8_t>((value >> shift) & 0xFF);
        if (byte != 0 || started) {
            out.push_back(byte);
            started = true;
        }
    }
    if (out.empty()) {
        out.push_back(0);
    }
    return out;
}

inline std::vector<uint8_t> uintEl(uint32_t id, uint64_t value)
{
    return element(id, uintPayload(value));
}

inline std::vector<uint8_t> strEl(uint32_t id, const std::string& value)
{
    return element(id, std::vector<uint8_t>(value.begin(), value.end()));
}

inline std::vector<uint8_t> floatEl(uint32_t id, double value)
{
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    std::vector<uint8_t> payload(8);
    for (int i = 7; i >= 0; --i) {
        payload[static_cast<size_t>(i)] = static_cast<uint8_t>(bits & 0xFF);
        bits >>= 8;
    }
    return element(id, payload);
}

inline std::vector<uint8_t> ebmlHeader(const std::string& doc_type)
{
    return element(Id::EBMLHeader, uintEl(Id::EBMLVersion, 1)
                                 + uintEl(Id::EBMLReadVersion, 1)
                                 + strEl(Id::DocType, doc_type)
                                 + uintEl(Id::DocTypeVersion, 4)
                                 + uintEl(Id::DocTypeReadVersion, 2));
}
} // namespace MatroskaBuilder

#endif // MATROSKA_EBML_BUILDER_H

/*
 * BlockParser.cpp - Matroska block headers and the four lacing modes.
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

size_t BlockParser::decodeSignedVInt(const uint8_t* data, size_t avail, int64_t& value)
{
    uint64_t raw = 0;
    const size_t used = EBMLReader::decodeVInt(data, avail, raw, /*keep_marker=*/false);
    if (used == 0) {
        return 0;
    }
    // Biased by half the range the width can hold: 2^(7n-1) - 1, which is 63
    // for one byte and 8191 for two. Reading it unsigned leaves every delta
    // after the first too large by exactly that.
    const unsigned value_bits = static_cast<unsigned>(used) * 7;
    const int64_t bias = static_cast<int64_t>((1ULL << (value_bits - 1)) - 1);
    value = static_cast<int64_t>(raw) - bias;
    return used;
}

bool BlockParser::parse(const uint8_t* data, size_t size, BlockHeader& header,
                        std::vector<BlockFrame>& frames)
{
    frames.clear();
    if (!data) {
        return false;
    }

    size_t offset = 0;

    // Track number, as an ordinary size-style VINT.
    uint64_t track = 0;
    const size_t track_length = EBMLReader::decodeVInt(data, size, track,
                                                       /*keep_marker=*/false);
    if (track_length == 0) {
        return false;
    }
    offset += track_length;

    // Timestamp: two bytes, big-endian, and signed. A frame may sit before the
    // cluster that holds it, so reading this unsigned puts such a frame roughly
    // 65 seconds into the future at a millisecond tick.
    if (size - offset < 3) {
        return false; // timestamp and flags do not fit
    }
    header.timestamp_offset = static_cast<int16_t>(
        (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1]);
    offset += 2;

    const uint8_t flags = data[offset++];
    header.track_number = track;
    header.keyframe = (flags & 0x80) != 0;
    header.invisible = (flags & 0x08) != 0;
    header.discardable = (flags & 0x01) != 0;
    header.lacing = static_cast<Lacing>((flags >> 1) & 0x03);

    if (header.lacing == Lacing::None) {
        frames.push_back(BlockFrame{data + offset, size - offset});
        return true;
    }

    // Laced: one byte holding the frame count minus one, then the sizes.
    if (offset >= size) {
        return false;
    }
    const size_t frame_count = static_cast<size_t>(data[offset++]) + 1;

    std::vector<size_t> sizes;
    sizes.reserve(frame_count);

    switch (header.lacing) {
    case Lacing::Xiph: {
        // Each size is a run of 0xFF bytes plus a terminator below 0xFF, all
        // added together. The last frame's size is not stored: it is whatever
        // is left, which is why only frame_count - 1 sizes are read.
        for (size_t i = 0; i + 1 < frame_count; ++i) {
            size_t frame_size = 0;
            for (;;) {
                if (offset >= size) {
                    return false;
                }
                const uint8_t byte = data[offset++];
                frame_size += byte;
                if (byte != 0xFF) {
                    break;
                }
            }
            sizes.push_back(frame_size);
        }
        break;
    }
    case Lacing::EBML: {
        // The first size is an unsigned VINT; every one after it is a signed
        // delta from its predecessor, which is what keeps the encoding compact
        // when frames are nearly the same length.
        // n frames store n-1 sizes: the first outright, then n-2 deltas, with
        // the last deduced. A single frame therefore stores none at all, and
        // reading a size regardless would eat the first byte of the frame --
        // silently, since what is left still adds up to a plausible block.
        if (frame_count > 1) {
            uint64_t first = 0;
            const size_t used = EBMLReader::decodeVInt(data + offset, size - offset,
                                                       first, /*keep_marker=*/false);
            if (used == 0) {
                return false;
            }
            offset += used;
            sizes.push_back(static_cast<size_t>(first));

            int64_t previous = static_cast<int64_t>(first);
            for (size_t i = 1; i + 1 < frame_count; ++i) {
                int64_t delta = 0;
                const size_t delta_used = decodeSignedVInt(data + offset, size - offset, delta);
                if (delta_used == 0) {
                    return false;
                }
                offset += delta_used;
                previous += delta;
                if (previous < 0) {
                    return false; // a delta that takes a size below zero
                }
                sizes.push_back(static_cast<size_t>(previous));
            }
        }
        break;
    }
    case Lacing::Fixed: {
        // No sizes at all: the remaining bytes divide evenly among the frames,
        // and a remainder means the block is not what it claims to be.
        const size_t remaining = size - offset;
        if (frame_count == 0 || remaining % frame_count != 0) {
            return false;
        }
        const size_t each = remaining / frame_count;
        for (size_t i = 0; i < frame_count; ++i) {
            sizes.push_back(each);
        }
        break;
    }
    case Lacing::None:
        return false; // handled above; unreachable
    }

    // Fixed lacing already accounts for every frame. The others leave the last
    // one implicit, as the remainder after the stated sizes.
    if (header.lacing != Lacing::Fixed) {
        size_t stated = 0;
        for (size_t frame_size : sizes) {
            // Checked as it accumulates rather than at the end: the sizes come
            // out of the file and could otherwise be made to overflow the sum
            // and pass a check against the payload length.
            if (frame_size > size - offset || stated > (size - offset) - frame_size) {
                return false;
            }
            stated += frame_size;
        }
        sizes.push_back((size - offset) - stated);
    }

    if (sizes.size() != frame_count) {
        return false;
    }

    size_t total = 0;
    for (size_t frame_size : sizes) {
        if (frame_size > size - offset || total > (size - offset) - frame_size) {
            return false;
        }
        total += frame_size;
    }
    if (total != size - offset) {
        return false; // the frames must account for the block exactly
    }

    frames.reserve(frame_count);
    for (size_t frame_size : sizes) {
        frames.push_back(BlockFrame{data + offset, frame_size});
        offset += frame_size;
    }
    return true;
}

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3

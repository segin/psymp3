/*
 * AC3BitReader.h - Most-significant-bit-first bit reader for AC-3.
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

#ifndef AC3BITREADER_H
#define AC3BITREADER_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Reads an AC-3 bit stream.
///
/// A/52 §5.3 states that every element arrives most significant bit first, so
/// this hands bits back in that order and nothing in the decoder has to think
/// about it again.
///
/// Reading past the end yields zeros and sets an overrun flag rather than
/// throwing. A decoder that has mis-tracked its position will read nonsense
/// either way; what matters is that it cannot read outside the buffer, and
/// that the caller can ask afterwards whether it happened. Bit allocation is
/// the case in point -- get it wrong and the mantissa reads run off the end of
/// the frame, which is a symptom worth being able to detect rather than a
/// crash.
class AC3BitReader {
public:
    AC3BitReader(const uint8_t* data, size_t size)
        : m_data(data)
        , m_size(size)
    {
    }

    /// Reads @p bits (0 to 32) and returns them right-aligned.
    uint32_t read(unsigned bits)
    {
        uint32_t value = 0;
        while (bits-- > 0) {
            value = (value << 1) | readBit();
        }
        return value;
    }

    /// Reads one bit.
    uint32_t readBit()
    {
        const size_t byte = m_position >> 3;
        if (byte >= m_size) {
            m_overrun = true;
            ++m_position;
            return 0;
        }
        const unsigned shift = 7 - (m_position & 7);
        ++m_position;
        return (m_data[byte] >> shift) & 1u;
    }

    /// Reads @p bits without consuming them.
    uint32_t peek(unsigned bits)
    {
        const size_t saved = m_position;
        const bool saved_overrun = m_overrun;
        const uint32_t value = read(bits);
        m_position = saved;
        m_overrun = saved_overrun;
        return value;
    }

    void skip(unsigned bits) { m_position += bits; }

    /// Position in bits from the start of the buffer.
    size_t tell() const { return m_position; }
    void seek(size_t bit_position) { m_position = bit_position; }

    /// True once a read has gone past the end of the buffer.
    bool overrun() const { return m_overrun; }
    /// Bits still available; zero once the buffer is exhausted.
    size_t remaining() const
    {
        const size_t total = m_size * 8;
        return m_position >= total ? 0 : total - m_position;
    }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_position = 0;
    bool m_overrun = false;
};

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // AC3BITREADER_H

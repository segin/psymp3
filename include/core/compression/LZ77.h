/*
 * LZ77.h - Lempel-Ziv '77 compression implementation
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill <segin2005@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef PSYMP3_CORE_COMPRESSION_LZ77_H
#define PSYMP3_CORE_COMPRESSION_LZ77_H

#include "core/compression/Compressor.h"
#include "core/compression/Decompressor.h"

namespace PsyMP3 {
namespace Core {
namespace Compression {

/**
 * @brief Configurable LZ77 Compressor implementation
 */
class LZ77Compressor : public Compressor {
public:
    /**
     * @brief Construct a new LZ77Compressor
     * 
     * @param window_size Size of the sliding window (default: 4096)
     * @param lookahead_buffer_size Size of the lookahead buffer (default: 18)
     */
    LZ77Compressor(size_t window_size = 4096, size_t lookahead_buffer_size = 18);

    std::vector<uint8_t> compress(const uint8_t* data, size_t size) override;

private:
    size_t m_window_size;
    size_t m_lookahead_buffer_size;
};

/**
 * @brief LZ77 Decompressor implementation
 */
class LZ77Decompressor : public Decompressor {
public:
    std::vector<uint8_t> decompress(const uint8_t* data, size_t size) override;
};

} // namespace Compression
} // namespace Core
} // namespace PsyMP3

#endif // PSYMP3_CORE_COMPRESSION_LZ77_H

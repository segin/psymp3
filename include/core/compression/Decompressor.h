/*
 * Decompressor.h - Decompressor interface
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

#ifndef PSYMP3_CORE_COMPRESSION_DECOMPRESSOR_H
#define PSYMP3_CORE_COMPRESSION_DECOMPRESSOR_H

#include <vector>
#include <cstdint>
#include <cstddef>

namespace PsyMP3 {
namespace Core {
namespace Compression {

/**
 * @brief Interface for decompression algorithms
 */
class Decompressor {
public:
    virtual ~Decompressor() = default;

    /**
     * @brief Decompress data
     * 
     * @param data Compressed data pointer
     * @param size Compressed data size in bytes
     * @return Decompressed data
     */
    virtual std::vector<uint8_t> decompress(const uint8_t* data, size_t size) = 0;
};

} // namespace Compression
} // namespace Core
} // namespace PsyMP3

#endif // PSYMP3_CORE_COMPRESSION_DECOMPRESSOR_H

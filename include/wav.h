/*
 * wav.h - RIFF WAVE format decoder.
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef WAVSTREAM_H
#define WAVSTREAM_H

#include "IOHandler.h"
class WaveStream : public Stream {
public:
    explicit WaveStream(const TagLib::String& path);
    ~WaveStream() override;

    size_t getData(size_t len, void *buf) override;
    void seekTo(unsigned long pos) override;
    bool eof() override;

private:
    void parseHeaders();

    enum class WaveEncoding {
        Unsupported,
        PCM,
        IEEE_FLOAT,
        ALAW,
        MULAW
    };

    std::unique_ptr<IOHandler> m_handler;
    WaveEncoding m_encoding = WaveEncoding::Unsupported;
    uint16_t m_bits_per_sample = 0;
    uint16_t m_bytes_per_sample = 0;
    uint32_t m_data_chunk_offset = 0;
    uint32_t m_data_chunk_size = 0;
    uint32_t m_bytes_read_from_data = 0;
};

#endif // WAVSTREAM_H
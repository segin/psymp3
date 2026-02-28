/*
 * CodecHeaderParser.cpp - Codec identification and header parsing
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill II <segin2005@gmail.com>
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

#include <memory>
#include <cstring>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

std::unique_ptr<CodecHeaderParser> CodecHeaderParser::create(ogg_packet* bos_packet) {
    if (!bos_packet || bos_packet->bytes < 1) return nullptr;

    const unsigned char* data = bos_packet->packet;
    
    // Check identification strings
    
    // Vorbis: "\01vorbis"
    if (bos_packet->bytes >= 7 && 
        memcmp(data, "\x01vorbis", 7) == 0) {
        return std::make_unique<VorbisHeaderParser>();
    }
    
    // Opus: "OpusHead"
    if (bos_packet->bytes >= 8 &&
        memcmp(data, "OpusHead", 8) == 0) {
        return std::make_unique<OpusHeaderParser>();
    }
    
    // FLAC: "\x7fFLAC"
    if (bos_packet->bytes >= 5 &&
        memcmp(data, "\x7f" "FLAC", 5) == 0) {
        return std::make_unique<FLACHeaderParser>();
    }
    
    // Speex: "Speex   "
    if (bos_packet->bytes >= 8 &&
        memcmp(data, "Speex   ", 8) == 0) {
        return std::make_unique<SpeexHeaderParser>();
    }

    return nullptr;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

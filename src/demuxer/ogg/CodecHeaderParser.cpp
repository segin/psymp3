/*
 * CodecHeaderParser.cpp - Codec identification and header parsing
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 */

#include "demuxer/ogg/CodecHeaderParser.h"
#include "demuxer/ogg/VorbisHeaderParser.h"
#include "demuxer/ogg/OpusHeaderParser.h"
#include "demuxer/ogg/FLACHeaderParser.h"
#include "demuxer/ogg/SpeexHeaderParser.h"
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

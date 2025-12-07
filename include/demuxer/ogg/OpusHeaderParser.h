/*
 * OpusHeaderParser.h - Opus header parsing
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#ifndef HAS_OPUSHEADERPARSER_H
#define HAS_OPUSHEADERPARSER_H

#include "CodecHeaderParser.h"

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

class OpusHeaderParser : public CodecHeaderParser {
public:
    OpusHeaderParser() : m_headers_count(0) {}
    
    bool parseHeader(ogg_packet* packet) override {
        // Todo: Implement
        m_headers_count++;
        return true;
    }
    
    bool isHeadersComplete() const override {
        return m_headers_count >= 2;
    }
    
    CodecInfo getCodecInfo() const override {
        CodecInfo info;
        info.codec_name = "Opus";
        return info;
    }

private:
    int m_headers_count;
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_OPUSHEADERPARSER_H

/*
 * FLACHeaderParser.h - FLAC header parsing
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#ifndef HAS_FLACHEADERPARSER_H
#define HAS_FLACHEADERPARSER_H

#include "CodecHeaderParser.h"

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

class FLACHeaderParser : public CodecHeaderParser {
public:
    FLACHeaderParser() : m_headers_count(0) {}
    
    bool parseHeader(ogg_packet* packet) override {
        // Todo: Implement
        m_headers_count++;
        return true;
    }
    
    bool isHeadersComplete() const override {
        return m_headers_count >= 1; // Simplification
    }
    
    CodecInfo getCodecInfo() const override {
        CodecInfo info;
        info.codec_name = "FLAC";
        return info;
    }

private:
    int m_headers_count;
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_FLACHEADERPARSER_H

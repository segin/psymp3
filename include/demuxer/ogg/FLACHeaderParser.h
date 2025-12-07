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
    FLACHeaderParser();
    
    bool parseHeader(ogg_packet* packet) override;
    bool isHeadersComplete() const override;
    CodecInfo getCodecInfo() const override;

private:
    int m_headers_count;
    CodecInfo m_info;
    bool m_streaminfo_found;
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_FLACHEADERPARSER_H

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
    OpusHeaderParser();
    
    bool parseHeader(ogg_packet* packet) override;
    bool isHeadersComplete() const override;
    CodecInfo getCodecInfo() const override;
    OggVorbisComment getVorbisComment() const override;

private:
    int m_headers_count;
    CodecInfo m_info;
    OggVorbisComment m_comment;
    
    /**
     * @brief Parse VorbisComment data from OpusTags header packet
     * @param data Pointer to comment data (after "OpusTags" signature)
     * @param size Size of comment data
     * @return true if parsed successfully
     */
    bool parseOpusTags(const unsigned char* data, size_t size);
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_OPUSHEADERPARSER_H

/*
 * VorbisHeaderParser.h - Vorbis header parsing
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#ifndef HAS_VORBISHEADERPARSER_H
#define HAS_VORBISHEADERPARSER_H

#include "CodecHeaderParser.h"

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

class VorbisHeaderParser : public CodecHeaderParser {
public:
    VorbisHeaderParser();
    
    bool parseHeader(ogg_packet* packet) override;
    bool isHeadersComplete() const override;
    CodecInfo getCodecInfo() const override;
    OggVorbisComment getVorbisComment() const override;

private:
    int m_headers_count;
    CodecInfo m_info;
    OggVorbisComment m_comment;
    
    /**
     * @brief Parse VorbisComment data from comment header packet
     * @param data Pointer to comment data (after "vorbis" signature)
     * @param size Size of comment data
     * @return true if parsed successfully
     */
    bool parseVorbisComment(const unsigned char* data, size_t size);
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_VORBISHEADERPARSER_H

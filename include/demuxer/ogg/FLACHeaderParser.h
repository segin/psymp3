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
    OggVorbisComment getVorbisComment() const override;

private:
    int m_headers_count;
    CodecInfo m_info;
    bool m_streaminfo_found;

    // Ogg-FLAC mapping: the ID header (first packet) carries a 16-bit
    // big-endian count of the header packets that follow it (metadata blocks
    // such as VORBIS_COMMENT). Headers are complete only once all of them have
    // been consumed; treating STREAMINFO alone as complete leaks the comment
    // block into the audio stream and drops the tags.
    int m_expected_headers;      ///< Header packets expected after the first (0 = unknown)
    bool m_last_metadata_seen;   ///< A metadata block with the last-block flag was parsed
    OggVorbisComment m_comment;  ///< Parsed VORBIS_COMMENT metadata

    /**
     * @brief Parse a FLAC VORBIS_COMMENT metadata block body
     * @param data Pointer to the block body (after the 4-byte metadata header)
     * @param size Size of the block body
     * @return true if parsed successfully
     */
    bool parseVorbisCommentBlock(const unsigned char* data, size_t size);
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_FLACHEADERPARSER_H

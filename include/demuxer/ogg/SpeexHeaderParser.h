/*
 * SpeexHeaderParser.h - Speex header parsing
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#ifndef HAS_SPEEXHEADERPARSER_H
#define HAS_SPEEXHEADERPARSER_H



namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

class SpeexHeaderParser : public CodecHeaderParser {
public:
    SpeexHeaderParser();
    
    bool parseHeader(ogg_packet* packet) override;
    bool isHeadersComplete() const override;
    CodecInfo getCodecInfo() const override;

private:
    int m_headers_count;
    CodecInfo m_info;
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_SPEEXHEADERPARSER_H

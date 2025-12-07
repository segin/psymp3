/*
 * CodecHeaderParser.h - Codec identification and header parsing
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * This file defines the base class and factory for parsing codec-specific
 * headers from Ogg packets.
 */

#ifndef HAS_CODECHEADERPARSER_H
#define HAS_CODECHEADERPARSER_H

#include <ogg/ogg.h>
#include <memory>
#include <string>
#include <vector>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

// Forward decl
struct CodecInfo {
    std::string codec_name;
    int channels;
    long rate;
    // Add other common fields
};

class CodecHeaderParser {
public:
    virtual ~CodecHeaderParser() = default;

    /**
     * @brief Parse a header packet
     * @param packet The packet to parse
     * @return true if parsed successfully
     */
    virtual bool parseHeader(ogg_packet* packet) = 0;

    /**
     * @brief Check if we have all necessary headers
     */
    virtual bool isHeadersComplete() const = 0;

    /**
     * @brief Get codec info
     */
    virtual CodecInfo getCodecInfo() const = 0;

    /**
     * @brief Factory method: Identify codec from BOS packet
     */
    static std::unique_ptr<CodecHeaderParser> create(ogg_packet* bos_packet);
};

// Derived classes will be implemented in .cpp or separate headers if large
// For now, minimal definitions

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_CODECHEADERPARSER_H

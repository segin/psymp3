/*
 * MetadataExtractor.h - Metadata extractor for iTunes/ISO metadata parsing
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef METADATAEXTRACTOR_H
#define METADATAEXTRACTOR_H

namespace PsyMP3 {
namespace Demuxer {
namespace ISO {

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Metadata extractor for iTunes/ISO metadata parsing
 */
class MetadataExtractor {
public:
    MetadataExtractor() = default;
    ~MetadataExtractor() = default;
    
    std::map<std::string, std::string> ExtractMetadata(std::shared_ptr<IOHandler> io, uint64_t udtaOffset, uint64_t size);
    
private:
    static constexpr int MAX_RECURSION_DEPTH = 32;

    bool ParseUdtaBox(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata, int depth = 0);
    bool ParseMetaBox(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata, int depth = 0);
    bool ParseIlstBox(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata);
    bool ParseiTunesMetadataAtom(std::shared_ptr<IOHandler> io, uint32_t atomType, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata);
    
    std::string ExtractTextMetadata(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size);
    
    // Helper methods for reading big-endian values
    uint32_t ReadUInt32BE(std::shared_ptr<IOHandler> io, uint64_t offset);
    uint64_t ReadUInt64BE(std::shared_ptr<IOHandler> io, uint64_t offset);
};


} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
#endif // METADATAEXTRACTOR_H
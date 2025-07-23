/*
 * ISODemuxerMetadataExtractor.h - Metadata extractor for iTunes/ISO metadata parsing
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef ISODEMUXERMETADATAEXTRACTOR_H
#define ISODEMUXERMETADATAEXTRACTOR_H

#include "Demuxer.h"
#include <memory>
#include <map>
#include <string>

/**
 * @brief Metadata extractor for iTunes/ISO metadata parsing
 */
class ISODemuxerMetadataExtractor {
public:
    ISODemuxerMetadataExtractor() = default;
    ~ISODemuxerMetadataExtractor() = default;
    
    std::map<std::string, std::string> ExtractMetadata(std::shared_ptr<IOHandler> io, uint64_t udtaOffset, uint64_t size);
    
private:
    bool ParseUdtaBox(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata);
    bool ParseMetaBox(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata);
    bool ParseIlstBox(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata);
    bool ParseiTunesMetadataAtom(std::shared_ptr<IOHandler> io, uint32_t atomType, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata);
    
    std::string ExtractTextMetadata(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size);
    
    // Helper methods for reading big-endian values
    uint32_t ReadUInt32BE(std::shared_ptr<IOHandler> io, uint64_t offset);
    uint64_t ReadUInt64BE(std::shared_ptr<IOHandler> io, uint64_t offset);
};

#endif // ISODEMUXERMETADATAEXTRACTOR_H
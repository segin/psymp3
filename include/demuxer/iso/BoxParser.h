/*
 * BoxParser.h - ISO box structure parser
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef BOXPARSER_H
#define BOXPARSER_H

namespace PsyMP3 {
namespace Demuxer {
namespace ISO {

// No direct includes - all includes should be in psymp3.h

/**
 * @brief ISO box header structure
 */
struct BoxHeader {
    uint32_t type;
    uint64_t size;
    uint64_t dataOffset;
    bool extendedSize = false; // Track if this was parsed as extended size
    
    bool isExtendedSize() const { return extendedSize; }
};

/**
 * @brief Box parser component for recursive ISO box structure parsing
 */
class BoxParser {
public:
    static constexpr uint32_t MAX_BOX_DEPTH = 32;

    explicit BoxParser(std::shared_ptr<IOHandler> io);
    ~BoxParser() = default;
    
    bool ParseMovieBox(uint64_t offset, uint64_t size, uint32_t depth = 0);
    bool ParseTrackBox(uint64_t offset, uint64_t size, AudioTrackInfo& track, uint32_t depth = 0);
    bool ParseSampleTableBox(uint64_t offset, uint64_t size, SampleTableInfo& tables, uint32_t depth = 0);
    bool ParseFragmentBox(uint64_t offset, uint64_t size);
    
    // Core box parsing functionality
    BoxHeader ReadBoxHeader(uint64_t offset);
    bool ValidateBoxSize(const BoxHeader& header, uint64_t containerSize);
    bool ParseBoxRecursively(uint64_t offset, uint64_t size, 
                            std::function<bool(const BoxHeader&, uint64_t, uint32_t)> handler, uint32_t depth = 0);
    
    // Additional parsing methods for file type and movie box parsing
    bool ParseFileTypeBox(uint64_t offset, uint64_t size, std::string& containerType);
    bool ParseMediaBox(uint64_t offset, uint64_t size, AudioTrackInfo& track, bool& foundAudio, uint32_t depth = 0);
    bool ParseMediaBoxWithSampleTables(uint64_t offset, uint64_t size, AudioTrackInfo& track, bool& foundAudio, SampleTableInfo& sampleTables, uint32_t depth = 0);
    bool ParseHandlerBox(uint64_t offset, uint64_t size, std::string& handlerType);
    bool ParseSampleDescriptionBox(uint64_t offset, uint64_t size, AudioTrackInfo& track, uint32_t depth = 0);
    
    // Codec-specific configuration parsing
    bool ParseAACConfiguration(uint64_t offset, uint64_t size, AudioTrackInfo& track);
    bool ParseALACConfiguration(uint64_t offset, uint64_t size, AudioTrackInfo& track);
    bool ParseFLACConfiguration(uint64_t offset, uint64_t size, AudioTrackInfo& track);
    
    // Telephony codec configuration and validation
    bool ConfigureTelephonyCodec(AudioTrackInfo& track, const std::string& codecType);
    bool ValidateTelephonyParameters(AudioTrackInfo& track);
    void ApplyTelephonyDefaults(AudioTrackInfo& track, const std::string& codecType);
    
    // Sample table parsing methods
    bool ParseTimeToSampleBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseSampleToChunkBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseSampleSizeBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseChunkOffsetBox(uint64_t offset, uint64_t size, SampleTableInfo& tables, bool is64Bit);
    bool ParseSyncSampleBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    
    uint32_t ReadUInt32BE(uint64_t offset);
    uint64_t ReadUInt64BE(uint64_t offset);
    std::string BoxTypeToString(uint32_t boxType);
    bool SkipUnknownBox(const BoxHeader& header);
    
private:
    std::shared_ptr<IOHandler> io;
    std::stack<BoxHeader> boxStack;
    uint64_t fileSize;
    
    bool IsContainerBox(uint32_t boxType);
};


} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
#endif // BOXPARSER_H
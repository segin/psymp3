/*
 * ISODemuxer.cpp - ISO Base Media File Format demuxer implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "ISODemuxer.h"
#include <algorithm>
#include <stdexcept>
#include <cstring>

ISODemuxer::ISODemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)), selectedTrackIndex(-1), currentSampleIndex(0), m_eof(false) {
    initializeComponents();
}

ISODemuxer::~ISODemuxer() {
    cleanup();
}

void ISODemuxer::initializeComponents() {
    // Initialize core components with shared IOHandler
    std::shared_ptr<IOHandler> sharedHandler(m_handler.get(), [](IOHandler*) {
        // Custom deleter that does nothing - we don't want to delete the handler
        // since it's owned by the unique_ptr in the base class
    });
    
    boxParser = std::make_unique<BoxParser>(sharedHandler);
    sampleTables = std::make_unique<SampleTableManager>();
    fragmentHandler = std::make_unique<FragmentHandler>();
    metadataExtractor = std::make_unique<MetadataExtractor>();
    streamManager = std::make_unique<StreamManager>();
    seekingEngine = std::make_unique<SeekingEngine>();
}

void ISODemuxer::cleanup() {
    // Components will be automatically cleaned up by unique_ptr destructors
    audioTracks.clear();
    selectedTrackIndex = -1;
    currentSampleIndex = 0;
}

// BoxParser implementation
BoxParser::BoxParser(std::shared_ptr<IOHandler> io) : io(io), fileSize(0) {
    // Get file size for validation
    if (io) {
        io->seek(0, SEEK_END);
        fileSize = static_cast<uint64_t>(io->tell());
        io->seek(0, SEEK_SET);
    }
}

BoxHeader BoxParser::ReadBoxHeader(uint64_t offset) {
    BoxHeader header = {};
    
    if (!io || offset >= fileSize) {
        return header;
    }
    
    // Seek to box position
    if (io->seek(static_cast<long>(offset), SEEK_SET) != 0) {
        return header;
    }
    
    // Read basic box header (8 bytes minimum)
    if (offset + 8 > fileSize) {
        return header;
    }
    
    // Read size (4 bytes, big-endian)
    uint32_t size32 = ReadUInt32BE(offset);
    
    // Read type (4 bytes, big-endian)
    header.type = ReadUInt32BE(offset + 4);
    
    // Handle extended size
    if (size32 == 1) {
        // Extended size - read 8 more bytes
        if (offset + 16 > fileSize) {
            return header;
        }
        header.size = ReadUInt64BE(offset + 8);
        header.dataOffset = offset + 16;
    } else if (size32 == 0) {
        // Size extends to end of file
        header.size = fileSize - offset;
        header.dataOffset = offset + 8;
    } else {
        // Normal size
        header.size = size32;
        header.dataOffset = offset + 8;
    }
    
    return header;
}

bool BoxParser::ValidateBoxSize(const BoxHeader& header, uint64_t containerSize) {
    // Check if box size is valid
    if (header.size == 0) {
        return false;
    }
    
    // Check if box fits within container
    if (header.size > containerSize) {
        return false;
    }
    
    // Check if box extends beyond file
    uint64_t boxStart = header.dataOffset - (header.isExtendedSize() ? 16 : 8);
    if (boxStart + header.size > fileSize) {
        return false;
    }
    
    // Check minimum header size
    uint64_t headerSize = header.isExtendedSize() ? 16 : 8;
    if (header.size < headerSize) {
        return false;
    }
    
    return true;
}

bool BoxParser::ParseBoxRecursively(uint64_t offset, uint64_t size, 
                                   std::function<bool(const BoxHeader&, uint64_t)> handler) {
    uint64_t currentOffset = offset;
    uint64_t endOffset = offset + size;
    
    while (currentOffset < endOffset) {
        // Read box header
        BoxHeader header = ReadBoxHeader(currentOffset);
        
        if (header.type == 0 || header.size == 0) {
            // Invalid box, skip remaining data
            break;
        }
        
        // Validate box size
        if (!ValidateBoxSize(header, endOffset - currentOffset)) {
            // Invalid box size, skip it
            currentOffset += 8; // Skip at least the basic header
            continue;
        }
        
        // Call handler for this box
        if (!handler(header, currentOffset)) {
            // Handler failed, but continue parsing other boxes
        }
        
        // Move to next box
        currentOffset += header.size;
        
        // Prevent infinite loops
        if (header.size < 8) {
            break;
        }
    }
    
    return true;
}

bool BoxParser::IsContainerBox(uint32_t boxType) {
    switch (boxType) {
        case BOX_MOOV:
        case BOX_TRAK:
        case BOX_MDIA:
        case BOX_MINF:
        case BOX_STBL:
        case BOX_EDTS:
        case BOX_DINF:
        case BOX_UDTA:
        case BOX_META:
        case BOX_ILST:
        case BOX_MOOF:
        case BOX_TRAF:
        case BOX_MFRA:
            return true;
        default:
            return false;
    }
}

uint32_t BoxParser::ReadUInt32BE(uint64_t offset) {
    if (!io || offset + 4 > fileSize) {
        return 0;
    }
    
    io->seek(static_cast<long>(offset), SEEK_SET);
    
    uint8_t bytes[4];
    if (io->read(bytes, 1, 4) != 4) {
        return 0;
    }
    
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           static_cast<uint32_t>(bytes[3]);
}

uint64_t BoxParser::ReadUInt64BE(uint64_t offset) {
    if (!io || offset + 8 > fileSize) {
        return 0;
    }
    
    io->seek(static_cast<long>(offset), SEEK_SET);
    
    uint8_t bytes[8];
    if (io->read(bytes, 1, 8) != 8) {
        return 0;
    }
    
    return (static_cast<uint64_t>(bytes[0]) << 56) |
           (static_cast<uint64_t>(bytes[1]) << 48) |
           (static_cast<uint64_t>(bytes[2]) << 40) |
           (static_cast<uint64_t>(bytes[3]) << 32) |
           (static_cast<uint64_t>(bytes[4]) << 24) |
           (static_cast<uint64_t>(bytes[5]) << 16) |
           (static_cast<uint64_t>(bytes[6]) << 8) |
           static_cast<uint64_t>(bytes[7]);
}

std::string BoxParser::BoxTypeToString(uint32_t boxType) {
    std::string result(4, '\0');
    result[0] = static_cast<char>((boxType >> 24) & 0xFF);
    result[1] = static_cast<char>((boxType >> 16) & 0xFF);
    result[2] = static_cast<char>((boxType >> 8) & 0xFF);
    result[3] = static_cast<char>(boxType & 0xFF);
    return result;
}

bool BoxParser::SkipUnknownBox(const BoxHeader& header) {
    // For unknown boxes, we simply skip them by returning true
    // The recursive parser will automatically move to the next box
    return true;
}

bool BoxParser::ParseMovieBox(uint64_t offset, uint64_t size) {
    // Basic implementation - will be expanded in later tasks
    return true;
}

bool BoxParser::ParseTrackBox(uint64_t offset, uint64_t size, AudioTrackInfo& track) {
    // Basic implementation - will be expanded in later tasks
    return true;
}

bool BoxParser::ParseSampleTableBox(uint64_t offset, uint64_t size, SampleTableInfo& tables) {
    // Basic implementation - will be expanded in later tasks
    return true;
}

bool BoxParser::ParseFragmentBox(uint64_t offset, uint64_t size) {
    // Basic implementation - will be expanded in later tasks
    return true;
}

// SampleTableManager implementation
bool SampleTableManager::BuildSampleTables(const SampleTableInfo& rawTables) {
    // Basic implementation - will be expanded in later tasks
    return true;
}

SampleTableManager::SampleInfo SampleTableManager::GetSampleInfo(uint64_t sampleIndex) {
    // Basic implementation - will be expanded in later tasks
    return SampleInfo{};
}

uint64_t SampleTableManager::TimeToSample(double timestamp) {
    // Basic implementation - will be expanded in later tasks
    return 0;
}

double SampleTableManager::SampleToTime(uint64_t sampleIndex) {
    // Basic implementation - will be expanded in later tasks
    return 0.0;
}

// FragmentHandler implementation
bool FragmentHandler::ProcessMovieFragment(uint64_t moofOffset) {
    // Basic implementation - will be expanded in later tasks
    return true;
}

bool FragmentHandler::UpdateSampleTables(const SampleTableInfo& traf) {
    // Basic implementation - will be expanded in later tasks
    return true;
}

// MetadataExtractor implementation
std::map<std::string, std::string> MetadataExtractor::ExtractMetadata(uint64_t udtaOffset, uint64_t size) {
    // Basic implementation - will be expanded in later tasks
    return std::map<std::string, std::string>();
}

std::string MetadataExtractor::ExtractTextMetadata(const std::vector<uint8_t>& data) {
    // Basic implementation - will be expanded in later tasks
    return std::string();
}

// StreamManager implementation
void StreamManager::AddAudioTrack(const AudioTrackInfo& track) {
    tracks.push_back(track);
}

std::vector<StreamInfo> StreamManager::GetStreamInfos() const {
    std::vector<StreamInfo> streams;
    for (const auto& track : tracks) {
        StreamInfo info;
        info.stream_id = track.trackId;
        info.codec_type = "audio";
        info.codec_name = track.codecType;
        info.sample_rate = track.sampleRate;
        info.channels = track.channelCount;
        info.bits_per_sample = track.bitsPerSample;
        info.bitrate = track.avgBitrate;
        info.codec_data = track.codecConfig;
        info.duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
        info.duration_samples = info.sample_rate > 0 ? (info.duration_ms * info.sample_rate) / 1000ULL : 0;
        streams.push_back(info);
    }
    return streams;
}

AudioTrackInfo* StreamManager::GetTrack(uint32_t trackId) {
    for (auto& track : tracks) {
        if (track.trackId == trackId) {
            return &track;
        }
    }
    return nullptr;
}

// SeekingEngine implementation
bool SeekingEngine::SeekToTimestamp(double timestamp, AudioTrackInfo& track, SampleTableManager& sampleTables) {
    // Basic implementation - will be expanded in later tasks
    return true;
}

uint64_t SeekingEngine::BinarySearchTimeToSample(double timestamp, const std::vector<SampleTableManager::SampleInfo>& samples) {
    // Basic implementation - will be expanded in later tasks
    return 0;
}

bool ISODemuxer::parseContainer() {
    if (m_parsed) {
        return true;
    }
    
    try {
        // Get file size
        m_handler->seek(0, SEEK_END);
        off_t file_size = m_handler->tell();
        m_handler->seek(0, SEEK_SET);
        
        if (file_size <= 0) {
            return false;
        }
        
        // Use BoxParser to parse the movie box
        // For now, we'll do basic parsing until BoxParser is fully implemented
        if (!boxParser->ParseMovieBox(0, static_cast<uint64_t>(file_size))) {
            return false;
        }
        
        // Calculate duration from audio tracks
        for (const auto& track : audioTracks) {
            uint64_t track_duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
            m_duration_ms = std::max(m_duration_ms, track_duration_ms);
        }
        
        m_parsed = true;
        return !audioTracks.empty();
        
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<StreamInfo> ISODemuxer::getStreams() const {
    if (streamManager) {
        return streamManager->GetStreamInfos();
    }
    return std::vector<StreamInfo>();
}

StreamInfo ISODemuxer::getStreamInfo(uint32_t stream_id) const {
    auto streams = getStreams();
    auto it = std::find_if(streams.begin(), streams.end(),
                          [stream_id](const StreamInfo& info) {
                              return info.stream_id == stream_id;
                          });
    
    if (it != streams.end()) {
        return *it;
    }
    
    return StreamInfo{};
}

MediaChunk ISODemuxer::readChunk() {
    // Find the first available audio track if none selected
    if (selectedTrackIndex == -1 && !audioTracks.empty()) {
        selectedTrackIndex = 0;
    }
    
    if (selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(audioTracks.size())) {
        return readChunk(audioTracks[selectedTrackIndex].trackId);
    }
    
    return MediaChunk{};
}

MediaChunk ISODemuxer::readChunk(uint32_t stream_id) {
    if (!streamManager) {
        return MediaChunk{};
    }
    
    AudioTrackInfo* track = streamManager->GetTrack(stream_id);
    if (!track) {
        return MediaChunk{};
    }
    
    // Use SampleTableManager to get sample info
    if (!sampleTables) {
        return MediaChunk{};
    }
    
    auto sampleInfo = sampleTables->GetSampleInfo(currentSampleIndex);
    if (sampleInfo.size == 0) {
        m_eof = true;
        return MediaChunk{};
    }
    
    // Create MediaChunk with sample data
    MediaChunk chunk;
    chunk.stream_id = stream_id;
    chunk.data.resize(sampleInfo.size);
    chunk.timestamp_samples = currentSampleIndex;
    chunk.is_keyframe = sampleInfo.isKeyframe;
    chunk.file_offset = sampleInfo.offset;
    
    // Read sample data from file
    m_handler->seek(sampleInfo.offset, SEEK_SET);
    size_t bytes_read = m_handler->read(chunk.data.data(), 1, sampleInfo.size);
    
    if (bytes_read != sampleInfo.size) {
        chunk.data.resize(bytes_read);
    }
    
    // Update position
    currentSampleIndex++;
    double timestamp = sampleTables->SampleToTime(currentSampleIndex);
    m_position_ms = static_cast<uint64_t>(timestamp * 1000.0);
    
    return chunk;
}

bool ISODemuxer::seekTo(uint64_t timestamp_ms) {
    if (!seekingEngine || !sampleTables || selectedTrackIndex == -1) {
        return false;
    }
    
    if (selectedTrackIndex >= static_cast<int>(audioTracks.size())) {
        return false;
    }
    
    AudioTrackInfo& track = audioTracks[selectedTrackIndex];
    double timestamp_seconds = timestamp_ms / 1000.0;
    
    bool success = seekingEngine->SeekToTimestamp(timestamp_seconds, track, *sampleTables);
    
    if (success) {
        currentSampleIndex = track.currentSampleIndex;
        m_position_ms = timestamp_ms;
        m_eof = false;
    }
    
    return success;
}

bool ISODemuxer::isEOF() const {
    return m_eof;
}

uint64_t ISODemuxer::getDuration() const {
    return m_duration_ms;
}

uint64_t ISODemuxer::getPosition() const {
    return m_position_ms;
}


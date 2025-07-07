/*
 * OggDemuxer.cpp - Ogg container demuxer implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

OggDemuxer::OggDemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)) {
}

bool OggDemuxer::parseContainer() {
    if (m_parsed) {
        return true;
    }
    
    try {
        // Get file size
        m_handler->seek(0, SEEK_END);
        m_file_size = m_handler->tell();
        m_handler->seek(0, SEEK_SET);
        m_current_offset = 0;
        
        // Parse headers by reading pages until all streams have complete headers
        bool all_headers_complete = false;
        int max_header_pages = 100; // Prevent infinite loops on malformed files
        
        while (!all_headers_complete && max_header_pages-- > 0) {
            OggPageHeader header;
            if (!readPageHeader(header)) {
                break;
            }
            
            std::vector<uint8_t> page_data;
            if (!readPageData(header, page_data)) {
                break;
            }
            
            // Create or update stream
            uint32_t stream_id = header.serial_number;
            if (m_streams.find(stream_id) == m_streams.end()) {
                OggStream stream;
                stream.serial_number = stream_id;
                m_streams[stream_id] = stream;
            }
            
            OggStream& stream = m_streams[stream_id];
            
            // Extract segment table for packet parsing
            std::vector<uint8_t> segment_table(header.page_segments);
            // Note: segment table should have been read as part of readPageData
            // For now, we'll reconstruct packets without exact segment boundaries
            
            // Parse packets from this page
            auto packets = parsePackets(header, page_data, segment_table);
            
            for (const auto& packet : packets) {
                if (!stream.headers_complete) {
                    // Try to identify codec from first packet
                    if (stream.codec_name.empty()) {
                        stream.codec_name = identifyCodec(packet.data);
                        stream.codec_type = stream.codec_name.empty() ? "unknown" : "audio";
                    }
                    
                    // Parse codec-specific headers
                    if (stream.codec_name == "vorbis") {
                        parseVorbisHeaders(stream, packet);
                    } else if (stream.codec_name == "flac") {
                        parseFLACHeaders(stream, packet);
                    } else if (stream.codec_name == "opus") {
                        parseOpusHeaders(stream, packet);
                    } else if (stream.codec_name == "speex") {
                        parseSpeexHeaders(stream, packet);
                    }
                    
                    stream.header_packets.push_back(packet);
                    
                    // Check if headers are complete (codec-specific logic)
                    if (stream.codec_name == "vorbis" && stream.header_packets.size() >= 3) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "flac" && stream.header_packets.size() >= 1) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "opus" && stream.header_packets.size() >= 2) {
                        stream.headers_complete = true;
                    }
                }
            }
            
            stream.last_granule = header.granule_position;
            stream.last_page_sequence = header.page_sequence;
            
            // Check if all streams have complete headers
            all_headers_complete = true;
            for (const auto& [id, s] : m_streams) {
                if (s.codec_type == "audio" && !s.headers_complete) {
                    all_headers_complete = false;
                    break;
                }
            }
        }
        
        // Calculate duration from the parsed streams
        calculateDuration();
        
        // Reset to beginning for data reading
        m_handler->seek(0, SEEK_SET);
        m_current_offset = 0;
        
        m_parsed = true;
        return !m_streams.empty();
        
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<StreamInfo> OggDemuxer::getStreams() const {
    std::vector<StreamInfo> streams;
    
    for (const auto& [stream_id, ogg_stream] : m_streams) {
        if (ogg_stream.codec_type != "audio") {
            continue; // Skip non-audio streams for now
        }
        
        StreamInfo info;
        info.stream_id = stream_id;
        info.codec_type = ogg_stream.codec_type;
        info.codec_name = ogg_stream.codec_name;
        info.sample_rate = ogg_stream.sample_rate;
        info.channels = ogg_stream.channels;
        info.bitrate = ogg_stream.bitrate;
        info.duration_samples = ogg_stream.total_samples;
        
        if (ogg_stream.sample_rate > 0) {
            info.duration_ms = (ogg_stream.total_samples * 1000ULL) / ogg_stream.sample_rate;
        }
        
        // Include codec setup data
        info.codec_data = ogg_stream.codec_setup_data;
        
        streams.push_back(info);
    }
    
    return streams;
}

StreamInfo OggDemuxer::getStreamInfo(uint32_t stream_id) const {
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

MediaChunk OggDemuxer::readChunk() {
    // Find the best audio stream
    uint32_t best_stream = findBestAudioStream();
    if (best_stream != 0) {
        return readChunk(best_stream);
    }
    
    return MediaChunk{};
}

MediaChunk OggDemuxer::readChunk(uint32_t stream_id) {
    // Check if we have a queued packet for this stream
    while (!m_packet_queue.empty()) {
        OggPacket packet = m_packet_queue.front();
        m_packet_queue.pop();
        
        if (packet.stream_id == stream_id) {
            MediaChunk chunk;
            chunk.stream_id = stream_id;
            chunk.data = std::move(packet.data);
            chunk.timestamp_samples = packet.granule_position;
            chunk.timestamp_ms = granuleToMs(packet.granule_position, stream_id);
            chunk.is_keyframe = true; // Audio packets are typically always keyframes
            
            m_position_ms = chunk.timestamp_ms;
            return chunk;
        }
    }
    
    // Need to read more packets
    fillPacketQueue(stream_id);
    
    // Try again
    while (!m_packet_queue.empty()) {
        OggPacket packet = m_packet_queue.front();
        m_packet_queue.pop();
        
        if (packet.stream_id == stream_id) {
            MediaChunk chunk;
            chunk.stream_id = stream_id;
            chunk.data = std::move(packet.data);
            chunk.timestamp_samples = packet.granule_position;
            chunk.timestamp_ms = granuleToMs(packet.granule_position, stream_id);
            chunk.is_keyframe = true;
            
            m_position_ms = chunk.timestamp_ms;
            return chunk;
        }
    }
    
    m_eof = true;
    return MediaChunk{};
}

bool OggDemuxer::seekTo(uint64_t timestamp_ms) {
    // Find the target granule position for the best audio stream
    uint32_t target_stream = findBestAudioStream();
    if (target_stream == 0) {
        return false;
    }
    
    uint64_t target_granule = msToGranule(timestamp_ms, target_stream);
    
    // Clear packet queue
    while (!m_packet_queue.empty()) {
        m_packet_queue.pop();
    }
    
    // For simplicity, do a linear search for the target granule
    // A real implementation would use bisection search
    bool found = seekToPage(target_granule, target_stream);
    
    if (found) {
        m_position_ms = timestamp_ms;
        m_eof = false;
    }
    
    return found;
}

bool OggDemuxer::isEOF() const {
    return m_eof;
}

uint64_t OggDemuxer::getDuration() const {
    return m_duration_ms;
}

uint64_t OggDemuxer::getPosition() const {
    return m_position_ms;
}

bool OggDemuxer::readPageHeader(OggPageHeader& header) {
    if (m_current_offset + 27 > m_file_size) {
        return false;
    }
    
    m_handler->seek(m_current_offset, SEEK_SET);
    
    // Read and verify capture pattern
    if (m_handler->read(header.capture_pattern, 1, 4) != 4 ||
        std::memcmp(header.capture_pattern, "OggS", 4) != 0) {
        return false;
    }
    
    // Read rest of header
    header.version = Demuxer::readLE<uint8_t>();
    header.header_type = Demuxer::readLE<uint8_t>();
    header.granule_position = Demuxer::readLE<uint64_t>();
    header.serial_number = Demuxer::readLE<uint32_t>();
    header.page_sequence = Demuxer::readLE<uint32_t>();
    header.checksum = Demuxer::readLE<uint32_t>();
    header.page_segments = Demuxer::readLE<uint8_t>();
    
    return true;
}

bool OggDemuxer::readPageData(const OggPageHeader& header, std::vector<uint8_t>& page_data) {
    // Read segment table
    std::vector<uint8_t> segment_table(header.page_segments);
    if (m_handler->read(segment_table.data(), 1, header.page_segments) != header.page_segments) {
        return false;
    }
    
    // Calculate total page data size
    size_t total_size = 0;
    for (uint8_t segment_size : segment_table) {
        total_size += segment_size;
    }
    
    // Read page data
    page_data.resize(total_size);
    if (total_size > 0) {
        if (m_handler->read(page_data.data(), 1, total_size) != total_size) {
            return false;
        }
    }
    
    // Update current offset
    m_current_offset = m_handler->tell();
    
    return true;
}

std::vector<OggPacket> OggDemuxer::parsePackets(const OggPageHeader& header, 
                                               const std::vector<uint8_t>& page_data,
                                               const std::vector<uint8_t>& segment_table) {
    std::vector<OggPacket> packets;
    
    // For simplicity, treat the entire page as one packet
    // A complete implementation would properly parse segment boundaries
    if (!page_data.empty()) {
        OggPacket packet;
        packet.stream_id = header.serial_number;
        packet.data = page_data;
        packet.granule_position = header.granule_position;
        packet.is_first_packet = header.isFirstPage();
        packet.is_last_packet = header.isLastPage();
        packet.is_continued = header.isContinuedPacket();
        
        packets.push_back(packet);
    }
    
    return packets;
}

std::string OggDemuxer::identifyCodec(const std::vector<uint8_t>& packet_data) {
    if (hasSignature(packet_data, "\x01vorbis")) {
        return "vorbis";
    } else if (hasSignature(packet_data, "\x7f""FLAC")) {
        return "flac";
    } else if (hasSignature(packet_data, "OpusHead")) {
        return "opus";
    } else if (hasSignature(packet_data, "Speex   ")) {
        return "speex";
    }
    
    return "";
}

bool OggDemuxer::parseVorbisHeaders(OggStream& stream, const OggPacket& packet) {
    if (packet.data.size() < 7) {
        return false;
    }
    
    uint8_t packet_type = packet.data[0];
    
    if (packet_type == 1 && hasSignature(packet.data, "\x01vorbis")) {
        // Identification header
        if (packet.data.size() >= 30) {
            stream.channels = readLE<uint8_t>(packet.data, 11);
            stream.sample_rate = readLE<uint32_t>(packet.data, 12);
            stream.bitrate = readLE<uint32_t>(packet.data, 16); // Maximum bitrate
        }
        return true;
    } else if (packet_type == 3 && hasSignature(packet.data, "\x03vorbis")) {
        // Comment header - skip for now
        return true;
    } else if (packet_type == 5 && hasSignature(packet.data, "\x05vorbis")) {
        // Setup header
        stream.codec_setup_data = packet.data;
        return true;
    }
    
    return false;
}

bool OggDemuxer::parseFLACHeaders(OggStream& stream, const OggPacket& packet) {
    if (hasSignature(packet.data, "\x7f""FLAC")) {
        // Ogg FLAC identification header
        if (packet.data.size() >= 51) {
            // Skip Ogg FLAC header, parse embedded STREAMINFO
            size_t streaminfo_offset = 13; // After "\x7fFLAC" + version + header count + "fLaC" + STREAMINFO header
            
            if (packet.data.size() >= streaminfo_offset + 34) {
                // Parse STREAMINFO block
                uint32_t info1 = readBE<uint32_t>(packet.data, streaminfo_offset + 10);
                stream.sample_rate = (info1 >> 12) & 0xFFFFF;  // 20 bits
                stream.channels = ((info1 >> 9) & 0x7) + 1;    // 3 bits + 1
                
                // Extract total samples (36 bits, split across boundaries)
                uint64_t total_samples = ((uint64_t)(info1 & 0xF) << 32) | readBE<uint32_t>(packet.data, streaminfo_offset + 18);
                stream.total_samples = total_samples;
            }
        }
        return true;
    }
    
    return false;
}

bool OggDemuxer::parseOpusHeaders(OggStream& stream, const OggPacket& packet) {
    if (hasSignature(packet.data, "OpusHead")) {
        // Opus identification header
        if (packet.data.size() >= 19) {
            stream.channels = readLE<uint8_t>(packet.data, 9);
            // Note: Opus always runs at 48kHz internally, but can be presented at other rates
            stream.sample_rate = 48000;
        }
        return true;
    } else if (hasSignature(packet.data, "OpusTags")) {
        // Opus comment header
        return true;
    }
    
    return false;
}

bool OggDemuxer::parseSpeexHeaders(OggStream& stream, const OggPacket& packet) {
    if (hasSignature(packet.data, "Speex   ")) {
        // Speex header
        if (packet.data.size() >= 80) {
            stream.sample_rate = readLE<uint32_t>(packet.data, 36);
            stream.channels = readLE<uint32_t>(packet.data, 48);
        }
        return true;
    }
    
    return false;
}

void OggDemuxer::calculateDuration() {
    m_duration_ms = 0;
    
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.sample_rate > 0) {
            uint64_t stream_duration = (stream.total_samples * 1000ULL) / stream.sample_rate;
            m_duration_ms = std::max(m_duration_ms, stream_duration);
        }
    }
}

uint64_t OggDemuxer::granuleToMs(uint64_t granule, uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it == m_streams.end() || it->second.sample_rate == 0) {
        return 0;
    }
    
    return (granule * 1000ULL) / it->second.sample_rate;
}

uint64_t OggDemuxer::msToGranule(uint64_t timestamp_ms, uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it == m_streams.end() || it->second.sample_rate == 0) {
        return 0;
    }
    
    return (timestamp_ms * it->second.sample_rate) / 1000ULL;
}

uint32_t OggDemuxer::findBestAudioStream() const {
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio") {
            return stream_id;
        }
    }
    return 0;
}

bool OggDemuxer::seekToPage(uint64_t target_granule, uint32_t stream_id) {
    // Simple linear search - a real implementation would use binary search
    m_handler->seek(0, SEEK_SET);
    m_current_offset = 0;
    
    while (m_current_offset < m_file_size) {
        OggPageHeader header;
        if (!readPageHeader(header)) {
            break;
        }
        
        if (header.serial_number == stream_id && header.granule_position >= target_granule) {
            // Found target page
            m_handler->seek(m_current_offset, SEEK_SET);
            return true;
        }
        
        // Skip this page
        std::vector<uint8_t> dummy_data;
        if (!readPageData(header, dummy_data)) {
            break;
        }
    }
    
    return false;
}

void OggDemuxer::fillPacketQueue(uint32_t target_stream_id) {
    constexpr int MAX_PAGES = 10; // Limit to prevent infinite loops
    int pages_read = 0;
    
    while (pages_read < MAX_PAGES && m_current_offset < m_file_size) {
        OggPageHeader header;
        if (!readPageHeader(header)) {
            break;
        }
        
        std::vector<uint8_t> page_data;
        if (!readPageData(header, page_data)) {
            break;
        }
        
        std::vector<uint8_t> segment_table; // Should be filled properly
        auto packets = parsePackets(header, page_data, segment_table);
        
        for (auto& packet : packets) {
            m_packet_queue.push(std::move(packet));
        }
        
        pages_read++;
        
        // Check if we got packets for the target stream
        bool found_target = false;
        auto queue_copy = m_packet_queue;
        while (!queue_copy.empty()) {
            if (queue_copy.front().stream_id == target_stream_id) {
                found_target = true;
                break;
            }
            queue_copy.pop();
        }
        
        if (found_target) {
            break;
        }
    }
}

bool OggDemuxer::hasSignature(const std::vector<uint8_t>& data, const char* signature) {
    size_t sig_len = std::strlen(signature);
    if (data.size() < sig_len) {
        return false;
    }
    
    return std::memcmp(data.data(), signature, sig_len) == 0;
}
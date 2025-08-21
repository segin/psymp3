/*
 * OggDemuxer.cpp - Ogg container demuxer implementation using libogg
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// OggDemuxer is built if HAVE_OGGDEMUXER is defined
#ifdef HAVE_OGGDEMUXER

OggDemuxer::OggDemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)) {
    Debug::log("loader", "OggDemuxer constructor called - initializing libogg sync state");
    Debug::log("ogg", "OggDemuxer constructor called - initializing libogg sync state");
    Debug::log("demuxer", "OggDemuxer constructor called - initializing libogg sync state");
    // Initialize libogg sync state
    int sync_result = ogg_sync_init(&m_sync_state);
    if (sync_result != 0) {
        Debug::log("loader", "OggDemuxer: ogg_sync_init failed with code: ", sync_result);
        Debug::log("ogg", "OggDemuxer: ogg_sync_init failed with code: ", sync_result);
        Debug::log("demuxer", "OggDemuxer: ogg_sync_init failed with code: ", sync_result);
        throw std::runtime_error("Failed to initialize ogg sync state");
    }
    Debug::log("loader", "OggDemuxer constructor completed successfully");
    Debug::log("ogg", "OggDemuxer constructor completed successfully");
    Debug::log("demuxer", "OggDemuxer constructor completed successfully");
}

OggDemuxer::~OggDemuxer() {
    // Clean up libogg streams
    for (auto& [stream_id, ogg_stream] : m_ogg_streams) {
        ogg_stream_clear(&ogg_stream);
    }
    
    // Clean up sync state
    ogg_sync_clear(&m_sync_state);
}

bool OggDemuxer::parseContainer() {
    Debug::log("loader", "OggDemuxer::parseContainer called");
    Debug::log("ogg", "OggDemuxer::parseContainer called");
    Debug::log("demuxer", "OggDemuxer::parseContainer called");
    
    if (isParsed()) {
        Debug::log("loader", "OggDemuxer::parseContainer already parsed, returning true");
        Debug::log("ogg", "OggDemuxer::parseContainer already parsed, returning true");
        return true;
    }
    
    try {
        // Get file size using IOHandler's getFileSize() method
        m_file_size = m_handler->getFileSize();
        if (m_file_size <= 0) {
            m_file_size = 0;
        }
        
        Debug::log("loader", "OggDemuxer: File size detection using getFileSize() - file_size=", m_file_size, " (hex=0x", std::hex, m_file_size, std::dec, ")");
        Debug::log("ogg", "OggDemuxer: File size detection using getFileSize() - file_size=", m_file_size, " (hex=0x", std::hex, m_file_size, std::dec, ")");
        Debug::log("demuxer", "OggDemuxer: File size detection using getFileSize() - file_size=", m_file_size, " (hex=0x", std::hex, m_file_size, std::dec, ")");
        
        // Log the actual file position and bytes read when the problem occurs
        Debug::log("loader", "OggDemuxer: Initial m_file_size in parseContainer: ", m_file_size);
        Debug::log("ogg", "OggDemuxer: Initial m_file_size in parseContainer: ", m_file_size);
        Debug::log("demuxer", "OggDemuxer: Initial m_file_size in parseContainer: ", m_file_size);
        
        // Read initial data to parse headers with error recovery
        bool initial_read_success = performIOWithRetry([this]() {
            return readIntoSyncBuffer(8192);
        }, "initial header read");
        
        if (!initial_read_success) {
            reportError("IO", "Failed to read initial Ogg data for header parsing");
            return false;
        }
        
        // Parse headers using libogg
        bool all_headers_complete = false;
        int max_pages = 500; // Increased limit to find all headers
        int consecutive_no_progress = 0;
        
        while (!all_headers_complete && max_pages-- > 0) {
            bool made_progress = false;
            
            try {
                made_progress = processPages();
            } catch (const std::exception& e) {
                reportError("Processing", "Exception processing pages during header parsing: " + std::string(e.what()));
                
                // Try to recover by synchronizing to next page boundary
                if (synchronizeToPageBoundary()) {
                    consecutive_no_progress++;
                    if (consecutive_no_progress > 5) {
                        Debug::log("ogg", "OggDemuxer: Too many sync attempts, enabling fallback mode");
                        enableFallbackMode();
                    }
                    continue;
                } else {
                    break;
                }
            }
            
            if (!made_progress) {
                // Need more data
                bool read_success = performIOWithRetry([this]() {
                    return readIntoSyncBuffer(4096);
                }, "header parsing data read");
                
                if (!read_success) {
                    consecutive_no_progress++;
                    if (consecutive_no_progress > 10) {
                        Debug::log("ogg", "OggDemuxer: No progress for 10 iterations, stopping header parsing");
                        break;
                    }
                } else {
                    consecutive_no_progress = 0;
                }
                continue;
            }
            
            consecutive_no_progress = 0;
            
            // Check if all audio streams have complete headers
            all_headers_complete = true;
            for (const auto& [id, stream] : m_streams) {
                if (stream.codec_type == "audio" && !stream.headers_complete) {
                    all_headers_complete = false;
                    Debug::log("ogg", "OggDemuxer: Stream ", id, " codec=", stream.codec_name, 
                                       " headers not complete, header_count=", stream.header_packets.size());
                    break;
                }
            }
            
            if (all_headers_complete) {
                Debug::log("ogg", "OggDemuxer: All headers complete, stopping header parsing");
            }
        }
        
        calculateDuration();
        
        // Rewind to beginning of file for sequential packet reading
        m_handler->seek(0, SEEK_SET);
        
        // Clear sync state and reinitialize for sequential reading
        ogg_sync_clear(&m_sync_state);
        ogg_sync_init(&m_sync_state);
        
        // Reset all stream states for sequential reading
        for (auto& [stream_id, ogg_stream] : m_ogg_streams) {
            ogg_stream_clear(&ogg_stream);
            ogg_stream_init(&ogg_stream, stream_id);
        }
        
        Debug::log("ogg", "OggDemuxer: Headers parsed, rewound to beginning for sequential packet reading");
        
        setParsed(true);
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "Error parsing container: ", e.what());
        return false;
    }
}

bool OggDemuxer::readIntoSyncBuffer(size_t bytes) {
    try {
        // Validate input parameters
        if (bytes == 0 || bytes > 1048576) { // Limit to 1MB per read
            Debug::log("ogg", "OggDemuxer: Invalid buffer size requested: ", bytes);
            return false;
        }
        
        // Thread-safe access to libogg sync state and I/O operations
        std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
        std::lock_guard<std::mutex> io_lock(m_io_mutex);
        
        char* buffer = ogg_sync_buffer(&m_sync_state, static_cast<long>(bytes));
        if (!buffer) {
            Debug::log("ogg", "OggDemuxer: ogg_sync_buffer failed - out of memory or invalid sync state");
            return false;
        }
        
        off_t current_pos_before = m_handler->tell();
        if (current_pos_before < 0) {
            Debug::log("ogg", "OggDemuxer: Invalid file position before read");
            return false;
        }
        
        long bytes_read = m_handler->read(buffer, 1, bytes);
        
        Debug::log("ogg", "OggDemuxer: readIntoSyncBuffer at ", m_position_ms, "ms - pos_before=", current_pos_before, 
                       ", requested=", bytes, ", bytes_read=", bytes_read);
        
        if (bytes_read < 0) {
            Debug::log("ogg", "OggDemuxer: I/O error during read: ", bytes_read);
            return false;
        }
        
        if (bytes_read == 0) {
            // Check if we're at EOF or if this is an I/O error
            if (m_handler->eof()) {
                Debug::log("ogg", "OggDemuxer: Reached end of file");
                setEOF(true);
            } else {
                Debug::log("ogg", "OggDemuxer: No data available (temporary)");
            }
            return false;
        }
        
        // Validate that bytes_read doesn't exceed what we requested
        if (bytes_read > static_cast<long>(bytes)) {
            Debug::log("ogg", "OggDemuxer: Read more bytes than requested - potential buffer overflow");
            bytes_read = static_cast<long>(bytes);
        }
        
        int sync_result = ogg_sync_wrote(&m_sync_state, bytes_read);
        if (sync_result != 0) {
            Debug::log("ogg", "OggDemuxer: ogg_sync_wrote failed with code: ", sync_result);
            // Try to recover by resetting sync state
            ogg_sync_reset(&m_sync_state);
            return false;
        }
        
        // Log file position advancement around the problem area
        long pos_after_write = m_handler->tell();
        Debug::log("ogg", "OggDemuxer: readIntoSyncBuffer success - read ", bytes_read, " bytes, pos advanced from ", 
                       current_pos_before, " to ", pos_after_write);
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in readIntoSyncBuffer: ", e.what());
        return false;
    }
}

bool OggDemuxer::processPages() {
    ogg_page page;
    bool processed_any_pages = false;
    
    try {
        // Thread-safe access to libogg sync state
        std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
        
        while (ogg_sync_pageout(&m_sync_state, &page) == 1) {
            processed_any_pages = true;
            
            // Validate page structure before accessing any fields
            if (!validateOggPage(&page)) {
                Debug::log("ogg", "OggDemuxer: Invalid page structure, skipping page");
                continue;
            }
            
            uint32_t stream_id = ogg_page_serialno(&page);
            
            // Validate stream ID
            if (stream_id == 0) {
                Debug::log("ogg", "OggDemuxer: Invalid stream ID 0, skipping page");
                continue;
            }
            
            // Initialize stream if it's new
            if (m_streams.find(stream_id) == m_streams.end()) {
                OggStream stream;
                stream.serial_number = stream_id;
                m_streams[stream_id] = stream;
                
                // Initialize libogg stream state
                if (ogg_stream_init(&m_ogg_streams[stream_id], stream_id) != 0) {
                    Debug::log("ogg", "OggDemuxer: Failed to initialize ogg stream for ID ", stream_id);
                    m_streams.erase(stream_id);
                    continue;
                }
            }
            
            // Add page to stream with error handling
            int pagein_result = ogg_stream_pagein(&m_ogg_streams[stream_id], &page);
            if (pagein_result != 0) {
                Debug::log("ogg", "OggDemuxer: Failed to add page to stream ", stream_id, ", error code: ", pagein_result);
                continue;
            }
        
            // Extract packets from this stream
            ogg_packet packet;
            int packet_result;
            while ((packet_result = ogg_stream_packetout(&m_ogg_streams[stream_id], &packet)) == 1) {
                // Validate packet structure
                if (!validateOggPacket(&packet, stream_id)) {
                    Debug::log("ogg", "OggDemuxer: Invalid packet from stream ", stream_id, ", skipping");
                    continue;
                }
                
                Debug::log("ogg", "OggDemuxer: Extracted packet from stream ", stream_id, ", size=", packet.bytes);
                OggStream& stream = m_streams[stream_id];
                
                if (!stream.headers_complete) {
                    // Try to identify codec from first packet
                    if (stream.codec_name.empty()) {
                        try {
                            std::vector<uint8_t> packet_data(packet.packet, packet.packet + packet.bytes);
                            stream.codec_name = identifyCodec(packet_data);
                            stream.codec_type = stream.codec_name.empty() ? "unknown" : "audio";
                            
                            if (stream.codec_name.empty()) {
                                Debug::log("ogg", "OggDemuxer: Could not identify codec for stream ", stream_id);
                            }
                        } catch (const std::exception& e) {
                            Debug::log("ogg", "OggDemuxer: Exception identifying codec: ", e.what());
                            continue;
                        }
                    }
                    
                    // Parse codec-specific headers
                    try {
                        OggPacket ogg_packet;
                        ogg_packet.stream_id = stream_id;
                        ogg_packet.data.assign(packet.packet, packet.packet + packet.bytes);
                        ogg_packet.granule_position = packet.granulepos;
                        ogg_packet.is_first_packet = packet.b_o_s;
                        ogg_packet.is_last_packet = packet.e_o_s;
                
                        Debug::log("ogg", "OggDemuxer: Processing packet for stream ", stream_id, ", codec=", stream.codec_name, 
                                       ", packet_size=", packet.bytes, ", headers_complete=", stream.headers_complete);
                        
                        bool is_header_packet = false;
                        try {
#ifdef HAVE_VORBIS
                            if (stream.codec_name == "vorbis") {
                                is_header_packet = parseVorbisHeaders(stream, ogg_packet);
                            } else
#endif
#ifdef HAVE_FLAC
                            if (stream.codec_name == "flac") {
                                is_header_packet = parseFLACHeaders(stream, ogg_packet);
                            } else
#endif
#ifdef HAVE_OPUS
                            if (stream.codec_name == "opus") {
                                is_header_packet = parseOpusHeaders(stream, ogg_packet);
                                Debug::log("ogg", "OggDemuxer: parseOpusHeaders returned ", is_header_packet, " for packet size ", ogg_packet.data.size());
                            } else
#endif
                            if (stream.codec_name == "speex") {
                                is_header_packet = parseSpeexHeaders(stream, ogg_packet);
                            } else {
                                Debug::log("ogg", "OggDemuxer: Unknown codec ", stream.codec_name, " for stream ", stream_id);
                            }
                        } catch (const std::exception& e) {
                            Debug::log("ogg", "OggDemuxer: Exception parsing headers for codec ", stream.codec_name, ": ", e.what());
                            is_header_packet = false;
                        }
                
                // Only add to header packets if it was actually a valid header
                if (is_header_packet) {
                    stream.header_packets.push_back(ogg_packet);
                    
                    Debug::log("ogg", "OggDemuxer: Added header packet ", stream.header_packets.size(), " for stream ", stream_id, ", codec=", stream.codec_name);
                    
                    // Check if headers are complete (codec-specific logic)
                    if (stream.codec_name == "vorbis" && stream.header_packets.size() >= 3) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "flac" && stream.header_packets.size() >= 1) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "opus" && stream.header_packets.size() >= 2) {
                        stream.headers_complete = true;
                        Debug::log("ogg", "OggDemuxer: Opus headers complete for stream ", stream_id, ", header_count=", stream.header_packets.size());
                    } else if (stream.codec_name == "opus") {
                        Debug::log("ogg", "OggDemuxer: Opus headers still incomplete for stream ", stream_id, ", header_count=", stream.header_packets.size(), " (need 2)");
                    }
                    
                    Debug::log("ogg", "OggDemuxer: Stream ", stream_id, " now has ", stream.header_packets.size(), " header packets, codec=", stream.codec_name, ", headers_complete=", stream.headers_complete);
                } else {
                    // If this packet wasn't a header but we haven't completed headers yet,
                    // this might be an audio packet that came before we finished parsing headers.
                    // Queue it for later processing after headers are complete.
                    
                    Debug::log("ogg", "OggDemuxer: Non-header packet during header parsing, stream_id=", stream_id, 
                                       ", packet_size=", packet.bytes, ", header_packets_count=", stream.header_packets.size());
                    
                    // Queue the packet for later processing
                    stream.m_packet_queue.push_back(ogg_packet);
                    }
                    
                    } catch (const std::exception& e) {
                        Debug::log("ogg", "OggDemuxer: Exception creating OggPacket: ", e.what());
                        continue;
                    }
                } else {
                    // Headers are complete, queue the packet for normal processing
                    try {
                        stream.m_packet_queue.push_back(OggPacket{stream_id, 
                                                                  std::vector<uint8_t>(packet.packet, packet.packet + packet.bytes), 
                                                                  static_cast<uint64_t>(packet.granulepos), 
                                                                  (bool)packet.b_o_s, 
                                                                  (bool)packet.e_o_s});
                    } catch (const std::exception& e) {
                        Debug::log("ogg", "OggDemuxer: Exception queuing packet: ", e.what());
                        continue;
                    }
                }
            }
        }
        
        return processed_any_pages;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in processPages: ", e.what());
        return false;
    }
}

std::vector<StreamInfo> OggDemuxer::getStreams() const {
    std::vector<StreamInfo> streams;
    
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.headers_complete) {
            StreamInfo info;
            info.stream_id = stream_id;
            info.codec_name = stream.codec_name;
            info.codec_type = stream.codec_type;
            info.sample_rate = stream.sample_rate;
            info.channels = stream.channels;
            info.bitrate = stream.bitrate;
            info.artist = stream.artist;
            info.title = stream.title;
            info.album = stream.album;
            info.duration_ms = m_duration_ms;
            info.duration_samples = stream.total_samples;
            
            // Populate codec_data with header packets for decoder initialization
            if (!stream.header_packets.empty()) {
                // For Ogg codecs, we need to provide all header packets
                // Concatenate all header packets with length prefixes
                for (const auto& header_packet : stream.header_packets) {
                    // Add 4-byte length prefix (little-endian)
                    uint32_t packet_size = static_cast<uint32_t>(header_packet.data.size());
                    info.codec_data.push_back(packet_size & 0xFF);
                    info.codec_data.push_back((packet_size >> 8) & 0xFF);
                    info.codec_data.push_back((packet_size >> 16) & 0xFF);
                    info.codec_data.push_back((packet_size >> 24) & 0xFF);
                    
                    // Add packet data
                    info.codec_data.insert(info.codec_data.end(), 
                                         header_packet.data.begin(), 
                                         header_packet.data.end());
                }
            }
            
            // Set codec tag based on codec type
            if (stream.codec_name == "vorbis") {
                info.codec_tag = 0x566F7262; // "Vorb"
            } else if (stream.codec_name == "opus") {
                info.codec_tag = 0x4F707573; // "Opus"
            } else if (stream.codec_name == "flac") {
                info.codec_tag = 0x664C6143; // "fLaC"
            } else if (stream.codec_name == "speex") {
                info.codec_tag = 0x53706578; // "Spex"
            }
            
            streams.push_back(info);
        }
    }
    
    return streams;
}

StreamInfo OggDemuxer::getStreamInfo(uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it != m_streams.end() && it->second.headers_complete) {
        const OggStream& stream = it->second;
        StreamInfo info;
        info.stream_id = stream_id;
        info.codec_name = stream.codec_name;
        info.codec_type = stream.codec_type;
        info.sample_rate = stream.sample_rate;
        info.channels = stream.channels;
        info.bitrate = stream.bitrate;
        info.artist = stream.artist;
        info.title = stream.title;
        info.album = stream.album;
        info.duration_ms = m_duration_ms;
        info.duration_samples = stream.total_samples;
        
        // Populate codec_data with header packets for decoder initialization
        if (!stream.header_packets.empty()) {
            // For Ogg codecs, we need to provide all header packets
            // Concatenate all header packets with length prefixes
            for (const auto& header_packet : stream.header_packets) {
                // Add 4-byte length prefix (little-endian)
                uint32_t packet_size = static_cast<uint32_t>(header_packet.data.size());
                info.codec_data.push_back(packet_size & 0xFF);
                info.codec_data.push_back((packet_size >> 8) & 0xFF);
                info.codec_data.push_back((packet_size >> 16) & 0xFF);
                info.codec_data.push_back((packet_size >> 24) & 0xFF);
                
                // Add packet data
                info.codec_data.insert(info.codec_data.end(), 
                                     header_packet.data.begin(), 
                                     header_packet.data.end());
            }
        }
        
        // Set codec tag based on codec type
        if (stream.codec_name == "vorbis") {
            info.codec_tag = 0x566F7262; // "Vorb"
        } else if (stream.codec_name == "opus") {
            info.codec_tag = 0x4F707573; // "Opus"
        } else if (stream.codec_name == "flac") {
            info.codec_tag = 0x664C6143; // "fLaC"
        } else if (stream.codec_name == "speex") {
            info.codec_tag = 0x53706578; // "Spex"
        }
        
        return info;
    }
    
    return StreamInfo{};
}

MediaChunk OggDemuxer::readChunk() {
    // Read from the best audio stream
    uint32_t stream_id = findBestAudioStream();
    if (stream_id == 0) {
        m_eof = true;
        return MediaChunk{};
    }
    
    return readChunk(stream_id);
}

MediaChunk OggDemuxer::readChunk(uint32_t stream_id) {
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        reportError("Stream", "Stream ID " + std::to_string(stream_id) + " not found");
        m_eof = true;
        return MediaChunk{};
    }
    
    // Check if stream is corrupted
    if (m_corrupted_streams.find(stream_id) != m_corrupted_streams.end()) {
        reportError("Stream", "Stream " + std::to_string(stream_id) + " is corrupted");
        m_eof = true;
        return MediaChunk{};
    }
    
    OggStream& stream = stream_it->second;
    
    // Validate stream state before reading
    if (!validateAndRepairStreamState(stream_id)) {
        if (isolateStreamError(stream_id, "readChunk validation failed")) {
            // Try to find another valid stream
            uint32_t fallback_stream = findBestAudioStream();
            if (fallback_stream != 0 && fallback_stream != stream_id) {
                Debug::log("ogg", "OggDemuxer: Falling back to stream ", fallback_stream, " after stream ", stream_id, " failed");
                return readChunk(fallback_stream);
            }
        }
        m_eof = true;
        return MediaChunk{};
    }

    // First, send header packets if they haven't been sent yet
    if (!stream.headers_sent && stream.headers_complete) {
        if (stream.next_header_index < stream.header_packets.size()) {
            const OggPacket& header_packet = stream.header_packets[stream.next_header_index];
            stream.next_header_index++;
            
            MediaChunk chunk;
            chunk.stream_id = stream_id;
            chunk.data = header_packet.data;
            chunk.granule_position = 0;
            chunk.is_keyframe = true;
            
            if (stream.next_header_index >= stream.header_packets.size()) {
                stream.headers_sent = true;
            }
            return chunk;
        }
    }

    // If the queue is empty, read more data until we get a packet for our stream
    int retry_count = 0;
    const int max_retries = 3;
    
    bool queue_empty;
    {
        std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
        queue_empty = stream.m_packet_queue.empty();
    }
    
    while (queue_empty && !isEOFAtomic() && retry_count < max_retries) {
        bool read_success = performIOWithRetry([this]() {
            return readIntoSyncBuffer(4096);
        }, "reading Ogg data for packet queue");
        
        if (!read_success) {
            if (m_fallback_mode || retry_count >= max_retries - 1) {
                setEOF(true);
                break;
            }
            
            // Try to recover from I/O error
            long current_pos;
            {
                std::lock_guard<std::mutex> lock(m_io_mutex);
                current_pos = m_handler->tell();
            }
            if (recoverFromCorruptedPage(current_pos)) {
                retry_count++;
                continue;
            } else {
                setEOF(true);
                break;
            }
        }
        
        try {
            if (!processPages()) {
                // Processing failed, try to synchronize
                if (synchronizeToPageBoundary()) {
                    retry_count++;
                    continue;
                } else {
                    setEOF(true);
                    break;
                }
            }
        } catch (const std::exception& e) {
            reportError("Processing", "Exception processing Ogg pages: " + std::string(e.what()));
            if (isolateStreamError(stream_id, "processPages exception")) {
                retry_count++;
                continue;
            } else {
                setEOF(true);
                break;
            }
        }
        
        retry_count = 0; // Reset retry count on successful processing
        
        // Check queue status again
        {
            std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
            queue_empty = stream.m_packet_queue.empty();
        }
    }

    // If we still have no packets after trying to read, we are at the end
    {
        std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
        if (stream.m_packet_queue.empty()) {
            setEOF(true);
            return MediaChunk{};
        }
    }

    // Pop the next packet from the queue (thread-safe)
    MediaChunk chunk;
    {
        std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
        const OggPacket& ogg_packet = stream.m_packet_queue.front();
        
        chunk.stream_id = stream_id;
        chunk.data = ogg_packet.data;
        chunk.granule_position = ogg_packet.granule_position;

        stream.m_packet_queue.pop_front();
    }
    
    // Update our internal position tracker for getPosition() (thread-safe)
    if (chunk.granule_position != static_cast<uint64_t>(-1)) {
        uint64_t current_ms = granuleToMs(chunk.granule_position, stream_id);
        {
            std::lock_guard<std::mutex> lock(m_state_mutex);
            if (current_ms > m_position_ms) {
                m_position_ms = current_ms;
            }
        }
    }

    return chunk;
}

bool OggDemuxer::seekTo(uint64_t timestamp_ms) {
    // Find the best audio stream to use for seeking
    uint32_t stream_id = findBestAudioStream();
    if (stream_id == 0) {
        reportError("Seeking", "No valid audio stream found for seeking");
        return false;
    }
    
    // Validate seek timestamp (thread-safe)
    uint64_t current_duration;
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        current_duration = m_duration_ms;
    }
    
    if (timestamp_ms > current_duration && current_duration > 0) {
        uint64_t clamped_timestamp;
        if (handleSeekingError(timestamp_ms, clamped_timestamp)) {
            timestamp_ms = clamped_timestamp;
        } else {
            return false;
        }
    }

    // Clear all stream packet queues (thread-safe)
    {
        std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
        for (auto& [id, stream] : m_streams) {
            stream.m_packet_queue.clear();
        }
    }
    
    // If seeking to beginning, do it directly
    if (timestamp_ms == 0) {
        {
            std::lock_guard<std::mutex> io_lock(m_io_mutex);
            m_handler->seek(0, SEEK_SET);
        }
        
        {
            std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
            ogg_sync_reset(&m_sync_state);
            
            for (auto& [id, ogg_stream] : m_ogg_streams) {
                ogg_stream_reset(&ogg_stream);
            }
        }
        
        // Do NOT resend headers after seeking - decoder state should be maintained
        // Headers are only sent once at the beginning of playback
        for (auto& [id, stream] : m_streams) {
            stream.total_samples_processed = 0;
        }
        
        updatePosition(0);
        setEOF(false);
        return true;
    }
    
    // Convert timestamp to granule position for seeking
    uint64_t target_granule = msToGranule(timestamp_ms, stream_id);
    
    // Use bisection search to find the target position with error recovery
    bool success = false;
    try {
        success = performIOWithRetry([this, target_granule, stream_id]() {
            return seekToPage(target_granule, stream_id);
        }, "seeking to Ogg page");
        
        if (success) {
            // Update sample tracking for the target stream
            // Note: We do NOT resend headers after seeking - the decoder should
            // maintain its state and only needs new audio data packets.
            // Headers are only sent once at the beginning of playback.
            OggStream& stream = m_streams[stream_id];
            stream.total_samples_processed = target_granule;
        } else {
            // Try fallback seeking strategies
            uint64_t clamped_timestamp;
            if (handleSeekingError(timestamp_ms, clamped_timestamp)) {
                success = true;
                timestamp_ms = clamped_timestamp;
            }
        }
    } catch (const std::exception& e) {
        reportError("Seeking", "Exception during seek operation: " + std::string(e.what()));
        
        // Try to recover by seeking to beginning
        if (timestamp_ms != 0) {
            Debug::log("ogg", "OggDemuxer: Seek failed, attempting fallback to beginning");
            success = seekTo(0);
        }
    }
    
    return success;
}

bool OggDemuxer::isEOF() const {
    return isEOFAtomic();
}

uint64_t OggDemuxer::getDuration() const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_duration_ms;
}

uint64_t OggDemuxer::getPosition() const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_position_ms;
}

uint64_t OggDemuxer::getGranulePosition(uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it != m_streams.end()) {
        return it->second.total_samples_processed;
    }
    return 0;
}

// Static function implementation
bool OggDemuxer::hasSignature(const std::vector<uint8_t>& data, const char* signature) {
    size_t sig_len = std::strlen(signature);
    if (data.size() < sig_len) {
        return false;
    }
    
    return std::memcmp(data.data(), signature, sig_len) == 0;
}

bool OggDemuxer::validateOggPage(const ogg_page* page) {
    if (!page) {
        Debug::log("ogg", "OggDemuxer: validateOggPage - null page pointer");
        return false;
    }
    
    // Check if page header is accessible
    if (!page->header || page->header_len < 27) {
        Debug::log("ogg", "OggDemuxer: validateOggPage - invalid header pointer or length");
        return false;
    }
    
    // Validate OggS capture pattern
    if (page->header[0] != 'O' || page->header[1] != 'g' || 
        page->header[2] != 'g' || page->header[3] != 'S') {
        Debug::log("ogg", "OggDemuxer: validateOggPage - invalid capture pattern");
        return false;
    }
    
    // Check version (should be 0)
    if (page->header[4] != 0) {
        Debug::log("ogg", "OggDemuxer: validateOggPage - unsupported version: ", static_cast<int>(page->header[4]));
        return false;
    }
    
    // Validate page body
    if (page->body_len > 0 && !page->body) {
        Debug::log("ogg", "OggDemuxer: validateOggPage - invalid body pointer with non-zero length");
        return false;
    }
    
    // Check segment count and validate against header length
    uint8_t segments = page->header[26];
    if (page->header_len != 27 + segments) {
        Debug::log("ogg", "OggDemuxer: validateOggPage - header length mismatch: expected ", 27 + segments, ", got ", page->header_len);
        return false;
    }
    
    // Validate that body length matches sum of segment lengths
    if (segments > 0) {
        long expected_body_len = 0;
        for (int i = 0; i < segments; i++) {
            expected_body_len += page->header[27 + i];
        }
        if (expected_body_len != page->body_len) {
            Debug::log("ogg", "OggDemuxer: validateOggPage - body length mismatch: expected ", expected_body_len, ", got ", page->body_len);
            return false;
        }
    }
    
    return true;
}

bool OggDemuxer::validateOggPacket(const ogg_packet* packet, uint32_t stream_id) {
    if (!packet) {
        Debug::log("ogg", "OggDemuxer: validateOggPacket - null packet pointer for stream ", stream_id);
        return false;
    }
    
    // Check packet data pointer and size
    if (packet->bytes <= 0) {
        Debug::log("ogg", "OggDemuxer: validateOggPacket - invalid packet size: ", packet->bytes, " for stream ", stream_id);
        return false;
    }
    
    if (!packet->packet) {
        Debug::log("ogg", "OggDemuxer: validateOggPacket - null packet data pointer for stream ", stream_id);
        return false;
    }
    
    // Reasonable size limit (10MB per packet should be more than enough for audio)
    if (packet->bytes > 10485760) {
        Debug::log("ogg", "OggDemuxer: validateOggPacket - packet too large: ", packet->bytes, " bytes for stream ", stream_id);
        return false;
    }
    
    // Validate granule position (should not be invalid unless it's a header packet)
    if (packet->granulepos < -1) {
        Debug::log("ogg", "OggDemuxer: validateOggPacket - invalid granule position: ", packet->granulepos, " for stream ", stream_id);
        return false;
    }
    
    return true;
}

std::string OggDemuxer::identifyCodec(const std::vector<uint8_t>& packet_data) {
#ifdef HAVE_VORBIS
    if (OggDemuxer::hasSignature(packet_data, "\x01vorbis")) {
        return "vorbis";
    }
#endif

#ifdef HAVE_FLAC
    if (OggDemuxer::hasSignature(packet_data, "\x7f""FLAC")) {
        return "flac";
    }
#endif

#ifdef HAVE_OPUS
    if (OggDemuxer::hasSignature(packet_data, "OpusHead")) {
        return "opus";
    }
#endif

    // Speex is not conditionally compiled in this implementation
    if (OggDemuxer::hasSignature(packet_data, "Speex   ")) {
        return "speex";
    }
    
    return "";
}


#ifdef HAVE_VORBIS
bool OggDemuxer::parseVorbisHeaders(OggStream& stream, const OggPacket& packet) {
    if (packet.data.size() < 7) {
        return false;
    }
    
    uint8_t packet_type = packet.data[0];
    
    if (packet_type == 1 && OggDemuxer::hasSignature(packet.data, "\x01vorbis")) {
        // Identification header
        if (packet.data.size() >= 30) {
            stream.channels = OggDemuxer::readLE<uint8_t>(packet.data, 11);
            stream.sample_rate = OggDemuxer::readLE<uint32_t>(packet.data, 12);
            stream.bitrate = OggDemuxer::readLE<uint32_t>(packet.data, 16); // Maximum bitrate
        }
        return true;
    } else if (packet_type == 3 && OggDemuxer::hasSignature(packet.data, "\x03vorbis")) {
        // Comment header
        parseVorbisComments(stream, packet);
        return true;
    } else if (packet_type == 5 && OggDemuxer::hasSignature(packet.data, "\x05vorbis")) {
        // Setup header
        stream.codec_setup_data = packet.data;
        return true;
    }
    
    return false;
}
#endif // HAVE_VORBIS

#ifdef HAVE_VORBIS
void OggDemuxer::parseVorbisComments(OggStream& stream, const OggPacket& packet)
{
    // Vorbis comment header format:
    // 7 bytes: "\x03vorbis"
    // 4 bytes: vendor_length (little-endian)
    // vendor_length bytes: vendor string
    // 4 bytes: user_comment_list_length (little-endian)
    // For each comment:
    //   4 bytes: length (little-endian)
    //   length bytes: comment in UTF-8 format "FIELD=value"

    if (packet.data.size() < 15) { // 7 + 4 + 4
        return; // Too small
    }

    size_t offset = 7; // Skip "\x03vorbis"

    // Read vendor string length
    uint32_t vendor_length = readLE<uint32_t>(packet.data, offset);
    offset += 4;

    if (offset + vendor_length > packet.data.size()) {
        return; // Invalid vendor length
    }

    offset += vendor_length; // Skip vendor string

    if (offset + 4 > packet.data.size()) {
        return; // No room for comment count
    }

    // Read number of user comments
    uint32_t comment_count = readLE<uint32_t>(packet.data, offset);
    offset += 4;

    // Parse each comment
    for (uint32_t i = 0; i < comment_count && offset < packet.data.size(); i++) {
        if (offset + 4 > packet.data.size()) {
            break;
        }

        uint32_t comment_length = readLE<uint32_t>(packet.data, offset);
        offset += 4;

        if (offset + comment_length > packet.data.size()) {
            break;
        }

        // Extract comment string
        std::string comment(reinterpret_cast<const char*>(packet.data.data() + offset), comment_length);
        offset += comment_length;

        // Parse FIELD=value format
        size_t equals_pos = comment.find('=');
        if (equals_pos != std::string::npos) {
            std::string field = comment.substr(0, equals_pos);
            std::string value = comment.substr(equals_pos + 1);

            // Convert field to uppercase for case-insensitive comparison
            std::transform(field.begin(), field.end(), field.begin(), ::toupper);

            if (field == "ARTIST") {
                stream.artist = value;
            } else if (field == "TITLE") {
                stream.title = value;
            } else if (field == "ALBUM") {
                stream.album = value;
            }

            if (field == "METADATA_BLOCK_PICTURE") {
                Debug::log("ogg", "OggDemuxer: Vorbis Comments - ", field, "=<", value.size(), " bytes of binary data>");
            } else {
                Debug::log("ogg", "OggDemuxer: Vorbis Comments - ", field, "=", value);
            }
        }
    }
}
#endif // HAVE_VORBIS

#ifdef HAVE_FLAC
bool OggDemuxer::parseFLACHeaders(OggStream& stream, const OggPacket& packet) {
    if (OggDemuxer::hasSignature(packet.data, "\x7f""FLAC")) {
        // Ogg FLAC identification header
        if (packet.data.size() >= 51) {
            // Skip Ogg FLAC header, parse embedded STREAMINFO
            size_t streaminfo_offset = 13; // After "\x7fFLAC" + version + header count + "fLaC" + STREAMINFO header
            
            if (packet.data.size() >= streaminfo_offset + 34) {
                // Parse STREAMINFO block
                uint32_t info1 = OggDemuxer::readBE<uint32_t>(packet.data, streaminfo_offset + 10);
                stream.sample_rate = (info1 >> 12) & 0xFFFFF;  // 20 bits
                stream.channels = ((info1 >> 9) & 0x7) + 1;    // 3 bits + 1
                
                // Extract total samples (36 bits, split across boundaries)
                uint64_t total_samples = ((uint64_t)(info1 & 0xF) << 32) | OggDemuxer::readBE<uint32_t>(packet.data, streaminfo_offset + 18);
                stream.total_samples = total_samples;
            }
        }
        return true;
    }
    
    return false;
}
#endif // HAVE_FLAC

#ifdef HAVE_OPUS
bool OggDemuxer::parseOpusHeaders(OggStream& stream, const OggPacket& packet) {
    std::string first_bytes;
    for (size_t i = 0; i < std::min(size_t(16), packet.data.size()); i++) {
        char c = packet.data[i];
        if (c >= 32 && c <= 126) {
            first_bytes += c;
        } else {
            first_bytes += "\\x" + std::to_string(static_cast<unsigned char>(c));
        }
    }
    Debug::log("ogg", "OggDemuxer: parseOpusHeaders called, packet_size=", packet.data.size(), ", first 16 bytes: '", first_bytes, "'");
    
    if (OggDemuxer::hasSignature(packet.data, "OpusHead")) {
        // Opus identification header - must be first
        if (packet.data.size() >= 19) {
            stream.channels = OggDemuxer::readLE<uint8_t>(packet.data, 9);
            // Note: Opus always runs at 48kHz internally, but can be presented at other rates
            stream.sample_rate = 48000;
            stream.pre_skip = OggDemuxer::readLE<uint16_t>(packet.data, 10); // Read pre-skip
            
            Debug::log("ogg", "OggDemuxer: OpusHead header found, channels=", stream.channels, ", pre_skip=", stream.pre_skip, ", packet_size=", packet.data.size());
        }
        return true;
    } else if (OggDemuxer::hasSignature(packet.data, "OpusTags")) {
        // Opus comment header - must be second
        Debug::log("ogg", "OggDemuxer: OpusTags header found, packet_size=", packet.data.size());
        
        // Parse OpusTags metadata (follows Vorbis comment format)
        parseOpusTags(stream, packet);
        return true;
    }
    
    Debug::log("ogg", "OggDemuxer: Unknown Opus packet, size=", packet.data.size());
    
    return false;
}
#endif // HAVE_OPUS

bool OggDemuxer::parseSpeexHeaders(OggStream& stream, const OggPacket& packet) {
    if (OggDemuxer::hasSignature(packet.data, "Speex   ")) {
        // Speex header
        if (packet.data.size() >= 80) {
            stream.sample_rate = OggDemuxer::readLE<uint32_t>(packet.data, 36);
            stream.channels = OggDemuxer::readLE<uint32_t>(packet.data, 48);
        }
        return true;
    }
    
    return false;
}

#ifdef HAVE_OPUS
void OggDemuxer::parseOpusTags(OggStream& stream, const OggPacket& packet) {
    // OpusTags format follows Vorbis comment structure:
    // 8 bytes: "OpusTags"
    // 4 bytes: vendor_length (little-endian)
    // vendor_length bytes: vendor string
    // 4 bytes: user_comment_list_length (little-endian)
    // For each comment:
    //   4 bytes: length (little-endian)
    //   length bytes: comment in UTF-8 format "FIELD=value"
    
    if (packet.data.size() < 16) {
        return; // Too small to contain valid OpusTags
    }
    
    size_t offset = 8; // Skip "OpusTags"
    
    // Read vendor string length
    uint32_t vendor_length = readLE<uint32_t>(packet.data, offset);
    offset += 4;
    
    if (offset + vendor_length > packet.data.size()) {
        return; // Invalid vendor length
    }
    
    offset += vendor_length; // Skip vendor string
    
    if (offset + 4 > packet.data.size()) {
        return; // No room for comment count
    }
    
    // Read number of user comments
    uint32_t comment_count = readLE<uint32_t>(packet.data, offset);
    offset += 4;
    
    // Parse each comment
    for (uint32_t i = 0; i < comment_count && offset < packet.data.size(); i++) {
        if (offset + 4 > packet.data.size()) {
            break;
        }
        
        uint32_t comment_length = readLE<uint32_t>(packet.data, offset);
        offset += 4;
        
        if (offset + comment_length > packet.data.size()) {
            break;
        }
        
        // Extract comment string
        std::string comment(reinterpret_cast<const char*>(packet.data.data() + offset), comment_length);
        offset += comment_length;
        
        // Parse FIELD=value format
        size_t equals_pos = comment.find('=');
        if (equals_pos != std::string::npos) {
            std::string field = comment.substr(0, equals_pos);
            std::string value = comment.substr(equals_pos + 1);
            
            // Convert field to uppercase for case-insensitive comparison
            std::transform(field.begin(), field.end(), field.begin(), ::toupper);
            
            if (field == "ARTIST") {
                stream.artist = value;
            } else if (field == "TITLE") {
                stream.title = value;
            } else if (field == "ALBUM") {
                stream.album = value;
            }
            
            // Don't dump large binary fields like METADATA_BLOCK_PICTURE
            if (field == "METADATA_BLOCK_PICTURE") {
                Debug::log("ogg", "OggDemuxer: OpusTags - ", field, "=<", value.size(), " bytes of binary data>");
            } else {
                Debug::log("ogg", "OggDemuxer: OpusTags - ", field, "=", value);
            }
        }
    }
}
#endif // HAVE_OPUS

void OggDemuxer::calculateDuration() {
    Debug::log("ogg", "OggDemuxer: calculateDuration() called");
    m_duration_ms = 0;
    
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.sample_rate > 0) {
            Debug::log("ogg", "OggDemuxer: calculateDuration - Stream ", stream_id, " (", stream.codec_name, ") has total_samples=", stream.total_samples, ", sample_rate=", stream.sample_rate);
            if (stream.total_samples > 0) {
                uint64_t stream_duration = (stream.total_samples * 1000ULL) / stream.sample_rate;
                m_duration_ms = std::max(m_duration_ms, stream_duration);
                Debug::log("ogg", "OggDemuxer: calculateDuration - Calculated duration from samples: ", stream_duration, "ms");
            }
        }
    }
    
    // If we couldn't determine duration from container, try using tracked max granule
    if (m_duration_ms == 0) {
        if (m_max_granule_seen > 0) {
            Debug::log("ogg", "OggDemuxer: calculateDuration - Using tracked max granule: ", m_max_granule_seen);
            // Use the tracked maximum granule position
            uint32_t stream_id = findBestAudioStream();
            if (stream_id != 0) {
                m_duration_ms = granuleToMs(m_max_granule_seen, stream_id);
                Debug::log("ogg", "OggDemuxer: calculateDuration - Duration from tracked max granule: ", m_max_granule_seen, " -> ", m_duration_ms, "ms");
            }
        } else {
            Debug::log("ogg", "OggDemuxer: calculateDuration - Falling back to reading last page.");
            // Fall back to reading last page
            uint64_t last_granule = getLastGranulePosition();
            Debug::log("ogg", "OggDemuxer: calculateDuration - getLastGranulePosition returned: ", last_granule);
            if (last_granule > 0) {
                // Use the first audio stream for duration calculation
                uint32_t stream_id = findBestAudioStream();
                if (stream_id != 0) {
                    m_duration_ms = granuleToMs(last_granule, stream_id);
                    Debug::log("ogg", "OggDemuxer: calculateDuration - Duration from last granule: ", m_duration_ms, "ms");
                }
            }
        }
    }
    
    // Final fallback - use a reasonable default
    if (m_duration_ms == 0) {
        Debug::log("ogg", "OggDemuxer: calculateDuration - Could not determine duration - no fallback used");
        // No fallback - leave as 0 if duration cannot be determined
    }
}

uint64_t OggDemuxer::granuleToMs(uint64_t granule, uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it == m_streams.end()) {
        Debug::log("ogg", "OggDemuxer: granuleToMs - Stream not found. stream_id=", stream_id);
        return 0;
    }
    
    const OggStream& stream = it->second;
    
    // Validate stream has required information
    if (stream.sample_rate == 0) {
        Debug::log("ogg", "OggDemuxer: granuleToMs - Invalid sample_rate is 0. stream_id=", stream_id, ", codec=", stream.codec_name);
        return 0;
    }
    
    // Check for invalid granule positions
    if (granule == static_cast<uint64_t>(-1) || granule > 0x7FFFFFFFFFFFFFFULL) {
        Debug::log("ogg", "OggDemuxer: granuleToMs - Invalid granule position: ", granule);
        return 0;
    }
    
    // Special case: granule position 0 is always time 0
    if (granule == 0) {
        return 0;
    }
    
    Debug::log("ogg", "OggDemuxer: granuleToMs - Input granule=", granule, ", stream_id=", stream_id, 
               ", codec=", stream.codec_name, ", sample_rate=", stream.sample_rate, ", pre_skip=", stream.pre_skip);
    
    // Codec-specific granule position handling
    if (stream.codec_name == "opus") {
        // Opus granule positions are always in 48kHz sample units per RFC 7845 Section 4.2
        // The granule position represents the total number of 48kHz samples that have been
        // decoded, including pre-skip samples. For timing, we subtract pre-skip.
        
        // Validate pre-skip value (should be reasonable)
        if (stream.pre_skip > 48000) { // More than 1 second of pre-skip is suspicious
            Debug::log("ogg", "OggDemuxer: granuleToMs (Opus) - Warning: large pre_skip value: ", stream.pre_skip);
        }
        
        uint64_t effective_granule;
        if (granule > stream.pre_skip) {
            effective_granule = granule - stream.pre_skip;
        } else {
            // If granule is less than pre-skip, we're still in the pre-skip region
            effective_granule = 0;
        }
        
        // Convert 48kHz samples to milliseconds
        uint64_t result = (effective_granule * 1000ULL) / 48000ULL;
        Debug::log("ogg", "OggDemuxer: granuleToMs (Opus) - effective_granule=", effective_granule, ", result=", result, "ms");
        return result;
        
    } else if (stream.codec_name == "vorbis") {
        // Vorbis granule positions represent the sample number of the last sample
        // in the packet that ends on this page. The granule position is in units
        // of the stream's sample rate.
        
        uint64_t result = (granule * 1000ULL) / stream.sample_rate;
        Debug::log("ogg", "OggDemuxer: granuleToMs (Vorbis) - granule=", granule, ", sample_rate=", stream.sample_rate, ", result=", result, "ms");
        return result;
        
    } else if (stream.codec_name == "flac") {
        // FLAC-in-Ogg granule positions are sample-based like Vorbis
        // The granule position represents the sample number of the first sample
        // in the packet that ends on this page.
        
        uint64_t result = (granule * 1000ULL) / stream.sample_rate;
        Debug::log("ogg", "OggDemuxer: granuleToMs (FLAC) - granule=", granule, ", sample_rate=", stream.sample_rate, ", result=", result, "ms");
        return result;
        
    } else if (stream.codec_name == "speex") {
        // Speex granule positions are sample-based at the stream's sample rate
        
        uint64_t result = (granule * 1000ULL) / stream.sample_rate;
        Debug::log("ogg", "OggDemuxer: granuleToMs (Speex) - granule=", granule, ", sample_rate=", stream.sample_rate, ", result=", result, "ms");
        return result;
        
    } else {
        // Unknown codec - use generic sample-based conversion
        Debug::log("ogg", "OggDemuxer: granuleToMs - Unknown codec, using generic conversion: ", stream.codec_name);
        uint64_t result = (granule * 1000ULL) / stream.sample_rate;
        Debug::log("ogg", "OggDemuxer: granuleToMs (Generic) - result=", result, "ms");
        return result;
    }
}

uint64_t OggDemuxer::msToGranule(uint64_t timestamp_ms, uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it == m_streams.end()) {
        Debug::log("ogg", "OggDemuxer: msToGranule - Stream not found. stream_id=", stream_id);
        return 0;
    }
    
    const OggStream& stream = it->second;
    
    // Validate stream has required information
    if (stream.sample_rate == 0) {
        Debug::log("ogg", "OggDemuxer: msToGranule - Invalid sample_rate is 0. stream_id=", stream_id, ", codec=", stream.codec_name);
        return 0;
    }
    
    // Special case: timestamp 0 handling depends on codec
    if (timestamp_ms == 0) {
        if (stream.codec_name == "opus") {
            // For Opus, granule 0 would be before pre-skip, so return pre-skip value
            return stream.pre_skip;
        } else {
            // For other codecs, timestamp 0 maps to granule 0
            return 0;
        }
    }
    
    Debug::log("ogg", "OggDemuxer: msToGranule - Input timestamp_ms=", timestamp_ms, ", stream_id=", stream_id, 
               ", codec=", stream.codec_name, ", sample_rate=", stream.sample_rate, ", pre_skip=", stream.pre_skip);
    
    // Codec-specific timestamp to granule conversion
    if (stream.codec_name == "opus") {
        // Opus granule positions are always in 48kHz sample units per RFC 7845 Section 4.2
        // To convert from timestamp to granule, we convert to 48kHz samples and add pre-skip
        
        // Validate pre-skip value
        if (stream.pre_skip > 48000) {
            Debug::log("ogg", "OggDemuxer: msToGranule (Opus) - Warning: large pre_skip value: ", stream.pre_skip);
        }
        
        uint64_t samples_48k = (timestamp_ms * 48000ULL) / 1000ULL;
        uint64_t result = samples_48k + stream.pre_skip;
        
        Debug::log("ogg", "OggDemuxer: msToGranule (Opus) - samples_48k=", samples_48k, ", pre_skip=", stream.pre_skip, ", result=", result);
        return result;
        
    } else if (stream.codec_name == "vorbis") {
        // Vorbis granule positions are sample-based at the stream's sample rate
        
        uint64_t result = (timestamp_ms * stream.sample_rate) / 1000ULL;
        Debug::log("ogg", "OggDemuxer: msToGranule (Vorbis) - timestamp_ms=", timestamp_ms, ", sample_rate=", stream.sample_rate, ", result=", result);
        return result;
        
    } else if (stream.codec_name == "flac") {
        // FLAC-in-Ogg granule positions are sample-based at the stream's sample rate
        
        uint64_t result = (timestamp_ms * stream.sample_rate) / 1000ULL;
        Debug::log("ogg", "OggDemuxer: msToGranule (FLAC) - timestamp_ms=", timestamp_ms, ", sample_rate=", stream.sample_rate, ", result=", result);
        return result;
        
    } else if (stream.codec_name == "speex") {
        // Speex granule positions are sample-based at the stream's sample rate
        
        uint64_t result = (timestamp_ms * stream.sample_rate) / 1000ULL;
        Debug::log("ogg", "OggDemuxer: msToGranule (Speex) - timestamp_ms=", timestamp_ms, ", sample_rate=", stream.sample_rate, ", result=", result);
        return result;
        
    } else {
        // Unknown codec - use generic sample-based conversion
        Debug::log("ogg", "OggDemuxer: msToGranule - Unknown codec, using generic conversion: ", stream.codec_name);
        uint64_t result = (timestamp_ms * stream.sample_rate) / 1000ULL;
        Debug::log("ogg", "OggDemuxer: msToGranule (Generic) - result=", result);
        return result;
    }
}

uint32_t OggDemuxer::findBestAudioStream() const {
    // Find the first audio stream
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio") {
            return stream_id;
        }
    }
    
    return 0; // No audio streams found
}

uint64_t OggDemuxer::getLastGranulePosition() {
    if (m_file_size == 0) {
        Debug::log("ogg", "OggDemuxer: getLastGranulePosition - file_size is 0");
        return 0;
    }
    
    Debug::log("ogg", "OggDemuxer: getLastGranulePosition - file_size=", m_file_size);
    
    // Save current position
    long current_pos = m_handler->tell();
    
    // Robust backward scanning with multiple strategies
    uint64_t last_granule = 0;
    
    // Strategy 1: Efficient backward scanning from end of file
    // Start with smaller chunks and expand if needed
    const std::vector<size_t> scan_sizes = {65536, 262144, 1048576, 4194304}; // 64KB, 256KB, 1MB, 4MB
    
    for (size_t scan_size : scan_sizes) {
        if (scan_size > m_file_size) {
            scan_size = m_file_size;
        }
        
        long scan_start = std::max(0L, static_cast<long>(m_file_size) - static_cast<long>(scan_size));
        
        Debug::log("ogg", "OggDemuxer: getLastGranulePosition - trying scan_size=", scan_size, ", scan_start=", scan_start);
        
        if (m_handler->seek(scan_start, SEEK_SET) != 0) {
            Debug::log("ogg", "OggDemuxer: getLastGranulePosition - seek failed for scan_start=", scan_start);
            continue;
        }
        
        std::vector<uint8_t> scan_buffer(scan_size);
        long bytes_read = m_handler->read(scan_buffer.data(), 1, scan_size);
        
        if (bytes_read <= 0) {
            Debug::log("ogg", "OggDemuxer: getLastGranulePosition - read failed, bytes_read=", bytes_read);
            continue;
        }
        
        uint64_t found_granule = scanBufferForLastGranule(scan_buffer, bytes_read);
        Debug::log("ogg", "OggDemuxer: getLastGranulePosition - scanBufferForLastGranule returned=", found_granule, " for scan_size=", scan_size);
        if (found_granule > 0) {
            last_granule = found_granule;
            Debug::log("ogg", "OggDemuxer: getLastGranulePosition - found granule=", last_granule, " with scan_size=", scan_size);
            break;
        }
        
        // If we've scanned the entire file, no point in trying larger sizes
        if (scan_size >= m_file_size) {
            break;
        }
    }
    
    // Strategy 2: If backward scanning failed, try forward scanning from a reasonable position
    if (last_granule == 0 && m_file_size > 1048576) {
        Debug::log("ogg", "OggDemuxer: getLastGranulePosition - backward scan failed, trying forward scan");
        
        // Start scanning from 75% of file size
        long forward_start = static_cast<long>(m_file_size * 3 / 4);
        if (m_handler->seek(forward_start, SEEK_SET) == 0) {
            last_granule = scanForwardForLastGranule(forward_start);
        }
    }
    
    // Strategy 3: Fallback to header-provided information if available
    if (last_granule == 0) {
        Debug::log("ogg", "OggDemuxer: getLastGranulePosition - scanning failed, checking header info");
        last_granule = getLastGranuleFromHeaders();
    }
    
    // Restore original position
    m_handler->seek(current_pos, SEEK_SET);
    
    Debug::log("ogg", "OggDemuxer: getLastGranulePosition - final result=", last_granule);
    
    return last_granule;
}

uint64_t OggDemuxer::scanBufferForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size) {
    uint64_t last_granule = 0;
    size_t pages_found = 0;
    
    // Scan backward through buffer for "OggS" signatures
    // This gives us the most recent pages first
    if (buffer_size >= 4) {
        for (size_t i = buffer_size - 4; i != SIZE_MAX; i--) {
            size_t check_pos = i;
            
            if (buffer[check_pos] == 'O' && buffer[check_pos + 1] == 'g' && 
                buffer[check_pos + 2] == 'g' && buffer[check_pos + 3] == 'S') {
                
                // Validate this looks like a real Ogg page header
                if (check_pos + 27 <= buffer_size) {
                    // Check version (should be 0)
                    if (buffer[check_pos + 4] != 0) {
                        continue;
                    }
                    
                    // Extract granule position (little-endian, 8 bytes at offset 6)
                    uint64_t granule = 0;
                    for (int j = 0; j < 8; j++) {
                        granule |= (static_cast<uint64_t>(buffer[check_pos + 6 + j]) << (j * 8));
                    }
                    
                    // Validate granule position
                    if (granule != static_cast<uint64_t>(-1) && granule < 0x7FFFFFFFFFFFFFFULL) {
                        // Check page segments count is reasonable
                        uint8_t page_segments = buffer[check_pos + 26];
                        if (page_segments <= 255 && check_pos + 27 + page_segments <= buffer_size) {
                            if (granule > last_granule) {
                                last_granule = granule;
                            }
                            pages_found++;
                            
                            Debug::log("ogg", "OggDemuxer: scanBufferForLastGranule - found page at offset ", check_pos, 
                                       ", granule=", granule, ", segments=", static_cast<int>(page_segments));
                        }
                    }
                }
            }
        }
    }
    
    Debug::log("ogg", "OggDemuxer: scanBufferForLastGranule - found ", pages_found, " pages, last_granule=", last_granule);
    return last_granule;
}

uint64_t OggDemuxer::scanForwardForLastGranule(long start_position) {
    uint64_t last_granule = 0;
    const size_t CHUNK_SIZE = 65536; // 64KB chunks
    std::vector<uint8_t> buffer(CHUNK_SIZE);
    
    long current_pos = start_position;
    
    while (current_pos < static_cast<long>(m_file_size)) {
        if (m_handler->seek(current_pos, SEEK_SET) != 0) {
            break;
        }
        
        size_t bytes_to_read = std::min(CHUNK_SIZE, static_cast<size_t>(m_file_size - current_pos));
        long bytes_read = m_handler->read(buffer.data(), 1, bytes_to_read);
        
        if (bytes_read <= 0) {
            break;
        }
        
        uint64_t chunk_granule = scanBufferForLastGranule(buffer, bytes_read);
        if (chunk_granule > last_granule) {
            last_granule = chunk_granule;
        }
        
        current_pos += bytes_read;
    }
    
    Debug::log("ogg", "OggDemuxer: scanForwardForLastGranule - last_granule=", last_granule);
    return last_granule;
}

uint64_t OggDemuxer::getLastGranuleFromHeaders() {
    uint64_t max_total_samples = 0;
    
    // Check if any stream headers contain total sample information
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.total_samples > 0) {
            Debug::log("ogg", "OggDemuxer: getLastGranuleFromHeaders - stream ", stream_id, 
                       " has total_samples=", stream.total_samples, ", codec=", stream.codec_name);
            
            // Convert total samples to granule position based on codec
            uint64_t granule_from_samples = 0;
            
            if (stream.codec_name == "opus") {
                // For Opus, granule position includes pre-skip
                granule_from_samples = stream.total_samples + stream.pre_skip;
            } else {
                // For other codecs, granule position equals sample count
                granule_from_samples = stream.total_samples;
            }
            
            if (granule_from_samples > max_total_samples) {
                max_total_samples = granule_from_samples;
            }
        }
    }
    
    Debug::log("ogg", "OggDemuxer: getLastGranuleFromHeaders - max_total_samples=", max_total_samples);
    return max_total_samples;
}

// According to RFC 6716, Section 3.1
static const int opus_samples_per_frame_lk[32] = {
    480, 960, 1920, 2880, // SILK-only NB
    480, 960, 1920, 2880, // SILK-only MB
    480, 960, 1920, 2880, // SILK-only WB
    480, 960,             // Hybrid SWB
    480, 960,             // Hybrid FB
    -1, -1,              // Reserved
    120, 240, 480, 960,    // CELT-only NB
    120, 240, 480, 960,    // CELT-only WB
    120, 240, 480, 960     // CELT-only SWB/FB
};

int OggDemuxer::getOpusPacketSampleCount(const OggPacket& packet) {
    if (packet.data.empty()) {
        return 0;
    }

    uint8_t toc = packet.data[0];
    int config = (toc >> 3) & 0x1F;
    int frames;

    switch (toc & 0x03) {
        case 0:
            frames = 1;
            break;
        case 1:
        case 2:
            frames = 2;
            break;
        case 3:
            if (packet.data.size() < 2) return 0;
            frames = packet.data[1] & 0x3F;
            break;
        default:
            return 0; // Unreachable
    }

    return frames * opus_samples_per_frame_lk[config];
}

bool OggDemuxer::seekToPage(uint64_t target_granule, uint32_t stream_id)
{
    if (m_file_size == 0) return false;

    Debug::log("ogg", "OggDemuxer: seekToPage - target_granule=", target_granule, ", stream_id=", stream_id, ", file_size=", m_file_size);

    // Implement proper bisection search algorithm
    long left = 0;
    long right = static_cast<long>(m_file_size);
    long best_pos = 0;
    uint64_t best_granule = 0;
    bool found_valid_page = false;
    
    // Maximum iterations to prevent infinite loops
    const int max_iterations = 32;
    int iterations = 0;
    
    while (left < right && iterations < max_iterations) {
        iterations++;
        long mid = left + (right - left) / 2;
        
        Debug::log("ogg", "OggDemuxer: seekToPage bisection iteration ", iterations, " - left=", left, ", mid=", mid, ", right=", right);
        
        // Find granule position at this file offset
        uint64_t granule_at_mid = findGranuleAtOffset(mid, stream_id);
        
        if (granule_at_mid == static_cast<uint64_t>(-1)) {
            // No valid page found at this position, try moving right
            left = mid + 1;
            Debug::log("ogg", "OggDemuxer: seekToPage - no valid page at offset ", mid, ", moving right");
            continue;
        }
        
        Debug::log("ogg", "OggDemuxer: seekToPage - found granule ", granule_at_mid, " at offset ", mid, ", target=", target_granule);
        
        // Update best position if this is closer to our target
        if (!found_valid_page || 
            (granule_at_mid <= target_granule && granule_at_mid > best_granule) ||
            (best_granule > target_granule && granule_at_mid < best_granule)) {
            best_pos = mid;
            best_granule = granule_at_mid;
            found_valid_page = true;
        }
        
        if (granule_at_mid < target_granule) {
            // Need to search right half
            left = mid + 1;
        } else if (granule_at_mid > target_granule) {
            // Need to search left half
            right = mid - 1;
        } else {
            // Exact match found
            best_pos = mid;
            best_granule = granule_at_mid;
            break;
        }
    }
    
    if (!found_valid_page) {
        Debug::log("ogg", "OggDemuxer: seekToPage - no valid pages found during bisection, seeking to beginning");
        m_handler->seek(0, SEEK_SET);
        ogg_sync_reset(&m_sync_state);
        m_position_ms = 0;
        m_eof = false;
        return true;
    }
    
    Debug::log("ogg", "OggDemuxer: seekToPage - bisection completed after ", iterations, " iterations, best_pos=", best_pos, ", best_granule=", best_granule);
    
    // Seek to the best position found
    m_handler->seek(best_pos, SEEK_SET);
    
    // Reset sync and stream states for clean reading
    ogg_sync_reset(&m_sync_state);
    for (auto& [sid, ogg_stream] : m_ogg_streams) {
        ogg_stream_reset(&ogg_stream);
    }
    
    // Update position tracking
    m_position_ms = granuleToMs(best_granule, stream_id);
    m_eof = false;
    
    Debug::log("ogg", "OggDemuxer: seekToPage completed - final position=", m_position_ms, "ms");
    return true;
}

uint64_t OggDemuxer::findGranuleAtOffset(long file_offset, uint32_t stream_id) {
    // Save current position
    long original_pos = m_handler->tell();
    
    // Seek to the target offset
    if (m_handler->seek(file_offset, SEEK_SET) != 0) {
        Debug::log("ogg", "OggDemuxer: findGranuleAtOffset - failed to seek to offset ", file_offset);
        m_handler->seek(original_pos, SEEK_SET);
        return static_cast<uint64_t>(-1);
    }
    
    // Create a temporary sync state for this search
    ogg_sync_state temp_sync;
    if (ogg_sync_init(&temp_sync) != 0) {
        Debug::log("ogg", "OggDemuxer: findGranuleAtOffset - failed to initialize temp sync state");
        m_handler->seek(original_pos, SEEK_SET);
        return static_cast<uint64_t>(-1);
    }
    
    uint64_t found_granule = static_cast<uint64_t>(-1);
    
    try {
        // Read data into temporary sync buffer
        const size_t read_size = 8192;
        char* buffer = ogg_sync_buffer(&temp_sync, read_size);
        if (!buffer) {
            throw std::runtime_error("Failed to get sync buffer");
        }
        
        long bytes_read = m_handler->read(buffer, 1, read_size);
        if (bytes_read <= 0) {
            throw std::runtime_error("Failed to read data");
        }
        
        if (ogg_sync_wrote(&temp_sync, bytes_read) != 0) {
            throw std::runtime_error("Failed to write to sync buffer");
        }
        
        // Look for pages in the buffer
        ogg_page page;
        while (ogg_sync_pageout(&temp_sync, &page) == 1) {
            // Validate page before accessing its fields
            if (!validateOggPage(&page)) {
                continue;
            }
            
            uint32_t page_stream_id = ogg_page_serialno(&page);
            uint64_t page_granule = ogg_page_granulepos(&page);
            
            if (page_stream_id == stream_id && page_granule != static_cast<uint64_t>(-1)) {
                found_granule = page_granule;
                Debug::log("ogg", "OggDemuxer: findGranuleAtOffset - found granule ", page_granule, " for stream ", stream_id, " at offset ", file_offset);
                break;
            }
        }
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: findGranuleAtOffset - exception: ", e.what());
    }
    
    // Clean up temporary sync state
    ogg_sync_clear(&temp_sync);
    
    // Restore original position
    m_handler->seek(original_pos, SEEK_SET);
    
    return found_granule;
}

// Stub implementations for disabled codecs to prevent compilation errors

#ifndef HAVE_VORBIS
bool OggDemuxer::parseVorbisHeaders(OggStream& stream, const OggPacket& packet) {
    Debug::log("ogg", "OggDemuxer: Vorbis support not compiled in");
    return false;
}

void OggDemuxer::parseVorbisComments(OggStream& stream, const OggPacket& packet) {
    Debug::log("ogg", "OggDemuxer: Vorbis support not compiled in");
}
#endif // !HAVE_VORBIS

#ifndef HAVE_FLAC
bool OggDemuxer::parseFLACHeaders(OggStream& stream, const OggPacket& packet) {
    Debug::log("ogg", "OggDemuxer: FLAC support not compiled in");
    return false;
}
#endif // !HAVE_FLAC

#ifndef HAVE_OPUS
bool OggDemuxer::parseOpusHeaders(OggStream& stream, const OggPacket& packet) {
    Debug::log("ogg", "OggDemuxer: Opus support not compiled in");
    return false;
}

void OggDemuxer::parseOpusTags(OggStream& stream, const OggPacket& packet) {
    Debug::log("ogg", "OggDemuxer: Opus support not compiled in");
}
#endif // !HAVE_OPUS

#endif // HAVE_OGGDEMUXER

// Error recovery method implementations

bool OggDemuxer::skipToNextValidSection() const {
    if (!m_handler) {
        return false;
    }
    
    Debug::log("ogg", "OggDemuxer: Attempting to skip to next valid section");
    
    // Save current position
    long current_pos = m_handler->tell();
    if (current_pos < 0) {
        return false;
    }
    
    // Look for next "OggS" signature
    const uint8_t ogg_signature[] = {'O', 'g', 'g', 'S'};
    uint8_t buffer[4096];
    
    while (!m_handler->eof()) {
        size_t bytes_read = m_handler->read(buffer, 1, sizeof(buffer));
        if (bytes_read < 4) {
            break;
        }
        
        // Search for Ogg signature in buffer
        for (size_t i = 0; i <= bytes_read - 4; ++i) {
            if (std::memcmp(buffer + i, ogg_signature, 4) == 0) {
                // Found potential Ogg page, seek to it
                long found_pos = m_handler->tell() - static_cast<long>(bytes_read) + static_cast<long>(i);
                if (m_handler->seek(found_pos, SEEK_SET) == 0) {
                    Debug::log("ogg", "OggDemuxer: Found next Ogg page at offset ", found_pos);
                    const_cast<OggDemuxer*>(this)->m_last_valid_position = found_pos;
                    return true;
                }
            }
        }
        
        // Overlap search - move back 3 bytes to catch signatures spanning buffer boundaries
        if (bytes_read >= 4) {
            m_handler->seek(-3, SEEK_CUR);
        }
    }
    
    Debug::log("ogg", "OggDemuxer: No valid Ogg page found, restoring position");
    m_handler->seek(current_pos, SEEK_SET);
    return false;
}

bool OggDemuxer::resetInternalState() const {
    Debug::log("ogg", "OggDemuxer: Resetting internal state");
    
    try {
        // Reset libogg sync state
        ogg_sync_reset(&const_cast<OggDemuxer*>(this)->m_sync_state);
        
        // Reset all stream states
        for (auto& [stream_id, ogg_stream] : const_cast<OggDemuxer*>(this)->m_ogg_streams) {
            ogg_stream_reset(&ogg_stream);
        }
        
        // Clear packet queues but preserve stream metadata
        for (auto& [stream_id, stream] : const_cast<OggDemuxer*>(this)->m_streams) {
            stream.m_packet_queue.clear();
            stream.partial_packet_data.clear();
            stream.last_page_sequence = 0;
        }
        
        // Reset EOF state
        const_cast<OggDemuxer*>(this)->m_eof = false;
        
        Debug::log("ogg", "OggDemuxer: Internal state reset successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception during state reset: ", e.what());
        return false;
    }
}

bool OggDemuxer::enableFallbackMode() const {
    Debug::log("ogg", "OggDemuxer: Enabling fallback parsing mode");
    
    const_cast<OggDemuxer*>(this)->m_fallback_mode = true;
    
    // In fallback mode, we're more lenient with:
    // - Page validation
    // - Packet boundaries
    // - Stream synchronization
    // - Error recovery
    
    return true;
}

bool OggDemuxer::recoverFromCorruptedPage(long file_offset) {
    Debug::log("ogg", "OggDemuxer: Attempting to recover from corrupted page at offset ", file_offset);
    
    // Mark current position as potentially corrupted
    if (file_offset > 0) {
        m_handler->seek(file_offset, SEEK_SET);
    }
    
    // Try to find next valid page
    if (!skipToNextValidSection()) {
        Debug::log("ogg", "OggDemuxer: Could not find next valid section after corruption");
        return false;
    }
    
    // Reset sync state to start fresh from new position
    if (!resetInternalState()) {
        Debug::log("ogg", "OggDemuxer: Failed to reset state after corruption recovery");
        return false;
    }
    
    Debug::log("ogg", "OggDemuxer: Successfully recovered from corrupted page");
    return true;
}

bool OggDemuxer::handleSeekingError(uint64_t timestamp_ms, uint64_t& clamped_timestamp) {
    Debug::log("ogg", "OggDemuxer: Handling seeking error for timestamp ", timestamp_ms, "ms");
    
    // Clamp timestamp to valid range
    if (timestamp_ms > m_duration_ms && m_duration_ms > 0) {
        clamped_timestamp = m_duration_ms;
        Debug::log("ogg", "OggDemuxer: Clamped timestamp to duration: ", clamped_timestamp, "ms");
    } else if (timestamp_ms == 0) {
        // Seeking to beginning should always work
        clamped_timestamp = 0;
        return seekTo(0);
    } else {
        // Try seeking to approximate position
        clamped_timestamp = std::min(timestamp_ms, m_duration_ms);
    }
    
    // Try seeking with clamped timestamp
    if (seekTo(clamped_timestamp)) {
        Debug::log("ogg", "OggDemuxer: Successfully seeked to clamped timestamp: ", clamped_timestamp, "ms");
        return true;
    }
    
    // If that fails, try seeking to beginning
    if (seekTo(0)) {
        clamped_timestamp = 0;
        Debug::log("ogg", "OggDemuxer: Fallback seek to beginning successful");
        return true;
    }
    
    Debug::log("ogg", "OggDemuxer: All seeking attempts failed");
    return false;
}

bool OggDemuxer::isolateStreamError(uint32_t stream_id, const std::string& error_context) {
    Debug::log("ogg", "OggDemuxer: Isolating error for stream ", stream_id, " in context: ", error_context);
    
    // Mark stream as corrupted
    m_corrupted_streams.insert(stream_id);
    
    // Clear stream's packet queue to prevent further issues
    auto stream_it = m_streams.find(stream_id);
    if (stream_it != m_streams.end()) {
        stream_it->second.m_packet_queue.clear();
        stream_it->second.partial_packet_data.clear();
        
        Debug::log("ogg", "OggDemuxer: Cleared packet queue for corrupted stream ", stream_id);
    }
    
    // Reset the libogg stream state for this stream
    auto ogg_stream_it = m_ogg_streams.find(stream_id);
    if (ogg_stream_it != m_ogg_streams.end()) {
        ogg_stream_reset(&ogg_stream_it->second);
        Debug::log("ogg", "OggDemuxer: Reset libogg stream state for stream ", stream_id);
    }
    
    // Check if we have other valid streams
    bool has_valid_streams = false;
    for (const auto& [id, stream] : m_streams) {
        if (m_corrupted_streams.find(id) == m_corrupted_streams.end() && 
            stream.codec_type == "audio" && stream.headers_complete) {
            has_valid_streams = true;
            break;
        }
    }
    
    if (!has_valid_streams) {
        Debug::log("ogg", "OggDemuxer: No valid streams remaining after isolation");
        return false;
    }
    
    Debug::log("ogg", "OggDemuxer: Successfully isolated stream error, ", 
               m_corrupted_streams.size(), " streams now corrupted");
    return true;
}

bool OggDemuxer::synchronizeToPageBoundary() {
    Debug::log("ogg", "OggDemuxer: Synchronizing to page boundary");
    
    if (!m_handler) {
        return false;
    }
    
    // Reset sync state to clear any partial data
    ogg_sync_reset(&m_sync_state);
    
    // Try to find next page boundary
    if (!skipToNextValidSection()) {
        return false;
    }
    
    // Read some data into sync buffer to establish synchronization
    if (!readIntoSyncBuffer(4096)) {
        return false;
    }
    
    // Try to extract a page to verify synchronization
    ogg_page page;
    int result = ogg_sync_pageout(&m_sync_state, &page);
    
    if (result == 1) {
        Debug::log("ogg", "OggDemuxer: Successfully synchronized to page boundary");
        return true;
    }
    
    Debug::log("ogg", "OggDemuxer: Failed to synchronize to page boundary");
    return false;
}

bool OggDemuxer::validateAndRepairStreamState(uint32_t stream_id) {
    Debug::log("ogg", "OggDemuxer: Validating and repairing stream state for stream ", stream_id);
    
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        Debug::log("ogg", "OggDemuxer: Stream ", stream_id, " not found");
        return false;
    }
    
    OggStream& stream = stream_it->second;
    
    // Check if stream is marked as corrupted
    if (m_corrupted_streams.find(stream_id) != m_corrupted_streams.end()) {
        Debug::log("ogg", "OggDemuxer: Stream ", stream_id, " is marked as corrupted");
        return false;
    }
    
    // Validate basic stream properties
    if (stream.codec_name.empty()) {
        Debug::log("ogg", "OggDemuxer: Stream ", stream_id, " has no codec name");
        return false;
    }
    
    if (stream.codec_type != "audio") {
        Debug::log("ogg", "OggDemuxer: Stream ", stream_id, " is not an audio stream");
        return false;
    }
    
    // Check if headers are complete
    if (!stream.headers_complete) {
        Debug::log("ogg", "OggDemuxer: Stream ", stream_id, " headers are incomplete");
        
        // Try to repair by checking if we have minimum required headers
        if (stream.codec_name == "vorbis" && stream.header_packets.size() >= 3) {
            stream.headers_complete = true;
            Debug::log("ogg", "OggDemuxer: Repaired Vorbis stream ", stream_id, " header state");
        } else if (stream.codec_name == "opus" && stream.header_packets.size() >= 2) {
            stream.headers_complete = true;
            Debug::log("ogg", "OggDemuxer: Repaired Opus stream ", stream_id, " header state");
        } else if (stream.codec_name == "flac" && stream.header_packets.size() >= 1) {
            stream.headers_complete = true;
            Debug::log("ogg", "OggDemuxer: Repaired FLAC stream ", stream_id, " header state");
        } else {
            return false;
        }
    }
    
    // Validate audio properties
    if (stream.sample_rate == 0 || stream.channels == 0) {
        Debug::log("ogg", "OggDemuxer: Stream ", stream_id, " has invalid audio properties");
        return false;
    }
    
    // Clear any corrupted packet data
    if (!stream.partial_packet_data.empty()) {
        stream.partial_packet_data.clear();
        Debug::log("ogg", "OggDemuxer: Cleared partial packet data for stream ", stream_id);
    }
    
    Debug::log("ogg", "OggDemuxer: Stream ", stream_id, " validation and repair successful");
    return true;
}

// Reference-pattern page extraction functions (following libvorbisfile)

int OggDemuxer::getNextPage(ogg_page* page, int64_t boundary) {
    if (!page) {
        Debug::log("ogg", "OggDemuxer::getNextPage: null page pointer");
        return -1;
    }
    
    std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
    
    while (true) {
        int result = ogg_sync_pageseek(&m_sync_state, page);
        
        if (result < 0) {
            // Skipped bytes due to corruption, continue searching
            Debug::log("ogg", "OggDemuxer::getNextPage: skipped ", -result, " bytes");
            continue;
        }
        
        if (result == 0) {
            // Need more data
            int data_result = getData(READSIZE);
            if (data_result <= 0) {
                Debug::log("ogg", "OggDemuxer::getNextPage: no more data available");
                return data_result; // EOF or error
            }
            continue;
        }
        
        // Got a page (result > 0)
        m_offset += result;
        
        // Check boundary condition if specified
        if (boundary >= 0 && m_offset > boundary) {
            Debug::log("ogg", "OggDemuxer::getNextPage: exceeded boundary at offset ", m_offset);
            return -1;
        }
        
        // Validate page structure
        if (!validateOggPage(page)) {
            Debug::log("ogg", "OggDemuxer::getNextPage: invalid page structure");
            continue;
        }
        
        Debug::log("ogg", "OggDemuxer::getNextPage: found valid page at offset ", m_offset, 
                   ", serial=", ogg_page_serialno(page), ", granule=", ogg_page_granulepos(page));
        return result;
    }
}

int OggDemuxer::getPrevPage(ogg_page* page) {
    if (!page) {
        Debug::log("ogg", "OggDemuxer::getPrevPage: null page pointer");
        return -1;
    }
    
    std::lock_guard<std::mutex> io_lock(m_io_mutex);
    std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
    
    int64_t begin = m_offset;
    int64_t end = begin;
    int64_t offset = -1;
    
    while (offset == -1) {
        begin -= CHUNKSIZE;
        if (begin < 0) {
            begin = 0;
        }
        
        // Seek to chunk start
        if (m_handler->seek(begin, SEEK_SET) != 0) {
            Debug::log("ogg", "OggDemuxer::getPrevPage: seek failed to offset ", begin);
            return -1;
        }
        
        // Clear sync state and read chunk
        ogg_sync_reset(&m_sync_state);
        
        char* buffer = ogg_sync_buffer(&m_sync_state, static_cast<long>(end - begin));
        if (!buffer) {
            Debug::log("ogg", "OggDemuxer::getPrevPage: failed to get sync buffer");
            return -1;
        }
        
        long bytes_read = m_handler->read(buffer, 1, end - begin);
        if (bytes_read <= 0) {
            Debug::log("ogg", "OggDemuxer::getPrevPage: read failed, bytes=", bytes_read);
            return -1;
        }
        
        ogg_sync_wrote(&m_sync_state, bytes_read);
        
        // Scan forward through this chunk looking for pages
        int64_t last_page_offset = -1;
        ogg_page temp_page;
        int64_t current_pos = begin;
        
        while (true) {
            int result = ogg_sync_pageseek(&m_sync_state, &temp_page);
            
            if (result < 0) {
                // Skip corrupted bytes
                current_pos += (-result);
                continue;
            }
            
            if (result == 0) {
                // No more pages in this chunk
                break;
            }
            
            // Found a page
            int64_t page_offset = current_pos;
            current_pos += result;
            
            if (page_offset < m_offset) {
                // This page is before our target position
                last_page_offset = page_offset;
                *page = temp_page; // Copy the page
            } else {
                // We've gone past our target
                break;
            }
        }
        
        if (last_page_offset != -1) {
            offset = last_page_offset;
            m_offset = offset;
            Debug::log("ogg", "OggDemuxer::getPrevPage: found page at offset ", offset);
            return 1;
        }
        
        if (begin == 0) {
            // Reached beginning of file
            Debug::log("ogg", "OggDemuxer::getPrevPage: reached beginning of file");
            return -1;
        }
        
        end = begin;
    }
    
    return -1;
}

int OggDemuxer::getPrevPageSerial(ogg_page* page, uint32_t serial_number) {
    if (!page) {
        Debug::log("ogg", "OggDemuxer::getPrevPageSerial: null page pointer");
        return -1;
    }
    
    std::lock_guard<std::mutex> io_lock(m_io_mutex);
    std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
    
    int64_t begin = m_offset;
    int64_t end = begin;
    int64_t offset = -1;
    
    while (offset == -1) {
        begin -= CHUNKSIZE;
        if (begin < 0) {
            begin = 0;
        }
        
        // Seek to chunk start
        if (m_handler->seek(begin, SEEK_SET) != 0) {
            Debug::log("ogg", "OggDemuxer::getPrevPageSerial: seek failed to offset ", begin);
            return -1;
        }
        
        // Clear sync state and read chunk
        ogg_sync_reset(&m_sync_state);
        
        char* buffer = ogg_sync_buffer(&m_sync_state, static_cast<long>(end - begin));
        if (!buffer) {
            Debug::log("ogg", "OggDemuxer::getPrevPageSerial: failed to get sync buffer");
            return -1;
        }
        
        long bytes_read = m_handler->read(buffer, 1, end - begin);
        if (bytes_read <= 0) {
            Debug::log("ogg", "OggDemuxer::getPrevPageSerial: read failed, bytes=", bytes_read);
            return -1;
        }
        
        ogg_sync_wrote(&m_sync_state, bytes_read);
        
        // Scan forward through this chunk looking for pages with matching serial
        int64_t last_page_offset = -1;
        ogg_page temp_page;
        int64_t current_pos = begin;
        
        while (true) {
            int result = ogg_sync_pageseek(&m_sync_state, &temp_page);
            
            if (result < 0) {
                // Skip corrupted bytes
                current_pos += (-result);
                continue;
            }
            
            if (result == 0) {
                // No more pages in this chunk
                break;
            }
            
            // Found a page
            int64_t page_offset = current_pos;
            current_pos += result;
            
            if (page_offset < m_offset && static_cast<uint32_t>(ogg_page_serialno(&temp_page)) == serial_number) {
                // This page is before our target position and has matching serial
                last_page_offset = page_offset;
                *page = temp_page; // Copy the page
            }
        }
        
        if (last_page_offset != -1) {
            offset = last_page_offset;
            m_offset = offset;
            Debug::log("ogg", "OggDemuxer::getPrevPageSerial: found page with serial ", serial_number, " at offset ", offset);
            return 1;
        }
        
        if (begin == 0) {
            // Reached beginning of file
            Debug::log("ogg", "OggDemuxer::getPrevPageSerial: reached beginning of file");
            return -1;
        }
        
        end = begin;
    }
    
    return -1;
}

int OggDemuxer::getData(size_t bytes_requested) {
    if (bytes_requested == 0) {
        bytes_requested = READSIZE;
    }
    
    // Limit read size to prevent excessive memory usage
    if (bytes_requested > CHUNKSIZE) {
        bytes_requested = CHUNKSIZE;
    }
    
    // Note: This method is called from getNextPage which already holds ogg_lock
    // We should not acquire ogg_lock here to avoid deadlock
    
    char* buffer = ogg_sync_buffer(&m_sync_state, static_cast<long>(bytes_requested));
    if (!buffer) {
        Debug::log("ogg", "OggDemuxer::getData: failed to get sync buffer for ", bytes_requested, " bytes");
        return -1;
    }
    
    long bytes_read;
    {
        std::lock_guard<std::mutex> io_lock(m_io_mutex);
        bytes_read = m_handler->read(buffer, 1, bytes_requested);
    }
    
    if (bytes_read < 0) {
        Debug::log("ogg", "OggDemuxer::getData: I/O error during read");
        return -1;
    }
    
    if (bytes_read == 0) {
        if (m_handler->eof()) {
            Debug::log("ogg", "OggDemuxer::getData: reached EOF");
            return 0; // EOF
        } else {
            Debug::log("ogg", "OggDemuxer::getData: no data available (temporary)");
            return -1; // Temporary error
        }
    }
    
    int sync_result = ogg_sync_wrote(&m_sync_state, bytes_read);
    if (sync_result != 0) {
        Debug::log("ogg", "OggDemuxer::getData: ogg_sync_wrote failed with code: ", sync_result);
        return -1;
    }
    
    Debug::log("ogg", "OggDemuxer::getData: read ", bytes_read, " bytes successfully");
    return static_cast<int>(bytes_read);
}
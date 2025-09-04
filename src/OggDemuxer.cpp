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

// Error codes following libvorbisfile/libopusfile patterns
// Note: We use our own constants to avoid conflicts with system headers
namespace OggErrorCodes {
    // Reference implementation error codes (avoiding conflicts)
    static constexpr int DEMUX_FALSE = -1;        // Request did not succeed
    static constexpr int DEMUX_EOF = -2;          // End of file reached
    static constexpr int DEMUX_HOLE = -3;         // Missing packet in stream
    static constexpr int DEMUX_EREAD = -128;      // Read/seek/tell operation failed
    static constexpr int DEMUX_EFAULT = -129;     // Memory allocation failed
    static constexpr int DEMUX_EIMPL = -130;      // Feature not implemented
    static constexpr int DEMUX_EINVAL = -131;     // Invalid argument
    static constexpr int DEMUX_ENOTFORMAT = -132; // Not a recognized stream
    static constexpr int DEMUX_EBADHEADER = -133; // Invalid/corrupt header
    static constexpr int DEMUX_EVERSION = -134;   // ID header has unrecognized version
    static constexpr int DEMUX_ENOTAUDIO = -135;  // Not an audio stream
    static constexpr int DEMUX_EBADPACKET = -136; // Invalid packet
    static constexpr int DEMUX_EBADLINK = -137;   // Invalid stream structure
    static constexpr int DEMUX_ENOSEEK = -138;    // Stream not seekable
    static constexpr int DEMUX_EBADTIMESTAMP = -139; // Invalid timestamp
    
    // Page size limits (following reference implementations)
    static constexpr size_t PAGE_SIZE_MAX = 65307; // Maximum Ogg page size
    
    // Parsing limits to prevent infinite loops
    static constexpr int MAX_HEADER_PAGES = 500;      // Maximum pages to scan for headers
    static constexpr int MAX_SEEK_ITERATIONS = 100;   // Maximum bisection iterations
    static constexpr int MAX_SYNC_ATTEMPTS = 10;      // Maximum synchronization attempts
    static constexpr int MAX_RECOVERY_ATTEMPTS = 3;   // Maximum error recovery attempts
}

OggDemuxer::OggDemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)) {
    Debug::log("loader", "OggDemuxer constructor called - initializing libogg sync state");
    Debug::log("ogg", "OggDemuxer constructor called - initializing libogg sync state");
    Debug::log("demuxer", "OggDemuxer constructor called - initializing libogg sync state");
    
    // Initialize libogg sync state with proper error handling (Requirement 8.1)
    int sync_result = ogg_sync_init(&m_sync_state);
    if (sync_result != 0) {
        Debug::log("loader", "OggDemuxer: ogg_sync_init failed with code: ", sync_result);
        Debug::log("ogg", "OggDemuxer: ogg_sync_init failed with code: ", sync_result);
        Debug::log("demuxer", "OggDemuxer: ogg_sync_init failed with code: ", sync_result);
        
        // Clean up any partially initialized state before throwing
        cleanupLiboggStructures_unlocked();
        throw std::runtime_error("Failed to initialize ogg sync state");
    }
    
    // Initialize packet queue limits to prevent memory exhaustion (Requirement 8.2)
    m_max_packet_queue_size = 100; // Bounded queue size
    m_total_memory_usage = 0;
    m_max_memory_usage = 50 * 1024 * 1024; // 50MB limit
    
    // Performance optimization settings (Requirements 8.1-8.7)
    m_read_ahead_buffer_size = 64 * 1024; // 64KB read-ahead for network streams
    m_page_cache_size = 32; // Cache up to 32 recently accessed pages
    m_io_buffer_size = 32 * 1024; // 32KB I/O buffer for efficient reads
    m_seek_hint_granularity = 1000; // 1 second granularity for seek hints
    
    // Initialize performance tracking
    m_bytes_read_total = 0;
    m_seek_operations = 0;
    m_cache_hits = 0;
    m_cache_misses = 0;
    
    Debug::log("loader", "OggDemuxer constructor completed successfully");
    Debug::log("ogg", "OggDemuxer constructor completed successfully");
    Debug::log("demuxer", "OggDemuxer constructor completed successfully");
}

OggDemuxer::~OggDemuxer() {
    Debug::log("ogg", "OggDemuxer destructor called - cleaning up resources");
    
    // Thread-safe cleanup of all resources (Requirement 8.3)
    std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
    std::lock_guard<std::mutex> packet_lock(m_packet_queue_mutex);
    
    cleanupLiboggStructures_unlocked();
    cleanupPerformanceCaches_unlocked();
    
    Debug::log("ogg", "OggDemuxer destructor completed - all resources cleaned up");
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
        
        // Read initial data to parse headers with error recovery (Requirement 8.4)
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
        // Validate buffer size to prevent overflow (Requirement 8.4)
        if (!validateBufferSize(bytes, "readIntoSyncBuffer")) {
            reportError("IO", "Invalid buffer size for readIntoSyncBuffer", OggErrorCodes::DEMUX_EINVAL);
            return false;
        }
        
        // Limit to reasonable size to prevent memory exhaustion (following PAGE_SIZE_MAX)
        if (bytes > OggErrorCodes::PAGE_SIZE_MAX * 4) {
            Debug::log("ogg", "OggDemuxer: Large buffer size requested, clamping: ", bytes);
            bytes = OggErrorCodes::PAGE_SIZE_MAX * 4;
        }
        
        // Thread-safe access to libogg sync state and I/O operations
        std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
        std::lock_guard<std::mutex> io_lock(m_io_mutex);
        
        // Get buffer from libogg with error handling like _get_data() in libvorbisfile
        char* buffer = ogg_sync_buffer(&m_sync_state, static_cast<long>(bytes));
        
        // Add null pointer checks for all dynamic allocations (Requirement 8.5)
        if (!buffer) {
            // Memory allocation failure - return OP_EFAULT like reference implementations
            reportError("Memory", "ogg_sync_buffer failed - memory allocation failure", OggErrorCodes::DEMUX_EFAULT);
            return false;
        }
        
        // Validate buffer pointer before use (additional safety check)
        if (reinterpret_cast<uintptr_t>(buffer) < 4096) {
            // Suspicious pointer value, likely invalid
            reportError("Memory", "ogg_sync_buffer returned suspicious pointer", OggErrorCodes::DEMUX_EFAULT);
            return false;
        }
        
        // Additional memory safety: Check if buffer is within reasonable memory range
        if (reinterpret_cast<uintptr_t>(buffer) > UINTPTR_MAX - bytes) {
            // Buffer pointer + size would overflow
            reportError("Memory", "Buffer pointer would cause overflow", OggErrorCodes::DEMUX_EFAULT);
            return false;
        }
        
        // Validate file position before read
        off_t current_pos_before = m_handler->tell();
        if (current_pos_before < 0) {
            reportError("IO", "Invalid file position before read", OggErrorCodes::DEMUX_EREAD);
            return false;
        }
        
        // Perform optimized I/O operation with error handling like _get_data() patterns
        long bytes_read;
        if (!optimizedRead_unlocked(buffer, 1, bytes, bytes_read)) {
            reportError("IO", "Optimized read operation failed", OggErrorCodes::DEMUX_EREAD);
            return false;
        }
        
        Debug::log("ogg", "OggDemuxer: readIntoSyncBuffer at ", m_position_ms, "ms - pos_before=", current_pos_before, 
                       ", requested=", bytes, ", bytes_read=", bytes_read);
        
        // Handle I/O errors following reference implementation patterns
        if (bytes_read < 0) {
            reportError("IO", "I/O error during read operation", OggErrorCodes::DEMUX_EREAD);
            return false;
        }
        
        if (bytes_read == 0) {
            // Handle EOF like _get_data() in libvorbisfile
            if (m_handler->eof()) {
                Debug::log("ogg", "OggDemuxer: Reached end of file");
                setEOF(true);
                return false; // Return OP_EOF behavior
            } else {
                // Temporary unavailability (e.g., network stream)
                Debug::log("ogg", "OggDemuxer: No data available (temporary)");
                return false;
            }
        }
        
        // Validate bytes_read to prevent buffer overflow
        if (bytes_read > static_cast<long>(bytes)) {
            Debug::log("ogg", "OggDemuxer: Read more bytes than requested - clamping to prevent overflow");
            bytes_read = static_cast<long>(bytes);
        }
        
        // Submit data to libogg with error handling
        int sync_result = ogg_sync_wrote(&m_sync_state, bytes_read);
        if (sync_result != 0) {
            // libogg internal error - try to recover by resetting sync state
            Debug::log("ogg", "OggDemuxer: ogg_sync_wrote failed with code: ", sync_result);
            reportError("Processing", "ogg_sync_wrote failed", OggErrorCodes::DEMUX_EFAULT);
            
            // Recovery attempt: reset sync state like reference implementations
            ogg_sync_reset(&m_sync_state);
            return false;
        }
        
        // Log successful operation
        long pos_after_write = m_handler->tell();
        Debug::log("ogg", "OggDemuxer: readIntoSyncBuffer success - read ", bytes_read, " bytes, pos advanced from ", 
                       current_pos_before, " to ", pos_after_write);
        
        return true;
        
    } catch (const std::bad_alloc& e) {
        // Memory allocation failure - return OP_EFAULT like reference implementations
        reportError("Memory", "Memory allocation failed in readIntoSyncBuffer: " + std::string(e.what()), 
                   OggErrorCodes::DEMUX_EFAULT);
        return false;
    } catch (const std::exception& e) {
        // General exception handling
        reportError("Processing", "Exception in readIntoSyncBuffer: " + std::string(e.what()), 
                   OggErrorCodes::DEMUX_EFAULT);
        return false;
    }
}

bool OggDemuxer::processPages() {
    ogg_page page;
    bool processed_any_pages = false;
    int page_count = 0;
    
    try {
        // Thread-safe access to libogg sync state
        std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
        
        // Bounded parsing loop to prevent infinite hangs (Requirement 7.12)
        while (page_count < OggErrorCodes::MAX_HEADER_PAGES) {
            int pageout_result = ogg_sync_pageout(&m_sync_state, &page);
            
            if (pageout_result == 0) {
                // Need more data - this is normal
                break;
            } else if (pageout_result < 0) {
                // Corrupted page detected - handle using ogg_sync_pageseek() patterns (Requirement 7.1)
                Debug::log("ogg", "OggDemuxer: Corrupted page detected (pageout returned ", pageout_result, "), skipping");
                
                // Use ogg_sync_pageseek() for robust page discovery like libvorbisfile
                long seek_result = ogg_sync_pageseek(&m_sync_state, &page);
                if (seek_result < 0) {
                    // Bytes skipped due to corruption
                    Debug::log("ogg", "OggDemuxer: Skipped ", -seek_result, " corrupted bytes");
                    continue;
                } else if (seek_result == 0) {
                    // Need more data
                    break;
                }
                // seek_result > 0 means page found, continue processing
            }
            
            processed_any_pages = true;
            page_count++;
            
            // Validate page structure before accessing any fields (Requirement 7.1)
            if (!validateOggPage(&page)) {
                Debug::log("ogg", "OggDemuxer: Invalid page structure, skipping page");
                continue;
            }
            
            // Check page size limits (Requirement 7.12)
            size_t page_size = static_cast<size_t>(page.header_len + page.body_len);
            if (page_size > OggErrorCodes::PAGE_SIZE_MAX) {
                Debug::log("ogg", "OggDemuxer: Page size ", page_size, " exceeds maximum ", 
                          OggErrorCodes::PAGE_SIZE_MAX, ", skipping");
                reportError("Format", "Page size exceeds maximum allowed size", OggErrorCodes::DEMUX_EBADHEADER);
                continue;
            }
            
            uint32_t stream_id = ogg_page_serialno(&page);
            
            // Validate stream ID (Requirement 7.4)
            if (stream_id == 0) {
                Debug::log("ogg", "OggDemuxer: Invalid stream ID 0, skipping page");
                reportError("Format", "Invalid stream serial number", OggErrorCodes::DEMUX_EBADHEADER);
                continue;
            }
            
            // Initialize stream if it's new
            if (m_streams.find(stream_id) == m_streams.end()) {
                // Check for duplicate serial numbers (Requirement 7.4)
                if (m_ogg_streams.find(stream_id) != m_ogg_streams.end()) {
                    Debug::log("ogg", "OggDemuxer: Duplicate stream serial number ", stream_id);
                    reportError("Format", "Duplicate stream serial number detected", OggErrorCodes::DEMUX_EBADHEADER);
                    continue;
                }
                
                OggStream stream;
                stream.serial_number = stream_id;
                m_streams[stream_id] = stream;
                
                // Initialize libogg stream state with error handling (Requirement 7.5)
                if (ogg_stream_init(&m_ogg_streams[stream_id], stream_id) != 0) {
                    Debug::log("ogg", "OggDemuxer: Failed to initialize ogg stream for ID ", stream_id);
                    reportError("Memory", "Failed to initialize libogg stream state", OggErrorCodes::DEMUX_EFAULT);
                    m_streams.erase(stream_id);
                    continue;
                }
            }
            
            // Add page to stream with error handling following libogg patterns
            int pagein_result = ogg_stream_pagein(&m_ogg_streams[stream_id], &page);
            if (pagein_result != 0) {
                Debug::log("ogg", "OggDemuxer: Failed to add page to stream ", stream_id, ", error code: ", pagein_result);
                
                // Handle different pagein error conditions
                if (pagein_result < 0) {
                    // Page rejected by stream (wrong serial number, out of sequence, etc.)
                    reportError("Format", "Page rejected by stream (sequence error)", OggErrorCodes::DEMUX_EBADLINK);
                } else {
                    // Other error
                    reportError("Processing", "ogg_stream_pagein failed", OggErrorCodes::DEMUX_EFAULT);
                }
                continue;
            }
        
            // Perform periodic maintenance for memory safety
            performPeriodicMaintenance_unlocked();
            
            // Extract packets from this stream with error handling
            ogg_packet packet;
            int packet_result;
            int packet_count = 0;
            
            // Bounded packet extraction to prevent infinite loops
            while (packet_count < 100 && (packet_result = ogg_stream_packetout(&m_ogg_streams[stream_id], &packet))) {
                packet_count++;
                
                // Check for infinite loop in packet processing
                if (detectInfiniteLoop_unlocked("packet_extraction")) {
                    Debug::log("ogg", "OggDemuxer: Breaking packet extraction loop due to infinite loop detection");
                    break;
                }
                
                if (packet_result < 0) {
                    // Packet hole detected - return OV_HOLE/OP_HOLE like reference implementations (Requirement 7.3)
                    Debug::log("ogg", "OggDemuxer: Packet hole detected in stream ", stream_id, " (result=", packet_result, ")");
                    
                    // Continue processing - holes are recoverable in Ogg streams
                    continue;
                }
                
                // packet_result == 1: valid packet extracted
                
                // Validate packet structure (Requirement 7.3)
                if (!validateOggPacket(&packet, stream_id)) {
                    Debug::log("ogg", "OggDemuxer: Invalid packet from stream ", stream_id, ", skipping");
                    reportError("Format", "Invalid packet structure", OggErrorCodes::DEMUX_EBADPACKET);
                    continue;
                }
                
                // Validate packet size to prevent memory issues (Requirement 7.5)
                if (packet.bytes < 0 || static_cast<size_t>(packet.bytes) > OggErrorCodes::PAGE_SIZE_MAX) {
                    Debug::log("ogg", "OggDemuxer: Invalid packet size ", packet.bytes, " from stream ", stream_id);
                    reportError("Format", "Invalid packet size", OggErrorCodes::DEMUX_EBADPACKET);
                    continue;
                }
                
                Debug::log("ogg", "OggDemuxer: Extracted packet from stream ", stream_id, ", size=", packet.bytes);
                OggStream& stream = m_streams[stream_id];
                
                if (!stream.headers_complete) {
                    // Try to identify codec from first packet (Requirement 7.4)
                    if (stream.codec_name.empty()) {
                        try {
                            // Validate packet data before processing
                            if (packet.packet == nullptr || packet.bytes <= 0) {
                                Debug::log("ogg", "OggDemuxer: Invalid packet data for codec identification");
                                reportError("Format", "Invalid packet data", OggErrorCodes::DEMUX_EBADPACKET);
                                continue;
                            }
                            
                            std::vector<uint8_t> packet_data(packet.packet, packet.packet + packet.bytes);
                            stream.codec_name = identifyCodec(packet_data);
                            stream.codec_type = stream.codec_name.empty() ? "unknown" : "audio";
                            
                            // Handle codec identification failure (Requirement 7.4)
                            if (stream.codec_name.empty()) {
                                Debug::log("ogg", "OggDemuxer: Could not identify codec for stream ", stream_id);
                                reportError("Format", "Unknown codec type - returning OP_ENOTFORMAT", OggErrorCodes::DEMUX_ENOTFORMAT);
                                
                                // Continue scanning like libopusfile - don't fail completely
                                continue;
                            }
                            
                            Debug::log("ogg", "OggDemuxer: Identified codec '", stream.codec_name, "' for stream ", stream_id);
                            
                        } catch (const std::bad_alloc& e) {
                            // Memory allocation failure (Requirement 7.5)
                            Debug::log("ogg", "OggDemuxer: Memory allocation failed during codec identification: ", e.what());
                            reportError("Memory", "Memory allocation failed during codec identification", OggErrorCodes::DEMUX_EFAULT);
                            continue;
                        } catch (const std::exception& e) {
                            Debug::log("ogg", "OggDemuxer: Exception identifying codec: ", e.what());
                            reportError("Processing", "Exception during codec identification", OggErrorCodes::DEMUX_EFAULT);
                            continue;
                        }
                    }
                    
                    // Parse codec-specific headers with comprehensive error handling
                    try {
                        // Create OggPacket with minimal copy optimization (Requirement 8.5)
                        OggPacket ogg_packet;
                        
                        if (!processPacketWithMinimalCopy_unlocked(packet, stream_id, ogg_packet)) {
                            Debug::log("ogg", "OggDemuxer: Failed to process packet with minimal copy optimization");
                            reportError("Processing", "Failed to process packet efficiently", OggErrorCodes::DEMUX_EFAULT);
                            continue;
                        }
                
                        Debug::log("ogg", "OggDemuxer: Processing packet for stream ", stream_id, ", codec=", stream.codec_name, 
                                       ", packet_size=", packet.bytes, ", headers_complete=", stream.headers_complete);
                        
                        bool is_header_packet = false;
                        
                        // Parse headers with codec-specific error handling
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
                                // Unknown codec - handle like libopusfile (Requirement 7.4)
                                Debug::log("ogg", "OggDemuxer: Unknown codec ", stream.codec_name, " for stream ", stream_id);
                                reportError("Format", "Unknown codec type", OggErrorCodes::DEMUX_ENOTFORMAT);
                                is_header_packet = false;
                            }
                        } catch (const std::bad_alloc& e) {
                            // Memory allocation failure during header parsing (Requirement 7.5)
                            Debug::log("ogg", "OggDemuxer: Memory allocation failed during header parsing: ", e.what());
                            reportError("Memory", "Memory allocation failed during header parsing", OggErrorCodes::DEMUX_EFAULT);
                            is_header_packet = false;
                        } catch (const std::exception& e) {
                            // Malformed metadata handling (Requirement 7.8)
                            Debug::log("ogg", "OggDemuxer: Exception parsing headers for codec ", stream.codec_name, ": ", e.what());
                            reportError("Format", "Malformed header data - parsing what's possible", OggErrorCodes::DEMUX_EBADHEADER);
                            
                            // Parse what's possible and continue like opus_tags_parse() (Requirement 7.8)
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
                    
                    // Queue the packet for later processing with memory management (Requirement 8.2)
                    {
                        std::lock_guard<std::mutex> packet_lock(m_packet_queue_mutex);
                        if (enforcePacketQueueLimits_unlocked(stream_id)) {
                            stream.m_packet_queue.push_back(ogg_packet);
                            
                            // Track memory usage
                            size_t packet_memory = sizeof(OggPacket) + ogg_packet.data.size();
                            m_total_memory_usage.fetch_add(packet_memory);
                        }
                    }
                }
                
                } catch (const std::exception& e) {
                    Debug::log("ogg", "OggDemuxer: Exception creating OggPacket: ", e.what());
                    continue;
                }
            } else {
                    // Headers are complete, queue the packet for normal processing with memory management
                    try {
                        OggPacket data_packet{stream_id, 
                                            std::vector<uint8_t>(packet.packet, packet.packet + packet.bytes), 
                                            static_cast<uint64_t>(packet.granulepos), 
                                            (bool)packet.b_o_s, 
                                            (bool)packet.e_o_s};
                        
                        // Add to queue with memory management
                        {
                            std::lock_guard<std::mutex> packet_lock(m_packet_queue_mutex);
                            if (enforcePacketQueueLimits_unlocked(stream_id)) {
                                stream.m_packet_queue.push_back(data_packet);
                                
                                // Track memory usage
                                size_t packet_memory = sizeof(OggPacket) + data_packet.data.size();
                                m_total_memory_usage.fetch_add(packet_memory);
                            }
                        }
                        
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

    // First, send header packets if they haven't been sent yet (send once per stream, never resend after seeks)
    if (!stream.headers_sent && stream.headers_complete) {
        if (stream.next_header_index < stream.header_packets.size()) {
            const OggPacket& header_packet = stream.header_packets[stream.next_header_index];
            stream.next_header_index++;
            
            MediaChunk chunk;
            chunk.stream_id = stream_id;
            chunk.data = header_packet.data;
            chunk.granule_position = 0;
            chunk.is_keyframe = true;
            
            Debug::log("ogg", "OggDemuxer::readChunk - Sending header packet ", stream.next_header_index, 
                      " for stream ", stream_id, " (", header_packet.data.size(), " bytes)");
            
            if (stream.next_header_index >= stream.header_packets.size()) {
                stream.headers_sent = true;
                Debug::log("ogg", "OggDemuxer::readChunk - All header packets sent for stream ", stream_id);
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
        // Try to read more data using reference implementation patterns
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
        
        // Process packets using _fetch_and_process_packet patterns
        try {
            int fetch_result = fetchAndProcessPacket();
            if (fetch_result < 0) {
                // Error or EOF
                setEOF(true);
                break;
            } else if (fetch_result == 0) {
                // Need more data - continue reading
                retry_count++;
                if (retry_count >= max_retries) {
                    Debug::log("ogg", "OggDemuxer::readChunk - Max retries reached, setting EOF");
                    setEOF(true);
                    break;
                }
                continue;
            }
            // fetch_result == 1 means packet was processed successfully
            retry_count = 0; // Reset retry count on successful processing
        } catch (const std::exception& e) {
            reportError("Processing", "Exception in fetchAndProcessPacket: " + std::string(e.what()));
            if (isolateStreamError(stream_id, "fetchAndProcessPacket exception")) {
                retry_count++;
                continue;
            } else {
                setEOF(true);
                break;
            }
        }
        
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

    // Pop the next packet from the queue (thread-safe) with memory tracking
    MediaChunk chunk;
    {
        std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
        const OggPacket& ogg_packet = stream.m_packet_queue.front();
        
        chunk.stream_id = stream_id;
        chunk.data = ogg_packet.data;
        chunk.granule_position = ogg_packet.granule_position;

        // Track memory usage when removing packet (Requirement 8.2)
        size_t packet_memory = sizeof(OggPacket) + ogg_packet.data.size();
        m_total_memory_usage.fetch_sub(packet_memory);

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

void OggDemuxer::fillPacketQueue(uint32_t target_stream_id) {
    // Following _fetch_and_process_packet patterns from libvorbisfile
    Debug::log("ogg", "OggDemuxer::fillPacketQueue - Filling packet queue for stream ", target_stream_id);
    
    auto stream_it = m_streams.find(target_stream_id);
    if (stream_it == m_streams.end()) {
        Debug::log("ogg", "OggDemuxer::fillPacketQueue - Stream ", target_stream_id, " not found");
        return;
    }
    
    OggStream& stream = stream_it->second;
    
    // Check if queue is already full (bounded queue to prevent memory exhaustion)
    const size_t MAX_QUEUE_SIZE = 100; // Limit to prevent memory exhaustion
    {
        std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
        if (stream.m_packet_queue.size() >= MAX_QUEUE_SIZE) {
            Debug::log("ogg", "OggDemuxer::fillPacketQueue - Queue full for stream ", target_stream_id, 
                      " (", stream.m_packet_queue.size(), " packets)");
            return;
        }
    }
    
    // Read more data and process pages until we have packets for the target stream
    int attempts = 0;
    const int MAX_ATTEMPTS = 10;
    
    while (attempts < MAX_ATTEMPTS) {
        // Check if we already have packets for this stream
        {
            std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
            if (!stream.m_packet_queue.empty()) {
                Debug::log("ogg", "OggDemuxer::fillPacketQueue - Found packets for stream ", target_stream_id);
                return;
            }
        }
        
        // Try to read more data
        if (!readIntoSyncBuffer(4096)) {
            Debug::log("ogg", "OggDemuxer::fillPacketQueue - Failed to read more data, attempt ", attempts + 1);
            if (m_handler->eof()) {
                setEOF(true);
                return;
            }
            attempts++;
            continue;
        }
        
        // Process pages to extract packets
        if (!processPages()) {
            Debug::log("ogg", "OggDemuxer::fillPacketQueue - Failed to process pages, attempt ", attempts + 1);
            attempts++;
            continue;
        }
        
        attempts = 0; // Reset attempts on successful processing
    }
    
    Debug::log("ogg", "OggDemuxer::fillPacketQueue - Completed after ", attempts, " attempts");
}

int OggDemuxer::fetchAndProcessPacket() {
    // Following _fetch_and_process_packet patterns from libvorbisfile
    // Returns: 1 = packet processed, 0 = need more data, -1 = error/EOF
    
    try {
        // Thread-safe access to libogg sync state
        std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
        
        ogg_page page;
        int page_result = ogg_sync_pageout(&m_sync_state, &page);
        
        if (page_result == 0) {
            // Need more data
            Debug::log("ogg", "OggDemuxer::fetchAndProcessPacket - Need more data");
            return 0;
        }
        
        if (page_result < 0) {
            // Skipped bytes due to sync error - handle like reference implementations
            Debug::log("ogg", "OggDemuxer::fetchAndProcessPacket - Sync error, skipped ", -page_result, " bytes");
            return 0; // Continue processing like libvorbisfile
        }
        
        // Validate page structure
        if (!validateOggPage(&page)) {
            Debug::log("ogg", "OggDemuxer::fetchAndProcessPacket - Invalid page structure");
            return 0; // Skip invalid page like reference implementations
        }
        
        uint32_t stream_id = ogg_page_serialno(&page);
        
        // Initialize stream if it's new
        if (m_streams.find(stream_id) == m_streams.end()) {
            OggStream stream;
            stream.serial_number = stream_id;
            m_streams[stream_id] = stream;
            
            // Initialize libogg stream state
            if (ogg_stream_init(&m_ogg_streams[stream_id], stream_id) != 0) {
                Debug::log("ogg", "OggDemuxer::fetchAndProcessPacket - Failed to initialize stream ", stream_id);
                return -1;
            }
        }
        
        // Add page to stream using ogg_stream_pagein() patterns
        int pagein_result = ogg_stream_pagein(&m_ogg_streams[stream_id], &page);
        if (pagein_result != 0) {
            Debug::log("ogg", "OggDemuxer::fetchAndProcessPacket - Failed to add page to stream ", stream_id);
            return 0; // Continue like reference implementations
        }
        
        // Extract packets from this stream using ogg_stream_packetout() patterns
        ogg_packet packet;
        int packet_result;
        bool processed_any_packets = false;
        
        while ((packet_result = ogg_stream_packetout(&m_ogg_streams[stream_id], &packet)) == 1) {
            processed_any_packets = true;
            
            // Validate packet structure
            if (!validateOggPacket(&packet, stream_id)) {
                Debug::log("ogg", "OggDemuxer::fetchAndProcessPacket - Invalid packet from stream ", stream_id);
                continue;
            }
            
            OggStream& stream = m_streams[stream_id];
            
            // Create OggPacket structure with minimal copy optimization (Requirement 8.5)
            OggPacket ogg_packet;
            if (!processPacketWithMinimalCopy_unlocked(packet, stream_id, ogg_packet)) {
                Debug::log("ogg", "OggDemuxer: Failed to process packet with minimal copy in fetchAndProcessPacket");
                continue;
            }
            
            // Handle header packets vs data packets
            if (!stream.headers_complete) {
                // Try to identify codec from first packet if not already done
                if (stream.codec_name.empty()) {
                    stream.codec_name = identifyCodec(ogg_packet.data);
                    stream.codec_type = stream.codec_name.empty() ? "unknown" : "audio";
                }
                
                // Parse codec-specific headers
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
                    } else
#endif
                    if (stream.codec_name == "speex") {
                        is_header_packet = parseSpeexHeaders(stream, ogg_packet);
                    }
                } catch (const std::exception& e) {
                    Debug::log("ogg", "OggDemuxer::fetchAndProcessPacket - Exception parsing headers: ", e.what());
                    continue;
                }
                
                if (is_header_packet) {
                    stream.header_packets.push_back(ogg_packet);
                    
                    // Check if headers are complete (codec-specific logic)
                    if (stream.codec_name == "vorbis" && stream.header_packets.size() >= 3) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "flac" && stream.header_packets.size() >= 1) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "opus" && stream.header_packets.size() >= 2) {
                        stream.headers_complete = true;
                    }
                } else {
                    // Queue non-header packet for later processing with memory management
                    std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
                    if (enforcePacketQueueLimits_unlocked(stream_id)) {
                        stream.m_packet_queue.push_back(ogg_packet);
                        
                        // Track memory usage (Requirement 8.2)
                        size_t packet_memory = sizeof(OggPacket) + ogg_packet.data.size();
                        m_total_memory_usage.fetch_add(packet_memory);
                    }
                }
            } else {
                // Headers are complete, queue the data packet with memory management
                std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
                if (enforcePacketQueueLimits_unlocked(stream_id)) {
                    stream.m_packet_queue.push_back(ogg_packet);
                    
                    // Track memory usage (Requirement 8.2)
                    size_t packet_memory = sizeof(OggPacket) + ogg_packet.data.size();
                    m_total_memory_usage.fetch_add(packet_memory);
                    
                    // Update position tracking from granule positions using safe arithmetic
                    if (ogg_packet.granule_position != static_cast<uint64_t>(-1)) {
                        uint64_t current_ms = granuleToMs(ogg_packet.granule_position, stream_id);
                        std::lock_guard<std::mutex> state_lock(m_state_mutex);
                        if (current_ms > m_position_ms) {
                            m_position_ms = current_ms;
                        }
                    }
                }
            }
        }
        
        // Handle packet holes like reference implementations
        if (packet_result < 0) {
            // Packet hole detected - return OV_HOLE/OP_HOLE equivalent
            Debug::log("ogg", "OggDemuxer::fetchAndProcessPacket - Packet hole detected in stream ", stream_id);
            // In our implementation, we continue processing rather than returning an error
            // This matches the behavior of libvorbisfile/libopusfile
        }
        
        return processed_any_packets ? 1 : 0;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer::fetchAndProcessPacket - Exception: ", e.what());
        return -1;
    }
}

bool OggDemuxer::seekTo(uint64_t timestamp_ms) {
    try {
        // Find the best audio stream to use for seeking
        uint32_t stream_id = findBestAudioStream();
        if (stream_id == 0) {
            reportError("Seeking", "No valid audio stream found for seeking", OggErrorCodes::DEMUX_ENOTAUDIO);
            return false;
        }
        
        // Validate seek timestamp (thread-safe) - Requirement 7.7
        uint64_t current_duration;
        {
            std::lock_guard<std::mutex> lock(m_state_mutex);
            current_duration = m_duration_ms;
        }
        
        // Handle seeking beyond file boundaries (Requirement 7.7)
        if (timestamp_ms > current_duration && current_duration > 0) {
            Debug::log("ogg", "OggDemuxer: Seek timestamp ", timestamp_ms, "ms exceeds duration ", current_duration, "ms");
            
            uint64_t clamped_timestamp;
            if (handleSeekingError(timestamp_ms, clamped_timestamp)) {
                timestamp_ms = clamped_timestamp;
                Debug::log("ogg", "OggDemuxer: Clamped seek timestamp to ", timestamp_ms, "ms");
            } else {
                reportError("Seeking", "Failed to clamp seek timestamp to valid range", OggErrorCodes::DEMUX_EINVAL);
                return false;
            }
        }
        
        // Validate that the stream is seekable
        if (current_duration == 0) {
            Debug::log("ogg", "OggDemuxer: Stream duration unknown - may not be seekable");
            // Continue anyway - some streams may be seekable even without known duration
        }

        // Clear all stream packet queues (thread-safe) with memory tracking
        {
            std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
            for (auto& [id, stream] : m_streams) {
                // Calculate memory to free (Requirement 8.2)
                size_t queue_memory = 0;
                for (const auto& packet : stream.m_packet_queue) {
                    queue_memory += sizeof(OggPacket) + packet.data.size();
                }
                
                stream.m_packet_queue.clear();
                m_total_memory_usage.fetch_sub(queue_memory);
                
                Debug::log("ogg", "OggDemuxer: Cleared packet queue for stream ", id, 
                           " during seek, freed ", queue_memory, " bytes");
            }
        }
        
        // If seeking to beginning, do it directly with error handling
        if (timestamp_ms == 0) {
            try {
                // Seek to file beginning with I/O error handling (Requirement 7.6)
                {
                    std::lock_guard<std::mutex> io_lock(m_io_mutex);
                    if (m_handler->seek(0, SEEK_SET) != 0) {
                        reportError("IO", "Failed to seek to beginning of file", OggErrorCodes::DEMUX_EREAD);
                        return false;
                    }
                }
                
                // Reset libogg state like reference implementations (Requirement 7.6)
                {
                    std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
                    resetSyncStateAfterSeek_unlocked();
                    
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
                Debug::log("ogg", "OggDemuxer: Successfully seeked to beginning");
                return true;
                
            } catch (const std::exception& e) {
                reportError("Seeking", "Exception during seek to beginning: " + std::string(e.what()), 
                           OggErrorCodes::DEMUX_EREAD);
                return false;
            }
        }
    
        // Performance optimization: Check seek hints first (Requirement 8.3)
        m_seek_operations++;
        int64_t hint_offset = -1;
        uint64_t hint_granule = 0;
        
        {
            std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
            if (findBestSeekHint_unlocked(timestamp_ms, hint_offset, hint_granule)) {
                Debug::log("ogg", "OggDemuxer: Using seek hint for timestamp ", timestamp_ms, "ms");
                // TODO: Use hint_offset and hint_granule to optimize bisection search
            }
        }
        
        // Convert timestamp to granule position for seeking with validation
        uint64_t target_granule;
        try {
            target_granule = msToGranule(timestamp_ms, stream_id);
            
            // Validate granule position (Requirement 7.9)
            if (target_granule == static_cast<uint64_t>(-1)) {
                Debug::log("ogg", "OggDemuxer: Invalid granule position calculated for timestamp ", timestamp_ms);
                reportError("Seeking", "Invalid granule position calculated", OggErrorCodes::DEMUX_EBADTIMESTAMP);
                return false;
            }
            
        } catch (const std::exception& e) {
            reportError("Seeking", "Exception calculating target granule: " + std::string(e.what()), 
                       OggErrorCodes::DEMUX_EINVAL);
            return false;
        }
        
        // Performance optimization: Check page cache for nearby pages (Requirement 8.6)
        int64_t cache_offset = -1;
        uint64_t cache_granule = 0;
        
        {
            std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
            if (findCachedPageNearTarget_unlocked(target_granule, stream_id, cache_offset, cache_granule)) {
                Debug::log("ogg", "OggDemuxer: Using cached page for granule ", target_granule);
                // TODO: Use cache_offset and cache_granule to optimize bisection search
            }
        }
        
        // Use bisection search to find the target position with error recovery (Requirement 7.11)
        bool success = false;
        int seek_attempts = 0;
        const int max_seek_attempts = OggErrorCodes::MAX_RECOVERY_ATTEMPTS;
        
        while (!success && seek_attempts < max_seek_attempts) {
            seek_attempts++;
            
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
                    
                    Debug::log("ogg", "OggDemuxer: Successfully seeked to granule ", target_granule, 
                              " (timestamp ", timestamp_ms, "ms) on attempt ", seek_attempts);
                    break;
                    
                } else {
                    Debug::log("ogg", "OggDemuxer: Bisection search failed on attempt ", seek_attempts);
                    
                    // Fall back to linear search or return appropriate error codes (Requirement 7.11)
                    if (seek_attempts == max_seek_attempts) {
                        uint64_t clamped_timestamp;
                        if (handleSeekingError(timestamp_ms, clamped_timestamp)) {
                            success = true;
                            timestamp_ms = clamped_timestamp;
                            Debug::log("ogg", "OggDemuxer: Fallback seek successful to ", clamped_timestamp, "ms");
                        } else {
                            reportError("Seeking", "All seeking attempts failed", OggErrorCodes::DEMUX_ENOSEEK);
                        }
                    }
                }
                
            } catch (const std::exception& e) {
                Debug::log("ogg", "OggDemuxer: Exception during seek attempt ", seek_attempts, ": ", e.what());
                reportError("Seeking", "Exception during seek operation: " + std::string(e.what()), 
                           OggErrorCodes::DEMUX_EFAULT);
                
                // Try to recover by seeking to beginning on last attempt
                if (seek_attempts == max_seek_attempts && timestamp_ms != 0) {
                    Debug::log("ogg", "OggDemuxer: All seek attempts failed, trying fallback to beginning");
                    success = seekTo(0);
                    if (success) {
                        timestamp_ms = 0;
                    }
                }
            }
        }
        
        // Update position if successful
        if (success) {
            updatePosition(timestamp_ms);
            setEOF(false);
            Debug::log("ogg", "OggDemuxer: Seek completed successfully to ", timestamp_ms, "ms");
        } else {
            Debug::log("ogg", "OggDemuxer: Seek failed after all attempts");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        // Top-level exception handling
        reportError("Seeking", "Critical exception during seek operation: " + std::string(e.what()), 
                   OggErrorCodes::DEMUX_EFAULT);
        return false;
    }
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
    // Following reference implementation patterns for codec detection
    if (packet_data.empty()) {
        Debug::log("ogg", "OggDemuxer: identifyCodec - Empty packet data");
        return "";
    }
    
    // Log packet signature for debugging
    std::string signature_debug;
    for (size_t i = 0; i < std::min(size_t(16), packet_data.size()); i++) {
        char c = packet_data[i];
        if (c >= 32 && c <= 126) {
            signature_debug += c;
        } else {
            signature_debug += "\\x" + std::to_string(static_cast<unsigned char>(c));
        }
    }
    Debug::log("ogg", "OggDemuxer: identifyCodec - Checking packet signature: '", signature_debug, "' (", packet_data.size(), " bytes)");

#ifdef HAVE_VORBIS
    // Vorbis identification header: "\x01vorbis" (following vorbis_synthesis_idheader() logic)
    if (OggDemuxer::hasSignature(packet_data, "\x01vorbis")) {
        Debug::log("ogg", "OggDemuxer: identifyCodec - Identified as Vorbis");
        return "vorbis";
    }
#endif

#ifdef HAVE_FLAC
    // FLAC-in-Ogg identification header: "\x7fFLAC" (following Ogg FLAC specification)
    if (OggDemuxer::hasSignature(packet_data, "\x7f""FLAC")) {
        Debug::log("ogg", "OggDemuxer: identifyCodec - Identified as FLAC");
        return "flac";
    }
#endif

#ifdef HAVE_OPUS
    // Opus identification header: "OpusHead" (following opus_head_parse() logic from RFC 7845)
    if (OggDemuxer::hasSignature(packet_data, "OpusHead")) {
        Debug::log("ogg", "OggDemuxer: identifyCodec - Identified as Opus");
        return "opus";
    }
#endif

    // Speex identification header: "Speex   " (8 bytes with spaces)
    // Note: Speex support is not implemented yet
    if (OggDemuxer::hasSignature(packet_data, "Speex   ")) {
        Debug::log("ogg", "OggDemuxer: identifyCodec - Speex detected but not supported");
        return ""; // Return empty string for unsupported codecs
    }
    
    // Theora identification header: "\x80theora" (video codec, not supported in audio demuxer)
    if (packet_data.size() >= 7 && packet_data[0] == 0x80 && 
        OggDemuxer::hasSignature(packet_data, "\x80theora")) {
        Debug::log("ogg", "OggDemuxer: identifyCodec - Theora video detected (not supported in audio demuxer)");
        return ""; // Return empty string for video codecs
    }
    
    // Unknown codec - following libopusfile pattern of returning empty string
    Debug::log("ogg", "OggDemuxer: identifyCodec - Unknown codec signature");
    return "";
}


#ifdef HAVE_VORBIS
bool OggDemuxer::parseVorbisHeaders(OggStream& stream, const OggPacket& packet) {
    // Following libvorbisfile vorbis_synthesis_idheader() equivalent logic
    if (packet.data.size() < 7) {
        Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Packet too small (", packet.data.size(), " bytes)");
        return false;
    }
    
    uint8_t packet_type = packet.data[0];
    Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Processing packet type ", static_cast<int>(packet_type), ", size=", packet.data.size());
    
    if (packet_type == 1 && OggDemuxer::hasSignature(packet.data, "\x01vorbis")) {
        // Vorbis identification header (following vorbis_synthesis_idheader() logic)
        Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Vorbis identification header found");
        
        if (packet.data.size() < 30) {
            Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Vorbis ID header too small");
            return false;
        }
        
        // Validate Vorbis version (should be 0)
        uint32_t vorbis_version = OggDemuxer::readLE<uint32_t>(packet.data, 7);
        if (vorbis_version != 0) {
            Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Invalid Vorbis version: ", vorbis_version);
            return false;
        }
        
        // Extract audio parameters
        stream.channels = OggDemuxer::readLE<uint8_t>(packet.data, 11);
        stream.sample_rate = OggDemuxer::readLE<uint32_t>(packet.data, 12);
        
        // Validate parameters (following libvorbis validation)
        if (stream.channels == 0 || stream.channels > 255) {
            Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Invalid channel count: ", stream.channels);
            return false;
        }
        
        if (stream.sample_rate == 0) {
            Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Invalid sample rate: ", stream.sample_rate);
            return false;
        }
        
        // Extract bitrate information (optional, can be 0)
        stream.bitrate = OggDemuxer::readLE<uint32_t>(packet.data, 16); // Maximum bitrate
        
        // Validate blocksize information
        uint8_t blocksize_info = packet.data[28];
        uint8_t blocksize_0 = blocksize_info & 0x0F;
        uint8_t blocksize_1 = (blocksize_info >> 4) & 0x0F;
        
        if (blocksize_0 < 6 || blocksize_0 > 13 || blocksize_1 < 6 || blocksize_1 > 13 || blocksize_0 > blocksize_1) {
            Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Invalid blocksize info: ", static_cast<int>(blocksize_0), "/", static_cast<int>(blocksize_1));
            return false;
        }
        
        // Check framing bit (must be 1)
        if ((packet.data[29] & 0x01) != 0x01) {
            Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Invalid framing bit");
            return false;
        }
        
        Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Valid Vorbis ID header: channels=", stream.channels, 
                  ", sample_rate=", stream.sample_rate, ", bitrate=", stream.bitrate);
        return true;
        
    } else if (packet_type == 3 && OggDemuxer::hasSignature(packet.data, "\x03vorbis")) {
        // Vorbis comment header (following vorbis_synthesis_headerin() logic)
        Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Vorbis comment header found");
        parseVorbisComments(stream, packet);
        return true;
        
    } else if (packet_type == 5 && OggDemuxer::hasSignature(packet.data, "\x05vorbis")) {
        // Vorbis setup header (following vorbis_synthesis_headerin() logic)
        Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Vorbis setup header found");
        stream.codec_setup_data = packet.data;
        return true;
    }
    
    Debug::log("ogg", "OggDemuxer: parseVorbisHeaders - Unknown Vorbis packet type: ", static_cast<int>(packet_type));
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
    // Following RFC 9639 FLAC specification and Ogg FLAC mapping
    if (OggDemuxer::hasSignature(packet.data, "\x7f""FLAC")) {
        Debug::log("ogg", "OggDemuxer: parseFLACHeaders - Ogg FLAC identification header found, packet_size=", packet.data.size());
        
        // Ogg FLAC identification header structure:
        // 5 bytes: "\x7fFLAC" signature
        // 1 byte: major version
        // 1 byte: minor version  
        // 2 bytes: number of header packets (big-endian)
        // 4 bytes: "fLaC" native FLAC signature
        // 4 bytes: STREAMINFO metadata block header
        // 34 bytes: STREAMINFO data
        
        if (packet.data.size() >= 51) { // 5 + 1 + 1 + 2 + 4 + 4 + 34 = 51 bytes minimum
            // Validate native FLAC signature at offset 9
            if (packet.data[9] != 'f' || packet.data[10] != 'L' || 
                packet.data[11] != 'a' || packet.data[12] != 'C') {
                Debug::log("ogg", "OggDemuxer: parseFLACHeaders - Invalid native FLAC signature");
                return false;
            }
            
            // STREAMINFO metadata block header at offset 13-16
            // Should be: 0x80 (last block flag + type 0), 0x00, 0x00, 0x22 (length 34)
            if (packet.data[13] != 0x80 || packet.data[16] != 0x22) {
                Debug::log("ogg", "OggDemuxer: parseFLACHeaders - Invalid STREAMINFO block header");
                return false;
            }
            
            // STREAMINFO data starts at offset 17
            size_t streaminfo_offset = 17;
            
            if (packet.data.size() >= streaminfo_offset + 34) {
                Debug::log("ogg", "OggDemuxer: parseFLACHeaders - Parsing STREAMINFO block at offset ", streaminfo_offset);
                
                // Validate STREAMINFO structure
                // Min block size (2 bytes) at offset 17-18
                // Max block size (2 bytes) at offset 19-20  
                // Min frame size (3 bytes) at offset 21-23
                // Max frame size (3 bytes) at offset 24-26
                // Audio info (8 bytes) at offset 27-34
                size_t audio_info_offset = streaminfo_offset + 10; // Should be offset 27
                
                Debug::log("ogg", "OggDemuxer: parseFLACHeaders - Audio info at offset ", audio_info_offset, 
                          " (packet size ", packet.data.size(), ")");
                
                // Parse sample rate (20 bits), channels-1 (3 bits), bits per sample-1 (5 bits), total samples high (4 bits)
                // Following RFC 9639 FLAC STREAMINFO format
                uint32_t info1 = OggDemuxer::readBE<uint32_t>(packet.data, audio_info_offset);
                
                stream.sample_rate = (info1 >> 12) & 0xFFFFF;  // 20 bits
                stream.channels = ((info1 >> 9) & 0x7) + 1;    // 3 bits + 1
                uint32_t bits_per_sample = ((info1 >> 4) & 0x1F) + 1; // 5 bits + 1
                
                // Extract total samples (36 bits total, 4 bits in first word, 32 bits in second word)
                uint64_t total_samples_high = info1 & 0xF; // 4 bits
                uint32_t total_samples_low = OggDemuxer::readBE<uint32_t>(packet.data, audio_info_offset + 4);
                stream.total_samples = (total_samples_high << 32) | total_samples_low;
                
                Debug::log("ogg", "OggDemuxer: parseFLACHeaders - Parsed STREAMINFO: sample_rate=", stream.sample_rate, 
                          ", channels=", stream.channels, ", bits_per_sample=", bits_per_sample, 
                          ", total_samples=", stream.total_samples);
            } else {
                Debug::log("ogg", "OggDemuxer: parseFLACHeaders - Packet too small for complete STREAMINFO");
                return false;
            }
        } else {
            Debug::log("ogg", "OggDemuxer: parseFLACHeaders - Packet too small for Ogg FLAC header");
            return false;
        }
        return true;
    }
    
    return false;
}
#endif // HAVE_FLAC

#ifdef HAVE_OPUS
bool OggDemuxer::parseOpusHeaders(OggStream& stream, const OggPacket& packet) {
    // Following libopusfile opus_head_parse() equivalent logic
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
        // Opus identification header (following opus_head_parse() logic from RFC 7845)
        Debug::log("ogg", "OggDemuxer: parseOpusHeaders - OpusHead identification header found");
        
        if (packet.data.size() < 19) {
            Debug::log("ogg", "OggDemuxer: parseOpusHeaders - OpusHead header too small (", packet.data.size(), " bytes, need 19)");
            return false;
        }
        
        // Validate OpusHead version (should be 1)
        uint8_t version = packet.data[8];
        if (version != 1) {
            Debug::log("ogg", "OggDemuxer: parseOpusHeaders - Invalid OpusHead version: ", static_cast<int>(version));
            return false;
        }
        
        // Extract channel count
        stream.channels = OggDemuxer::readLE<uint8_t>(packet.data, 9);
        if (stream.channels == 0 || stream.channels > 255) {
            Debug::log("ogg", "OggDemuxer: parseOpusHeaders - Invalid channel count: ", stream.channels);
            return false;
        }
        
        // Extract pre-skip (number of samples to skip at beginning)
        stream.pre_skip = OggDemuxer::readLE<uint16_t>(packet.data, 10);
        
        // Extract input sample rate (for informational purposes only)
        uint32_t input_sample_rate = OggDemuxer::readLE<uint32_t>(packet.data, 12);
        
        // Opus always operates at 48kHz internally (RFC 7845 Section 4.2)
        stream.sample_rate = 48000;
        
        // Extract output gain (optional, can be 0)
        int16_t output_gain = static_cast<int16_t>(OggDemuxer::readLE<uint16_t>(packet.data, 16));
        
        // Extract channel mapping family (0 = mono/stereo, 1 = surround)
        uint8_t channel_mapping_family = packet.data[18];
        
        // Validate channel mapping family
        if (channel_mapping_family > 1) {
            Debug::log("ogg", "OggDemuxer: parseOpusHeaders - Unsupported channel mapping family: ", static_cast<int>(channel_mapping_family));
            // Continue anyway - we can still decode basic streams
        }
        
        // For channel mapping family 1, need additional data
        if (channel_mapping_family == 1) {
            if (packet.data.size() < static_cast<size_t>(21 + stream.channels)) {
                Debug::log("ogg", "OggDemuxer: parseOpusHeaders - OpusHead header too small for channel mapping");
                return false;
            }
            // Skip detailed channel mapping validation for now
        }
        
        Debug::log("ogg", "OggDemuxer: parseOpusHeaders - Valid OpusHead header: channels=", stream.channels, 
                  ", pre_skip=", stream.pre_skip, ", input_sample_rate=", input_sample_rate, 
                  ", output_gain=", output_gain, ", mapping_family=", static_cast<int>(channel_mapping_family));
        return true;
        
    } else if (OggDemuxer::hasSignature(packet.data, "OpusTags")) {
        // Opus comment header (following opus_tags_parse() logic)
        Debug::log("ogg", "OggDemuxer: parseOpusHeaders - OpusTags comment header found, packet_size=", packet.data.size());
        
        // Parse OpusTags metadata (follows Vorbis comment format per RFC 7845)
        parseOpusTags(stream, packet);
        return true;
    }
    
    Debug::log("ogg", "OggDemuxer: parseOpusHeaders - Unknown Opus packet, size=", packet.data.size());
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
    Debug::log("ogg", "OggDemuxer: calculateDuration() called - following libvorbisfile/libopusfile patterns");
    m_duration_ms = 0;
    
    // Strategy 1: Prefer header-provided total sample counts (libopusfile pattern)
    uint64_t longest_stream_duration = 0;
    uint32_t longest_stream_id = 0;
    
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.sample_rate > 0 && stream.total_samples > 0) {
            Debug::log("ogg", "OggDemuxer: calculateDuration - Stream ", stream_id, " (", stream.codec_name, 
                       ") has total_samples=", stream.total_samples, ", sample_rate=", stream.sample_rate);
            
            // Calculate duration using codec-specific granule-to-time conversion
            uint64_t granule_from_samples = 0;
            if (stream.codec_name == "opus") {
                // For Opus, granule position includes pre-skip (libopusfile pattern)
                granule_from_samples = stream.total_samples + stream.pre_skip;
            } else {
                // For Vorbis and FLAC, granule position equals sample count
                granule_from_samples = stream.total_samples;
            }
            
            uint64_t stream_duration = granuleToMs(granule_from_samples, stream_id);
            Debug::log("ogg", "OggDemuxer: calculateDuration - Stream ", stream_id, " duration from samples: ", stream_duration, "ms");
            
            // Use the longest stream for duration calculation (libvorbisfile pattern)
            if (stream_duration > longest_stream_duration) {
                longest_stream_duration = stream_duration;
                longest_stream_id = stream_id;
            }
        }
    }
    
    if (longest_stream_duration > 0) {
        m_duration_ms = longest_stream_duration;
        Debug::log("ogg", "OggDemuxer: calculateDuration - Using longest stream ", longest_stream_id, " duration: ", m_duration_ms, "ms");
        return;
    }
    
    // Strategy 2: Use tracked maximum granule position during parsing
    if (m_max_granule_seen > 0) {
        Debug::log("ogg", "OggDemuxer: calculateDuration - Using tracked max granule: ", m_max_granule_seen);
        uint32_t stream_id = findBestAudioStream();
        if (stream_id != 0) {
            m_duration_ms = granuleToMs(m_max_granule_seen, stream_id);
            Debug::log("ogg", "OggDemuxer: calculateDuration - Duration from tracked max granule: ", m_max_granule_seen, " -> ", m_duration_ms, "ms");
            return;
        }
    }
    
    // Strategy 3: Fall back to reading last page using op_get_last_page() patterns
    Debug::log("ogg", "OggDemuxer: calculateDuration - Falling back to reading last page using op_get_last_page patterns");
    uint64_t last_granule = getLastGranulePosition();
    Debug::log("ogg", "OggDemuxer: calculateDuration - getLastGranulePosition returned: ", last_granule);
    
    if (last_granule > 0) {
        uint32_t stream_id = findBestAudioStream();
        if (stream_id != 0) {
            m_duration_ms = granuleToMs(last_granule, stream_id);
            Debug::log("ogg", "OggDemuxer: calculateDuration - Duration from last granule: ", m_duration_ms, "ms");
            return;
        }
    }
    
    // Strategy 4: Report unknown duration rather than incorrect values (requirement 6.9)
    Debug::log("ogg", "OggDemuxer: calculateDuration - Could not determine duration - reporting unknown (0)");
    m_duration_ms = 0; // Report unknown duration rather than incorrect values
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
        
        // Use safe granule arithmetic to subtract pre-skip
        int64_t diff_result;
        int diff_status = const_cast<OggDemuxer*>(this)->granposDiff(&diff_result, 
                                                                     static_cast<int64_t>(granule), 
                                                                     static_cast<int64_t>(stream.pre_skip));
        
        if (diff_status != 0 || diff_result < 0) {
            // If safe subtraction failed or result is negative, we're in pre-skip region
            effective_granule = 0;
            Debug::log("ogg", "OggDemuxer: granuleToMs (Opus) - Safe granule subtraction failed or negative result, using 0");
        } else {
            effective_granule = static_cast<uint64_t>(diff_result);
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
        
        // Use safe granule arithmetic to add pre-skip
        int64_t result_granule;
        int add_result = const_cast<OggDemuxer*>(this)->granposAdd(&result_granule, 
                                                                   static_cast<int64_t>(samples_48k), 
                                                                   static_cast<int32_t>(stream.pre_skip));
        
        if (add_result != 0 || result_granule < 0) {
            Debug::log("ogg", "OggDemuxer: msToGranule (Opus) - Safe granule addition failed, falling back to simple addition");
            uint64_t result = samples_48k + stream.pre_skip;
            Debug::log("ogg", "OggDemuxer: msToGranule (Opus) - samples_48k=", samples_48k, ", pre_skip=", stream.pre_skip, ", result=", result);
            return result;
        }
        
        uint64_t result = static_cast<uint64_t>(result_granule);
        Debug::log("ogg", "OggDemuxer: msToGranule (Opus) - samples_48k=", samples_48k, ", pre_skip=", stream.pre_skip, ", result=", result, " (using safe arithmetic)");
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

// Safe granule position arithmetic functions (following libopusfile patterns)

int OggDemuxer::granposAdd(int64_t* dst_gp, int64_t src_gp, int32_t delta) {
    // Following op_granpos_add() patterns from libopusfile
    // Handle invalid granule position (-1) like reference implementations
    
    if (!dst_gp) {
        Debug::log("ogg", "OggDemuxer: granposAdd - NULL destination pointer");
        return -1;
    }
    
    // Invalid granule position (-1) in two's complement
    if (src_gp == -1) {
        Debug::log("ogg", "OggDemuxer: granposAdd - Invalid source granule position (-1)");
        *dst_gp = -1;
        return -1;
    }
    
    // Check for overflow that would wrap past -1
    // Range organization: [ -1 (invalid) ][ 0 ... INT64_MAX ][ INT64_MIN ... -2 ][ -1 (invalid) ]
    
    int64_t result;
    
    if (delta >= 0) {
        // Adding positive delta
        if (src_gp > INT64_MAX - delta) {
            // Would overflow past INT64_MAX, wrapping to negative range
            result = src_gp + delta; // This will wrap to negative
            
            // Check if we wrapped past -1 (invalid)
            if (result == -1) {
                Debug::log("ogg", "OggDemuxer: granposAdd - Overflow wrapped to invalid granule position (-1)");
                *dst_gp = -1;
                return -1;
            }
        } else {
            result = src_gp + delta;
        }
    } else {
        // Adding negative delta (subtraction)
        if (src_gp < INT64_MIN - delta) {
            // Would underflow past INT64_MIN
            Debug::log("ogg", "OggDemuxer: granposAdd - Underflow detected, src_gp=", src_gp, ", delta=", delta);
            *dst_gp = -1;
            return -1;
        } else {
            result = src_gp + delta;
            
            // Check if result is invalid (-1)
            if (result == -1) {
                Debug::log("ogg", "OggDemuxer: granposAdd - Result is invalid granule position (-1)");
                *dst_gp = -1;
                return -1;
            }
        }
    }
    
    *dst_gp = result;
    Debug::log("ogg", "OggDemuxer: granposAdd - Success: src_gp=", src_gp, ", delta=", delta, ", result=", result);
    return 0;
}

int OggDemuxer::granposDiff(int64_t* delta, int64_t gp_a, int64_t gp_b) {
    // Following op_granpos_diff() patterns from libopusfile
    // Handle wraparound from positive to negative values correctly
    
    if (!delta) {
        Debug::log("ogg", "OggDemuxer: granposDiff - NULL delta pointer");
        return -1;
    }
    
    // Invalid granule positions (-1) in two's complement
    if (gp_a == -1 || gp_b == -1) {
        Debug::log("ogg", "OggDemuxer: granposDiff - Invalid granule position: gp_a=", gp_a, ", gp_b=", gp_b);
        *delta = 0;
        return -1;
    }
    
    // Calculate difference with wraparound handling
    // Range organization: [ -1 (invalid) ][ 0 ... INT64_MAX ][ INT64_MIN ... -2 ][ -1 (invalid) ]
    
    int64_t result = gp_a - gp_b;
    
    // Check for wraparound conditions
    if (gp_a >= 0 && gp_b < 0) {
        // gp_a is in positive range, gp_b is in negative range
        // This is a normal case where gp_a > gp_b
        // No special handling needed
    } else if (gp_a < 0 && gp_b >= 0) {
        // gp_a is in negative range, gp_b is in positive range
        // This means gp_a > gp_b (negative values are larger in granule position ordering)
        // No special handling needed for the subtraction
    }
    
    // Check if result is invalid (-1)
    if (result == -1) {
        Debug::log("ogg", "OggDemuxer: granposDiff - Result is invalid granule position (-1)");
        *delta = 0;
        return -1;
    }
    
    *delta = result;
    Debug::log("ogg", "OggDemuxer: granposDiff - Success: gp_a=", gp_a, ", gp_b=", gp_b, ", delta=", result);
    return 0;
}

int OggDemuxer::granposCmp(int64_t gp_a, int64_t gp_b) {
    // Following op_granpos_cmp() patterns from libopusfile
    // Handle proper ordering with invalid granule positions
    
    // Invalid granule positions (-1) are always equal to each other and less than valid positions
    if (gp_a == -1 && gp_b == -1) {
        return 0; // Both invalid, equal
    }
    if (gp_a == -1) {
        return -1; // Invalid is less than valid
    }
    if (gp_b == -1) {
        return 1; // Valid is greater than invalid
    }
    
    // Both are valid granule positions
    // Range organization: [ 0 ... INT64_MAX ][ INT64_MIN ... -2 ]
    // Negative values (INT64_MIN to -2) are considered larger than positive values
    
    bool a_is_negative = (gp_a < 0);
    bool b_is_negative = (gp_b < 0);
    
    if (a_is_negative && !b_is_negative) {
        // gp_a is in negative range (larger), gp_b is in positive range (smaller)
        return 1;
    } else if (!a_is_negative && b_is_negative) {
        // gp_a is in positive range (smaller), gp_b is in negative range (larger)
        return -1;
    } else {
        // Both in same range, use normal comparison
        if (gp_a < gp_b) {
            return -1;
        } else if (gp_a > gp_b) {
            return 1;
        } else {
            return 0;
        }
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
    
    Debug::log("ogg", "OggDemuxer: getLastGranulePosition - file_size=", m_file_size, " - following op_get_last_page patterns");
    
    // Save current position
    long current_pos = m_handler->tell();
    
    // Strategy 1: Prefer header-provided total sample counts (libopusfile pattern)
    uint64_t header_granule = getLastGranuleFromHeaders();
    if (header_granule > 0) {
        Debug::log("ogg", "OggDemuxer: getLastGranulePosition - using header-provided granule=", header_granule);
        m_handler->seek(current_pos, SEEK_SET);
        return header_granule;
    }
    
    // Strategy 2: Chunk-based backward scanning with exponentially increasing chunk sizes
    // Following op_get_last_page() patterns from libopusfile
    uint64_t last_granule = 0;
    
    // Start with CHUNKSIZE and exponentially increase (libopusfile pattern)
    size_t chunk_size = CHUNKSIZE; // 64KB initial chunk
    const size_t max_chunk_size = 4 * 1024 * 1024; // 4MB maximum
    
    while (chunk_size <= m_file_size && last_granule == 0) {
        size_t scan_size = std::min(chunk_size, m_file_size);
        int64_t scan_start = std::max(0L, static_cast<int64_t>(m_file_size) - static_cast<int64_t>(scan_size));
        
        Debug::log("ogg", "OggDemuxer: getLastGranulePosition - backward scan chunk_size=", scan_size, ", scan_start=", scan_start);
        
        if (m_handler->seek(scan_start, SEEK_SET) != 0) {
            Debug::log("ogg", "OggDemuxer: getLastGranulePosition - seek failed for scan_start=", scan_start);
            break;
        }
        
        // Use getPrevPageSerial() patterns to find last valid page with serial number preference
        last_granule = scanBackwardForLastGranule(scan_start, scan_size);
        
        if (last_granule > 0) {
            Debug::log("ogg", "OggDemuxer: getLastGranulePosition - found granule=", last_granule, " with chunk_size=", scan_size);
            break;
        }
        
        // Exponentially increase chunk size (libopusfile pattern)
        if (chunk_size >= max_chunk_size || chunk_size >= m_file_size) {
            break;
        }
        chunk_size *= 2;
    }
    
    // Strategy 3: If backward scanning failed, try forward scanning from reasonable position
    if (last_granule == 0 && m_file_size > 1048576) {
        Debug::log("ogg", "OggDemuxer: getLastGranulePosition - backward scan failed, trying forward scan");
        
        // Start scanning from 75% of file size
        int64_t forward_start = static_cast<int64_t>(m_file_size * 3 / 4);
        if (m_handler->seek(forward_start, SEEK_SET) == 0) {
            last_granule = scanForwardForLastGranule(forward_start);
        }
    }
    
    // Restore original position
    m_handler->seek(current_pos, SEEK_SET);
    
    Debug::log("ogg", "OggDemuxer: getLastGranulePosition - final result=", last_granule);
    
    return last_granule;
}

uint64_t OggDemuxer::scanBackwardForLastGranule(int64_t scan_start, size_t scan_size) {
    Debug::log("ogg", "OggDemuxer: scanBackwardForLastGranule - scan_start=", scan_start, ", scan_size=", scan_size);
    
    std::vector<uint8_t> buffer(scan_size);
    long bytes_read = m_handler->read(buffer.data(), 1, scan_size);
    
    if (bytes_read <= 0) {
        Debug::log("ogg", "OggDemuxer: scanBackwardForLastGranule - read failed, bytes_read=", bytes_read);
        return 0;
    }
    
    uint64_t last_granule = 0;
    uint32_t preferred_serial = 0;
    bool found_preferred_serial = false;
    
    // Find the best audio stream serial number for preference (libopusfile pattern)
    uint32_t best_audio_stream = findBestAudioStream();
    if (best_audio_stream != 0) {
        auto stream_it = m_streams.find(best_audio_stream);
        if (stream_it != m_streams.end()) {
            preferred_serial = stream_it->second.serial_number;
            found_preferred_serial = true;
            Debug::log("ogg", "OggDemuxer: scanBackwardForLastGranule - preferred_serial=", preferred_serial);
        }
    }
    
    // Scan backward through buffer for "OggS" signatures (libopusfile pattern)
    // This gives us the most recent pages first
    if (bytes_read >= 4) {
        for (int64_t i = bytes_read - 4; i >= 0; i--) {
            size_t check_pos = static_cast<size_t>(i);
            
            if (buffer[check_pos] == 'O' && buffer[check_pos + 1] == 'g' && 
                buffer[check_pos + 2] == 'g' && buffer[check_pos + 3] == 'S') {
                
                // Validate this looks like a real Ogg page header
                if (check_pos + 27 <= static_cast<size_t>(bytes_read)) {
                    // Check version (should be 0)
                    if (buffer[check_pos + 4] != 0) {
                        continue;
                    }
                    
                    // Extract serial number (little-endian, 4 bytes at offset 14)
                    uint32_t serial_number = 0;
                    for (int j = 0; j < 4; j++) {
                        serial_number |= (static_cast<uint32_t>(buffer[check_pos + 14 + j]) << (j * 8));
                    }
                    
                    // Extract granule position (little-endian, 8 bytes at offset 6)
                    uint64_t granule = 0;
                    for (int j = 0; j < 8; j++) {
                        granule |= (static_cast<uint64_t>(buffer[check_pos + 6 + j]) << (j * 8));
                    }
                    
                    // Validate granule position (not -1 and reasonable range)
                    if (granule != static_cast<uint64_t>(-1) && granule < 0x7FFFFFFFFFFFFFFULL) {
                        // Check page segments count is reasonable
                        uint8_t page_segments = buffer[check_pos + 26];
                        if (page_segments <= 255 && check_pos + 27 + page_segments <= static_cast<size_t>(bytes_read)) {
                            
                            // Prefer pages with matching serial number (libopusfile pattern)
                            if (found_preferred_serial && serial_number == preferred_serial) {
                                if (granule > last_granule) {
                                    last_granule = granule;
                                    Debug::log("ogg", "OggDemuxer: scanBackwardForLastGranule - found preferred page at offset ", check_pos, 
                                               ", granule=", granule, ", serial=", serial_number);
                                }
                            } else if (!found_preferred_serial || last_granule == 0) {
                                // Use any valid granule if no preferred serial or no preferred granule found yet
                                if (granule > last_granule) {
                                    last_granule = granule;
                                    Debug::log("ogg", "OggDemuxer: scanBackwardForLastGranule - found page at offset ", check_pos, 
                                               ", granule=", granule, ", serial=", serial_number);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    Debug::log("ogg", "OggDemuxer: scanBackwardForLastGranule - last_granule=", last_granule);
    return last_granule;
}

uint64_t OggDemuxer::scanChunkForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size, 
                                                     uint32_t preferred_serial, bool has_preferred_serial) {
    uint64_t last_granule = 0;
    
    // Scan forward through buffer for "OggS" signatures
    if (buffer_size >= 4) {
        for (size_t i = 0; i <= buffer_size - 4; i++) {
            if (buffer[i] == 'O' && buffer[i + 1] == 'g' && 
                buffer[i + 2] == 'g' && buffer[i + 3] == 'S') {
                
                // Validate this looks like a real Ogg page header
                if (i + 27 <= buffer_size) {
                    // Check version (should be 0)
                    if (buffer[i + 4] != 0) {
                        continue;
                    }
                    
                    // Extract serial number (little-endian, 4 bytes at offset 14)
                    uint32_t serial_number = 0;
                    for (int j = 0; j < 4; j++) {
                        serial_number |= (static_cast<uint32_t>(buffer[i + 14 + j]) << (j * 8));
                    }
                    
                    // Extract granule position (little-endian, 8 bytes at offset 6)
                    uint64_t granule = 0;
                    for (int j = 0; j < 8; j++) {
                        granule |= (static_cast<uint64_t>(buffer[i + 6 + j]) << (j * 8));
                    }
                    
                    // Validate granule position (not -1 and reasonable range)
                    if (granule != static_cast<uint64_t>(-1) && granule < 0x7FFFFFFFFFFFFFFULL) {
                        // Check page segments count is reasonable
                        uint8_t page_segments = buffer[i + 26];
                        if (page_segments <= 255 && i + 27 + page_segments <= buffer_size) {
                            
                            // Prefer pages with matching serial number (libopusfile pattern)
                            if (has_preferred_serial && serial_number == preferred_serial) {
                                if (granule > last_granule) {
                                    last_granule = granule;
                                }
                            } else if (!has_preferred_serial || last_granule == 0) {
                                // Use any valid granule if no preferred serial or no preferred granule found yet
                                if (granule > last_granule) {
                                    last_granule = granule;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
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

uint64_t OggDemuxer::scanForwardForLastGranule(int64_t start_position) {
    Debug::log("ogg", "OggDemuxer: scanForwardForLastGranule - start_position=", start_position);
    
    uint64_t last_granule = 0;
    uint32_t preferred_serial = 0;
    bool found_preferred_serial = false;
    
    // Find the best audio stream serial number for preference (libopusfile pattern)
    uint32_t best_audio_stream = findBestAudioStream();
    if (best_audio_stream != 0) {
        auto stream_it = m_streams.find(best_audio_stream);
        if (stream_it != m_streams.end()) {
            preferred_serial = stream_it->second.serial_number;
            found_preferred_serial = true;
            Debug::log("ogg", "OggDemuxer: scanForwardForLastGranule - preferred_serial=", preferred_serial);
        }
    }
    
    std::vector<uint8_t> buffer(CHUNKSIZE); // Use CHUNKSIZE constant
    int64_t current_pos = start_position;
    
    while (current_pos < static_cast<int64_t>(m_file_size)) {
        if (m_handler->seek(current_pos, SEEK_SET) != 0) {
            break;
        }
        
        size_t bytes_to_read = std::min(CHUNKSIZE, static_cast<size_t>(m_file_size - current_pos));
        long bytes_read = m_handler->read(buffer.data(), 1, bytes_to_read);
        
        if (bytes_read <= 0) {
            break;
        }
        
        // Scan this chunk for pages with serial number preference
        uint64_t chunk_granule = scanChunkForLastGranule(buffer, bytes_read, preferred_serial, found_preferred_serial);
        if (chunk_granule > last_granule) {
            last_granule = chunk_granule;
        }
        
        current_pos += bytes_read;
    }
    
    Debug::log("ogg", "OggDemuxer: scanForwardForLastGranule - last_granule=", last_granule);
    return last_granule;
}

uint64_t OggDemuxer::getLastGranuleFromHeaders() {
    uint64_t max_granule = 0;
    
    // Check if any stream headers contain total sample information (libopusfile pattern)
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.total_samples > 0) {
            Debug::log("ogg", "OggDemuxer: getLastGranuleFromHeaders - stream ", stream_id, 
                       " has total_samples=", stream.total_samples, ", codec=", stream.codec_name);
            
            // Convert total samples to granule position based on codec (following reference patterns)
            uint64_t granule_from_samples = 0;
            
            if (stream.codec_name == "opus") {
                // For Opus, granule position uses 48kHz rate and includes pre-skip (libopusfile pattern)
                // Following opus_granule_sample() equivalent logic
                granule_from_samples = stream.total_samples + stream.pre_skip;
                Debug::log("ogg", "OggDemuxer: getLastGranuleFromHeaders - Opus: total_samples=", stream.total_samples, 
                           ", pre_skip=", stream.pre_skip, ", granule=", granule_from_samples);
            } else if (stream.codec_name == "vorbis") {
                // For Vorbis, granule position equals sample count at codec sample rate (libvorbisfile pattern)
                granule_from_samples = stream.total_samples;
                Debug::log("ogg", "OggDemuxer: getLastGranuleFromHeaders - Vorbis: total_samples=", stream.total_samples, 
                           ", granule=", granule_from_samples);
            } else if (stream.codec_name == "flac") {
                // For FLAC-in-Ogg, granule position equals sample count like Vorbis
                granule_from_samples = stream.total_samples;
                Debug::log("ogg", "OggDemuxer: getLastGranuleFromHeaders - FLAC: total_samples=", stream.total_samples, 
                           ", granule=", granule_from_samples);
            } else {
                // For other codecs, use sample count as granule position
                granule_from_samples = stream.total_samples;
                Debug::log("ogg", "OggDemuxer: getLastGranuleFromHeaders - ", stream.codec_name, ": total_samples=", stream.total_samples, 
                           ", granule=", granule_from_samples);
            }
            
            if (granule_from_samples > max_granule) {
                max_granule = granule_from_samples;
            }
        }
    }
    
    Debug::log("ogg", "OggDemuxer: getLastGranuleFromHeaders - max_granule=", max_granule);
    return max_granule;
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

    // Implement bisection search algorithm following ov_pcm_seek_page/op_pcm_seek_page patterns
    int64_t begin = 0;
    int64_t end = static_cast<int64_t>(m_file_size);
    int64_t best_pos = 0;
    uint64_t best_granule = 0;
    bool found_valid_page = false;
    
    // Maximum iterations to prevent infinite loops (following reference implementations)
    const int max_iterations = 64;
    int iterations = 0;
    
    // Bisection search loop (following libvorbisfile/libopusfile patterns)
    while (begin < end && iterations < max_iterations) {
        iterations++;
        
        // Calculate midpoint (following reference implementation patterns)
        int64_t bisect = begin + (end - begin) / 2;
        
        Debug::log("ogg", "OggDemuxer: seekToPage bisection iteration ", iterations, " - begin=", begin, ", bisect=", bisect, ", end=", end);
        
        // Use ogg_sync_pageseek() for page discovery (NOT ogg_sync_pageout())
        // This follows libvorbisfile patterns exactly
        uint64_t granule_at_bisect = static_cast<uint64_t>(-1);
        
        // First try to examine packets using ogg_stream_packetpeek() for more accuracy
        if (!examinePacketsAtPosition(bisect, stream_id, granule_at_bisect)) {
            // Fall back to page-level granule discovery
            granule_at_bisect = findGranuleAtOffsetUsingPageseek(bisect, stream_id);
        }
        
        if (granule_at_bisect == static_cast<uint64_t>(-1)) {
            // No valid page found at this position, adjust search interval
            // Following libvorbisfile error recovery patterns
            if (bisect - begin < end - bisect) {
                begin = bisect + 1;
            } else {
                end = bisect - 1;
            }
            Debug::log("ogg", "OggDemuxer: seekToPage - no valid page at offset ", bisect, ", adjusting interval");
            continue;
        }
        
        Debug::log("ogg", "OggDemuxer: seekToPage - found granule ", granule_at_bisect, " at offset ", bisect, ", target=", target_granule);
        
        // Update best position if this is closer to our target (using safe granule comparison)
        bool is_better_match = false;
        if (!found_valid_page) {
            is_better_match = true;
        } else {
            // Use safe granule comparison functions
            int cmp_bisect_target = granposCmp(granule_at_bisect, target_granule);
            int cmp_bisect_best = granposCmp(granule_at_bisect, best_granule);
            int cmp_best_target = granposCmp(best_granule, target_granule);
            
            // Better if: (bisect <= target && bisect > best) || (best > target && bisect < best)
            is_better_match = (cmp_bisect_target <= 0 && cmp_bisect_best > 0) ||
                             (cmp_best_target > 0 && cmp_bisect_best < 0);
        }
        
        if (is_better_match) {
            best_pos = bisect;
            best_granule = granule_at_bisect;
            found_valid_page = true;
        }
        
        // Adjust bisection interval based on safe granule comparison
        int cmp_result = granposCmp(granule_at_bisect, target_granule);
        if (cmp_result < 0) {
            // granule_at_bisect < target_granule: search right half
            begin = bisect + 1;
        } else if (cmp_result > 0) {
            // granule_at_bisect > target_granule: search left half  
            end = bisect;
        } else {
            // Exact match found (cmp_result == 0)
            best_pos = bisect;
            best_granule = granule_at_bisect;
            break;
        }
        
        // Linear scanning fallback when bisection interval becomes small
        // Following libvorbisfile patterns for small intervals
        if (end - begin < static_cast<int64_t>(CHUNKSIZE / 4)) {
            Debug::log("ogg", "OggDemuxer: seekToPage - interval too small, switching to linear scan");
            int64_t linear_result = linearScanForTarget(begin, end, target_granule, stream_id);
            if (linear_result >= 0) {
                best_pos = linear_result;
                found_valid_page = true;
            }
            break;
        }
    }
    
    if (!found_valid_page) {
        Debug::log("ogg", "OggDemuxer: seekToPage - no valid pages found during bisection, seeking to beginning");
        {
            std::lock_guard<std::mutex> io_lock(m_io_mutex);
            m_handler->seek(0, SEEK_SET);
        }
        
        {
            std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
            ogg_sync_reset(&m_sync_state);
        }
        
        updatePosition(0);
        setEOF(false);
        return true;
    }
    
    Debug::log("ogg", "OggDemuxer: seekToPage - bisection completed after ", iterations, " iterations, best_pos=", best_pos, ", best_granule=", best_granule);
    
    // Seek to the best position found and reset states
    {
        std::lock_guard<std::mutex> io_lock(m_io_mutex);
        m_handler->seek(best_pos, SEEK_SET);
    }
    
    // Reset sync and stream states for clean reading (following reference patterns)
    {
        std::lock_guard<std::mutex> ogg_lock(m_ogg_state_mutex);
        ogg_sync_reset(&m_sync_state);
        
        for (auto& [sid, ogg_stream] : m_ogg_streams) {
            ogg_stream_reset(&ogg_stream);
        }
    }
    
    // Update position tracking
    updatePosition(granuleToMs(best_granule, stream_id));
    setEOF(false);
    
    Debug::log("ogg", "OggDemuxer: seekToPage completed - final position=", getPosition(), "ms");
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

uint64_t OggDemuxer::findGranuleAtOffsetUsingPageseek(int64_t file_offset, uint32_t stream_id) {
    // Save current position
    long original_pos;
    {
        std::lock_guard<std::mutex> io_lock(m_io_mutex);
        original_pos = m_handler->tell();
        
        // Seek to the target offset
        if (m_handler->seek(file_offset, SEEK_SET) != 0) {
            Debug::log("ogg", "OggDemuxer: findGranuleAtOffsetUsingPageseek - failed to seek to offset ", file_offset);
            m_handler->seek(original_pos, SEEK_SET);
            return static_cast<uint64_t>(-1);
        }
    }
    
    // Create a temporary sync state for this search
    ogg_sync_state temp_sync;
    if (ogg_sync_init(&temp_sync) != 0) {
        Debug::log("ogg", "OggDemuxer: findGranuleAtOffsetUsingPageseek - failed to initialize temp sync state");
        std::lock_guard<std::mutex> io_lock(m_io_mutex);
        m_handler->seek(original_pos, SEEK_SET);
        return static_cast<uint64_t>(-1);
    }
    
    uint64_t found_granule = static_cast<uint64_t>(-1);
    
    try {
        // Read data into temporary sync buffer
        const size_t read_size = CHUNKSIZE;  // Use CHUNKSIZE like reference implementations
        char* buffer = ogg_sync_buffer(&temp_sync, read_size);
        if (!buffer) {
            throw std::runtime_error("Failed to get sync buffer");
        }
        
        long bytes_read;
        {
            std::lock_guard<std::mutex> io_lock(m_io_mutex);
            bytes_read = m_handler->read(buffer, 1, read_size);
        }
        
        if (bytes_read <= 0) {
            throw std::runtime_error("Failed to read data");
        }
        
        if (ogg_sync_wrote(&temp_sync, bytes_read) != 0) {
            throw std::runtime_error("Failed to write to sync buffer");
        }
        
        // Use ogg_sync_pageseek() for page discovery (following libvorbisfile patterns)
        ogg_page page;
        long page_offset = 0;
        
        while (true) {
            long result = ogg_sync_pageseek(&temp_sync, &page);
            
            if (result < 0) {
                // Skipped bytes, continue searching
                page_offset -= result;
                continue;
            } else if (result == 0) {
                // Need more data, but we're doing a single-buffer search
                break;
            } else {
                // Found a page, result is the page size
                page_offset += result;
                
                // Validate page before accessing its fields
                if (!validateOggPage(&page)) {
                    continue;
                }
                
                uint32_t page_stream_id = ogg_page_serialno(&page);
                uint64_t page_granule = ogg_page_granulepos(&page);
                
                if (page_stream_id == stream_id && page_granule != static_cast<uint64_t>(-1)) {
                    found_granule = page_granule;
                    Debug::log("ogg", "OggDemuxer: findGranuleAtOffsetUsingPageseek - found granule ", page_granule, " for stream ", stream_id, " at offset ", file_offset);
                    break;
                }
            }
        }
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: findGranuleAtOffsetUsingPageseek - exception: ", e.what());
    }
    
    // Clean up temporary sync state
    ogg_sync_clear(&temp_sync);
    
    // Restore original position
    {
        std::lock_guard<std::mutex> io_lock(m_io_mutex);
        m_handler->seek(original_pos, SEEK_SET);
    }
    
    return found_granule;
}

int64_t OggDemuxer::linearScanForTarget(int64_t begin, int64_t end, uint64_t target_granule, uint32_t stream_id) {
    Debug::log("ogg", "OggDemuxer: linearScanForTarget - scanning from ", begin, " to ", end, " for target ", target_granule);
    
    int64_t best_pos = -1;
    uint64_t best_granule = static_cast<uint64_t>(-1);
    bool found_any = false;
    
    // Scan forward in small increments (following reference implementation patterns)
    const int64_t scan_increment = 2048;  // Small increments for linear scan
    
    for (int64_t pos = begin; pos < end; pos += scan_increment) {
        uint64_t granule_at_pos = findGranuleAtOffsetUsingPageseek(pos, stream_id);
        
        if (granule_at_pos != static_cast<uint64_t>(-1)) {
            if (!found_any || 
                (granule_at_pos <= target_granule && granule_at_pos > best_granule) ||
                (best_granule > target_granule && granule_at_pos < best_granule)) {
                best_pos = pos;
                best_granule = granule_at_pos;
                found_any = true;
                
                // If we found exact match or overshot, we can stop
                if (granule_at_pos >= target_granule) {
                    break;
                }
            }
        }
    }
    
    Debug::log("ogg", "OggDemuxer: linearScanForTarget - found best position ", best_pos, " with granule ", best_granule);
    return best_pos;
}

bool OggDemuxer::examinePacketsAtPosition(int64_t file_offset, uint32_t stream_id, uint64_t& granule_position) {
    // Save current position
    long original_pos;
    {
        std::lock_guard<std::mutex> io_lock(m_io_mutex);
        original_pos = m_handler->tell();
        
        // Seek to the target offset
        if (m_handler->seek(file_offset, SEEK_SET) != 0) {
            Debug::log("ogg", "OggDemuxer: examinePacketsAtPosition - failed to seek to offset ", file_offset);
            m_handler->seek(original_pos, SEEK_SET);
            return false;
        }
    }
    
    // Create temporary sync and stream states for examination
    ogg_sync_state temp_sync;
    ogg_stream_state temp_stream;
    
    if (ogg_sync_init(&temp_sync) != 0) {
        Debug::log("ogg", "OggDemuxer: examinePacketsAtPosition - failed to initialize temp sync state");
        std::lock_guard<std::mutex> io_lock(m_io_mutex);
        m_handler->seek(original_pos, SEEK_SET);
        return false;
    }
    
    if (ogg_stream_init(&temp_stream, stream_id) != 0) {
        Debug::log("ogg", "OggDemuxer: examinePacketsAtPosition - failed to initialize temp stream state");
        ogg_sync_clear(&temp_sync);
        std::lock_guard<std::mutex> io_lock(m_io_mutex);
        m_handler->seek(original_pos, SEEK_SET);
        return false;
    }
    
    bool found_granule = false;
    granule_position = static_cast<uint64_t>(-1);
    
    try {
        // Read data into temporary sync buffer
        const size_t read_size = CHUNKSIZE;
        char* buffer = ogg_sync_buffer(&temp_sync, read_size);
        if (!buffer) {
            throw std::runtime_error("Failed to get sync buffer");
        }
        
        long bytes_read;
        {
            std::lock_guard<std::mutex> io_lock(m_io_mutex);
            bytes_read = m_handler->read(buffer, 1, read_size);
        }
        
        if (bytes_read <= 0) {
            throw std::runtime_error("Failed to read data");
        }
        
        if (ogg_sync_wrote(&temp_sync, bytes_read) != 0) {
            throw std::runtime_error("Failed to write to sync buffer");
        }
        
        // Process pages and examine packets using ogg_stream_packetpeek()
        ogg_page page;
        while (ogg_sync_pageout(&temp_sync, &page) == 1) {
            // Validate page before accessing its fields
            if (!validateOggPage(&page)) {
                continue;
            }
            
            uint32_t page_stream_id = ogg_page_serialno(&page);
            if (page_stream_id != stream_id) {
                continue;
            }
            
            // Add page to stream
            if (ogg_stream_pagein(&temp_stream, &page) != 0) {
                continue;
            }
            
            // Use ogg_stream_packetpeek() to examine packets without consuming them
            // This follows libvorbisfile/libopusfile patterns exactly
            ogg_packet packet;
            int packet_result = ogg_stream_packetpeek(&temp_stream, &packet);
            
            if (packet_result == 1) {
                // Successfully peeked at a packet
                if (packet.granulepos != -1) {
                    granule_position = static_cast<uint64_t>(packet.granulepos);
                    found_granule = true;
                    Debug::log("ogg", "OggDemuxer: examinePacketsAtPosition - peeked granule ", granule_position, " for stream ", stream_id, " at offset ", file_offset);
                    break;
                }
            } else if (packet_result == 0) {
                // Need more data, but we're doing a single-buffer examination
                break;
            } else {
                // Error occurred
                Debug::log("ogg", "OggDemuxer: examinePacketsAtPosition - packet peek error for stream ", stream_id);
                break;
            }
        }
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: examinePacketsAtPosition - exception: ", e.what());
    }
    
    // Clean up temporary states
    ogg_stream_clear(&temp_stream);
    ogg_sync_clear(&temp_sync);
    
    // Restore original position
    {
        std::lock_guard<std::mutex> io_lock(m_io_mutex);
        m_handler->seek(original_pos, SEEK_SET);
    }
    
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

// Performance optimization method implementations (Requirements 8.1-8.7)

bool OggDemuxer::performReadAheadBuffering_unlocked(size_t target_buffer_size) {
    try {
        // Implement read-ahead buffering for network sources (Requirement 8.4)
        if (target_buffer_size == 0) {
            target_buffer_size = m_read_ahead_buffer_size;
        }
        
        // Limit buffer size to prevent memory exhaustion
        target_buffer_size = std::min(target_buffer_size, m_max_memory_usage / 4);
        
        // This method assumes locks are already held, so we can't call readIntoSyncBuffer
        // Instead, we'll implement the buffering logic directly here
        
        // Get buffer from libogg
        char* buffer = ogg_sync_buffer(&m_sync_state, static_cast<long>(target_buffer_size));
        if (!buffer) {
            Debug::log("ogg", "OggDemuxer: ogg_sync_buffer failed in performReadAheadBuffering_unlocked");
            return false;
        }
        
        // Perform optimized read
        long bytes_read;
        if (!optimizedRead_unlocked(buffer, 1, target_buffer_size, bytes_read)) {
            return false;
        }
        
        if (bytes_read <= 0) {
            return false;
        }
        
        // Submit data to libogg
        int sync_result = ogg_sync_wrote(&m_sync_state, bytes_read);
        if (sync_result != 0) {
            Debug::log("ogg", "OggDemuxer: ogg_sync_wrote failed in performReadAheadBuffering_unlocked");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in performReadAheadBuffering_unlocked: ", e.what());
        return false;
    }
}

void OggDemuxer::cachePageForSeeking_unlocked(int64_t file_offset, uint64_t granule_position, 
                                              uint32_t stream_id, const std::vector<uint8_t>& page_data) {
    try {
        std::lock_guard<std::mutex> cache_lock(m_page_cache_mutex);
        
        // Remove oldest entries if cache is full (Requirement 8.6)
        while (m_page_cache.size() >= m_page_cache_size) {
            m_page_cache.erase(m_page_cache.begin());
        }
        
        // Add new cache entry
        CachedPage cached_page;
        cached_page.file_offset = file_offset;
        cached_page.granule_position = granule_position;
        cached_page.stream_id = stream_id;
        cached_page.page_data = page_data;
        cached_page.access_time = std::chrono::steady_clock::now();
        
        m_page_cache.push_back(std::move(cached_page));
        
        Debug::log("ogg", "OggDemuxer: Cached page at offset ", file_offset, " granule ", granule_position);
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in cachePageForSeeking_unlocked: ", e.what());
    }
}

bool OggDemuxer::findCachedPageNearTarget_unlocked(uint64_t target_granule, uint32_t stream_id, 
                                                   int64_t& file_offset, uint64_t& granule_position) {
    try {
        std::lock_guard<std::mutex> cache_lock(m_page_cache_mutex);
        
        int64_t best_offset = -1;
        uint64_t best_granule = 0;
        uint64_t best_distance = UINT64_MAX;
        
        // Find closest cached page (Requirement 8.6)
        for (const auto& cached_page : m_page_cache) {
            if (cached_page.stream_id != stream_id) continue;
            
            uint64_t distance = (cached_page.granule_position > target_granule) ?
                               (cached_page.granule_position - target_granule) :
                               (target_granule - cached_page.granule_position);
            
            if (distance < best_distance) {
                best_distance = distance;
                best_offset = cached_page.file_offset;
                best_granule = cached_page.granule_position;
            }
        }
        
        if (best_offset >= 0) {
            file_offset = best_offset;
            granule_position = best_granule;
            m_cache_hits++;
            Debug::log("ogg", "OggDemuxer: Cache hit for granule ", target_granule, " -> offset ", best_offset);
            return true;
        }
        
        m_cache_misses++;
        return false;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in findCachedPageNearTarget_unlocked: ", e.what());
        return false;
    }
}

void OggDemuxer::addSeekHint_unlocked(uint64_t timestamp_ms, int64_t file_offset, uint64_t granule_position) {
    try {
        std::lock_guard<std::mutex> hints_lock(m_seek_hints_mutex);
        
        // Only add hint if it's at the right granularity (Requirement 8.3)
        if (timestamp_ms % m_seek_hint_granularity != 0) {
            return;
        }
        
        // Check if we already have a hint for this timestamp
        for (const auto& hint : m_seek_hints) {
            if (hint.timestamp_ms == timestamp_ms) {
                return; // Already have this hint
            }
        }
        
        // Add new seek hint
        SeekHint hint;
        hint.timestamp_ms = timestamp_ms;
        hint.file_offset = file_offset;
        hint.granule_position = granule_position;
        
        m_seek_hints.push_back(hint);
        
        // Keep hints sorted by timestamp
        std::sort(m_seek_hints.begin(), m_seek_hints.end(), 
                 [](const SeekHint& a, const SeekHint& b) {
                     return a.timestamp_ms < b.timestamp_ms;
                 });
        
        // Limit number of hints to prevent memory growth
        if (m_seek_hints.size() > 1000) {
            m_seek_hints.resize(1000);
        }
        
        Debug::log("ogg", "OggDemuxer: Added seek hint at ", timestamp_ms, "ms -> offset ", file_offset);
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in addSeekHint_unlocked: ", e.what());
    }
}

bool OggDemuxer::findBestSeekHint_unlocked(uint64_t target_timestamp_ms, int64_t& file_offset, 
                                          uint64_t& granule_position) {
    try {
        std::lock_guard<std::mutex> hints_lock(m_seek_hints_mutex);
        
        if (m_seek_hints.empty()) {
            return false;
        }
        
        // Find closest hint using binary search (Requirement 8.3)
        auto it = std::lower_bound(m_seek_hints.begin(), m_seek_hints.end(), target_timestamp_ms,
                                  [](const SeekHint& hint, uint64_t timestamp) {
                                      return hint.timestamp_ms < timestamp;
                                  });
        
        if (it != m_seek_hints.end()) {
            file_offset = it->file_offset;
            granule_position = it->granule_position;
            Debug::log("ogg", "OggDemuxer: Found seek hint for ", target_timestamp_ms, "ms -> offset ", file_offset);
            return true;
        }
        
        // Use last hint if target is beyond all hints
        if (!m_seek_hints.empty()) {
            const auto& last_hint = m_seek_hints.back();
            file_offset = last_hint.file_offset;
            granule_position = last_hint.granule_position;
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in findBestSeekHint_unlocked: ", e.what());
        return false;
    }
}

bool OggDemuxer::optimizedRead_unlocked(void* buffer, size_t size, size_t count, long& bytes_read) {
    try {
        // Optimize I/O operations with efficient buffering (Requirements 8.1, 8.3)
        size_t total_bytes = size * count;
        
        // Use larger reads for better I/O efficiency
        if (total_bytes < m_io_buffer_size) {
            // For small reads, use the I/O buffer size
            total_bytes = std::min(m_io_buffer_size, total_bytes);
        }
        
        bytes_read = m_handler->read(buffer, 1, total_bytes);
        
        if (bytes_read > 0) {
            m_bytes_read_total += bytes_read;
        }
        
        return bytes_read >= 0;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in optimizedRead_unlocked: ", e.what());
        bytes_read = -1;
        return false;
    }
}

bool OggDemuxer::processPacketWithMinimalCopy_unlocked(const ogg_packet& packet, uint32_t stream_id, 
                                                       OggPacket& output_packet) {
    try {
        // Minimize memory copying in packet processing (Requirement 8.5)
        output_packet.stream_id = stream_id;
        
        // Validate packet before processing
        if (packet.packet == nullptr || packet.bytes <= 0) {
            return false;
        }
        
        // Use move semantics and reserve capacity to minimize copies
        output_packet.data.clear();
        output_packet.data.reserve(packet.bytes);
        
        // Single copy operation instead of multiple small copies
        output_packet.data.assign(packet.packet, packet.packet + packet.bytes);
        
        // Set other fields without additional copying
        output_packet.granule_position = (packet.granulepos == -1) ? 0 : static_cast<uint64_t>(packet.granulepos);
        output_packet.is_first_packet = packet.b_o_s;
        output_packet.is_last_packet = packet.e_o_s;
        output_packet.is_continued = false; // libogg handles continuation internally
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in processPacketWithMinimalCopy_unlocked: ", e.what());
        return false;
    }
}

void OggDemuxer::cleanupPerformanceCaches_unlocked() {
    try {
        // Clean up page cache
        {
            std::lock_guard<std::mutex> cache_lock(m_page_cache_mutex);
            m_page_cache.clear();
        }
        
        // Clean up seek hints
        {
            std::lock_guard<std::mutex> hints_lock(m_seek_hints_mutex);
            m_seek_hints.clear();
        }
        
        Debug::log("ogg", "OggDemuxer: Performance caches cleaned up");
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in cleanupPerformanceCaches_unlocked: ", e.what());
    }
}

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

// Memory Safety and Resource Management Implementation (Task 10)

void OggDemuxer::cleanupLiboggStructures_unlocked() {
    Debug::log("ogg", "OggDemuxer: Cleaning up libogg structures");
    
    // Clean up all ogg_stream_state structures (Requirement 8.1)
    for (auto& [stream_id, ogg_stream] : m_ogg_streams) {
        Debug::log("ogg", "OggDemuxer: Clearing ogg_stream_state for stream ", stream_id);
        ogg_stream_clear(&ogg_stream);
    }
    m_ogg_streams.clear();
    
    // Clean up ogg_sync_state (Requirement 8.1)
    Debug::log("ogg", "OggDemuxer: Clearing ogg_sync_state");
    ogg_sync_clear(&m_sync_state);
    
    // Clear packet queues and reset memory usage (Requirement 8.2)
    for (auto& [stream_id, stream] : m_streams) {
        size_t queue_memory = stream.m_packet_queue.size() * sizeof(OggPacket);
        for (const auto& packet : stream.m_packet_queue) {
            queue_memory += packet.data.size();
        }
        
        stream.m_packet_queue.clear();
        m_total_memory_usage.fetch_sub(queue_memory);
        
        Debug::log("ogg", "OggDemuxer: Cleared packet queue for stream ", stream_id, 
                   ", freed ", queue_memory, " bytes");
    }
    
    Debug::log("ogg", "OggDemuxer: All libogg structures cleaned up successfully");
}

bool OggDemuxer::validateBufferSize(size_t requested_size, const char* operation_name) {
    // Validate buffer sizes to prevent overflow (Requirement 8.4)
    if (requested_size == 0) {
        Debug::log("ogg", "OggDemuxer: Invalid buffer size 0 for operation: ", operation_name);
        return false;
    }
    
    // Check against PAGE_SIZE_MAX to prevent buffer overflows
    if (requested_size > OggErrorCodes::PAGE_SIZE_MAX) {
        Debug::log("ogg", "OggDemuxer: Buffer size ", requested_size, 
                   " exceeds PAGE_SIZE_MAX (", OggErrorCodes::PAGE_SIZE_MAX, ") for operation: ", operation_name);
        return false;
    }
    
    // Check against reasonable memory limits
    if (requested_size > m_max_memory_usage / 4) {
        Debug::log("ogg", "OggDemuxer: Buffer size ", requested_size, 
                   " exceeds memory limit for operation: ", operation_name);
        return false;
    }
    
    return true;
}

bool OggDemuxer::enforcePacketQueueLimits_unlocked(uint32_t stream_id) {
    // Check and enforce packet queue limits (Requirement 8.2)
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        return true; // Stream doesn't exist, nothing to limit
    }
    
    OggStream& stream = stream_it->second;
    
    // Check queue size limit
    if (stream.m_packet_queue.size() >= m_max_packet_queue_size) {
        Debug::log("ogg", "OggDemuxer: Packet queue for stream ", stream_id, 
                   " reached limit (", stream.m_packet_queue.size(), "/", m_max_packet_queue_size, ")");
        
        // Remove oldest packets to make room (FIFO behavior)
        while (stream.m_packet_queue.size() >= m_max_packet_queue_size && !stream.m_packet_queue.empty()) {
            const OggPacket& oldest = stream.m_packet_queue.front();
            size_t packet_memory = sizeof(OggPacket) + oldest.data.size();
            
            stream.m_packet_queue.pop_front();
            m_total_memory_usage.fetch_sub(packet_memory);
            
            Debug::log("ogg", "OggDemuxer: Removed oldest packet from stream ", stream_id, 
                       ", freed ", packet_memory, " bytes");
        }
    }
    
    // Check total memory usage
    if (m_total_memory_usage.load() > m_max_memory_usage) {
        Debug::log("ogg", "OggDemuxer: Total memory usage ", m_total_memory_usage.load(), 
                   " exceeds limit ", m_max_memory_usage);
        
        // Remove packets from all streams to reduce memory usage
        for (auto& [id, stream_obj] : m_streams) {
            while (!stream_obj.m_packet_queue.empty() && 
                   m_total_memory_usage.load() > m_max_memory_usage * 0.8) {
                const OggPacket& packet = stream_obj.m_packet_queue.front();
                size_t packet_memory = sizeof(OggPacket) + packet.data.size();
                
                stream_obj.m_packet_queue.pop_front();
                m_total_memory_usage.fetch_sub(packet_memory);
            }
        }
    }
    
    return true;
}

void OggDemuxer::resetSyncStateAfterSeek_unlocked() {
    // Reset sync state after seeks like reference implementations (Requirement 12.9)
    Debug::log("ogg", "OggDemuxer: Resetting sync state after seek using ogg_sync_reset()");
    
    // Use ogg_sync_reset() like libvorbisfile and libopusfile
    ogg_sync_reset(&m_sync_state);
    
    // Clear any buffered data that's no longer valid
    m_offset = 0;
    m_end = 0;
    
    Debug::log("ogg", "OggDemuxer: Sync state reset completed");
}

void OggDemuxer::resetStreamState_unlocked(uint32_t stream_id, uint32_t new_serial_number) {
    // Reset stream state for stream switching like reference implementations (Requirement 12.10)
    Debug::log("ogg", "OggDemuxer: Resetting stream state for stream ", stream_id, 
               " with new serial number ", new_serial_number);
    
    auto ogg_stream_it = m_ogg_streams.find(stream_id);
    if (ogg_stream_it != m_ogg_streams.end()) {
        // Use ogg_stream_reset_serialno() like libvorbisfile and libopusfile
        int reset_result = ogg_stream_reset_serialno(&ogg_stream_it->second, new_serial_number);
        if (reset_result != 0) {
            Debug::log("ogg", "OggDemuxer: ogg_stream_reset_serialno failed with code: ", reset_result);
            
            // Fallback: clear and reinitialize the stream
            ogg_stream_clear(&ogg_stream_it->second);
            if (ogg_stream_init(&ogg_stream_it->second, new_serial_number) != 0) {
                Debug::log("ogg", "OggDemuxer: Failed to reinitialize stream after reset failure");
                m_ogg_streams.erase(ogg_stream_it);
                return;
            }
        }
    } else {
        // Initialize new stream state
        ogg_stream_state new_stream;
        if (ogg_stream_init(&new_stream, new_serial_number) == 0) {
            m_ogg_streams[stream_id] = new_stream;
        } else {
            Debug::log("ogg", "OggDemuxer: Failed to initialize new stream state for stream ", stream_id);
            return;
        }
    }
    
    // Clear packet queue for this stream
    auto stream_it = m_streams.find(stream_id);
    if (stream_it != m_streams.end()) {
        OggStream& stream = stream_it->second;
        
        // Calculate memory to free
        size_t queue_memory = 0;
        for (const auto& packet : stream.m_packet_queue) {
            queue_memory += sizeof(OggPacket) + packet.data.size();
        }
        
        stream.m_packet_queue.clear();
        m_total_memory_usage.fetch_sub(queue_memory);
        
        // Reset stream state
        stream.headers_sent = false;
        stream.next_header_index = 0;
        stream.total_samples_processed = 0;
        
        Debug::log("ogg", "OggDemuxer: Cleared packet queue and reset state for stream ", stream_id, 
                   ", freed ", queue_memory, " bytes");
    }
    
    Debug::log("ogg", "OggDemuxer: Stream state reset completed for stream ", stream_id);
}

// Additional memory safety methods for comprehensive resource management

bool OggDemuxer::performMemoryAudit_unlocked() {
    // Perform comprehensive memory audit (Requirement 8.2, 8.3)
    Debug::log("ogg", "OggDemuxer: Performing memory audit");
    
    size_t calculated_memory = 0;
    size_t packet_count = 0;
    
    // Audit all packet queues
    for (const auto& [stream_id, stream] : m_streams) {
        for (const auto& packet : stream.m_packet_queue) {
            calculated_memory += sizeof(OggPacket) + packet.data.size();
            packet_count++;
        }
        
        // Audit header packets
        for (const auto& packet : stream.header_packets) {
            calculated_memory += sizeof(OggPacket) + packet.data.size();
        }
    }
    
    size_t reported_memory = m_total_memory_usage.load();
    
    Debug::log("ogg", "OggDemuxer: Memory audit - calculated: ", calculated_memory, 
               ", reported: ", reported_memory, ", packets: ", packet_count);
    
    // Allow small discrepancies due to atomic operations
    if (calculated_memory > reported_memory * 1.1 || reported_memory > calculated_memory * 1.1) {
        Debug::log("ogg", "OggDemuxer: Memory audit mismatch detected - correcting");
        m_total_memory_usage.store(calculated_memory);
        return false;
    }
    
    return true;
}

void OggDemuxer::enforceMemoryLimits_unlocked() {
    // Enforce strict memory limits to prevent exhaustion (Requirement 8.2)
    size_t current_memory = m_total_memory_usage.load();
    
    if (current_memory > m_max_memory_usage) {
        Debug::log("ogg", "OggDemuxer: Memory limit exceeded (", current_memory, "/", m_max_memory_usage, 
                   ") - performing emergency cleanup");
        
        // Emergency cleanup: remove packets from all streams
        size_t freed_memory = 0;
        for (auto& [stream_id, stream] : m_streams) {
            while (!stream.m_packet_queue.empty() && 
                   m_total_memory_usage.load() > m_max_memory_usage * 0.7) {
                const OggPacket& packet = stream.m_packet_queue.front();
                size_t packet_memory = sizeof(OggPacket) + packet.data.size();
                
                stream.m_packet_queue.pop_front();
                m_total_memory_usage.fetch_sub(packet_memory);
                freed_memory += packet_memory;
            }
        }
        
        Debug::log("ogg", "OggDemuxer: Emergency cleanup freed ", freed_memory, " bytes");
    }
}

bool OggDemuxer::validateLiboggStructures_unlocked() {
    // Validate libogg structures for corruption (Requirement 8.1, 8.3)
    Debug::log("ogg", "OggDemuxer: Validating libogg structures");
    
    // Basic validation of sync state
    // Note: libogg doesn't provide direct validation functions, so we do basic checks
    
    // Validate stream states
    for (const auto& [stream_id, ogg_stream] : m_ogg_streams) {
        // Check if stream ID matches
        if (ogg_stream.serialno != static_cast<int>(stream_id)) {
            Debug::log("ogg", "OggDemuxer: Stream ID mismatch detected for stream ", stream_id);
            return false;
        }
    }
    
    Debug::log("ogg", "OggDemuxer: libogg structure validation completed");
    return true;
}

void OggDemuxer::performPeriodicMaintenance_unlocked() {
    // Perform periodic maintenance to prevent resource leaks (Requirement 8.2, 8.3)
    static int maintenance_counter = 0;
    static auto last_maintenance = std::chrono::steady_clock::now();
    maintenance_counter++;
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(now - last_maintenance);
    
    // Perform maintenance every 100 operations OR every 30 seconds (whichever comes first)
    if (maintenance_counter % 100 == 0 || time_since_last.count() >= 30) {
        last_maintenance = now;
        Debug::log("ogg", "OggDemuxer: Performing periodic maintenance (cycle ", maintenance_counter, ")");
        
        // Memory audit
        performMemoryAudit_unlocked();
        
        // Enforce memory limits
        enforceMemoryLimits_unlocked();
        
        // Validate structures
        validateLiboggStructures_unlocked();
        
        // Clean up empty streams
        auto stream_it = m_streams.begin();
        while (stream_it != m_streams.end()) {
            const OggStream& stream = stream_it->second;
            if (stream.m_packet_queue.empty() && 
                stream.header_packets.empty() && 
                !stream.headers_complete) {
                
                uint32_t stream_id = stream_it->first;
                Debug::log("ogg", "OggDemuxer: Removing empty stream ", stream_id);
                
                // Clean up ogg_stream_state
                auto ogg_stream_it = m_ogg_streams.find(stream_id);
                if (ogg_stream_it != m_ogg_streams.end()) {
                    ogg_stream_clear(&ogg_stream_it->second);
                    m_ogg_streams.erase(ogg_stream_it);
                }
                
                stream_it = m_streams.erase(stream_it);
            } else {
                ++stream_it;
            }
        }
    }
}

bool OggDemuxer::detectInfiniteLoop_unlocked(const std::string& operation_name) {
    // Detect and prevent infinite loops in packet processing (Requirement 8.1)
    static std::map<std::string, std::chrono::steady_clock::time_point> operation_start_times;
    static std::map<std::string, int> operation_counters;
    
    auto now = std::chrono::steady_clock::now();
    
    // Initialize or update operation tracking
    if (operation_start_times.find(operation_name) == operation_start_times.end()) {
        operation_start_times[operation_name] = now;
        operation_counters[operation_name] = 1;
        return false; // Not an infinite loop
    }
    
    operation_counters[operation_name]++;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - operation_start_times[operation_name]);
    
    // Check for potential infinite loop conditions
    if (elapsed.count() > 10 && operation_counters[operation_name] > 1000) {
        Debug::log("ogg", "OggDemuxer: Potential infinite loop detected in operation '", operation_name, 
                   "' - ", operation_counters[operation_name], " iterations in ", elapsed.count(), " seconds");
        
        // Reset counters to prevent repeated warnings
        operation_start_times[operation_name] = now;
        operation_counters[operation_name] = 0;
        
        return true; // Infinite loop detected
    }
    
    // Reset counters periodically to prevent memory buildup
    if (elapsed.count() > 60) {
        operation_start_times[operation_name] = now;
        operation_counters[operation_name] = 1;
    }
    
    return false; // No infinite loop detected
}
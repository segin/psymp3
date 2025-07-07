/*
 * OggDemuxer.h - Ogg container demuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef OGGDEMUXER_H
#define OGGDEMUXER_H

#include "Demuxer.h"

/**
 * @brief Ogg page header structure
 */
struct OggPageHeader {
    uint8_t capture_pattern[4];    // "OggS"
    uint8_t version;               // Stream structure version (0)
    uint8_t header_type;           // Header type flags
    uint64_t granule_position;     // Granule position
    uint32_t serial_number;        // Bitstream serial number
    uint32_t page_sequence;        // Page sequence number
    uint32_t checksum;             // Page checksum
    uint8_t page_segments;         // Number of segments in page
    
    // Flags for header_type
    static constexpr uint8_t CONTINUED_PACKET = 0x01;
    static constexpr uint8_t FIRST_PAGE = 0x02;
    static constexpr uint8_t LAST_PAGE = 0x04;
    
    bool isContinuedPacket() const { return header_type & CONTINUED_PACKET; }
    bool isFirstPage() const { return header_type & FIRST_PAGE; }
    bool isLastPage() const { return header_type & LAST_PAGE; }
};

/**
 * @brief Ogg packet data
 */
struct OggPacket {
    uint32_t stream_id;
    std::vector<uint8_t> data;
    uint64_t granule_position;
    bool is_first_packet;
    bool is_last_packet;
    bool is_continued;
};

/**
 * @brief Information about an Ogg logical bitstream
 */
struct OggStream {
    uint32_t serial_number;
    std::string codec_name;        // "vorbis", "flac", "opus", "theora", etc.
    std::string codec_type;        // "audio", "video", "subtitle"
    
    // Stream-specific data
    std::vector<uint8_t> codec_setup_data;  // Codec-specific setup headers
    std::vector<OggPacket> header_packets;  // Codec header packets
    
    // Audio properties (filled from codec headers)
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint32_t bitrate = 0;
    uint64_t total_samples = 0;
    
    // State tracking
    bool headers_complete = false;
    std::vector<uint8_t> partial_packet_data;  // For packets split across pages
    uint64_t last_granule = 0;
    uint32_t last_page_sequence = 0;
};

/**
 * @brief Ogg container demuxer
 * 
 * The Ogg demuxer handles the Ogg container format, which can contain:
 * - Vorbis audio (.ogg)
 * - FLAC audio (.oga, .ogg)
 * - Opus audio (.opus, .oga)
 * - Theora video (.ogv)
 * - Speex audio (.spx)
 * - And other codecs
 * 
 * This demuxer focuses on audio streams but can be extended for video.
 */
class OggDemuxer : public Demuxer {
public:
    explicit OggDemuxer(std::unique_ptr<IOHandler> handler);
    
    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    MediaChunk readChunk() override;
    MediaChunk readChunk(uint32_t stream_id) override;
    bool seekTo(uint64_t timestamp_ms) override;
    bool isEOF() const override;
    uint64_t getDuration() const override;
    uint64_t getPosition() const override;
    
private:
    std::map<uint32_t, OggStream> m_streams;
    std::queue<OggPacket> m_packet_queue;
    uint64_t m_file_size = 0;
    uint64_t m_current_offset = 0;
    bool m_eof = false;
    
    /**
     * @brief Read an Ogg page header
     */
    bool readPageHeader(OggPageHeader& header);
    
    /**
     * @brief Read segment table and page data
     */
    bool readPageData(const OggPageHeader& header, std::vector<uint8_t>& page_data);
    
    /**
     * @brief Parse packets from page data
     */
    std::vector<OggPacket> parsePackets(const OggPageHeader& header, 
                                       const std::vector<uint8_t>& page_data,
                                       const std::vector<uint8_t>& segment_table);
    
    /**
     * @brief Identify codec from packet data
     */
    std::string identifyCodec(const std::vector<uint8_t>& packet_data);
    
    /**
     * @brief Parse Vorbis identification header
     */
    bool parseVorbisHeaders(OggStream& stream, const OggPacket& packet);
    
    /**
     * @brief Parse FLAC identification header
     */
    bool parseFLACHeaders(OggStream& stream, const OggPacket& packet);
    
    /**
     * @brief Parse Opus identification header
     */
    bool parseOpusHeaders(OggStream& stream, const OggPacket& packet);
    
    /**
     * @brief Parse Speex identification header
     */
    bool parseSpeexHeaders(OggStream& stream, const OggPacket& packet);
    
    /**
     * @brief Calculate duration from stream information
     */
    void calculateDuration();
    
    /**
     * @brief Convert granule position to timestamp for a given stream
     */
    uint64_t granuleToMs(uint64_t granule, uint32_t stream_id) const;
    
    /**
     * @brief Convert timestamp to granule position for a given stream
     */
    uint64_t msToGranule(uint64_t timestamp_ms, uint32_t stream_id) const;
    
    /**
     * @brief Find the best audio stream for playback
     */
    uint32_t findBestAudioStream() const;
    
    /**
     * @brief Seek to a specific page in the file
     */
    bool seekToPage(uint64_t target_granule, uint32_t stream_id);
    
    /**
     * @brief Read and queue packets until we have data for the requested stream
     */
    void fillPacketQueue(uint32_t target_stream_id);
    
    /**
     * @brief Check if packet data starts with given signature
     */
    static bool hasSignature(const std::vector<uint8_t>& data, const char* signature);
    
    /**
     * @brief Read little-endian values from packet data
     */
    template<typename T>
    static T readLE(const std::vector<uint8_t>& data, size_t offset) {
        if (offset + sizeof(T) > data.size()) {
            return 0;
        }
        T value;
        std::memcpy(&value, data.data() + offset, sizeof(T));
        return value;
    }
    
    /**
     * @brief Read big-endian values from packet data
     */
    template<typename T>
    static T readBE(const std::vector<uint8_t>& data, size_t offset) {
        if (offset + sizeof(T) > data.size()) {
            return 0;
        }
        T value;
        std::memcpy(&value, data.data() + offset, sizeof(T));
        if constexpr (sizeof(T) == 2) {
            return __builtin_bswap16(value);
        } else if constexpr (sizeof(T) == 4) {
            return __builtin_bswap32(value);
        } else if constexpr (sizeof(T) == 8) {
            return __builtin_bswap64(value);
        }
        return value;
    }
};

#endif // OGGDEMUXER_H
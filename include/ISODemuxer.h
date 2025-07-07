/*
 * ISODemuxer.h - ISO Base Media File Format demuxer (MP4, M4A, etc.)
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

#ifndef ISODEMUXER_H
#define ISODEMUXER_H

#include "Demuxer.h"
#include <map>
#include <vector>

/**
 * @brief ISO base media file format box header
 */
struct ISOBox {
    uint32_t size;           // Box size (including header)
    uint32_t type;           // Box type (FourCC)
    uint64_t extended_size;  // Extended size (for size==1)
    uint64_t data_offset;    // Offset to box data in file
    
    bool isExtendedSize() const { return size == 1; }
    uint64_t getContentSize() const {
        if (size == 1) {
            return extended_size - (8 + 8); // Header + extended size field
        } else {
            return size - 8; // Header size
        }
    }
};

/**
 * @brief Sample description entry
 */
struct SampleDescription {
    uint32_t format;         // Sample format (FourCC)
    uint16_t data_ref_index;
    std::vector<uint8_t> specific_data;  // Format-specific data
    
    // Audio-specific fields
    uint16_t channels = 0;
    uint16_t sample_size = 0;
    uint32_t sample_rate = 0;
};

/**
 * @brief Track information
 */
struct ISOTrack {
    uint32_t track_id;
    std::string handler_type;        // "soun", "vide", "text", etc.
    uint64_t duration = 0;           // In track timescale units
    uint32_t timescale = 1000;       // Units per second
    
    // Sample table information
    std::vector<uint64_t> chunk_offsets;     // stco/co64
    std::vector<uint32_t> samples_per_chunk; // stsc
    std::vector<uint32_t> sample_sizes;      // stsz
    std::vector<uint64_t> sample_times;      // stts (decoded to absolute times)
    std::vector<SampleDescription> sample_descriptions; // stsd
    
    // Current playback state
    uint32_t current_sample = 0;
    uint32_t current_chunk = 0;
    uint32_t samples_in_current_chunk = 0;
    
    // Derived information
    std::string codec_name;
    uint32_t bitrate = 0;
    
    /**
     * @brief Get duration in milliseconds
     */
    uint64_t getDurationMs() const {
        return timescale > 0 ? (duration * 1000ULL) / timescale : 0;
    }
    
    /**
     * @brief Convert track time to milliseconds
     */
    uint64_t trackTimeToMs(uint64_t track_time) const {
        return timescale > 0 ? (track_time * 1000ULL) / timescale : 0;
    }
    
    /**
     * @brief Convert milliseconds to track time
     */
    uint64_t msToTrackTime(uint64_t ms) const {
        return (ms * timescale) / 1000ULL;
    }
};

/**
 * @brief ISO Base Media File Format demuxer
 * 
 * This demuxer handles the ISO container format family:
 * - MP4 files (.mp4, .m4v)
 * - M4A files (.m4a)
 * - 3GP files (.3gp)
 * - MOV files (.mov) - QuickTime variant
 * 
 * The format can contain various codecs:
 * - Audio: AAC, MP3, ALAC, AC-3, etc.
 * - Video: H.264, H.265, MPEG-4, etc.
 */
class ISODemuxer : public Demuxer {
public:
    explicit ISODemuxer(std::unique_ptr<IOHandler> handler);
    
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
    // Common box types (as uint32_t big-endian values)
    static constexpr uint32_t FTYP_BOX = 0x66747970; // "ftyp"
    static constexpr uint32_t MOOV_BOX = 0x6D6F6F76; // "moov"
    static constexpr uint32_t MDAT_BOX = 0x6D646174; // "mdat"
    static constexpr uint32_t TRAK_BOX = 0x7472616B; // "trak"
    static constexpr uint32_t TKHD_BOX = 0x746B6864; // "tkhd"
    static constexpr uint32_t MDIA_BOX = 0x6D646961; // "mdia"
    static constexpr uint32_t MDHD_BOX = 0x6D646864; // "mdhd"
    static constexpr uint32_t HDLR_BOX = 0x68646C72; // "hdlr"
    static constexpr uint32_t MINF_BOX = 0x6D696E66; // "minf"
    static constexpr uint32_t STBL_BOX = 0x7374626C; // "stbl"
    static constexpr uint32_t STSD_BOX = 0x73747364; // "stsd"
    static constexpr uint32_t STTS_BOX = 0x73747473; // "stts"
    static constexpr uint32_t STSC_BOX = 0x73747363; // "stsc"
    static constexpr uint32_t STSZ_BOX = 0x7374737A; // "stsz"
    static constexpr uint32_t STCO_BOX = 0x7374636F; // "stco"
    static constexpr uint32_t CO64_BOX = 0x636F3634; // "co64"
    
    std::map<uint32_t, ISOTrack> m_tracks;
    uint64_t m_file_size = 0;
    bool m_eof = false;
    
    /**
     * @brief Read a box header
     */
    bool readBoxHeader(ISOBox& box);
    
    /**
     * @brief Skip to the end of a box
     */
    void skipBox(const ISOBox& box);
    
    /**
     * @brief Parse file type box (ftyp)
     */
    bool parseFtypBox(const ISOBox& box);
    
    /**
     * @brief Parse movie box (moov)
     */
    bool parseMoovBox(const ISOBox& box);
    
    /**
     * @brief Parse track box (trak)
     */
    bool parseTrakBox(const ISOBox& box, ISOTrack& track);
    
    /**
     * @brief Parse track header box (tkhd)
     */
    bool parseTkhdBox(const ISOBox& box, ISOTrack& track);
    
    /**
     * @brief Parse media box (mdia)
     */
    bool parseMdiaBox(const ISOBox& box, ISOTrack& track);
    
    /**
     * @brief Parse media header box (mdhd)
     */
    bool parseMdhdBox(const ISOBox& box, ISOTrack& track);
    
    /**
     * @brief Parse handler reference box (hdlr)
     */
    bool parseHdlrBox(const ISOBox& box, ISOTrack& track);
    
    /**
     * @brief Parse sample table box (stbl)
     */
    bool parseStblBox(const ISOBox& box, ISOTrack& track);
    
    /**
     * @brief Parse sample description box (stsd)
     */
    bool parseStsdBox(const ISOBox& box, ISOTrack& track);
    
    /**
     * @brief Parse time-to-sample box (stts)
     */
    bool parseSttsBox(const ISOBox& box, ISOTrack& track);
    
    /**
     * @brief Parse sample-to-chunk box (stsc)
     */
    bool parseStscBox(const ISOBox& box, ISOTrack& track);
    
    /**
     * @brief Parse sample size box (stsz)
     */
    bool parseStszBox(const ISOBox& box, ISOTrack& track);
    
    /**
     * @brief Parse chunk offset box (stco/co64)
     */
    bool parseStcoBox(const ISOBox& box, ISOTrack& track);
    
    /**
     * @brief Convert sample format to codec name
     */
    std::string formatToCodecName(uint32_t format) const;
    
    /**
     * @brief Find the best audio track
     */
    uint32_t findBestAudioTrack() const;
    
    /**
     * @brief Get sample data for a specific sample in a track
     */
    bool getSampleData(uint32_t track_id, uint32_t sample_index, MediaChunk& chunk);
    
    /**
     * @brief Calculate chunk and sample position for a track
     */
    void calculateSamplePosition(ISOTrack& track, uint32_t sample_index, 
                                uint32_t& chunk_index, uint32_t& sample_in_chunk);
    
    /**
     * @brief Seek to a specific sample in a track
     */
    bool seekToSample(uint32_t track_id, uint64_t target_time);
    
    /**
     * @brief Helper to read FourCC as string
     */
    static std::string fourCCToString(uint32_t fourcc);
};

#endif // ISODEMUXER_H
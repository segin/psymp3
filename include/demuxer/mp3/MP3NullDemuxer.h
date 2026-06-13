/*
 * MP3NullDemuxer.h - Null/passthrough demuxer for self-containerized MP3 streams
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef MP3NULLDEMUXER_H
#define MP3NULLDEMUXER_H

namespace PsyMP3 {
namespace Demuxer {
namespace MP3 {

/**
 * @brief Null/passthrough demuxer for MP3 streams
 *
 * MP3 is a self-containerizing format - the bitstream itself contains
 * all framing information. This demuxer handles ID3v2 tag skipping,
 * Xing/LAME VBR header parsing, and passes raw MP3 frame data as
 * MediaChunks for the codec to decode.
 */
class MP3NullDemuxer : public Demuxer {
public:
    explicit MP3NullDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler);
    ~MP3NullDemuxer() override = default;

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
    // Unlocked internal methods (assume locks held)
    bool parseContainer_unlocked();
    MediaChunk readChunk_unlocked();
    bool seekTo_unlocked(uint64_t timestamp_ms);

    // MP3 frame parsing helpers
    bool skipID3v2Tag_unlocked();
    bool parseFirstFrame_unlocked();
    bool parseXingHeader_unlocked(const std::vector<uint8_t>& frame_data, uint32_t sample_rate, uint16_t channels);
    bool findFrameSync_unlocked();

    // MP3 frame header validation
    static bool isValidFrameHeader(const uint8_t header[4]);
    static uint32_t getFrameSampleRate(const uint8_t header[4]);
    static uint16_t getFrameChannels(const uint8_t header[4]);
    static uint32_t getFrameBitrate(const uint8_t header[4]);
    static uint32_t getFrameSize(const uint8_t header[4]);
    static uint32_t getFrameSamples(const uint8_t header[4]);

    // Stream state
    StreamInfo m_stream_info;
    uint64_t m_data_start_offset = 0;   // Offset past ID3v2 tag
    uint64_t m_data_end_offset = 0;     // End of audio data (before ID3v1 tag)
    uint64_t m_file_size = 0;
    uint64_t m_current_sample = 0;      // Current position in samples
    uint64_t m_total_samples = 0;       // Total samples (if known from Xing)
    uint32_t m_total_frames = 0;        // Total frames (if known from Xing)
    uint32_t m_total_bytes = 0;         // Total audio bytes (if known from Xing)
    bool m_is_vbr = false;              // VBR flag from Xing header
    std::vector<uint8_t> m_xing_toc;    // Xing TOC for VBR seeking (100 entries)

    mutable std::mutex m_mutex;
};

} // namespace MP3
} // namespace Demuxer
} // namespace PsyMP3

#endif // MP3NULLDEMUXER_H

/*
 * AC3NullDemuxer.h - Null/passthrough demuxer for raw AC-3 and E-AC-3 streams
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_DEMUXER_AC3_AC3NULLDEMUXER_H
#define PSYMP3_DEMUXER_AC3_AC3NULLDEMUXER_H

namespace PsyMP3 {
namespace Demuxer {
namespace AC3 {

/**
 * @brief Null/passthrough demuxer for raw .ac3 and .eac3 elementary streams
 *
 * AC-3 is self-framing like MP3: every syncframe opens with the 0x0B77 sync
 * word and states its own length in the header. This demuxer walks that chain
 * and hands one syncframe per MediaChunk to the codec.
 *
 * Every syncframe is independently decodable -- exponents and bit allocation
 * restart at each one -- so unlike MLP no sync index is needed: a seek can
 * land on any frame. Frames need not share a length -- an AC-3 stream at
 * 44.1 kHz can mix the two sizes of a frmsizecod pair (Table 5.18), and
 * E-AC-3 frames may each state a different frmsiz -- so the frame walk
 * records where each of program 1's frames starts. The frame for a given
 * time is then found by dividing by the samples per frame and looking its
 * offset up.
 *
 * An E-AC-3 stream is reported as "eac3", so Media Information can name it;
 * the same codec decodes both.
 */
class AC3NullDemuxer : public Demuxer {
public:
    explicit AC3NullDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler);
    ~AC3NullDemuxer() override = default;

    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    MediaChunk readChunk() override;
    MediaChunk readChunk(uint32_t stream_id) override;
    bool seekTo(uint64_t timestamp_ms) override;
    bool isEOF() const override;
    uint64_t getDuration() const override;
    uint64_t getPosition() const override;
    std::string getContainerName() const override;

    /// Where the next chunk starts, in samples. A seek lands on a syncframe
    /// boundary before its target, and saying where lets DemuxedStream drop
    /// the difference rather than take the landing as exact.
    bool providesGranulePositions() const override { return true; }
    uint64_t getGranulePosition(uint32_t stream_id) const override;

private:
    bool parseContainer_unlocked();
    MediaChunk readChunk_unlocked();
    bool seekTo_unlocked(uint64_t timestamp_ms);
    bool readHeaderAt_unlocked(uint64_t offset, PsyMP3::Codec::AC3::AC3FrameHeader& header);
    /// The first confirmed syncframe at or after @p from, searching at most
    /// kSyncSearchLimit bytes: where it starts, and its header.
    bool findSyncframe_unlocked(uint64_t from, uint64_t& found_at,
                                PsyMP3::Codec::AC3::AC3FrameHeader& found);

    StreamInfo m_stream_info;
    bool m_eac3 = false;
    uint64_t m_file_size = 0;
    uint64_t m_data_start_offset = 0;   // first syncframe, past any leading junk
    uint64_t m_read_offset = 0;         // where readChunk resumes
    uint64_t m_current_sample = 0;
    uint64_t m_total_samples = 0;
    uint64_t m_frames = 0;
    unsigned m_samples_per_frame = 0;

    /// Byte offset of every syncframe that adds time: program 1's
    /// independent frames. Frame sizes vary -- 44.1 kHz AC-3 can mix two,
    /// and E-AC-3 and some broadcast captures vary more -- and an offset
    /// table is cheap next to guessing wrong on a seek.
    std::vector<uint64_t> m_frame_offsets;

    mutable std::mutex m_mutex;
};

} // namespace AC3
} // namespace Demuxer
} // namespace PsyMP3

#endif // PSYMP3_DEMUXER_AC3_AC3NULLDEMUXER_H

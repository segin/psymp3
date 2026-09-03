/*
 * MLPNullDemuxer.h - Null/passthrough demuxer for raw MLP/Dolby TrueHD streams
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef MLPNULLDEMUXER_H
#define MLPNULLDEMUXER_H

namespace PsyMP3 {
namespace Demuxer {
namespace MLP {

/**
 * @brief Null/passthrough demuxer for raw MLP/TrueHD streams
 *
 * Like MP3, MLP is self-containerizing: every access unit begins with a
 * four-byte header giving its own length, so the stream needs no container to
 * be framed. This demuxer walks that chain, hands one access unit per
 * MediaChunk to the codec, and keeps an index of the major syncs so that a seek
 * can land somewhere the decoder can start.
 */
class MLPNullDemuxer : public Demuxer {
public:
    explicit MLPNullDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler);
    ~MLPNullDemuxer() override = default;

    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    MediaChunk readChunk() override;
    MediaChunk readChunk(uint32_t stream_id) override;
    bool seekTo(uint64_t timestamp_ms) override;
    bool isEOF() const override;
    uint64_t getDuration() const override;
    uint64_t getPosition() const override;
    std::string getContainerName() const override { return "MLP"; }

private:
    /// A major sync, and the sample it starts at: the two things a seek needs.
    struct SyncPoint {
        uint64_t offset;
        uint64_t sample;
    };

    bool parseContainer_unlocked();
    MediaChunk readChunk_unlocked();
    bool seekTo_unlocked(uint64_t timestamp_ms);
    bool scanStream_unlocked();
    bool describeStream_unlocked();

    StreamInfo m_stream_info;
    uint64_t m_file_size = 0;
    uint64_t m_data_start_offset = 0;   // first access unit, past any leading tag
    uint64_t m_read_offset = 0;         // where readChunk resumes
    uint64_t m_current_sample = 0;
    uint64_t m_total_samples = 0;
    uint64_t m_access_units = 0;
    size_t m_samples_per_au = 0;

    /// Subsampled so that a long file does not carry an entry per major sync.
    std::vector<SyncPoint> m_sync_points;

    mutable std::mutex m_mutex;
};

} // namespace MLP
} // namespace Demuxer
} // namespace PsyMP3

#endif // MLPNULLDEMUXER_H

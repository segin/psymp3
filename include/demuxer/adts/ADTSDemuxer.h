/*
 * ADTSDemuxer.h - Demuxer for raw AAC in ADTS framing (.aac, AAC radio)
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_DEMUXER_ADTS_ADTSDEMUXER_H
#define PSYMP3_DEMUXER_ADTS_ADTSDEMUXER_H

namespace PsyMP3 {
namespace Demuxer {
namespace ADTS {

/**
 * @brief One ADTS frame header (ISO/IEC 13818-7 / 14496-3 adts_fixed_header
 *        and adts_variable_header).
 */
struct ADTSHeader {
    bool mpeg2 = false;             ///< ID bit: 1 = MPEG-2, 0 = MPEG-4
    bool has_crc = false;           ///< protection_absent == 0
    unsigned profile = 0;           ///< profile_ObjectType: audio object type - 1
    unsigned sf_index = 0;          ///< sampling_frequency_index
    unsigned sample_rate = 0;       ///< from sf_index
    unsigned channel_config = 0;    ///< channel_configuration (0 = PCE in stream)
    unsigned channels = 0;          ///< output channels for channel_config 1-7
    unsigned frame_length = 0;      ///< whole frame, header included, in bytes
    unsigned header_length = 0;     ///< 7, or 9 with the CRC
    unsigned raw_blocks = 0;        ///< raw_data_blocks in the frame (field + 1)
};

/**
 * @brief Raw AAC in ADTS framing: .aac files, and the audio/aac and
 *        audio/aacp streams many internet radio stations send.
 *
 * ADTS is self-framing like MP3: each frame opens with a 12-bit sync word
 * and states its length. The demuxer takes the header off each frame and
 * hands the raw_data_block to the AAC codec, which is configured with an
 * AudioSpecificConfig built from the first header, as a raw MP4 stream
 * would be. HE-AAC needs nothing extra: its SBR is signalled implicitly in
 * the data, and the decoder finds it there.
 *
 * A file is walked once, frame by frame, for its duration and an offset per
 * frame, which seeks look up. A live stream (IOHandler::isLiveStream()) is
 * not: it has no duration and is not seeked.
 *
 * Not handled: frames carrying more than one raw_data_block (skipped; the
 * blocks are not delimited without the CRC fields, and encoders do not write
 * them), and channel_configuration 0, whose program_config_element would
 * have to be lifted out of the first raw block into the AudioSpecificConfig.
 */
class ADTSDemuxer : public Demuxer {
public:
    /**
     * @brief Take ownership of the source; nothing is read until parseContainer().
     * @param handler The ADTS data.
     */
    explicit ADTSDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler);
    ~ADTSDemuxer() override = default;

    /**
     * @brief Find the first frame (past any ID3v2 tag), describe the stream
     *        from it and, for a file, walk every frame for duration and offsets.
     * @return true if a usable ADTS stream was found.
     */
    bool parseContainer() override;

    /**
     * @brief The one audio stream, once parsed.
     * @return It, or nothing before a successful parseContainer().
     */
    std::vector<StreamInfo> getStreams() const override;

    /**
     * @brief Describe the stream.
     * @param stream_id 0 or the stream's id.
     * @return Its StreamInfo, or an empty one for another id.
     */
    StreamInfo getStreamInfo(uint32_t stream_id) const override;

    /**
     * @brief The next frame's raw_data_block, without its ADTS header.
     * @return The chunk; an end-of-stream chunk or an empty one at the end.
     */
    MediaChunk readChunk() override;

    /**
     * @brief As readChunk(), for the stream's own id (or 0).
     * @param stream_id Which stream.
     * @return The chunk, or an empty one for another id.
     */
    MediaChunk readChunk(uint32_t stream_id) override;

    /**
     * @brief Move to the frame before the one holding @p timestamp_ms (the
     *        decoder needs a frame of overlap to rebuild its output).
     * @param timestamp_ms Target time.
     * @return false for a live stream or before parsing.
     */
    bool seekTo(uint64_t timestamp_ms) override;

    /**
     * @brief Whether the last frame has been read.
     * @return true at the end.
     */
    bool isEOF() const override;

    /**
     * @brief Length of a file in milliseconds.
     * @return It, or 0 for a live stream.
     */
    uint64_t getDuration() const override;

    /**
     * @brief Time of the next frame to be read.
     * @return Milliseconds from the start.
     */
    uint64_t getPosition() const override;

    /**
     * @brief The framing's name, for Media Information.
     * @return "ADTS".
     */
    std::string getContainerName() const override { return "ADTS"; }

    /**
     * @brief Parse an ADTS header.
     * @param data Bytes at a candidate frame start.
     * @param size How many there are (at least 7 are needed).
     * @param header Filled in on success.
     * @return true if the bytes are a plausible ADTS header: sync, layer 0, a
     *         known sample rate and a frame length that covers the header.
     */
    static bool parseHeader(const uint8_t* data, size_t size, ADTSHeader& header);

    /**
     * @brief The two-byte AudioSpecificConfig describing frames with this
     *        header: object type, sampling frequency index, channel
     *        configuration, and a GASpecificConfig of three zero bits.
     * @param header A parsed header.
     * @return The config bytes.
     */
    static std::vector<uint8_t> makeAudioSpecificConfig(const ADTSHeader& header);

private:
    /** @brief parseContainer() with m_mutex held. */
    bool parseContainer_unlocked();

    /** @brief readChunk() with m_mutex held. */
    MediaChunk readChunk_unlocked();

    /** @brief seekTo() with m_mutex held. */
    bool seekTo_unlocked(uint64_t timestamp_ms);

    /**
     * @brief Read and parse the header at @p offset.
     * @param offset Byte offset of a frame.
     * @param header Filled in on success.
     * @return true if a header is there.
     */
    bool readHeaderAt_unlocked(uint64_t offset, ADTSHeader& header);

    /**
     * @brief The first frame at or after @p from whose header parses and is
     *        followed by another header exactly where it says it ends (or by
     *        the end of a file), searching at most kSyncSearchLimit bytes.
     * @param from Where to start looking.
     * @param found_at Set to the frame's offset.
     * @param found Set to its header.
     * @return true if one was found.
     */
    bool findFrame_unlocked(uint64_t from, uint64_t& found_at, ADTSHeader& found);

    /**
     * @brief Offset just past an ID3v2 tag at the start, or 0 if there is none.
     * @return Where to start looking for frames.
     */
    uint64_t skipID3v2_unlocked();

    StreamInfo m_stream_info;
    bool m_live = false;                ///< internet radio: no end, no duration, no seeking
    uint64_t m_file_size = 0;           ///< 0 for a live stream
    uint64_t m_data_start_offset = 0;   ///< first frame
    uint64_t m_read_offset = 0;         ///< where readChunk resumes
    uint64_t m_current_sample = 0;      ///< samples before m_read_offset
    uint64_t m_total_samples = 0;       ///< a file's length in samples
    std::vector<uint64_t> m_frame_offsets;  ///< a file's frames, for seeking
    bool m_warned_multi_block = false;  ///< logged a skipped multi-block frame

    mutable std::mutex m_mutex;
};

} // namespace ADTS
} // namespace Demuxer
} // namespace PsyMP3

#endif // PSYMP3_DEMUXER_ADTS_ADTSDEMUXER_H

/*
 * RIFFDemuxer.h - RIFF container demuxer (WAV, AVI, etc.)
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

#ifndef RIFFDEMUXER_H
#define RIFFDEMUXER_H

#include "Demuxer.h"
#include <map>

/**
 * @brief RIFF chunk header
 */
struct RIFFChunk {
    uint32_t fourcc;      // Chunk identifier (FourCC)
    uint32_t size;        // Chunk size (excluding header)
    uint64_t data_offset; // Offset to chunk data in file
    
    bool isListChunk() const {
        return fourcc == 0x5453494C; // "LIST"
    }
    
    bool isRiffChunk() const {
        return fourcc == 0x46464952; // "RIFF"
    }
};

/**
 * @brief RIFF demuxer for WAV files and other RIFF-based formats
 * 
 * This demuxer handles the RIFF container format, which is used by:
 * - WAV files (RIFF WAVE)
 * - AVI files (RIFF AVI)
 * - And other Microsoft formats
 */
class RIFFDemuxer : public Demuxer {
public:
    explicit RIFFDemuxer(std::unique_ptr<IOHandler> handler);
    
    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    MediaChunk readChunk() override;
    MediaChunk readChunk(uint32_t stream_id) override;
    bool seekTo(uint64_t timestamp_ms) override;
    bool isEOF() const override;
    uint64_t getDuration() const override;
    uint64_t getPosition() const override;
    
    /**
     * @brief Get the RIFF form type (WAVE, AVI, etc.)
     */
    uint32_t getFormType() const { return m_form_type; }
    
    /**
     * @brief Check if this is a WAVE file
     */
    bool isWaveFile() const { return m_form_type == 0x45564157; } // "WAVE"
    
private:
    // RIFF FourCC constants
    static constexpr uint32_t RIFF_FOURCC = 0x46464952; // "RIFF"
    static constexpr uint32_t LIST_FOURCC = 0x5453494C; // "LIST"
    static constexpr uint32_t WAVE_FOURCC = 0x45564157; // "WAVE"
    static constexpr uint32_t FMT_FOURCC  = 0x20746d66; // "fmt "
    static constexpr uint32_t DATA_FOURCC = 0x61746164; // "data"
    static constexpr uint32_t FACT_FOURCC = 0x74636166; // "fact"
    
    // WAVE format tags (avoiding Windows header conflicts)
    static constexpr uint16_t WAVE_FORMAT_PCM = 0x0001;
    static constexpr uint16_t WAVE_FORMAT_MPEGLAYER3 = 0x0055;
    static constexpr uint16_t WAVE_FORMAT_IEEE_FLOAT = 0x0003;
    static constexpr uint16_t WAVE_FORMAT_ALAW = 0x0006;
    static constexpr uint16_t WAVE_FORMAT_MULAW = 0x0007;
    static constexpr uint16_t WAVE_FORMAT_EXTENSIBLE = 0xFFFE;
    
    struct AudioStreamData {
        uint32_t stream_id;
        uint64_t data_offset;      // Start of audio data
        uint64_t data_size;        // Size of audio data
        uint64_t current_offset;   // Current read position
        uint32_t bytes_per_frame;  // Bytes per sample frame (all channels)
        
        // Format information
        uint16_t format_tag;
        uint16_t channels;
        uint32_t sample_rate;
        uint32_t avg_bytes_per_sec;
        uint16_t block_align;
        uint16_t bits_per_sample;
        std::vector<uint8_t> extra_data; // Extra format data if any
    };
    
    uint32_t m_form_type = 0;                    // RIFF form type (WAVE, AVI, etc.)
    std::map<uint32_t, AudioStreamData> m_audio_streams;
    uint32_t m_current_stream_id = 0;
    bool m_eof = false;
    
    /**
     * @brief Read a RIFF chunk header
     */
    RIFFChunk readChunkHeader();
    
    /**
     * @brief Parse WAVE format chunk
     */
    bool parseWaveFormat(const RIFFChunk& chunk);
    
    /**
     * @brief Parse WAVE data chunk
     */
    bool parseWaveData(const RIFFChunk& chunk);
    
    /**
     * @brief Skip a chunk (seek past it)
     */
    void skipChunk(const RIFFChunk& chunk);
    
    /**
     * @brief Convert WAVE format tag to codec name
     */
    std::string formatTagToCodecName(uint16_t format_tag) const;
    
    /**
     * @brief Calculate position in milliseconds from byte offset
     */
    uint64_t byteOffsetToMs(uint64_t byte_offset, uint32_t stream_id) const;
    
    /**
     * @brief Calculate byte offset from position in milliseconds
     */
    uint64_t msToByteOffset(uint64_t timestamp_ms, uint32_t stream_id) const;
};

#endif // RIFFDEMUXER_H
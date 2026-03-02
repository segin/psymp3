/*
 * ChunkDemuxer.h - Universal chunk-based demuxer (RIFF/WAV, IFF, AIFF)
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

#ifndef CHUNKDEMUXER_H
#define CHUNKDEMUXER_H

namespace PsyMP3 {
namespace Demuxer {

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Universal chunk header for IFF/AIFF/RIFF formats
 */
struct Chunk {
    uint32_t fourcc;      // Chunk identifier (FourCC)
    uint32_t size;        // Chunk size (excluding header)
    uint64_t data_offset; // Offset to chunk data in file
    
    bool isContainer() const {
        return fourcc == 0x4D524F46 || // "FORM" (big-endian IFF/AIFF)
               fourcc == 0x46464952 || // "RIFF" (little-endian)
               fourcc == 0x5453494C;   // "LIST" (little-endian)
    }
};

/**
 * @brief Universal chunk-based demuxer
 * 
 * Supports:
 * - Microsoft RIFF (little-endian): WAV files
 * - Amiga IFF (big-endian): General interchange format
 * - Apple AIFF (big-endian): Audio interchange format
 */
class ChunkDemuxer : public Demuxer {
public:
    explicit ChunkDemuxer(std::unique_ptr<IOHandler> handler);
    
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
     * @brief Get the container form type (WAVE, AIFF, etc.)
     */
    uint32_t getFormType() const { return m_form_type; }
    
    /**
     * @brief Check endianness of the format
     */
    bool isBigEndian() const { return m_big_endian; }
    
    /**
     * @brief Check if this is a WAVE file
     */
    bool isWaveFile() const { return m_form_type == WAVE_FOURCC; }
    
    /**
     * @brief Check if this is an AIFF file
     */
    bool isAiffFile() const { return m_form_type == AIFF_FOURCC; }
    
private:
    // Container FourCC constants (Always read as Big-Endian)
    static constexpr uint32_t FORM_FOURCC = 0x464F524D; // "FORM"
    static constexpr uint32_t RIFF_FOURCC = 0x52494646; // "RIFF"
    static constexpr uint32_t LIST_FOURCC = 0x4C495354; // "LIST"
    
    // Format type constants (Read using container endianness)
    static constexpr uint32_t WAVE_FOURCC = 0x45564157; // "WAVE" (read as little-endian)
    static constexpr uint32_t AIFF_FOURCC = 0x41494646; // "AIFF" (read as big-endian)
    
    // RIFF/WAV chunk constants (FourCC always read as Big-Endian)
    static constexpr uint32_t FMT_FOURCC  = 0x666d7420; // "fmt "
    static constexpr uint32_t DATA_FOURCC = 0x64617461; // "data"
    static constexpr uint32_t FACT_FOURCC = 0x66616374; // "fact"
    
    // AIFF chunk constants (big-endian)
    static constexpr uint32_t COMM_FOURCC = 0x434F4D4D; // "COMM"
    static constexpr uint32_t SSND_FOURCC = 0x53534E44; // "SSND"
    
    // WAVE format tags
#ifndef WAVE_FORMAT_PCM
    static constexpr uint16_t WAVE_FORMAT_PCM = 0x0001;
#endif
    static constexpr uint16_t WAVE_FORMAT_MPEGLAYER3 = 0x0055;
    static constexpr uint16_t WAVE_FORMAT_IEEE_FLOAT = 0x0003;
    static constexpr uint16_t WAVE_FORMAT_ALAW = 0x0006;
    static constexpr uint16_t WAVE_FORMAT_MULAW = 0x0007;
    static constexpr uint16_t WAVE_FORMAT_EXTENSIBLE = 0xFFFE;
    
    // AIFF compression types
    static constexpr uint32_t AIFF_NONE = 0x4E4F4E45; // "NONE"
    static constexpr uint32_t AIFF_SOWT = 0x736F7774; // "sowt" (byte-swapped PCM)
    static constexpr uint32_t AIFF_FL32 = 0x666C3332; // "fl32" (32-bit float)
    static constexpr uint32_t AIFF_FL64 = 0x666C3634; // "fl64" (64-bit float)
    static constexpr uint32_t AIFF_ALAW = 0x616C6177; // "alaw"
    static constexpr uint32_t AIFF_ULAW = 0x756C6177; // "ulaw"
    
    struct AudioStreamData {
        uint32_t stream_id;
        uint64_t data_offset;      // Start of audio data
        uint64_t data_size;        // Size of audio data
        uint64_t current_offset;   // Current read position
        uint32_t bytes_per_frame;  // Bytes per sample frame (all channels)
        
        // Format information
        uint16_t format_tag;       // WAV format or derived from AIFF compression
        uint16_t channels;
        uint32_t sample_rate;
        uint32_t avg_bytes_per_sec;
        uint16_t block_align;
        uint16_t bits_per_sample;
        uint32_t compression_type; // AIFF compression type
        std::vector<uint8_t> extra_data;
        
        // AIFF-specific
        uint32_t ssnd_offset = 0;      // SSND chunk offset field
        uint32_t ssnd_block_size = 0;  // SSND chunk block size field
        
        // Metadata fields
        std::string title;         // Track title (NAME chunk in AIFF, INAM in WAV)
        std::string artist;        // Artist name (AUTH chunk in AIFF, IART in WAV)
        std::string album;         // Album name (from LIST INFO chunk in WAV)
        std::string copyright;     // Copyright info
        std::string comment;       // Comments/annotations
        
        // Additional format info
        uint32_t total_samples = 0;    // Total number of sample frames (from fact chunk)
        bool has_fact_chunk = false;   // Whether fact chunk was present
    };
    
    uint32_t m_container_fourcc = 0;             // FORM or RIFF
    uint32_t m_form_type = 0;                    // AIFF, WAVE, etc.
    bool m_big_endian = false;                   // Endianness of the format
    std::map<uint32_t, AudioStreamData> m_audio_streams;
    uint32_t m_current_stream_id = 0;
    uint64_t m_current_sample = 0;
    bool m_eof = false;
    
    /**
     * @brief Read chunk value with format-appropriate endianness
     */
    template<typename T>
    T readChunkValue() {
        return m_big_endian ? readBE<T>() : readLE<T>();
    }
    
    /**
     * @brief Read chunk header
     */
    Chunk readChunkHeader();
    
    /**
     * @brief Parse RIFF/WAV format chunk
     */
    bool parseWaveFormat(const Chunk& chunk);
    
    /**
     * @brief Parse RIFF/WAV data chunk
     */
    bool parseWaveData(const Chunk& chunk);
    
    /**
     * @brief Parse AIFF common chunk (COMM)
     */
    bool parseAiffCommon(const Chunk& chunk);
    
    /**
     * @brief Parse AIFF sound data chunk (SSND)
     */
    bool parseAiffSoundData(const Chunk& chunk);
    
    /**
     * @brief Parse WAV fact chunk
     */
    void parseWaveFact(const Chunk& chunk);
    
    /**
     * @brief Parse WAV LIST chunk for metadata
     */
    void parseWaveList(const Chunk& chunk);
    
    /**
     * @brief Parse AIFF NAME chunk
     */
    void parseAiffName(const Chunk& chunk);
    
    /**
     * @brief Parse AIFF AUTH chunk
     */
    void parseAiffAuth(const Chunk& chunk);
    
    /**
     * @brief Parse AIFF copyright chunk
     */
    void parseAiffCopyright(const Chunk& chunk);
    
    /**
     * @brief Parse AIFF annotation chunk
     */
    void parseAiffAnnotation(const Chunk& chunk);
    
    /**
     * @brief Skip a chunk (seek past it)
     */
    void skipChunk(const Chunk& chunk);
    
    /**
     * @brief Convert format info to codec name
     */
    std::string getCodecName(const AudioStreamData& stream) const;
    
    /**
     * @brief Convert WAVE format tag to codec name
     */
    std::string formatTagToCodecName(uint16_t format_tag) const;
    
    /**
     * @brief Convert AIFF compression type to codec name
     */
    std::string aiffCompressionToCodecName(uint32_t compression) const;
    
    /**
     * @brief Convert IEEE 80-bit extended precision to double
     */
    double ieee80ToDouble(const uint8_t ieee80[10]) const;
    
    /**
     * @brief Calculate position in milliseconds from byte offset
     */
    uint64_t byteOffsetToMs(uint64_t byte_offset, uint32_t stream_id) const;
    
    /**
     * @brief Calculate byte offset from position in milliseconds
     */
    uint64_t msToByteOffset(uint64_t timestamp_ms, uint32_t stream_id) const;
    
    // Error recovery methods (override base class methods)
    
    /**
     * @brief Skip to next valid chunk
     */
    bool skipToNextValidSection() const override;
    
    /**
     * @brief Reset chunk parsing state
     */
    bool resetInternalState() const override;
    
    /**
     * @brief Enable fallback parsing mode for corrupted chunk files
     */
    bool enableFallbackMode() const override;
    
    /**
     * @brief Recover from corrupted chunk header
     */
    bool recoverFromCorruptedChunk(const Chunk& chunk);
    
    /**
     * @brief Validate chunk header integrity
     */
    bool validateChunkHeader(const Chunk& chunk) const;
    
    /**
     * @brief Handle chunk size inconsistencies
     */
    bool handleChunkSizeError(Chunk& chunk);
    
private:
    // Error recovery state
    mutable bool m_fallback_mode = false;
    mutable long m_last_valid_chunk_position = 0;
};


} // namespace Demuxer
} // namespace PsyMP3
#endif // CHUNKDEMUXER_H
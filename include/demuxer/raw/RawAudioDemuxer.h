/*
 * RawAudioDemuxer.h - Raw audio format demuxer (no container)
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

#ifndef RAWAUDIODEMUXER_H
#define RAWAUDIODEMUXER_H

namespace PsyMP3 {
namespace Demuxer {
namespace Raw {

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Configuration for raw audio formats
 */
struct RawAudioConfig {
    std::string codec_name;    // "mulaw", "alaw", "pcm8", "pcm16le", etc.
    uint32_t sample_rate;      // Default sample rate (e.g., 8000 for telephony)
    uint16_t channels;         // Default channels (usually 1 for telephony)
    uint16_t bits_per_sample;  // Bits per sample
    bool little_endian;        // Byte order for multi-byte formats
    
    // Constructor with telephony defaults
    RawAudioConfig(const std::string& codec = "mulaw", 
                   uint32_t rate = 8000, 
                   uint16_t ch = 1, 
                   uint16_t bits = 8, 
                   bool le = true)
        : codec_name(codec), sample_rate(rate), channels(ch), 
          bits_per_sample(bits), little_endian(le) {}
};

/**
 * @brief Demuxer for raw audio files with no container format
 * 
 * This handles raw audio streams commonly found in telephony/VoIP:
 * - .ulaw/.ul files (8kHz, mono, mu-law)
 * - .alaw/.al files (8kHz, mono, A-law)  
 * - .pcm files (various sample rates and bit depths)
 * - .s16le/.s16be files (16-bit signed PCM)
 * - .f32le files (32-bit float PCM)
 * 
 * Since there's no header, format detection is based on file extension
 * and/or user-provided configuration.
 */
class RawAudioDemuxer : public Demuxer {
public:
    /**
     * @brief Constructor with automatic format detection
     * @param handler IOHandler for the file
     * @param file_path Original file path (for extension-based detection)
     */
    RawAudioDemuxer(std::unique_ptr<IOHandler> handler, const std::string& file_path);
    
    /**
     * @brief Constructor with explicit configuration
     * @param handler IOHandler for the file
     * @param config Raw audio format configuration
     */
    RawAudioDemuxer(std::unique_ptr<IOHandler> handler, const RawAudioConfig& config);
    
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
     * @brief Get the detected/configured format
     */
    const RawAudioConfig& getConfig() const { return m_config; }
    
    
private:
    RawAudioConfig m_config;
    std::string m_file_path;
    uint64_t m_file_size = 0;
    uint64_t m_current_offset = 0;
    uint32_t m_bytes_per_frame = 0;  // Bytes per sample frame (all channels)
    bool m_eof = false;
    
    /**
     * @brief Calculate duration based on file size and format
     */
    void calculateDuration();
    
    /**
     * @brief Convert byte offset to timestamp
     */
    uint64_t byteOffsetToMs(uint64_t byte_offset) const;
    
    /**
     * @brief Convert timestamp to byte offset
     */
    uint64_t msToByteOffset(uint64_t timestamp_ms) const;
};

/**
 * @brief Factory helper for raw audio format detection
 */
class RawAudioDetector {
public:
    /**
     * @brief Check if a file is likely a raw audio format
     * @param file_path File path to check
     * @param handler Optional IOHandler for content analysis
     * @return Configuration if detected, empty optional otherwise
     */
    static std::optional<RawAudioConfig> detectRawAudio(const std::string& file_path, 
                                                        IOHandler* handler = nullptr);
    
    /**
     * @brief Check if a file extension indicates a raw audio format
     */
    static bool isRawAudioExtension(const std::string& file_path);
    
    /**
     * @brief Detect raw audio configuration from file extension
     */
    static RawAudioConfig detectFromExtension(const std::string& file_path);
    
private:
    /**
     * @brief Map of file extensions to raw audio configurations
     */
    static const std::map<std::string, RawAudioConfig> s_extension_map;
};


} // namespace Raw
} // namespace Demuxer
} // namespace PsyMP3
#endif // RAWAUDIODEMUXER_H
/*
 * AudioCodec.h - Generic audio codec base classes
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

#ifndef AUDIOCODEC_H
#define AUDIOCODEC_H

#include "Demuxer.h"
#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <string>

/**
 * @brief Decoded audio frame
 */
struct AudioFrame {
    std::vector<int16_t> samples;    // Decoded PCM samples (16-bit signed)
    uint32_t sample_rate;            // Sample rate of this frame
    uint16_t channels;               // Number of channels
    uint64_t timestamp_samples;      // Timestamp in sample units
    uint64_t timestamp_ms;           // Timestamp in milliseconds
    
    /**
     * @brief Get the number of bytes in this frame
     */
    size_t getByteCount() const {
        return samples.size() * sizeof(int16_t);
    }
    
    /**
     * @brief Get the number of sample frames (samples per channel)
     */
    size_t getSampleFrameCount() const {
        return channels > 0 ? samples.size() / channels : 0;
    }
    
    /**
     * @brief Get duration of this frame in milliseconds
     */
    uint64_t getDurationMs() const {
        if (sample_rate == 0 || channels == 0) return 0;
        return (getSampleFrameCount() * 1000ULL) / sample_rate;
    }
};

/**
 * @brief Base class for all audio codecs
 * 
 * Audio codecs are responsible for decoding specific audio formats
 * (PCM, MP3, AAC, FLAC, etc.) into standard 16-bit PCM output.
 * They consume MediaChunks from demuxers and produce AudioFrames.
 */
class AudioCodec {
public:
    /**
     * @brief Initialize codec with stream information
     * @param stream_info Information about the audio stream
     */
    explicit AudioCodec(const StreamInfo& stream_info);
    virtual ~AudioCodec() = default;
    
    /**
     * @brief Initialize the codec with any necessary setup
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Decode a chunk of audio data
     * @param chunk Input data chunk from demuxer
     * @return Decoded audio frame, or empty frame if no output yet
     */
    virtual AudioFrame decode(const MediaChunk& chunk) = 0;
    
    /**
     * @brief Flush any remaining audio data from internal buffers
     * @return Decoded audio frame, or empty frame if no data remaining
     */
    virtual AudioFrame flush() = 0;
    
    /**
     * @brief Reset codec state (for seeking)
     */
    virtual void reset() = 0;
    
    /**
     * @brief Get the codec's name/type
     */
    virtual std::string getCodecName() const = 0;
    
    /**
     * @brief Check if this codec can handle the given stream info
     */
    virtual bool canDecode(const StreamInfo& stream_info) const = 0;
    
    /**
     * @brief Get stream information
     */
    const StreamInfo& getStreamInfo() const { return m_stream_info; }
    
protected:
    StreamInfo m_stream_info;
    bool m_initialized = false;
};

/**
 * @brief Factory for creating appropriate codecs based on stream information
 */
class AudioCodecFactory {
public:
    /**
     * @brief Create a codec for the given stream
     * @param stream_info Information about the audio stream
     * @return Unique pointer to appropriate codec, or nullptr if not supported
     */
    static std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
    
    /**
     * @brief Register a codec factory function
     * @param codec_name Name of the codec (e.g., "pcm", "mp3")
     * @param factory_func Function that creates codec instances
     */
    using CodecFactoryFunc = std::function<std::unique_ptr<AudioCodec>(const StreamInfo&)>;
    static void registerCodec(const std::string& codec_name, CodecFactoryFunc factory_func);
    
private:
    static std::map<std::string, CodecFactoryFunc> s_codec_factories;
};

/**
 * @brief Base class for simple PCM-based codecs
 * 
 * This handles codecs that can decode data in-place without complex
 * state management (like mu-law, A-law, simple PCM variants).
 */
class SimplePCMCodec : public AudioCodec {
public:
    explicit SimplePCMCodec(const StreamInfo& stream_info);
    
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    
protected:
    /**
     * @brief Convert raw sample data to 16-bit PCM
     * @param input_data Raw input data
     * @param output_samples Output vector to fill with 16-bit samples
     * @return Number of samples converted
     */
    virtual size_t convertSamples(const std::vector<uint8_t>& input_data, 
                                  std::vector<int16_t>& output_samples) = 0;
    
    /**
     * @brief Get number of bytes per input sample
     */
    virtual size_t getBytesPerInputSample() const = 0;
};

#endif // AUDIOCODEC_H
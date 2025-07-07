/*
 * DemuxedStream.h - Stream implementation using demuxer/codec architecture
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

#ifndef DEMUXEDSTREAM_H
#define DEMUXEDSTREAM_H

#include "stream.h"
#include "Demuxer.h"
#include "AudioCodec.h"
#include <memory>
#include <queue>
#include <cstring>

/**
 * @brief Stream implementation that uses the new demuxer/codec architecture
 * 
 * This class bridges the old Stream interface with the new modular
 * demuxer/codec system. It can handle any container format that has
 * a demuxer implementation and any audio codec that has a codec implementation.
 */
class DemuxedStream : public Stream {
public:
    /**
     * @brief Constructor for DemuxedStream
     * @param path File path to open
     * @param preferred_stream_id Preferred audio stream ID (0 = auto-select first audio stream)
     */
    explicit DemuxedStream(const TagLib::String& path, uint32_t preferred_stream_id = 0);
    
    /**
     * @brief Destructor
     */
    ~DemuxedStream() override = default;
    
    // Stream interface implementation
    size_t getData(size_t len, void *buf) override;
    void seekTo(unsigned long pos) override;
    bool eof() override;
    
    /**
     * @brief Get information about all available streams
     */
    std::vector<StreamInfo> getAvailableStreams() const;
    
    /**
     * @brief Switch to a different audio stream
     * @param stream_id Stream ID to switch to
     * @return true if switch was successful
     */
    bool switchToStream(uint32_t stream_id);
    
    /**
     * @brief Get current stream information
     */
    StreamInfo getCurrentStreamInfo() const;
    
    /**
     * @brief Get demuxer type name
     */
    std::string getDemuxerType() const;
    
    /**
     * @brief Get codec type name
     */
    std::string getCodecType() const;
    
private:
    std::unique_ptr<Demuxer> m_demuxer;
    std::unique_ptr<AudioCodec> m_codec;
    uint32_t m_current_stream_id = 0;
    
    // Audio frame buffering
    std::queue<AudioFrame> m_frame_buffer;
    AudioFrame m_current_frame;
    size_t m_current_frame_offset = 0;  // Byte offset within current frame
    
    bool m_eof_reached = false;
    
    /**
     * @brief Initialize demuxer and codec
     */
    bool initialize();
    
    /**
     * @brief Select the best audio stream from available streams
     */
    uint32_t selectBestAudioStream() const;
    
    /**
     * @brief Setup codec for the current stream
     */
    bool setupCodec();
    
    /**
     * @brief Fill the frame buffer by reading and decoding data
     */
    void fillFrameBuffer();
    
    /**
     * @brief Get next frame from buffer, filling if necessary
     */
    AudioFrame getNextFrame();
    
    /**
     * @brief Copy data from audio frame to output buffer
     * @param frame Source audio frame
     * @param frame_offset Byte offset within the frame
     * @param output_buf Destination buffer
     * @param output_len Maximum bytes to copy
     * @return Number of bytes actually copied
     */
    size_t copyFrameData(const AudioFrame& frame, size_t frame_offset, 
                         void* output_buf, size_t output_len);
    
    /**
     * @brief Update Stream base class properties from current stream info
     */
    void updateStreamProperties();
};

#endif // DEMUXEDSTREAM_H
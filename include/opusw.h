/*
 * opusw.h - Opus decoder class header using generic demuxer architecture
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef OPUSW_H
#define OPUSW_H

#ifdef HAVE_OGGDEMUXER

/**
 * @brief Opus decoder class using DemuxedStream with generic demuxer support
 * 
 * This class replaces the old opusfile-based implementation with a cleaner
 * approach that delegates to DemuxedStream which can use any demuxer
 * (OggDemuxer for .opus files, other demuxers for different containers)
 * and OpusCodec for audio decoding.
 */
class OpusFile : public Stream
{
public:
    OpusFile(TagLib::String name);
    virtual ~OpusFile();
    
    // Stream interface implementation
    virtual size_t getData(size_t len, void *buf) override;
    virtual void seekTo(unsigned long pos) override;
    virtual bool eof() override;
    
    // Override metadata methods to use Ogg container metadata
    virtual TagLib::String getArtist() override;
    virtual TagLib::String getTitle() override;
    virtual TagLib::String getAlbum() override;
    
private:
    std::unique_ptr<DemuxedStream> m_demuxed_stream;
};

/**
 * @brief Direct Opus codec using libopus
 * 
 * This codec processes Opus packets directly from any demuxer using
 * the low-level libopus API instead of opusfile.
 * Works with any container format that can deliver Opus packets.
 */
class OpusCodec : public AudioCodec
{
public:
    explicit OpusCodec(const StreamInfo& stream_info);
    ~OpusCodec() override;
    
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "opus"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    OpusDecoder* m_opus_decoder = nullptr;
    int m_header_packets_received = 0;
    bool m_decoder_initialized = false;
    std::vector<int16_t> m_output_buffer;
    
    // Opus stream properties
    int m_channels = 0;
    int m_sample_rate = 48000; // Opus always outputs at 48kHz
    
    bool processHeaderPacket(const std::vector<uint8_t>& packet_data);
};

#endif // HAVE_OGGDEMUXER

#endif // OPUSW_H

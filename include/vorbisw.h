/*
 * vorbisw.h - Ogg Vorbis decoder class header using OggDemuxer and libvorbis
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

#ifndef VORBISW_H
#define VORBISW_H

#ifdef HAVE_VORBIS

/**
 * @brief Vorbis decoder class using DemuxedStream with OggDemuxer
 * 
 * This class replaces the old vorbisfile-based implementation with a cleaner
 * approach that delegates to DemuxedStream which uses OggDemuxer for container
 * parsing and VorbisCodec for audio decoding.
 */
class Vorbis : public Stream
{
public:
    Vorbis(TagLib::String name);
    virtual ~Vorbis();
    
    // Stream interface implementation
    virtual size_t getData(size_t len, void *buf) override;
    virtual void seekTo(unsigned long pos) override;
    virtual bool eof() override;
    
private:
    std::unique_ptr<DemuxedStream> m_demuxed_stream;
};

/**
 * @brief Direct Vorbis codec using libvorbis
 * 
 * This codec processes Vorbis packets directly from OggDemuxer using
 * the low-level libvorbis API instead of vorbisfile.
 */
class VorbisCodec : public AudioCodec
{
public:
    explicit VorbisCodec(const StreamInfo& stream_info);
    ~VorbisCodec() override;
    
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "vorbis"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    vorbis_info m_vorbis_info;
    vorbis_comment m_vorbis_comment;
    vorbis_dsp_state m_vorbis_dsp;
    vorbis_block m_vorbis_block;
    
    int m_header_packets_received = 0;
    bool m_synthesis_initialized = false;
    std::vector<int16_t> m_output_buffer;
    
    bool processHeaderPacket(const std::vector<uint8_t>& packet_data);
    bool processSynthesis();
};

#endif // HAVE_VORBIS

#endif // VORBISW_H

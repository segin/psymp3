/*
 * opus.cpp - Opus decoder using generic demuxer architecture and libopus directly
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

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

// ========== OpusFile Stream Class ==========

OpusFile::OpusFile(TagLib::String name) : Stream()
{
    m_path = name; // Set path manually to avoid TagLib FileRef creation
    
    // Create a DemuxedStream to handle all parsing (container + metadata)
    m_demuxed_stream = std::make_unique<DemuxedStream>(name);
    
    // Copy properties from the demuxed stream
    m_rate = m_demuxed_stream->getRate();
    m_channels = m_demuxed_stream->getChannels();
    m_bitrate = m_demuxed_stream->getBitrate();
    m_length = m_demuxed_stream->getLength();
    m_slength = m_demuxed_stream->getSLength();
}

OpusFile::~OpusFile()
{
    // Unique pointer handles cleanup
}

size_t OpusFile::getData(size_t len, void *buf)
{
    return m_demuxed_stream->getData(len, buf);
}

void OpusFile::seekTo(unsigned long pos)
{
    m_demuxed_stream->seekTo(pos);
}

bool OpusFile::eof()
{
    return m_demuxed_stream->eof();
}

TagLib::String OpusFile::getArtist()
{
    return m_demuxed_stream->getArtist();
}

TagLib::String OpusFile::getTitle()
{
    return m_demuxed_stream->getTitle();
}

TagLib::String OpusFile::getAlbum()
{
    return m_demuxed_stream->getAlbum();
}

// OpusCodec implementation is now in OpusCodec.cpp

#endif // HAVE_OGGDEMUXER
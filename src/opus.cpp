/*
 * vorbis.cpp - Extends the Stream base class to decode Ogg Vorbis.
 * This file is part of PsyMP3.
 * Copyright © 2011-2024 Kirn Gill <segin2005@gmail.com>
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

OpusFile::OpusFile(TagLib::String name) : Stream(name)
{
    m_eof = false;
    open(name);
}

OpusFile::~OpusFile()
{
    op_free(static_cast<OggOpusFile *>(m_handle));
    m_handle = nullptr;
}

void OpusFile::open(TagLib::String name)
{
    int error;
    m_handle = op_open_file(name.toCString(true), &error);
    if(!m_handle) 
        throw InvalidMediaException("Failed to open Opus file: " + name.to8Bit(true));
    const OpusHead *m_oi = op_head(static_cast<OggOpusFile *>(m_handle), -1);
    m_channels = 2;
    m_bitrate = 0;
    m_slength = op_pcm_total(static_cast<OggOpusFile *>(m_handle), -1);
    m_rate = 48000;
    m_length = m_slength / 48;
}

void OpusFile::seekTo(unsigned long pos)
{
    op_pcm_seek(static_cast<OggOpusFile *>(m_handle), pos * 48);
    m_sposition = op_pcm_tell(static_cast<OggOpusFile *>(m_handle));
    m_position = m_sposition / 48;
}

size_t OpusFile::getData(size_t len, void *buf)
{
    auto nbuf = buf;
    auto nlen = len;
    auto ret = 0, tret = 0;
    do { 
        ret = op_read_stereo(static_cast<OggOpusFile *>(m_handle), static_cast<opus_int16 *>(nbuf), nlen / 2);
        if (ret == OP_HOLE || ret == OP_EBADLINK || ret == OP_EINVAL)
            throw BadFormatException("Failed to read Opus file");
        tret += ret;
        nlen -= ret * 4;
        nbuf = static_cast<char*>(buf) + (static_cast<char*>(buf) - static_cast<char*>(buf) + len - nlen);
        if(!tret) m_eof = true;
    } while (ret && nlen);
    m_sposition = op_pcm_tell(static_cast<OggOpusFile *>(m_handle));
    m_position = m_sposition / 48;
    return tret * 4;       
}

unsigned int OpusFile::getLength()
{
    return m_length;
}

unsigned long long OpusFile::getSLength()
{
    return m_slength;
}

unsigned long long OpusFile::getSPosition()
{
    return m_sposition;
}

unsigned int OpusFile::getChannels()
{
    return m_channels;
}

unsigned int OpusFile::getRate()
{
    return m_rate;
}

unsigned int OpusFile::getEncoding()
{
    return 0;
}

unsigned int OpusFile::getBitrate()
{
    return m_bitrate;
}

bool OpusFile::eof()
{
    return m_eof;
}


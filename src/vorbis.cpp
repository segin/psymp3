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

Vorbis::Vorbis(TagLib::String name) : Stream(name)
{
    m_handle = new OggVorbis_File;
    m_session = 0;
    open(name);
}

Vorbis::~Vorbis()
{
    ov_clear((OggVorbis_File *) m_handle);
    delete (OggVorbis_File *) m_handle;
    m_handle = NULL;
}

void Vorbis::open(TagLib::String name)
{
    int ret = ov_fopen((char *) name.toCString(true), (OggVorbis_File *) m_handle);
    switch (ret) {
    case OV_ENOTVORBIS:
        throw WrongFormatException("Not a Vorbis file: " + name);
        break;
    case OV_EREAD:
    case OV_EVERSION:
    case OV_EBADHEADER:
    case OV_EFAULT:
        throw BadFormatException("Bad file: " + name);
        //throw;
        break;
    default: // returned 0 for success
        m_vi = ov_info((OggVorbis_File *) m_handle, -1);
        switch(m_vi->channels) {
        case 1:
        case 2:
            m_channels = m_vi->channels;
            m_bitrate = m_vi->bitrate_nominal;
            m_length = ov_time_total((OggVorbis_File *) m_handle, -1) * 1000;
            m_slength = ov_pcm_total((OggVorbis_File *) m_handle, -1);
            break;
        default:
            // throw
            break;
        };
        break;
    };
}

void Vorbis::seekTo(unsigned long pos)
{
    ov_time_seek((OggVorbis_File *) m_handle, (double) pos / 1000.0);
    m_sposition = ov_pcm_tell((OggVorbis_File *) m_handle);
    m_position = ov_time_tell((OggVorbis_File *) m_handle) * 1000;
}

size_t Vorbis::getData(size_t len, void *buf)
{
    std::cout << "Vorbis::getData(): len = " << len << std::endl;
    long ret = ov_read((OggVorbis_File *) m_handle, (char *) buf, len, 0, 2, 1, &m_session);
    m_sposition = ov_pcm_tell((OggVorbis_File *) m_handle);
    m_position = ov_time_tell((OggVorbis_File *) m_handle) * 1000;
    return (size_t) ret;
}

unsigned int Vorbis::getLength()
{
    return m_length;
}

unsigned long long Vorbis::getSLength()
{
    return m_slength;
}

unsigned long long Vorbis::getSPosition()
{
    return m_sposition;
}

unsigned int Vorbis::getChannels()
{
    return m_channels;
}

unsigned int Vorbis::getRate()
{
    return m_rate;
}

unsigned int Vorbis::getEncoding()
{
    return 0;
}

unsigned int Vorbis::getBitrate()
{
    return m_bitrate;
}

bool Vorbis::eof()
{
    return m_eof;
}


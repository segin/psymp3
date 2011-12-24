/*
 * vorbis.cpp - Extends the Stream base class to decode Ogg Vorbis.
 * This file is part of PsyMP3.
 * Copyright © 2011 Kirn Gill <segin2005@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
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
    int ret = ov_fopen(name.toCString(true), (OggVorbis_File *) m_handle);
    switch (ret) {
    case OV_ENOTVORBIS:
        // throw WrongFormatException();
        throw;
        break;
    case OV_EREAD:
    case OV_EVERSION:
    case OV_EBADHEADER:
    case OV_EFAULT:
        //throw BadFormatException();
        throw;
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


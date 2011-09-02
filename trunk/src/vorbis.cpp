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
    open(name);
}

Vorbis::~Vorbis()
{
    ov_clear((OggVorbis_File *) m_handle);
    delete (OggVorbis_File *) m_handle;
}

void Vorbis::open(TagLib::String name)
{
    int ret = ov_fopen(name.toCString(true), (OggVorbis_File *) m_handle);
    switch (ret) {
    case OV_ENOTVORBIS:
        // throw WrongFormatException();
        break;
    case OV_EREAD:
    case OV_EVERSION:
    case OV_EBADHEADER:
    case OV_EFAULT:
        // throw BadFormatException();
        break;
    default: // returned 0 for success
        break;
    };
    vorbis_info *vi = ov_info((OggVorbis_File *) m_handle, -1);
    switch(vi->channels) {
    case 1:
    case 2:
        m_channels = vi->channels;
        m_bitrate = vi->bitrate_nominal;
        break;
    default:
        // throw
        break;
    };
}

unsigned int Vorbis::getLength()
{

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


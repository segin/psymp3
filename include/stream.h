/*
 * stream.h - Stream functionality base class header
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


#ifndef STREAM_H
#define STREAM_H

class Stream
{
    public:
        Stream();
        Stream(TagLib::String name);
        /** Default destructor */
        virtual ~Stream();
        virtual void open(TagLib::String name);
        TagLib::String getArtist();
        TagLib::String getTitle();
        TagLib::String getAlbum();
        virtual unsigned int getLength(); // in msec!
        virtual unsigned long long getSLength(); // in samples!
        virtual unsigned int getChannels();
        virtual unsigned int getRate();
        virtual unsigned int getEncoding(); // returns undefined
        virtual unsigned int getPosition(); // in msec!
        virtual unsigned int getSPosition(); // in samples!
        virtual unsigned int getBitrate(); // bitrate in bits per second!
        virtual size_t getData(size_t len, void *buf) = 0;
        virtual void seekTo(unsigned long pos) = 0;
        virtual bool eof() = 0;
    protected:
        void *          m_handle; // any handle type
        void *          m_buffer; // decoded audio buffer
        size_t          m_buflen; // buffer length
        TagLib::String  m_path;
        long            m_rate;
        int             m_bitrate;  // 0 if not applicable, average if vbr ?
        int             m_channels;
        int             m_length;    // in msec
        long long       m_slength;   // in samples; see getRate()
        int             m_position;  // in msec;
        long long       m_sposition; // in samples; needs to be at least 64bit.
        int             m_encoding;  // value ??? - for later use
        bool            m_eof;
    private:
        TagLib::FileRef *m_tags;
};

#endif // STREAM_H

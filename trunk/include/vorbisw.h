/*
 * vorbisw.h - Ogg Vorbis decoder class header
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

#ifndef VORBIS_H
#define VORBIS_H

class Vorbis : public Stream
{
    public:
        Vorbis(TagLib::String name);
        virtual ~Vorbis();
        void open(TagLib::String name);
        virtual unsigned int getLength();
        virtual unsigned long long getSLength();
        virtual unsigned long long getSPosition();
        virtual unsigned int getChannels();
        virtual unsigned int getRate();
        virtual unsigned int getBitrate();
        virtual unsigned int getEncoding(); // returns undefined
        virtual size_t getData(size_t len, void *buf);
        virtual void seekTo(unsigned long pos);
        virtual bool eof();
    protected:
    private:
        int m_session;
        vorbis_info *m_vi;
};

#endif // VORBIS_H

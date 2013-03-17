/*
 * flac.h - FLAC decoder class header
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef FLAC_H
#define FLAC_H

class Flac : public Stream
{
    public:
        /** Default constructor */
        Flac(TagLib::String name);
        /** Default destructor */
        virtual ~Flac();
        virtual void open(TagLib::String name);
        virtual unsigned int getChannels();
        virtual unsigned int getRate();
        virtual unsigned int getEncoding(); // returns undefined
        virtual unsigned int getPosition(); // in msec!
        virtual unsigned long long getSPosition(); // in samples!
        virtual unsigned int getLength(); // in msec!
        virtual unsigned long long getSLength(); // in samples!
        virtual size_t getData(size_t len, void *buf);
        virtual void seekTo(unsigned long pos);
        virtual bool eof();
        static void init();
        static void fini();
    protected:
    private:

};

#endif // LIBMPG123_H

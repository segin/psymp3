/*
 * libmpg123w.h - MP3 decoder class header
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

#ifndef LIBMPG123_H
#define LIBMPG123_H

class Libmpg123 : public Stream
{
    public:
        /** Default constructor */
        Libmpg123(TagLib::String name);
        /** Default destructor */
        virtual ~Libmpg123();
        virtual void open(TagLib::String name);
        virtual unsigned int getChannels();
        virtual unsigned int getRate();
        virtual unsigned int getEncoding(); // returns undefined
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

/*
 * libmpg123w.h - MP3 decoder class header
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

#ifndef LIBMPG123_H
#define LIBMPG123_H

// No direct includes - all includes should be in psymp3.h

#ifdef HAVE_MP3

class Libmpg123 : public Stream
{
    public:
        /** Default constructor */
        Libmpg123(TagLib::String name);
        /** Default destructor */
        virtual ~Libmpg123();
        virtual void open(TagLib::String name);
        virtual unsigned int getPosition(); // in msec!
        virtual unsigned long long getSPosition(); // in samples!
        virtual unsigned int getLength(); // in msec!
        virtual unsigned long long getSLength(); // in samples!
        virtual size_t getData(size_t len, void *buf);
        virtual void seekTo(unsigned long pos);
        virtual bool eof();
    protected:
    private:
        std::unique_ptr<IOHandler> m_handler;
        mpg123_handle* m_mpg_handle = nullptr;
};

#endif // HAVE_MP3

#endif // LIBMPG123_H

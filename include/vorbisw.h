/*
 * vorbisw.h - Ogg Vorbis decoder class header
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
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

/*
 * flac.h - FLAC decoder class header
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

#ifndef FLAC_H
#define FLAC_H

class FlacDecoder: public FLAC::Decoder::File
{
    public:
        FlacDecoder(TagLib::String path) : FLAC::Decoder::File(), m_path(path) { }
        TagLib::String m_path;
        long long      m_slength;
        long           m_rate;
        long           m_channels;
        long           m_bitdepth;
    protected:
        virtual ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]);
        virtual void metadata_callback(const ::FLAC__StreamMetadata *metadata);
        virtual void error_callback(::FLAC__StreamDecoderErrorStatus status);
    private:
    	FlacDecoder(const FlacDecoder&);
	    FlacDecoder &operator=(const FlacDecoder&);
};

class Flac : public Stream
{
    public:
        /** Default constructor */
        Flac(TagLib::String name);
        /** Default destructor */
        virtual ~Flac();
        virtual void open(TagLib::String name);
        virtual size_t getData(size_t len, void *buf);
        virtual void seekTo(unsigned long pos);
        virtual bool eof();
        static void init();
        static void fini();
    protected:
    private:
        FlacDecoder m_handle;
        char *sampbuf;
        FLAC__StreamMetadata_StreamInfo *m_info;
};

#endif // LIBMPG123_H

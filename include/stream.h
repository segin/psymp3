/*
 * stream.h - Stream functionality base class header
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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


#ifndef STREAM_H
#define STREAM_H

#include "lyrics.h"

class Stream
{
    public:
        Stream();
        Stream(TagLib::String name);
        /** Default destructor */
        virtual ~Stream();
        
        // Disable copy constructor and copy assignment operator
        Stream(const Stream&) = delete;
        Stream& operator=(const Stream&) = delete;
        
        // Enable move constructor and move assignment operator
        Stream(Stream&&) = default;
        Stream& operator=(Stream&&) = default;
        virtual void open(TagLib::String name);
        virtual TagLib::String getArtist();
        virtual TagLib::String getTitle();
        virtual TagLib::String getAlbum();
        TagLib::String getFilePath() const;
        
        // Lyrics support
        std::shared_ptr<LyricsFile> getLyrics() const;
        bool hasLyrics() const;
        
        virtual unsigned int getLength(); // in msec!
        virtual unsigned long long getSLength(); // in samples!
        virtual unsigned int getChannels();
        virtual unsigned int getRate();
        virtual unsigned int getEncoding(); // returns undefined
        virtual unsigned int getPosition(); // in msec!
        virtual unsigned long long getSPosition(); // in samples!
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
        
        // Lyrics support
        std::shared_ptr<LyricsFile> m_lyrics;
        
    private:
        std::unique_ptr<TagLib::FileRef> m_tags;
        std::unique_ptr<TagLibIOHandlerAdapter> m_taglib_stream;
        
        // Helper method to load lyrics for this stream
        void loadLyrics();
};

#endif // STREAM_H

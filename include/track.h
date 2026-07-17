/*
 * track.h - class header for track class
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

#ifndef TRACK_H
#define TRACK_H

class track
{
    public:
        // Disable copy constructor and copy assignment due to unique_ptr member
        track(const track&) = delete;
        track& operator=(const track&) = delete;
        // Enable default move constructor and move assignment
        track(track&&) = default;
        track& operator=(track&&) = default;
        TagLib::String GetArtist() const { return m_Artist; }
        TagLib::String GetTitle() const { return m_Title; }
        TagLib::String GetAlbum() const { return m_Album; }
        TagLib::String GetFilePath() const { return m_FilePath; }
        static bool isKnownRawAudioExtension(const TagLib::String& path);
        static bool shouldCreateTagLibRefForPath(const TagLib::String& path);
        explicit track(const TagLib::String& a_FilePath, const TagLib::String& extinf_artist = "", const TagLib::String& extinf_title = "", long extinf_duration = 0);
        void SetFilePath(TagLib::String val) { m_FilePath = val; }
        void setArtist(TagLib::String val) { m_Artist = val; }
        void setTitle(TagLib::String val) { m_Title = val; }
        void setAlbum(TagLib::String val) { m_Album = val; }
        unsigned int GetLen() const { return m_Len; }
        void SetLen(unsigned int val) { m_Len = val; }
        void loadTags();
        static TagLib::String nullstr;

        // Hook used by loadTags() to recover metadata from the actual decoder
        // when TagLib cannot parse a file (e.g. an MP3 without proper ID3 tags).
        // It fills artist/title/album/length_seconds and returns true on success.
        // Registered by mediafile.cpp so that binaries which do not link the
        // demuxer stack (small tools/tests) don't pull MediaFile into track.o;
        // when unset, the decoder fallback is simply skipped.
        using DecoderMetadataResolver = bool (*)(const TagLib::String& path,
            TagLib::String& artist, TagLib::String& title,
            TagLib::String& album, unsigned int& length_seconds);
        static void setDecoderMetadataResolver(DecoderMetadataResolver resolver)
            { s_decoder_resolver = resolver; }
    protected:
        TagLib::String m_Artist;
        TagLib::String m_Title;
        TagLib::String m_Album;
        TagLib::String m_FilePath;
        std::unique_ptr<TagLib::FileRef> m_FileRef;
        std::unique_ptr<TagLibIOHandlerAdapter> m_TagLibStream; // Keeps stream alive for FileRef
        unsigned int m_Len;
    private:
        // Fallback metadata loader used when TagLib cannot parse the file
        // (e.g. an MP3 without proper ID3 tags). Delegates to the registered
        // DecoderMetadataResolver, if any.
        void loadTagsFromDecoder();
        static DecoderMetadataResolver s_decoder_resolver;
};

#endif // TRACK_H

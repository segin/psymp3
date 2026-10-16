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

// No direct includes - all includes should be in psymp3.h

// Forward declarations for Tag framework
namespace PsyMP3 {
namespace Tag {
    class Tag;
    struct Picture;
}
}

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
        // First credited artist when the artist tag is multi-valued (several
        // Vorbis ARTIST fields, ID3v2.4 multi-value TPE1); equals getArtist()
        // otherwise. Used for Last.fm submission, which has no multi-artist
        // model — the full joined credit stays in getArtist() for display.
        virtual TagLib::String getPrimaryArtist();
        // MusicBrainz recording ID from the file's tags; empty when untagged.
        virtual TagLib::String getMusicBrainzID();
        // MusicBrainz release (album) ID; keys Cover Art Archive artwork.
        virtual TagLib::String getMusicBrainzReleaseID();
        TagLib::String getFilePath() const;
        
        // Tag framework integration
        /**
         * @brief Get metadata tag for this stream
         * 
         * Returns a reference to the Tag object containing metadata for this stream.
         * If no metadata is available, returns a NullTag instance.
         * 
         * @return Reference to Tag object (NullTag if no metadata)
         * 
         * @thread_safety Safe to call concurrently after stream is opened
         */
        virtual const PsyMP3::Tag::Tag& getTag() const;

        /**
         * @brief The picture that best stands for this track
         *
         * The embedded front cover if there is one, otherwise the first
         * embedded picture with any data; nullopt without either. PsyMP3's
         * own tag reader is asked first -- it carries ID3v2, FLAC and Ogg
         * pictures and works where TagLib cannot open the source -- and
         * TagLib's PICTURE property after it, which also reaches MP4 cover
         * art.
         */
        virtual std::optional<PsyMP3::Tag::Picture> getCoverArt() const;
        
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
        // Source bits per sample (16/24/32...); 0 = unknown/not applicable.
        virtual int getBitsPerSample();
        // Short codec name ("flac", "vorbis", ...). The base implementation
        // has no codec, so it reports the object's DYNAMIC class name via
        // RTTI (C++ has no reflection until C++26, but typeid on a
        // polymorphic object names the derived type) - every Stream subclass
        // therefore self-describes even without an override.
        virtual TagLib::String getCodecName();
        /// Decoder-reported profile ("AAC-LC", "HE-AACv2", ...); empty if none.
        virtual TagLib::String getCodecProfile() { return TagLib::String(); }
        virtual size_t getData(size_t len, void *buf) = 0;
        virtual void seekTo(unsigned long pos) = 0;
        virtual bool canSeek() const;
        virtual bool eof() = 0;

        /**
         * @brief Whether this is a live stream (internet radio), whose
         *        artist and title change while it plays.
         * @return false for files.
         */
        virtual bool isLive() const { return false; }

        /**
         * @brief A count that goes up each time the stream announces a new
         *        title, so the player can notice without comparing strings.
         *        Safe to call from the GUI thread every frame.
         * @return 0 for files, whose titles do not change.
         */
        virtual uint64_t getMetadataGeneration() const { return 0; }

        /**
         * @brief A count that goes up each time a live stream's picture
         *        changes (getCoverArt() then says something new) -- apart
         *        from titles, as a picture fetched late is no new song.
         * @return 0 for files.
         */
        virtual uint64_t getArtworkGeneration() const { return 0; }

        /**
         * @brief The live stream's own name (a radio station's icy-name).
         * @return The name, or empty.
         */
        virtual TagLib::String getStationName() const { return TagLib::String(); }

        /**
         * @brief Whether what plays now is music, as opposed to a station's
         *        own announcement or advert, which is not scrobbled.
         * @return true unless the stream says otherwise.
         */
        virtual bool isCurrentTitleMusic() const { return true; }

        /**
         * @brief How long the song a live stream is playing lasts, when the
         *        station says (iHeart does), for Last.fm's half-the-song rule.
         * @return Milliseconds, or 0 when unknown.
         */
        virtual uint64_t getCurrentTitleLengthMs() const { return 0; }

        /**
         * @brief Make a read waiting on the stream's source give up now, so
         *        whoever joins the decoder (stopping, switching tracks) is
         *        not held up by a stalled station. The stream ends; it is
         *        about to be dropped. Safe from any thread, and takes no lock
         *        the decoder can hold.
         */
        virtual void abortIO() {}

        /**
         * @brief Whether a live stream changed its sample rate or channels
         *        (a chained link, an advert at another rate) and waits, read
         *        as ended, for an audio device at its new format. The player
         *        takes it back, opens one, and calls acknowledgeFormatChange().
         * @return false unless such a change is waiting.
         */
        virtual bool hasPendingFormatChange() const { return false; }

        /** @brief The new format's device is open: carry on reading. */
        virtual void acknowledgeFormatChange() {}
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
        void loadLyrics();
        
        // Tag framework integration - stores metadata tag for this stream
        std::unique_ptr<PsyMP3::Tag::Tag> m_tag;
        
    private:
        std::unique_ptr<TagLib::FileRef> m_tags;
        std::unique_ptr<TagLibIOHandlerAdapter> m_taglib_stream;
};

#endif // STREAM_H

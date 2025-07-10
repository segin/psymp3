/*
 * stream.cpp - Stream functionality base class
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

#include "psymp3.h"

/**
 * @brief Default constructor for the Stream base class.
 */
Stream::Stream()
    : m_handle(nullptr),
      m_buffer(nullptr),
      m_buflen(0),
      m_rate(0),
      m_bitrate(0),
      m_channels(0),
      m_length(0),
      m_slength(0),
      m_position(0),
      m_sposition(0),
      m_encoding(0),
      m_eof(false)
{
    /* Initialize all member variables to prevent garbage values */
}

/**
 * @brief Constructs a Stream object associated with a file path.
 * 
 * This constructor initializes the stream with a file path and creates a
 * TagLib::FileRef to begin reading metadata.
 * @param name The file path of the media stream.
 */
Stream::Stream(TagLib::String name) 
    : m_handle(nullptr),
      m_buffer(nullptr),
      m_buflen(0),
      m_path(name),
      m_rate(0),
      m_bitrate(0),
      m_channels(0),
      m_length(0),
      m_slength(0),
      m_position(0),
      m_sposition(0),
      m_encoding(0),
      m_eof(false)
{
    try {
        // Create IOHandler-based stream for TagLib to ensure consistent file access
        auto io_handler = std::make_unique<FileIOHandler>(name);
        m_taglib_stream = std::make_unique<TagLibIOHandlerAdapter>(
            std::move(io_handler), name, true);
        
        // Use custom stream with TagLib
        m_tags = std::make_unique<TagLib::FileRef>(m_taglib_stream.get());
        
    } catch (std::exception& e) {
        Debug::runtime("Stream constructor: Failed to create TagLib stream: ", e.what());
        m_tags = nullptr;
        m_taglib_stream = nullptr;
    }
    
    loadLyrics(); // Load lyrics file if available
}

/**
 * @brief Destroys the Stream object.
 * 
 * The unique_ptr for tags is automatically handled.
 */
Stream::~Stream()
{
    // No longer need to delete m_tags, std::unique_ptr handles it.
}

/**
 * @brief Opens a media stream.
 * 
 * This is a virtual method intended to be overridden by derived classes.
 * The base class implementation does nothing.
 * @param name The file path of the media stream to open.
 */
void Stream::open(TagLib::String name)
{
    // Base class - do nothing.
    return;
}

/**
 * @brief Gets the artist metadata for the stream.
 * @return A TagLib::String containing the artist name, or an empty string if not available.
 */
TagLib::String Stream::getArtist()
{
    if(!m_tags) {
        Debug::runtime("Stream::getArtist: m_tags is null for file: ", getFilePath());
        return track::nullstr;
    }
    if(m_tags->isNull()) {
        Debug::runtime("Stream::getArtist: TagLib::FileRef is null (couldn't open file): ", getFilePath());
        return track::nullstr;
    }
    if(!m_tags->tag()) {
        Debug::runtime("Stream::getArtist: No tag data found in file: ", getFilePath());
        return track::nullstr;
    }
    TagLib::String artist = m_tags->tag()->artist();
    if(artist.isEmpty()) {
        Debug::runtime("Stream::getArtist: Artist tag is empty for file: ", getFilePath());
    }
    return artist;
}

/**
 * @brief Gets the title metadata for the stream.
 * @return A TagLib::String containing the track title, or an empty string if not available.
 */
TagLib::String Stream::getTitle()
{
    if(!m_tags) {
        Debug::runtime("Stream::getTitle: m_tags is null for file: ", getFilePath());
        return track::nullstr;
    }
    if(m_tags->isNull()) {
        Debug::runtime("Stream::getTitle: TagLib::FileRef is null (couldn't open file): ", getFilePath());
        return track::nullstr;
    }
    if(!m_tags->tag()) {
        Debug::runtime("Stream::getTitle: No tag data found in file: ", getFilePath());
        return track::nullstr;
    }
    TagLib::String title = m_tags->tag()->title();
    if(title.isEmpty()) {
        Debug::runtime("Stream::getTitle: Title tag is empty for file: ", getFilePath());
    }
    return title;
}

/**
 * @brief Gets the album metadata for the stream.
 * @return A TagLib::String containing the album name, or an empty string if not available.
 */
TagLib::String Stream::getAlbum()
{
    if(!m_tags) {
        Debug::runtime("Stream::getAlbum: m_tags is null for file: ", getFilePath());
        return track::nullstr;
    }
    if(m_tags->isNull()) {
        Debug::runtime("Stream::getAlbum: TagLib::FileRef is null (couldn't open file): ", getFilePath());
        return track::nullstr;
    }
    if(!m_tags->tag()) {
        Debug::runtime("Stream::getAlbum: No tag data found in file: ", getFilePath());
        return track::nullstr;
    }
    TagLib::String album = m_tags->tag()->album();
    if(album.isEmpty()) {
        Debug::runtime("Stream::getAlbum: Album tag is empty for file: ", getFilePath());
    }
    return album;
}

/**
 * @brief Gets the file path of the stream.
 * @return A TagLib::String containing the full file path.
 */
TagLib::String Stream::getFilePath() const
{
    return m_path;
}

/* Note that the base class version falls back to TagLib, which is inaccurate.
 * Having all this generic functionality in TagLib as well as the children codec
 * classes is to make writing child codecs easier by providing generic, working
 * functionality until the codec version is fully implemented.
 */
/**
 * @brief Gets the total length of the stream in milliseconds.
 * 
 * This base implementation falls back to using TagLib for the length, which may be
 * inaccurate. Derived classes should override this to provide a precise length
 * from the decoder library.
 * @return The length of the stream in milliseconds.
 */
unsigned int Stream::getLength()
{
    if(m_length) return m_length;
    if(!m_tags) return 0;
    if(m_tags->isNull()) {
        Debug::runtime("Stream::getLength: TagLib::FileRef is null (couldn't open file): ", getFilePath());
        return 0;
    }
    if(!m_tags->audioProperties()) {
        Debug::runtime("Stream::getLength: No audio properties found in file: ", getFilePath());
        return 0;
    }
    return m_tags->audioProperties()->lengthInMilliseconds(); // * 1000 to make msec
}

/* As the data we're getting from TagLib is inaccurate but percise, we can
 * simply multiply it by the sample rate to get the approximate length in
 * samples.
 */
/**
 * @brief Gets the total length of the stream in PCM samples.
 * 
 * This base implementation falls back to an approximation based on the TagLib
 * duration and sample rate. Derived classes should override this to provide a
 * precise sample count from the decoder library.
 * @return The total number of samples in the stream.
 */
unsigned long long Stream::getSLength()
{
    if(m_slength) return m_slength;
    if(!m_tags) return 0;
    if(m_tags->isNull()) {
        Debug::runtime("Stream::getSLength: TagLib::FileRef is null (couldn't open file): ", getFilePath());
        return 0;
    }
    if(!m_tags->audioProperties()) {
        Debug::runtime("Stream::getSLength: No audio properties found in file: ", getFilePath());
        return 0;
    }
    return static_cast<unsigned long long>(m_tags->audioProperties()->lengthInSeconds()) * m_tags->audioProperties()->sampleRate();
}

/* TagLib provides this information in a generic manner for a mulitude of
 * different formats. This will aid greatly in implementing child codec
 * classes by providing an already-working mechanism for grabbing the data.
 * However, eventually, the codec class must override these classes if
 * possible. Best to get the data from the codec and decoder libraries.
 */
/**
 * @brief Gets the number of audio channels in the stream.
 * 
 * This base implementation falls back to using TagLib. Derived classes should override this.
 * @return The number of channels (e.g., 1 for mono, 2 for stereo).
 */
unsigned int Stream::getChannels()
{
    if(m_channels) return m_channels;
    if(!m_tags) return 0;
    if(m_tags->isNull()) {
        Debug::runtime("Stream::getChannels: TagLib::FileRef is null (couldn't open file): ", getFilePath());
        return 0;
    }
    if(!m_tags->audioProperties()) {
        Debug::runtime("Stream::getChannels: No audio properties found in file: ", getFilePath());
        return 0;
    }
    return m_tags->audioProperties()->channels();
}

/**
 * @brief Gets the sample rate of the stream in Hz.
 * 
 * This base implementation falls back to using TagLib. Derived classes should override this.
 * @return The sample rate (e.g., 44100, 48000).
 */
unsigned int Stream::getRate()
{
    if(m_rate) return m_rate;
    if(!m_tags) return 0;
    if(m_tags->isNull()) {
        Debug::runtime("Stream::getRate: TagLib::FileRef is null (couldn't open file): ", getFilePath());
        return 0;
    }
    if(!m_tags->audioProperties()) {
        Debug::runtime("Stream::getRate: No audio properties found in file: ", getFilePath());
        return 0;
    }
    return m_tags->audioProperties()->sampleRate();
}

/**
 * @brief Gets the bitrate of the stream in kbps.
 * 
 * This base implementation falls back to using TagLib. Derived classes should override this.
 * @return The bitrate of the stream.
 */
unsigned int Stream::getBitrate()
{
    if(m_bitrate) return m_bitrate;
    if(!m_tags) return 0;
    if(m_tags->isNull()) {
        Debug::runtime("Stream::getBitrate: TagLib::FileRef is null (couldn't open file): ", getFilePath());
        return 0;
    }
    if(!m_tags->audioProperties()) {
        Debug::runtime("Stream::getBitrate: No audio properties found in file: ", getFilePath());
        return 0;
    }
    return m_tags->audioProperties()->bitrate();
}

/* This is the sample encoding. So far, we can force all output to be
 * signed 16-bit little endian. As such, this is, so far, unneeded and
 * implemented solely as a stub. However, it may be needed in the future,
 * so for now, I leave it in place.
 */
/**
 * @brief Gets the sample encoding format.
 * @note This is a stub for future use. Currently, all output is normalized to 16-bit signed integer.
 * @return An integer representing the encoding format (currently always 0).
 */
unsigned int Stream::getEncoding()
{
    return 0;
}

/**
 * @brief Gets the current playback position in milliseconds.
 * 
 * This value is updated by the derived class's `getData` method.
 * @return The current position in milliseconds.
 */
unsigned int Stream::getPosition()
{
    return m_position;
}

/**
 * @brief Gets the current playback position in PCM samples.
 * 
 * This value is updated by the derived class's `getData` method.
 * @return The current position in samples.
 */
unsigned long long Stream::getSPosition()
{
    return m_sposition;
}

/**
 * @brief Gets the lyrics file associated with this stream.
 * @return Shared pointer to the lyrics file, or nullptr if no lyrics available.
 */
std::shared_ptr<LyricsFile> Stream::getLyrics() const
{
    return m_lyrics;
}

/**
 * @brief Checks if this stream has lyrics available.
 * @return true if lyrics are loaded and available.
 */
bool Stream::hasLyrics() const
{
    return m_lyrics && m_lyrics->hasLyrics();
}

/**
 * @brief Loads lyrics for this stream from a file with the same base name.
 * This method is called automatically when the stream is opened.
 */
void Stream::loadLyrics()
{
    if (m_path.isEmpty()) {
        std::cout << "Stream::loadLyrics: No path set, skipping lyrics loading" << std::endl;
        return;
    }
    
    // Convert TagLib::String to std::string for lyrics utilities
    std::string file_path = m_path.to8Bit(true);
    std::cout << "Stream::loadLyrics: Looking for lyrics for: " << file_path << std::endl;
    
    std::string lyrics_path = LyricsUtils::findLyricsFile(file_path);
    
    if (!lyrics_path.empty()) {
        std::cout << "Stream::loadLyrics: Found lyrics file: " << lyrics_path << std::endl;
        m_lyrics = std::make_shared<LyricsFile>();
        if (m_lyrics->loadFromFile(lyrics_path)) {
            std::cout << "Stream::loadLyrics: Successfully loaded " << m_lyrics->getLines().size() << " lyric lines" << std::endl;
        } else {
            std::cout << "Stream::loadLyrics: Failed to parse lyrics file" << std::endl;
            // Failed to load lyrics, clear the pointer
            m_lyrics.reset();
        }
    } else {
        std::cout << "Stream::loadLyrics: No lyrics file found" << std::endl;
    }
}

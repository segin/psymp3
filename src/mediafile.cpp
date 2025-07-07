/*
 * mediafile.cpp - format abstraction :)
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

#include "psymp3.h"

bool MediaFile::exists(const TagLib::String& file) {
    auto filestring = 
#ifdef _WIN32
        file.toWString();
#else
        file.to8Bit(true);
#endif
    /* std::ifstream filestream;
    filestream.open(filestring);
    return filestream.good(); */
    return true;  
}

std::vector<std::string> &MediaFile::split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> MediaFile::split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

// Define a type for our creation functions for clarity
using StreamCreator = std::function<Stream*(const TagLib::String&)>;

// Format definition with multiple extensions
struct FormatInfo {
    std::vector<TagLib::String> extensions;
    StreamCreator creator;
};

// Create format registry with multiple extensions per format
static const std::vector<FormatInfo> format_registry = {
    // Playlists
    {{"M3U", "M3U8"}, [](const TagLib::String& name) { return new NullStream(name); }},
    
    // MPEG Audio (Layer-II and Layer-III use same decoder)
    {{"MP3", "MP2", "MPA"}, [](const TagLib::String& name) { return new Libmpg123(name); }},
    
    // Ogg containers (Vorbis primary, but could contain FLAC/Opus)
    {{"OGG", "OGA"}, [](const TagLib::String& name) { return new Vorbis(name); }},
    
    // Opus (usually in Ogg, but can be standalone)
    {{"OPUS"}, [](const TagLib::String& name) { return new OpusFile(name); }},
    
    // FLAC
    {{"FLAC", "FLA"}, [](const TagLib::String& name) { return new Flac(name); }},
    
    // RIFF/WAVE and variants
    {{"WAV", "WAVE", "BWF"}, [](const TagLib::String& name) { return new WaveStream(name); }},
    
    // A-law variants
    {{"AL", "ALAW"}, [](const TagLib::String& name) { return new WaveStream(name); }},
    
    // μ-law variants  
    {{"UL", "ULAW", "MULAW", "AU", "SND"}, [](const TagLib::String& name) { return new WaveStream(name); }},
    
    // Apple AIFF variants
    {{"AIF", "AIFF", "AIFC"}, [](const TagLib::String& name) { return new WaveStream(name); }},
    
    // Raw PCM variants
    {{"PCM", "RAW"}, [](const TagLib::String& name) { return new WaveStream(name); }}
};

// Build reverse lookup map for fast extension->creator resolution
static std::map<TagLib::String, StreamCreator> buildExtensionMap() {
    std::map<TagLib::String, StreamCreator> ext_map;
    for (const auto& format : format_registry) {
        for (const auto& ext : format.extensions) {
            ext_map[ext] = format.creator;
        }
    }
    return ext_map;
}

static const std::map<TagLib::String, StreamCreator> stream_factory = buildExtensionMap();

Stream *MediaFile::open(TagLib::String name)
{
    /*
    // For future expansion: Magic-based detection is more robust than extensions.
    // This block shows how it could be implemented.
    try {
        // The WaveStream constructor checks the file header.
        return new WaveStream(name);
    } catch (const WrongFormatException &e) {
        // It's not a WAVE file, so fall through to the extension-based factory.
    }
    */

#ifdef _RISCOS
    std::vector<std::string> tokens = split(name.to8Bit(true), '/');
#else
    std::vector<std::string> tokens = split(name.to8Bit(true), '.');
#endif
    TagLib::String ext(TagLib::String(tokens[tokens.size() - 1]).upper());
#ifdef DEBUG
    std::cout << "MediaFile::open(): " << ext << std::endl;
#endif
    
    auto it = stream_factory.find(ext);
    if (it != stream_factory.end()) {
        return it->second(name); // Call the creation function
    }

    throw InvalidMediaException("Unsupported format: " + ext);
}

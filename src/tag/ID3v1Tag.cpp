/*
 * ID3v1Tag.cpp - ID3v1/ID3v1.1 tag implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Tag {

// ============================================================================
// ID3v1 Genre List (Standard 80 + Winamp Extensions up to 191)
// ============================================================================

static const std::array<std::string, ID3v1Tag::GENRE_COUNT> s_genre_list = {{
    // Standard ID3v1 genres (0-79)
    "Blues",                  // 0
    "Classic Rock",           // 1
    "Country",                // 2
    "Dance",                  // 3
    "Disco",                  // 4
    "Funk",                   // 5
    "Grunge",                 // 6
    "Hip-Hop",                // 7
    "Jazz",                   // 8
    "Metal",                  // 9
    "New Age",                // 10
    "Oldies",                 // 11
    "Other",                  // 12
    "Pop",                    // 13
    "R&B",                    // 14
    "Rap",                    // 15
    "Reggae",                 // 16
    "Rock",                   // 17
    "Techno",                 // 18
    "Industrial",             // 19
    "Alternative",            // 20
    "Ska",                    // 21
    "Death Metal",            // 22
    "Pranks",                 // 23
    "Soundtrack",             // 24
    "Euro-Techno",            // 25
    "Ambient",                // 26
    "Trip-Hop",               // 27
    "Vocal",                  // 28
    "Jazz+Funk",              // 29
    "Fusion",                 // 30
    "Trance",                 // 31
    "Classical",              // 32
    "Instrumental",           // 33
    "Acid",                   // 34
    "House",                  // 35
    "Game",                   // 36
    "Sound Clip",             // 37
    "Gospel",                 // 38
    "Noise",                  // 39
    "AlternRock",             // 40
    "Bass",                   // 41
    "Soul",                   // 42
    "Punk",                   // 43
    "Space",                  // 44
    "Meditative",             // 45
    "Instrumental Pop",       // 46
    "Instrumental Rock",      // 47
    "Ethnic",                 // 48
    "Gothic",                 // 49
    "Darkwave",               // 50
    "Techno-Industrial",      // 51
    "Electronic",             // 52
    "Pop-Folk",               // 53
    "Eurodance",              // 54
    "Dream",                  // 55
    "Southern Rock",          // 56
    "Comedy",                 // 57
    "Cult",                   // 58
    "Gangsta",                // 59
    "Top 40",                 // 60
    "Christian Rap",          // 61
    "Pop/Funk",               // 62
    "Jungle",                 // 63
    "Native American",        // 64
    "Cabaret",                // 65
    "New Wave",               // 66
    "Psychedelic",            // 67
    "Rave",                   // 68
    "Showtunes",              // 69
    "Trailer",                // 70
    "Lo-Fi",                  // 71
    "Tribal",                 // 72
    "Acid Punk",              // 73
    "Acid Jazz",              // 74
    "Polka",                  // 75
    "Retro",                  // 76
    "Musical",                // 77
    "Rock & Roll",            // 78
    "Hard Rock",              // 79
    
    // Winamp extensions (80-191)
    "Folk",                   // 80
    "Folk-Rock",              // 81
    "National Folk",          // 82
    "Swing",                  // 83
    "Fast Fusion",            // 84
    "Bebop",                  // 85
    "Latin",                  // 86
    "Revival",                // 87
    "Celtic",                 // 88
    "Bluegrass",              // 89
    "Avantgarde",             // 90
    "Gothic Rock",            // 91
    "Progressive Rock",       // 92
    "Psychedelic Rock",       // 93
    "Symphonic Rock",         // 94
    "Slow Rock",              // 95
    "Big Band",               // 96
    "Chorus",                 // 97
    "Easy Listening",         // 98
    "Acoustic",               // 99
    "Humour",                 // 100
    "Speech",                 // 101
    "Chanson",                // 102
    "Opera",                  // 103
    "Chamber Music",          // 104
    "Sonata",                 // 105
    "Symphony",               // 106
    "Booty Bass",             // 107
    "Primus",                 // 108
    "Porn Groove",            // 109
    "Satire",                 // 110
    "Slow Jam",               // 111
    "Club",                   // 112
    "Tango",                  // 113
    "Samba",                  // 114
    "Folklore",               // 115
    "Ballad",                 // 116
    "Power Ballad",           // 117
    "Rhythmic Soul",          // 118
    "Freestyle",              // 119
    "Duet",                   // 120
    "Punk Rock",              // 121
    "Drum Solo",              // 122
    "A Cappella",             // 123
    "Euro-House",             // 124
    "Dance Hall",             // 125
    "Goa",                    // 126
    "Drum & Bass",            // 127
    "Club-House",             // 128
    "Hardcore Techno",        // 129
    "Terror",                 // 130
    "Indie",                  // 131
    "BritPop",                // 132
    "Negerpunk",              // 133
    "Polsk Punk",             // 134
    "Beat",                   // 135
    "Christian Gangsta Rap",  // 136
    "Heavy Metal",            // 137
    "Black Metal",            // 138
    "Crossover",              // 139
    "Contemporary Christian", // 140
    "Christian Rock",         // 141
    "Merengue",               // 142
    "Salsa",                  // 143
    "Thrash Metal",           // 144
    "Anime",                  // 145
    "JPop",                   // 146
    "Synthpop",               // 147
    "Abstract",               // 148
    "Art Rock",               // 149
    "Baroque",                // 150
    "Bhangra",                // 151
    "Big Beat",               // 152
    "Breakbeat",              // 153
    "Chillout",               // 154
    "Downtempo",              // 155
    "Dub",                    // 156
    "EBM",                    // 157
    "Eclectic",               // 158
    "Electro",                // 159
    "Electroclash",           // 160
    "Emo",                    // 161
    "Experimental",           // 162
    "Garage",                 // 163
    "Global",                 // 164
    "IDM",                    // 165
    "Illbient",               // 166
    "Industro-Goth",          // 167
    "Jam Band",               // 168
    "Krautrock",              // 169
    "Leftfield",              // 170
    "Lounge",                 // 171
    "Math Rock",              // 172
    "New Romantic",           // 173
    "Nu-Breakz",              // 174
    "Post-Punk",              // 175
    "Post-Rock",              // 176
    "Psytrance",              // 177
    "Shoegaze",               // 178
    "Space Rock",             // 179
    "Trop Rock",              // 180
    "World Music",            // 181
    "Neoclassical",           // 182
    "Audiobook",              // 183
    "Audio Theatre",          // 184
    "Neue Deutsche Welle",    // 185
    "Podcast",                // 186
    "Indie Rock",             // 187
    "G-Funk",                 // 188
    "Dubstep",                // 189
    "Garage Rock",            // 190
    "Psybient"                // 191
}};

// ============================================================================
// Static Methods
// ============================================================================

const std::array<std::string, ID3v1Tag::GENRE_COUNT>& ID3v1Tag::genreList() {
    return s_genre_list;
}

std::string ID3v1Tag::genreFromIndex(uint8_t index) {
    if (index < GENRE_COUNT) {
        return s_genre_list[index];
    }
    return ""; // Unknown genre (255 or out of range)
}

bool ID3v1Tag::isValid(const uint8_t* data) {
    if (!data) {
        return false;
    }
    // Check for "TAG" magic bytes
    return data[0] == 'T' && data[1] == 'A' && data[2] == 'G';
}

std::string ID3v1Tag::trimString(const char* data, size_t max_len) {
    if (!data || max_len == 0) {
        return "";
    }
    
    // Find actual string length (stop at null or max_len)
    size_t len = 0;
    while (len < max_len && data[len] != '\0') {
        len++;
    }
    
    // Trim trailing spaces
    while (len > 0 && (data[len - 1] == ' ' || data[len - 1] == '\0')) {
        len--;
    }
    
    return std::string(data, len);
}

std::string ID3v1Tag::normalizeKey(const std::string& key) {
    std::string result = key;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::unique_ptr<ID3v1Tag> ID3v1Tag::parse(const uint8_t* data) {
    if (!data || !isValid(data)) {
        Debug::log("tag", "ID3v1Tag::parse: Invalid data or missing TAG header");
        return nullptr;
    }
    
    auto tag = std::make_unique<ID3v1Tag>();
    
    // Parse fixed fields
    // Offset 0-2: "TAG" (already validated)
    // Offset 3-32: Title (30 bytes)
    tag->m_title = trimString(reinterpret_cast<const char*>(data + 3), 30);
    
    // Offset 33-62: Artist (30 bytes)
    tag->m_artist = trimString(reinterpret_cast<const char*>(data + 33), 30);
    
    // Offset 63-92: Album (30 bytes)
    tag->m_album = trimString(reinterpret_cast<const char*>(data + 63), 30);
    
    // Offset 93-96: Year (4 bytes, ASCII)
    std::string year_str = trimString(reinterpret_cast<const char*>(data + 93), 4);
    if (!year_str.empty()) {
        try {
            tag->m_year = static_cast<uint32_t>(std::stoul(year_str));
        } catch (...) {
            tag->m_year = 0;
        }
    }
    
    // Offset 97-126: Comment (30 bytes)
    // Check for ID3v1.1: byte 125 (offset 125) is 0x00 and byte 126 (offset 126) is non-zero
    if (data[125] == 0x00 && data[126] != 0x00) {
        // ID3v1.1 format: comment is 28 bytes, track number in byte 126
        tag->m_is_v1_1 = true;
        tag->m_comment = trimString(reinterpret_cast<const char*>(data + 97), 28);
        tag->m_track = data[126];
        Debug::log("tag", "ID3v1Tag::parse: Detected ID3v1.1 format, track=", tag->m_track);
    } else {
        // ID3v1.0 format: full 30-byte comment
        tag->m_is_v1_1 = false;
        tag->m_comment = trimString(reinterpret_cast<const char*>(data + 97), 30);
        tag->m_track = 0;
    }
    
    // Offset 127: Genre index
    tag->m_genre_index = data[127];
    
    Debug::log("tag", "ID3v1Tag::parse: Parsed tag - title='", tag->m_title,
               "', artist='", tag->m_artist, "', album='", tag->m_album,
               "', year=", tag->m_year, ", genre=", static_cast<int>(tag->m_genre_index));
    
    return tag;
}

// ============================================================================
// Tag Interface Implementation
// ============================================================================

std::string ID3v1Tag::genre() const {
    return genreFromIndex(m_genre_index);
}

std::string ID3v1Tag::getTag(const std::string& key) const {
    std::string normalized = normalizeKey(key);
    
    if (normalized == "TITLE" || normalized == "TIT2") {
        return m_title;
    } else if (normalized == "ARTIST" || normalized == "TPE1") {
        return m_artist;
    } else if (normalized == "ALBUM" || normalized == "TALB") {
        return m_album;
    } else if (normalized == "YEAR" || normalized == "DATE" || normalized == "TYER" || normalized == "TDRC") {
        return m_year > 0 ? std::to_string(m_year) : "";
    } else if (normalized == "COMMENT" || normalized == "COMM") {
        return m_comment;
    } else if (normalized == "GENRE" || normalized == "TCON") {
        return genre();
    } else if (normalized == "TRACK" || normalized == "TRACKNUMBER" || normalized == "TRCK") {
        return m_track > 0 ? std::to_string(m_track) : "";
    }
    
    return "";
}

std::vector<std::string> ID3v1Tag::getTagValues(const std::string& key) const {
    std::string value = getTag(key);
    if (value.empty()) {
        return {};
    }
    return {value};
}

std::map<std::string, std::string> ID3v1Tag::getAllTags() const {
    std::map<std::string, std::string> tags;
    
    if (!m_title.empty()) {
        tags["TITLE"] = m_title;
    }
    if (!m_artist.empty()) {
        tags["ARTIST"] = m_artist;
    }
    if (!m_album.empty()) {
        tags["ALBUM"] = m_album;
    }
    if (m_year > 0) {
        tags["YEAR"] = std::to_string(m_year);
    }
    if (!m_comment.empty()) {
        tags["COMMENT"] = m_comment;
    }
    if (m_genre_index < GENRE_COUNT) {
        tags["GENRE"] = genre();
    }
    if (m_track > 0) {
        tags["TRACK"] = std::to_string(m_track);
    }
    
    return tags;
}

bool ID3v1Tag::hasTag(const std::string& key) const {
    return !getTag(key).empty();
}

bool ID3v1Tag::isEmpty() const {
    return m_title.empty() && m_artist.empty() && m_album.empty() &&
           m_year == 0 && m_comment.empty() && m_track == 0 &&
           m_genre_index >= GENRE_COUNT;
}

std::string ID3v1Tag::formatName() const {
    return m_is_v1_1 ? "ID3v1.1" : "ID3v1";
}

} // namespace Tag
} // namespace PsyMP3

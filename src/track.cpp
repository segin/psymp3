/*
 * track.cpp - class implementation for track class
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
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

TagLib::String track::nullstr;

namespace {

std::optional<unsigned int> getSyntheticRawLengthSeconds(const TagLib::String& path)
{
    std::string utf8_path = path.to8Bit(true);
    std::string::size_type dot = utf8_path.find_last_of('.');
    if (dot == std::string::npos) {
        return std::nullopt;
    }

    std::string extension = utf8_path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    struct SyntheticRawConfig {
        uint32_t sample_rate;
        uint16_t bytes_per_frame;
        uint16_t decoded_samples_per_frame;
    };

    static const std::unordered_map<std::string, SyntheticRawConfig> kSyntheticRawConfigs = {
        {".ulaw", {8000, 1, 1}},
        {".ul",   {8000, 1, 1}},
        {".mulaw",{8000, 1, 1}},
        {".alaw", {8000, 1, 1}},
        {".al",   {8000, 1, 1}},
        {".g722", {16000, 1, 2}},
        {".722",  {16000, 1, 2}},
        {".pcm",  {8000, 2, 1}},
        {".s8",   {8000, 1, 1}},
        {".u8",   {8000, 1, 1}},
        {".s16le",{44100, 4, 1}},
        {".s16be",{44100, 4, 1}},
        {".s24le",{44100, 6, 1}},
        {".s24be",{44100, 6, 1}},
        {".s32le",{44100, 8, 1}},
        {".s32be",{44100, 8, 1}},
        {".f32le",{44100, 8, 1}},
        {".f32be",{44100, 8, 1}},
        {".f64le",{44100, 16, 1}},
        {".f64be",{44100, 16, 1}},
    };

    auto config_it = kSyntheticRawConfigs.find(extension);
    if (config_it == kSyntheticRawConfigs.end()) {
        return std::nullopt;
    }

    try {
        FileIOHandler handler(path);
        PsyMP3::IO::filesize_t file_size = handler.getFileSize();
        if (file_size <= 0) {
            return 0u;
        }

        const SyntheticRawConfig& config = config_it->second;
        if (config.sample_rate == 0 || config.bytes_per_frame == 0) {
            return std::nullopt;
        }

        uint64_t total_samples =
            (static_cast<uint64_t>(file_size) / config.bytes_per_frame) * config.decoded_samples_per_frame;
        return static_cast<unsigned int>(total_samples / config.sample_rate);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// Derives a human-readable display name from a path: the final path component
// with its extension stripped. Used as a last-resort title so a track is never
// shown as a blank row when no embedded title is available.
TagLib::String filenameStem(const TagLib::String& path)
{
    std::string s = path.to8Bit(true);
    std::string::size_type slash = s.find_last_of("/\\");
    if (slash != std::string::npos) {
        s.erase(0, slash + 1);
    }
    std::string::size_type dot = s.find_last_of('.');
    if (dot != std::string::npos && dot != 0) {
        s.erase(dot);
    }
    return TagLib::String(s, TagLib::String::UTF8);
}

} // namespace

bool track::isKnownRawAudioExtension(const TagLib::String& path)
{
    std::string utf8_path = path.to8Bit(true);
    std::string::size_type dot = utf8_path.find_last_of('.');
    if (dot == std::string::npos) {
        return false;
    }

    std::string extension = utf8_path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    static const std::unordered_set<std::string> kRawExtensions = {
        ".ulaw", ".ul", ".mulaw", ".alaw", ".al", ".pcm", ".s8", ".u8",
        ".g722", ".722",
        ".s16le", ".s16be", ".s24le", ".s24be", ".s32le", ".s32be",
        ".f32le", ".f32be", ".f64le", ".f64be"
    };

    return kRawExtensions.find(extension) != kRawExtensions.end();
}

bool track::shouldCreateTagLibRefForPath(const TagLib::String& path)
{
    if (path.isEmpty()) {
        return false;
    }

    return !isKnownRawAudioExtension(path);
}

/**
 * @brief Constructs a track from a file path and optional EXTINF metadata.
 *
 * If EXTINF artist, title, or duration data is provided it takes priority over
 * tag data embedded in the file. Any missing fields are subsequently filled
 * in from the file's embedded tags via `loadTags()`.
 *
 * @param a_FilePath      Filesystem path to the audio file.
 * @param extinf_artist   Artist string from EXTINF (may be empty).
 * @param extinf_title    Title string from EXTINF (may be empty).
 * @param extinf_duration Duration in seconds from EXTINF (0 means unknown).
 */
track::track(const TagLib::String& a_FilePath, const TagLib::String& extinf_artist, const TagLib::String& extinf_title, long extinf_duration)
    : m_Artist(extinf_artist),
      m_Title(extinf_title),
      m_FilePath(a_FilePath),
      m_Len(extinf_duration)
{
    // Deliberately do NOT read tags from disk here. Playlist EXTINF is treated as
    // authoritative, and a bare file (no EXTINF) falls back to its filename as a
    // placeholder title. The real tags are read from the file when the track
    // actually plays, and replace this metadata then (see
    // Player::handleTrackLoadSuccessEvent -> Playlist::updateTrackMetadataAt).
    //
    // Opening every file at load time was slow, raced the playback path (a whole
    // playlist could fail to play because a background tag-read and the play-load
    // opened the same decoder concurrently), and gave weight to TagLib parse
    // failures on valid media that simply has no usable tags.
    if (m_Title.isEmpty() && !m_FilePath.isEmpty()) {
        m_Title = filenameStem(m_FilePath);
    }
}

/**
 * @brief Loads ID/tag metadata from the audio file using TagLib.
 *
 * Creates a `FileIOHandler`-backed `TagLib::FileRef` for the track's file path
 * and populates `m_Artist`, `m_Title`, `m_Album`, and `m_Len` from the embedded
 * tags, only overwriting fields that were not already supplied via EXTINF.
 * Does nothing if `m_FilePath` is empty.
 */
void track::loadTags() { 
    // Skip tag loading if no file path provided (e.g., scrobbling metadata-only tracks)
    if (m_FilePath.isEmpty()) {
        return;
    }

    if (!shouldCreateTagLibRefForPath(m_FilePath)) {
        // Raw audio (telephony/PCM): no container and no embedded tags. Derive
        // a synthetic duration and fall back to the filename for the title.
        if (m_Len == 0) {
            auto synthetic_length = getSyntheticRawLengthSeconds(m_FilePath);
            if (synthetic_length.has_value()) {
                m_Len = *synthetic_length;
            }
        }
        if (m_Title.isEmpty()) {
            m_Title = filenameStem(m_FilePath);
        }
        return;
    }

    if (!m_FileRef) {
        try {
            // Create IOHandler-based stream for TagLib
            // This solves Unicode filename issues and provides unified I/O
            auto io_handler = std::make_unique<FileIOHandler>(m_FilePath);
            m_TagLibStream = std::make_unique<TagLibIOHandlerAdapter>(
                std::move(io_handler), m_FilePath, true);

            // Use custom stream with TagLib
            m_FileRef = std::make_unique<TagLib::FileRef>(m_TagLibStream.get());

        } catch (std::exception& e) {
            std::cerr << "track::loadTags(): Exception: " << e.what() << std::endl;
            m_FileRef = nullptr;
            m_TagLibStream = nullptr;
        }
    }

    if (m_FileRef && !m_FileRef->isNull() && m_FileRef->tag() && m_FileRef->audioProperties()) {
        // Only set if not already set by EXTINF data
        if (m_Artist.isEmpty()) m_Artist = m_FileRef->tag()->artist();
        if (m_Title.isEmpty()) m_Title = m_FileRef->tag()->title();

        // Always get album from TagLib as it's not part of EXTINF
        m_Album = m_FileRef->tag()->album();

        // Only set length if not already set by EXTINF data
        if (m_Len == 0) m_Len = m_FileRef->audioProperties()->lengthInSeconds();
    } else if (m_Title.isEmpty() || m_Len == 0) {
        // TagLib could not parse this file (e.g. an MP3 with no proper ID3
        // tags). Don't reject it or leave the entry blank just because TagLib
        // bailed: fall back to the demuxer/codec, which is the authority for
        // playback anyway. Skipped when EXTINF already supplied a title and
        // duration, so playlist loads don't pay for a needless decoder open.
        loadTagsFromDecoder();
    }

    // A track should never display as a blank row. If nothing supplied a
    // title, fall back to the file's name.
    if (m_Title.isEmpty()) {
        m_Title = filenameStem(m_FilePath);
    }
}

track::DecoderMetadataResolver track::s_decoder_resolver = nullptr;

/**
 * @brief Fallback metadata loader used when TagLib rejects the file.
 *
 * Delegates to the registered DecoderMetadataResolver (installed by
 * mediafile.cpp), which opens the file through the demuxer framework -- the same
 * path used for playback -- so an MP3 without proper ID3 tags still gets a real
 * length and any embedded tags instead of being shown as an empty, zero-length
 * entry. Only still-missing fields are filled. The resolver is decoupled via a
 * function pointer so track.o does not depend on MediaFile/the demuxer stack;
 * when no resolver is registered (tools/tests that don't link it), this is a
 * no-op. Failures are swallowed: a file the decoder also rejects keeps the
 * metadata it had.
 */
void track::loadTagsFromDecoder()
{
    if (!s_decoder_resolver) {
        return;
    }

    TagLib::String artist, title, album;
    unsigned int length_seconds = 0;
    try {
        if (!s_decoder_resolver(m_FilePath, artist, title, album, length_seconds)) {
            return;
        }
    } catch (const std::exception& e) {
        Debug::log("track", "track::loadTagsFromDecoder(): decoder fallback failed for ",
                   m_FilePath.to8Bit(true), ": ", e.what());
        return;
    }

    if (m_Artist.isEmpty()) m_Artist = artist;
    if (m_Title.isEmpty())  m_Title  = title;
    if (m_Album.isEmpty() && !album.isEmpty()) m_Album = album;
    if (m_Len == 0 && length_seconds > 0) m_Len = length_seconds;
}

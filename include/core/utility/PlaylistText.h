/*
 * PlaylistText.h - read M3U, PLS, XSPF and ASX playlists
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CORE_UTILITY_PLAYLISTTEXT_H
#define PSYMP3_CORE_UTILITY_PLAYLISTTEXT_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Core {
namespace Utility {

/** @brief The playlist formats PsyMP3 reads. */
enum class PlaylistFormat {
    Unknown,
    M3U,    ///< .m3u/.m3u8: one location per line, #EXTINF for titles
    PLS,    ///< .pls: SHOUTcast's INI form, FileN=/TitleN=/LengthN=
    XSPF,   ///< .xspf: XML Shareable Playlist Format
    ASX,    ///< .asx/.wax/.wvx: Windows Media's XML playlists
};

/** @brief One entry of a playlist, as the playlist writes it. */
struct PlaylistItem {
    std::string location;       ///< path, URL or file:// URI, as written (XML-unescaped)
    std::string artist;         ///< "" when the playlist names none
    std::string title;          ///< "" when the playlist names none
    long duration_seconds = 0;  ///< 0 when unknown (or a stream's -1)
};

/**
 * @brief Reading playlists in the formats internet radio and other players
 *        write, for local playlist files and "listen" links alike.
 *
 * The parsers return locations as written; making a relative one absolute
 * is for the caller, which knows where the playlist came from.
 */
class PlaylistText {
public:
    /**
     * @brief The format a file extension names.
     * @param extension Without the dot, any case.
     * @return It, or Unknown.
     */
    static PlaylistFormat formatForExtension(const std::string& extension);

    /**
     * @brief The format a media type names (an HTTP Content-Type).
     * @param mime_type Lower case, without parameters.
     * @return It, or Unknown.
     */
    static PlaylistFormat formatForMimeType(const std::string& mime_type);

    /**
     * @brief The format of playlist text, from what it begins with:
     *        "[playlist]", an XSPF or ASX root element, "#EXTM3U" or a
     *        location. @p fallback decides when the text does not.
     * @param text The playlist.
     * @param fallback What the name or Content-Type said.
     * @return The format.
     */
    static PlaylistFormat detect(const std::string& text, PlaylistFormat fallback);

    /**
     * @brief Read a playlist's entries, in order.
     * @param text The playlist.
     * @param format Its format; Unknown reads it as M3U.
     * @return The entries; empty if there are none or the text is broken.
     */
    static std::vector<PlaylistItem> parse(const std::string& text, PlaylistFormat format);

    /**
     * @brief Whether a location is a URL (scheme://...) rather than a path.
     * @param location As written.
     * @return true for scheme://, where the scheme is letters, digits, + - .
     */
    static bool isUrl(const std::string& location);

    /**
     * @brief The local path a file:// URI names, percent-decoded
     *        (file:///home/a%20b.mp3 is /home/a b.mp3; on Windows,
     *        file:///C:/x.mp3 is C:/x.mp3 and file://server/share/x.mp3 the
     *        UNC path //server/share/x.mp3).
     * @param uri The URI.
     * @return The path, or "" if it is not a file URI or names another
     *         machine where there is no way to reach it (not Windows).
     */
    static std::string fileUriToPath(const std::string& uri);

    /**
     * @brief Undo %XX escapes, as XSPF writes relative locations.
     * @param text The text.
     * @return It, decoded; a malformed escape is kept as written.
     */
    static std::string percentDecode(const std::string& text);

private:
    /**
     * @brief M3U: #EXTINF:seconds,Artist - Title lines name the location on
     *        the next line; other # lines are comments.
     * @param text The playlist.
     * @return Its entries.
     */
    static std::vector<PlaylistItem> parseM3U(const std::string& text);

    /**
     * @brief PLS: FileN=, TitleN= and LengthN= keys under [playlist], in N's
     *        order; keys in any case.
     * @param text The playlist.
     * @return Its entries.
     */
    static std::vector<PlaylistItem> parsePLS(const std::string& text);

    /**
     * @brief XSPF: each trackList/track's location, title, creator and
     *        duration (milliseconds).
     * @param text The playlist.
     * @return Its entries.
     */
    static std::vector<PlaylistItem> parseXSPF(const std::string& text);

    /**
     * @brief ASX: each entry's ref href (the first http(s) one or path, else
     *        the first), title and author, and each entryref href; element
     *        and attribute names in any case.
     * @param text The playlist.
     * @return Its entries.
     */
    static std::vector<PlaylistItem> parseASX(const std::string& text);

    /**
     * @brief ASX that is not well-formed XML (old players wrote href values
     *        unquoted), read tag by tag: each entry's first openable ref,
     *        each entryref. Titles are not read.
     * @param text The playlist.
     * @return Its entries.
     */
    static std::vector<PlaylistItem> parseLooseASX(const std::string& text);
};

} // namespace Utility
} // namespace Core
} // namespace PsyMP3

#endif // PSYMP3_CORE_UTILITY_PLAYLISTTEXT_H

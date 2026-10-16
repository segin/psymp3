/*
 * HLSIOHandler.h - I/O handler for HTTP Live Streaming radio
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill <segin2005@gmail.com>
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

#ifndef HLSIOHANDLER_H
#define HLSIOHANDLER_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace IO {
namespace HTTP {

/**
 * @brief Plays an HLS (HTTP Live Streaming, RFC 8216) radio station as one
 *        continuous stream.
 *
 * An HLS station is a playlist (.m3u8) of short media segments that the
 * server keeps extending. The handler fetches the playlist -- picking a
 * variant if it is a master playlist -- starts three segments from the live
 * edge, and from then on downloads each new segment and re-reads the playlist
 * as it grows. Each segment's audio goes to LiveStreamIOHandler's buffer, so
 * the demuxer sees a single endless stream.
 *
 * Supported: "packed audio" segments, which are a raw elementary stream with
 * an ID3 tag in front carrying a timestamp (RFC 8216 §3.4): AAC in ADTS (.aac)
 * and MPEG audio (.mp3). The ID3 tags are removed. Segments in MPEG-TS (.ts)
 * or fragmented MP4 (#EXT-X-MAP) are refused, as is encryption (#EXT-X-KEY)
 * and segments that are byte ranges of one resource (#EXT-X-BYTERANGE).
 *
 * Song titles come from #EXTINF: iHeart writes title="..." and artist="..."
 * there (and the kind of spot inside url="..."), others plain "Artist -
 * Title". Each segment's title is recorded where its audio starts.
 */
class HLSIOHandler : public LiveStreamIOHandler {
public:
    /** @brief A variant stream in a master playlist. */
    struct Variant {
        std::string uri;          ///< as written; resolve against the playlist URL
        uint64_t bandwidth = 0;   ///< BANDWIDTH, bits per second
        std::string codecs;       ///< CODECS, e.g. "mp4a.40.2"
    };

    /** @brief A segment in a media playlist. */
    struct Segment {
        std::string uri;          ///< as written
        uint64_t sequence = 0;    ///< media sequence number
        double duration = 0.0;    ///< EXTINF duration, seconds
        std::string title;        ///< the song, as splitStreamTitle() reads it; "" if none
    };

    /** @brief A parsed playlist: master (variants) or media (segments). */
    struct Playlist {
        bool valid = false;               ///< began with #EXTM3U
        std::vector<Variant> variants;    ///< non-empty for a master playlist
        std::vector<Segment> segments;    ///< a media playlist's segments
        double target_duration = 0.0;     ///< EXT-X-TARGETDURATION
        bool ended = false;               ///< EXT-X-ENDLIST: no more segments will come
        bool unsupported = false;         ///< encrypted, fragmented MP4, or byte ranges
        std::string unsupported_reason;   ///< why
    };

    /**
     * @brief Fetch the playlist at @p url, choose a variant if it is a master
     *        playlist, and wait for the first segment's audio.
     * @param url The .m3u8 URL.
     * @throws InvalidMediaException if the playlist cannot be had, is not
     *         HLS, uses unsupported segments, or the load is aborted.
     */
    explicit HLSIOHandler(const std::string& url);

    /** @brief Stop the producer and join it. */
    ~HLSIOHandler() override;

    /**
     * @brief The segments' media type, from the server's Content-Type or the
     *        segment names: "audio/aac" or "audio/mpeg".
     * @return It.
     */
    std::string getMimeType() const override;

    /**
     * @brief Parse playlist text.
     * @param text The playlist.
     * @return What it describes; .valid is false if it is not an M3U playlist.
     */
    static Playlist parsePlaylist(const std::string& text);

    /**
     * @brief The variant to play: the highest BANDWIDTH among those whose
     *        CODECS name no video, else the first.
     * @param variants A master playlist's variants; must not be empty.
     * @return Its index.
     */
    static size_t chooseVariant(const std::vector<Variant>& variants);

    /**
     * @brief Resolve a URI from a playlist against the playlist's own URL.
     * @param base The playlist's URL (after redirects).
     * @param reference The URI as written.
     * @return An absolute URL.
     */
    static std::string resolveUrl(const std::string& base, const std::string& reference);

    /**
     * @brief Remove the ID3 tags at the front of a packed-audio segment.
     * @param data The segment; changed in place.
     */
    static void stripLeadingID3(std::string& data);

    /**
     * @brief Whether a URL or Content-Type names an HLS playlist.
     * @param content_type The response's Content-Type, lower case, no parameters.
     * @param url The URL.
     * @return true for the HLS media types, or a path ending .m3u8.
     */
    static bool looksLikeHLS(const std::string& content_type, const std::string& url);

    // Segments to start behind the live edge (RFC 8216 §6.3.3 asks for no
    // fewer than three target durations).
    static constexpr size_t START_SEGMENTS_FROM_END = 3;
    // Failed playlist reloads in a row before the stream is given up.
    static constexpr int MAX_RELOAD_FAILURES = 5;
    /** @brief Default seconds without a new segment before a live station counts as stalled. */
    static constexpr int STALL_SECONDS = 30;

    /**
     * @brief How long a live station may bring no new segment before it is
     *        given up, for every handler from now on -- at least three
     *        target durations whatever this says. The tests shorten it.
     * @param seconds The limit.
     */
    static void setStallSeconds(int seconds);

    /**
     * @brief The setting setStallSeconds() made.
     * @return It; STALL_SECONDS until set.
     */
    static int stallSeconds();
    /** @brief Times a failed segment is fetched again before it is skipped. */
    static constexpr int SEGMENT_RETRIES = 2;

private:
    /**
     * @brief The producer: download segments in order, re-reading the media
     *        playlist when they run out, until the playlist ends, fails, or
     *        the handler is aborted.
     */
    void runWorker() override;

    const std::string m_url;
    std::string m_media_url;          // the media playlist (after variant choice and redirects)
    std::string m_mime_type;          // guarded by m_buf_mutex
    bool m_first_audio = false;       // guarded by m_buf_mutex; the constructor waits for it
};

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3

#endif // HLSIOHANDLER_H

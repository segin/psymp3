/*
 * HTTPStreamIOHandler.h - I/O handler for HTTP streams of no fixed length
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

#ifndef HTTPSTREAMIOHANDLER_H
#define HTTPSTREAMIOHANDLER_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace IO {
namespace HTTP {

/**
 * @brief Reads an HTTP response that may never end: ICY internet radio.
 *
 * HTTPIOHandler fetches a file piece by piece with range requests, each one a
 * request that must complete. A radio stream (Icecast, SHOUTcast) is one GET
 * whose body goes on for as long as the station broadcasts, has no length and
 * cannot be seeked. This handler makes that single request on the worker
 * thread LiveStreamIOHandler runs, and hands the body to its bounded buffer.
 *
 * SHOUTcast v1 servers answer "ICY 200 OK" rather than an HTTP status line.
 * libcurl does not recognise that: it refuses the reply unless HTTP/0.9 is
 * allowed, and then passes the status line and headers through as body bytes.
 * The handler allows HTTP/0.9 and reads such a header block off the front of
 * the body itself, so both kinds of server end up with the same headers.
 *
 * It asks for ICY metadata (Icy-MetaData: 1). A server that sends it says how
 * often in icy-metaint and interleaves the blocks with the audio; the handler
 * takes them out, so reads see audio alone, and records each StreamTitle with
 * the audio offset it starts at (getStreamTitleAt()).
 */
class HTTPStreamIOHandler : public LiveStreamIOHandler {
public:
    /**
     * @brief Connect to @p url and wait for the response headers.
     * @throws InvalidMediaException if the connection fails, the server
     *         answers with an error status, nothing arrives in time, or the
     *         load it is for is aborted.
     */
    explicit HTTPStreamIOHandler(const std::string& url);

    /** @brief Stop the transfer and join the worker thread. */
    ~HTTPStreamIOHandler() override;

    /** @brief The value of response header @p name (any case), or "". */
    std::string getHeader(const std::string& name) const;

    /** @brief Content-Type without parameters, lower case, or "". */
    std::string getMimeType() const override;

    /** @brief Content-Length, or -1 when the response did not give one. */
    int64_t getContentLength() const;

    /** @brief True when the server sent ICY headers (icy-name, icy-metaint...). */
    bool isIcy() const;

    /**
     * @brief True when the response is a stream rather than a file: it is ICY,
     *        or it has no Content-Length.
     */
    bool isLive() const;

    /**
     * @brief Never connect again once this response ends: for a reply being
     *        read whole (a playlist), which a reconnection would only repeat.
     */
    void disableReconnect();

    /**
     * @brief The URL the last transfer ended up at, after redirects, for
     *        resolving a playlist's relative entries. Known once a transfer
     *        has finished -- as a playlist read whole has.
     * @return It, or the URL asked for until then.
     */
    std::string getEffectiveUrl() const;

    /**
     * @brief The station's name from its icy-name header, decoded as
     *        decodeIcyText() does.
     * @return The name, or "" when the server gave none.
     */
    std::string getStationName() const override;

    /**
     * @brief Decode text from an ICY server: UTF-8 if it is valid UTF-8,
     *        otherwise Latin-1, which is what most older stations send.
     *
     * The one place PsyMP3 falls back from UTF-8: a station's titles reach
     * the screen, MPRIS, Discord and Last.fm, and many stations still send
     * Latin-1.
     * @param raw The bytes as sent.
     * @return UTF-8 text.
     */
    static std::string decodeIcyText(const std::string& raw);

    /**
     * @brief Find StreamTitle='...' in an ICY metadata block.
     *
     * The value ends at the first "';" after it, so titles that contain an
     * apostrophe ("Guns N' Roses") survive. Trailing NUL padding is ignored.
     * @param block The metadata block, as sent.
     * @param title Set to the raw (undecoded) value when found.
     * @return true if the block had a StreamTitle.
     */
    static bool parseStreamTitle(const std::string& block, std::string& title);

    /**
     * @brief Find field @p name ('StreamUrl', 'StreamArtwork', ...) in an ICY
     *        metadata block, read as parseStreamTitle() reads StreamTitle.
     * @param block The metadata block, as sent.
     * @param name The field's name.
     * @param value Set to the raw (undecoded) value when found.
     * @return true if the block had the field.
     */
    static bool parseIcyField(const std::string& block, const std::string& name, std::string& value);

    /** @brief A StreamTitle taken apart for display and scrobbling. */
    struct TitleParts {
        std::string artist;   ///< "" when the title names none
        std::string title;    ///< the song, or whatever the station announced
        bool is_music = true; ///< false for a station's own spots (iHeart song_spot other than "M" or "F")
        uint64_t length_ms = 0; ///< the song's length when the station says (iHeart length=), else 0
        std::string artwork_url; ///< the song's picture when the station links one (iHeart amgArtworkURL=), else ""
    };

    /**
     * @brief Split a decoded StreamTitle into artist and title.
     *
     * Plain titles are "Artist - Title", split at the first " - ". iHeart
     * appends key="value" attributes and puts the song in text=:
     * 'Ray Parker, Jr. - text="Ghostbusters" song_spot="M" ...' is artist
     * "Ray Parker, Jr.", title "Ghostbusters"; its station spots name no
     * artist and say song_spot="T". iHeart's newer form is all attributes,
     * comma-separated, the url= value holding unescaped quotes of its own:
     * 'title="Fine By Me",artist="ANDY GRAMMER",url="song_spot="F" ..."'.
     * Songs are song_spot M (broadcast stations) or F (digital ones).
     * Surrounding spaces are trimmed.
     * @param stream_title The title, decoded.
     * @return Its parts.
     */
    static TitleParts splitStreamTitle(const std::string& stream_title);

    /**
     * @brief Whether a title's artist is the station itself: its whole name,
     *        or the words it starts with ("181.FM" of "181.FM Awesome 80s"),
     *        ignoring case. Such a title is the station's own break or promo
     *        ("181.FM - Music Promo30"), not a song.
     * @param artist The title's artist.
     * @param station_name The station's name (icy-name).
     * @return true if the artist names the station.
     */
    static bool artistIsStation(const std::string& artist, const std::string& station_name);

    /**
     * @brief Whether @p url can be fetched as a picture: an http(s) URL.
     *        Stations fill the field with "", "null" or a bare host as often.
     * @param url The value, trimmed or not.
     * @return true if it is worth fetching.
     */
    static bool isArtworkUrl(const std::string& url);

    // Most bytes of audio allowed between metadata blocks; a larger
    // icy-metaint is taken as nonsense and ignored.
    static constexpr int64_t MAX_METAINT = 4 * 1024 * 1024;
    // Longest an ICY v1 header block may be before the reply is rejected.
    static constexpr size_t MAX_ICY_HEADER_BYTES = 16 * 1024;
    // Seconds without a byte (while not waiting on the reader) before the
    // connection is treated as dead.
    static constexpr int STALL_SECONDS = 30;
    // Seconds allowed to establish the connection.
    static constexpr int CONNECT_SECONDS = 10;
    // Reconnecting after a dropped ICY stream: waits double from 1 s up to
    // this, and the failure count resets once a connection has played this
    // long.
    static constexpr int MAX_RECONNECT_DELAY_SECONDS = 16;
    static constexpr int RECONNECT_RESET_SECONDS = 10;
    static constexpr int DEFAULT_RECONNECT_ATTEMPTS = 5;

    /**
     * @brief How many failed reconnections in a row an ICY stream survives
     *        before it is given up, for every handler from now on. 0 turns
     *        reconnecting off (the tests do, whose servers close on purpose).
     * @param attempts The number.
     */
    static void setReconnectAttempts(int attempts);

    /**
     * @brief The setting setReconnectAttempts() made.
     * @return It; DEFAULT_RECONNECT_ATTEMPTS until set.
     */
    static int reconnectAttempts();

private:
    /**
     * @brief The producer: run connections until the stream is over. An ICY
     *        stream that drops -- the server closes, an error, or a stall --
     *        is reconnected after 1, 2, 4... s (MAX_RECONNECT_DELAY_SECONDS at
     *        most), and given up after reconnectAttempts() failures in a row;
     *        anything else ends with its connection.
     */
    void runWorker() override;

    /**
     * @brief One connection: set up one libcurl GET (redirects followed,
     *        HTTP/0.9 allowed for SHOUTcast v1, ICY metadata requested, no
     *        overall timeout) and run it until the server closes, an error or
     *        stall ends it, or it is aborted.
     * @param error_text Receives why it failed.
     * @return true if it failed (an abort is not a failure).
     */
    bool runConnection(std::string& error_text);

    /**
     * @brief Reset what belongs to one connection -- status, the ICY v1
     *        head, the metadata framing -- before the next. Caller holds
     *        m_buf_mutex.
     */
    void startConnection_unlocked();

    /**
     * @brief libcurl CURLOPT_HEADERFUNCTION trampoline to onHeader().
     * @param data Header bytes (one line, with its line ending).
     * @param size Always 1.
     * @param nmemb Byte count.
     * @param self The HTTPStreamIOHandler.
     * @return Bytes taken; anything else makes libcurl abort.
     */
    static size_t headerCallback(char* data, size_t size, size_t nmemb, void* self);

    /**
     * @brief libcurl CURLOPT_WRITEFUNCTION trampoline to onBody().
     * @param data Body bytes.
     * @param size Always 1.
     * @param nmemb Byte count.
     * @param self The HTTPStreamIOHandler.
     * @return Bytes taken; anything else makes libcurl abort.
     */
    static size_t writeCallback(char* data, size_t size, size_t nmemb, void* self);

    /**
     * @brief libcurl CURLOPT_XFERINFOFUNCTION trampoline to onProgress().
     *        The transfer counters are unused.
     * @param self The HTTPStreamIOHandler.
     * @return 0 to continue, non-zero to abort the transfer.
     */
    static int progressCallback(void* self, curl_off_t, curl_off_t, curl_off_t, curl_off_t);

    /**
     * @brief Handle one response header line: a status line starts a new set
     *        of headers (redirects send several), anything else is stored.
     * @param data The line, with its line ending.
     * @param len Its length.
     * @return @p len, or 0 to abort once stopping.
     */
    size_t onHeader(const char* data, size_t len);

    /**
     * @brief Handle a chunk of response body: peel off an ICY v1 header block
     *        if there is one, mark the headers known, and pass the rest on.
     * @param data Body bytes.
     * @param len Their count.
     * @return @p len, or 0 to abort the transfer.
     */
    size_t onBody(const char* data, size_t len);

    /**
     * @brief Called by libcurl about once a second: ends the transfer when it
     *        is being stopped, or when no byte has come for STALL_SECONDS
     *        while not waiting on the reader.
     * @return true to continue, false to abort.
     */
    bool onProgress();

    /**
     * @brief Split body bytes into audio and ICY metadata.
     *
     * With an icy-metaint of N, the body is N audio bytes, a length byte L,
     * L * 16 bytes of metadata, N audio bytes, and so on. Audio goes to
     * appendAudio(); each finished metadata block to onMetadataBlock().
     * Without icy-metaint everything is audio. Worker thread only.
     * @param data Body bytes, with any ICY v1 header already taken off.
     * @param len Their count.
     * @return false if the transfer was aborted.
     */
    bool deliverBody(const char* data, size_t len);

    /**
     * @brief Record the StreamTitle of a finished metadata block, if it has
     *        one, through recordTitle().
     * @param block The block, NUL padding included.
     */
    void onMetadataBlock(const std::string& block);

    /**
     * @brief Collect the "ICY 200 OK" header block from the front of an
     *        HTTP/0.9 body and parse it into the status and m_headers.
     *
     * Sets m_icy_v1_done once the block has ended, or as soon as the body
     * turns out not to start with "ICY " (and then hands the bytes collected
     * so far on to the buffer). Sets m_failed if the block grows past
     * MAX_ICY_HEADER_BYTES. Caller holds m_buf_mutex.
     * @param data This chunk of body.
     * @param len Its length.
     * @return How many bytes of this chunk belonged to the header block; the
     *         rest is body.
     */
    size_t consumeIcyV1Header(const char* data, size_t len);

    /**
     * @brief Parse one "Name: value" line into m_headers under its lower-case
     *        name. Lines without a colon are ignored. Caller holds m_buf_mutex.
     * @param line The header line.
     */
    void addHeaderLine_unlocked(const std::string& line);

    /**
     * @brief A connection's response head is complete: derive the content
     *        length, whether the reply is ICY and its metadata framing. The
     *        first connection's wakes the constructor; a reconnection answered
     *        with an error status is marked failed. Once per connection.
     *        Caller holds m_buf_mutex.
     */
    void markHeadersReady_unlocked();

    const std::string m_url;

    // Guarded by m_buf_mutex.
    std::map<std::string, std::string> m_headers;  // lower-case names
    int m_status = 0;                 // HTTP or ICY status code; 0 if none
    int64_t m_content_length = -1;
    bool m_icy = false;
    bool m_headers_ready = false;     // the first response's head known (the constructor waits for it)
    bool m_conn_ready = false;        // this connection's head known
    bool m_conn_failed = false;       // this reconnection answered with an error status
    uint64_t m_conn_audio_bytes = 0;  // audio this connection delivered (worker thread)
    filesize_t m_conn_start = 0;      // audio offset this connection's audio begins at
    bool m_conn_titled = false;       // this connection has announced a title
    /** @brief The first response was an ICY stream without a length: a station worth reconnecting to. */
    bool m_station = false;
    /** @brief Where the last transfer's redirects led (CURLINFO_EFFECTIVE_URL); "" until one finishes. */
    std::string m_effective_url;
    /** @brief Set by disableReconnect(): the reply is read whole, not played. */
    std::atomic<bool> m_no_reconnect{false};

    // ICY v1 header block being collected from the front of the body.
    bool m_icy_v1_done = false;
    std::string m_icy_v1_header;

    // Metadata framing (icy-metaint), used by the worker thread alone once
    // the headers are known.
    int64_t m_metaint = 0;            // audio bytes per block; 0 = no metadata
    int64_t m_audio_left = 0;         // audio bytes before the next length byte
    int64_t m_meta_left = -1;         // metadata bytes still to come; -1 = length byte next
    std::string m_meta_block;
};

/**
 * @brief Open an http(s) URL with the handler that suits what it serves.
 *
 * Makes one GET. A stream (ICY headers, or no Content-Length) keeps the
 * HTTPStreamIOHandler that made it; an HLS playlist goes to HLSIOHandler; a
 * file of known length is handed to HTTPIOHandler, which can seek it with
 * range requests.
 * @throws InvalidMediaException if the URL cannot be opened.
 */
std::unique_ptr<IOHandler> openHTTPIOHandler(const std::string& url);

/**
 * @brief Keep a live stream's connection -- opened to find out what the URL
 *        serves -- for the next openHTTPIOHandler() of the same URL, which
 *        takes it instead of connecting again. One is kept, for 10 s at
 *        most; anything not live is simply closed.
 * @param url The URL it was opened for.
 * @param handler The handler.
 */
void parkLiveHandler(const std::string& url, std::unique_ptr<IOHandler> handler);

/**
 * @brief Take the connection parked for @p url, if it is fresh enough.
 * @param url The URL.
 * @return It, or null.
 */
std::unique_ptr<IOHandler> takeParkedHandler(const std::string& url);

/** @brief Close any parked connection (at shutdown, before the registry goes). */
void discardParkedHandlers();

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3

#endif // HTTPSTREAMIOHANDLER_H

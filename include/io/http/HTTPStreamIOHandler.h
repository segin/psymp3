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
 * @brief Reads an HTTP response that may never end: internet radio.
 *
 * HTTPIOHandler fetches a file piece by piece with range requests, each one a
 * request that must complete. A radio stream (Icecast, SHOUTcast) is one GET
 * whose body goes on for as long as the station broadcasts, has no length and
 * cannot be seeked. This handler makes that single request on a worker thread
 * and hands the body to the reader as it arrives, through a bounded buffer:
 * the worker waits when the reader is far enough behind, so a paused player
 * holds the connection open rather than buffering without limit.
 *
 * Seeking is limited to what the buffer can serve. The reader may go back
 * over the last HISTORY_BYTES it has read (format probes and demuxers read a
 * few bytes and step back) and forward by at most FORWARD_SEEK_BYTES, which
 * later reads wait for. Anything else fails. The size is always unknown.
 *
 * SHOUTcast v1 servers answer "ICY 200 OK" rather than an HTTP status line.
 * libcurl does not recognise that: it refuses the reply unless HTTP/0.9 is
 * allowed, and then passes the status line and headers through as body bytes.
 * The handler allows HTTP/0.9 and reads such a header block off the front of
 * the body itself, so both kinds of server end up with the same headers.
 *
 * It asks for ICY metadata (Icy-MetaData: 1). A server that sends it says how
 * often in icy-metaint and interleaves the blocks with the audio; the handler
 * takes them out, so reads see audio alone, and keeps each StreamTitle with
 * the audio offset it starts at (getStreamTitleAt()).
 */
class HTTPStreamIOHandler : public IOHandler {
public:
    /**
     * @brief Connect to @p url and wait for the response headers.
     * @throws InvalidMediaException if the connection fails, the server
     *         answers with an error status, or nothing arrives in time.
     */
    explicit HTTPStreamIOHandler(const std::string& url);

    /** @brief Stop the transfer and join the worker thread. */
    ~HTTPStreamIOHandler() override;

    HTTPStreamIOHandler(const HTTPStreamIOHandler&) = delete;
    HTTPStreamIOHandler& operator=(const HTTPStreamIOHandler&) = delete;

    /**
     * @brief Stop the transfer and close the handler.
     *
     * Overridden so that a read waiting for data is woken (and lets go of the
     * operation lock) before the base class takes that lock to close.
     * @return 0.
     */
    int close() override;

    /**
     * @brief True once closed, or once the transfer has ended and every byte
     *        it delivered has been read.
     */
    bool eof() override;

    /** @brief Always -1: a stream has no size, even after it has ended. */
    filesize_t getFileSize() override;

    /**
     * @brief Always true: whatever the response, this handler reads it as a
     *        stream, without size or seeking.
     * @return true.
     */
    bool isLiveStream() const override;

    /** @brief The value of response header @p name (any case), or "". */
    std::string getHeader(const std::string& name) const;

    /** @brief Content-Type without parameters, lower case, or "". */
    std::string getMimeType() const override;

    /** @brief Content-Length, or -1 when the response did not give one. */
    int64_t getContentLength() const { return m_content_length; }

    /** @brief True when the server sent ICY headers (icy-name, icy-metaint...). */
    bool isIcy() const { return m_icy; }

    /**
     * @brief True when the response is a stream rather than a file: it is ICY,
     *        or it has no Content-Length.
     */
    bool isLive() const { return m_icy || m_content_length < 0; }

    /**
     * @brief The station's name from its icy-name header, decoded as
     *        decodeIcyText() does.
     * @return The name, or "" when the server gave none.
     */
    std::string getStationName() const;

    /**
     * @brief The title the station announced for the audio at @p offset.
     *
     * ICY servers interleave metadata with the audio; the handler takes it
     * out, so reads return audio alone, and records each StreamTitle with the
     * audio offset it arrived at. A title applies from there until the next;
     * the first applies from offset 0.
     * @param offset Offset into the audio, as tell() counts it.
     * @return The title in effect there, or "" before the first title (or
     *         when the station sends none, or announces an empty one).
     */
    std::string getStreamTitleAt(filesize_t offset) const;

    /**
     * @brief How many titles the station has announced so far. A caller can
     *        compare it with an earlier value to notice a change cheaply.
     * @return The count, starting at 0.
     */
    uint64_t getStreamTitleCount() const;

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

    // Most bytes of audio allowed between metadata blocks; a larger
    // icy-metaint is taken as nonsense and ignored.
    static constexpr int64_t MAX_METAINT = 4 * 1024 * 1024;
    // Titles kept for getStreamTitleAt(); older ones are dropped.
    static constexpr size_t MAX_TITLES = 16;

    // How far behind the reader the buffer keeps data, for backward seeks.
    static constexpr size_t HISTORY_BYTES = 256 * 1024;
    // How far ahead of the reader the worker buffers before it waits.
    static constexpr size_t READ_AHEAD_BYTES = 1024 * 1024;
    // The furthest a seek may move ahead of the data received so far.
    static constexpr size_t FORWARD_SEEK_BYTES = 1024 * 1024;
    // Longest an ICY v1 header block may be before the reply is rejected.
    static constexpr size_t MAX_ICY_HEADER_BYTES = 16 * 1024;
    // Seconds without a byte (while not waiting on the reader) before the
    // connection is treated as dead.
    static constexpr int STALL_SECONDS = 30;
    // Seconds allowed to establish the connection.
    static constexpr int CONNECT_SECONDS = 10;

private:
    /**
     * @brief Copy body bytes to @p buffer, waiting for them if necessary.
     *
     * fread semantics: returns fewer than @p count elements only at the end
     * of the stream, on error, or when the handler is closed.
     * @param buffer Destination.
     * @param size Element size in bytes.
     * @param count Number of elements wanted.
     * @return Number of whole elements read.
     */
    size_t read_unlocked(void* buffer, size_t size, size_t count) override;

    /**
     * @brief Move the read position within what the buffer can serve.
     *
     * Allowed: back to the oldest byte still kept (HISTORY_BYTES behind the
     * reader), or forward to at most FORWARD_SEEK_BYTES past what has arrived.
     * SEEK_END always fails, since a stream has no end to measure from.
     * @param offset Offset relative to @p whence.
     * @param whence SEEK_SET or SEEK_CUR.
     * @return 0 on success, -1 (error ESPIPE) when the target is out of reach.
     */
    int seek_unlocked(filesize_t offset, int whence) override;

    /**
     * @brief The reader's position in the stream, in bytes from its start.
     * @return The position, or -1 once closed.
     */
    filesize_t tell_unlocked() override;

    /**
     * @brief Mark the handler closed. The transfer has already been stopped
     *        by close() before this runs under the operation lock.
     * @return Always 0.
     */
    int close_unlocked() override;

    /**
     * @brief Worker-thread body: make the GET and run it until the server
     *        closes, an error or stall ends it, or it is aborted. Records why
     *        it ended and wakes everything waiting on the buffer.
     */
    void runTransfer();

    /**
     * @brief Abort the transfer and join the worker thread. Safe to call more
     *        than once and from the destructor.
     */
    void stopTransfer();

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
     *        if there is one, mark the headers known, and queue the rest for
     *        the reader.
     * @param data Body bytes.
     * @param len Their count.
     * @return @p len, or 0 to abort the transfer.
     */
    size_t onBody(const char* data, size_t len);

    /**
     * @brief Called by libcurl about once a second: ends the transfer when it
     *        is being stopped, or when no byte has come for STALL_SECONDS
     *        while the worker was not waiting on the reader.
     * @return true to continue, false to abort.
     */
    bool onProgress();

    /**
     * @brief Queue body bytes for the reader. Waits while the reader is
     *        READ_AHEAD_BYTES or more behind, and drops data that has fallen
     *        more than HISTORY_BYTES behind it.
     * @param data Body bytes, with any ICY v1 header already taken off.
     * @param len Their count.
     * @return false if the transfer was aborted while waiting.
     */
    bool appendBody(const char* data, size_t len);

    /**
     * @brief Split body bytes into audio and ICY metadata.
     *
     * With an icy-metaint of N, the body is N audio bytes, a length byte L,
     * L * 16 bytes of metadata, N audio bytes, and so on. Audio goes to
     * appendBody(); each finished metadata block goes to onMetadataBlock().
     * Without icy-metaint everything is audio. Worker thread only.
     * @param data Body bytes, with any ICY v1 header already taken off.
     * @param len Their count.
     * @return false if the transfer was aborted.
     */
    bool deliverBody(const char* data, size_t len);

    /**
     * @brief Record the StreamTitle of a finished metadata block, if it has
     *        one and it differs from the last, at the current end of the audio.
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
     * @brief The response head is complete: derive the content length and
     *        whether the reply is ICY, and wake the constructor. Does nothing
     *        the second time. Caller holds m_buf_mutex.
     */
    void markHeadersReady_unlocked();

    const std::string m_url;

    // Everything below is guarded by m_buf_mutex.
    mutable std::mutex m_buf_mutex;
    std::condition_variable m_data_cv;   // reader and constructor wait here
    std::condition_variable m_space_cv;  // worker waits here for room

    std::vector<uint8_t> m_data;      // bytes [m_data_start, m_data_start + size)
    filesize_t m_data_start = 0;      // stream offset of m_data[0]
    filesize_t m_read_pos = 0;        // reader's stream offset

    std::map<std::string, std::string> m_headers;  // lower-case names
    int m_status = 0;                 // HTTP or ICY status code; 0 if none
    int64_t m_content_length = -1;
    bool m_icy = false;
    bool m_headers_ready = false;     // response head known
    bool m_finished = false;          // transfer over (end, error or abort)
    bool m_failed = false;            // transfer ended with an error
    std::string m_error_text;         // why, for the exception and the log

    // ICY v1 header block being collected from the front of the body.
    bool m_icy_v1_done = false;
    std::string m_icy_v1_header;

    // Titles announced, with the audio offset each starts at; newest last.
    std::deque<std::pair<filesize_t, std::string>> m_titles;
    uint64_t m_title_count = 0;

    // Metadata framing (icy-metaint), used by the worker thread alone once
    // the headers are known.
    int64_t m_metaint = 0;            // audio bytes per block; 0 = no metadata
    int64_t m_audio_left = 0;         // audio bytes before the next length byte
    int64_t m_meta_left = -1;         // metadata bytes still to come; -1 = length byte next
    std::string m_meta_block;

    // Read by the progress callback on the worker thread.
    std::atomic<bool> m_abort{false};
    std::atomic<bool> m_waiting_for_reader{false};
    std::atomic<int64_t> m_last_data_ms{0};  // steady-clock ms of last byte

    std::thread m_worker;
};

/**
 * @brief Open an http(s) URL with the handler that suits what it serves.
 *
 * Makes one GET. A stream (ICY headers, or no Content-Length) keeps the
 * HTTPStreamIOHandler that made it; a file of known length is handed to
 * HTTPIOHandler, which can seek it with range requests.
 * @throws InvalidMediaException if the URL cannot be opened.
 */
std::unique_ptr<IOHandler> openHTTPIOHandler(const std::string& url);

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3

#endif // HTTPSTREAMIOHANDLER_H

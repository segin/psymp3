/*
 * LiveStreamIOHandler.h - base for I/O handlers that read internet radio
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

#ifndef LIVESTREAMIOHANDLER_H
#define LIVESTREAMIOHANDLER_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace IO {
namespace HTTP {

class LiveStreamIOHandler;

/**
 * @brief The stream connections opened for one purpose -- loading one track --
 *        so they can be aborted together.
 *
 * A station that stalls holds whatever is reading it until the stall check
 * gives up, which takes half a minute. Skipping to another track while one
 * loads, or stopping, must not wait that long: the player opens each load
 * inside a Scope, and aborts the load's group when the load is no longer
 * wanted. Once the load has finished and its stream is playing, the player
 * detaches the group, after which aborting it touches nothing.
 */
class StreamAbortGroup {
public:
    /**
     * @brief Makes a group the one that live stream handlers created on this
     *        thread join, for the Scope's lifetime.
     */
    class Scope {
    public:
        /**
         * @brief Make @p group current on this thread.
         * @param group The group; may be null for none.
         */
        explicit Scope(std::shared_ptr<StreamAbortGroup> group);

        /** @brief Restore the group that was current before. */
        ~Scope();

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        std::shared_ptr<StreamAbortGroup> m_previous;
    };

    /**
     * @brief Abort every connection in the group, and refuse any that would
     *        join it later. Does nothing once detached.
     */
    void abort();

    /**
     * @brief The group's connections are in use now: forget them, so a later
     *        abort() cannot reach them.
     */
    void detach();

    /**
     * @brief Whether abort() has been called (before any detach()).
     * @return true once aborted.
     */
    bool isAborted() const;

    /**
     * @brief The group current on this thread.
     * @return It, or null.
     */
    static std::shared_ptr<StreamAbortGroup> current();

private:
    friend class LiveStreamIOHandler;

    /**
     * @brief Add a connection, unless the group is aborted or detached.
     * @param handler The connection.
     * @return false if the group is aborted: the connection must give up.
     */
    bool join(LiveStreamIOHandler* handler);

    /**
     * @brief Remove a connection that is going away.
     * @param handler The connection.
     */
    void leave(LiveStreamIOHandler* handler);

    mutable std::mutex m_mutex;
    std::set<LiveStreamIOHandler*> m_members;
    bool m_aborted = false;
    bool m_detached = false;
};

/**
 * @brief The reading side of an internet radio stream, whatever brings the
 *        audio in.
 *
 * A subclass runs a producer on a worker thread (runWorker()) -- one endless
 * GET for ICY, a round of playlist and segment fetches for HLS -- and hands
 * the audio over with appendAudio() and the titles with recordTitle(). This
 * class keeps them in a bounded buffer for the reader: the producer waits
 * when the reader is READ_AHEAD_BYTES behind, so a paused player holds the
 * connection open rather than buffering without limit, and the last
 * HISTORY_BYTES the reader has read are kept for the short backward seeks
 * format probes and demuxers make. Other seeks fail; the size is unknown.
 *
 * Every instance is registered, so abortAll() can wake every reader at
 * quit, and joins the StreamAbortGroup current on the creating thread.
 *
 * Thread safety: reads, seeks and tells come through IOHandler's operation
 * lock; the buffer and everything the producer shares with the reader are
 * guarded by m_buf_mutex.
 */
class LiveStreamIOHandler : public IOHandler {
public:
    /**
     * @brief Leave the abort group and the registry, then stop the worker if
     *        the subclass has not already.
     */
    ~LiveStreamIOHandler() override;

    LiveStreamIOHandler(const LiveStreamIOHandler&) = delete;
    LiveStreamIOHandler& operator=(const LiveStreamIOHandler&) = delete;

    /**
     * @brief Stop the producer and close the handler.
     *
     * Overridden so that a read waiting for data is woken (and lets go of the
     * operation lock) before the base class takes that lock to close.
     * @return 0.
     */
    int close() override;

    /**
     * @brief True once closed, or once the producer has finished and every
     *        byte it delivered has been read.
     */
    bool eof() override;

    /** @brief Always -1: a stream has no size, even after it has ended. */
    filesize_t getFileSize() override;

    /**
     * @brief Always true: these handlers read internet radio.
     * @return true.
     */
    bool isLiveStream() const override;

    /**
     * @brief The title the station announced for the audio at @p offset.
     *
     * The producer records each new title with the audio offset it arrived
     * at (recordTitle()); a title applies from there until the next, and the
     * first applies from offset 0.
     * @param offset Offset into the audio, as tell() counts it.
     * @return The title in effect there, or "" before the first title (or
     *         when the station sends none, or announces an empty one).
     */
    std::string getStreamTitleAt(filesize_t offset) const override;

    /**
     * @brief How many titles the station has announced so far. A caller can
     *        compare it with an earlier value to notice a change cheaply.
     * @return The count, starting at 0.
     */
    uint64_t getStreamTitleCount() const;

    /**
     * @brief Stop the producer without waiting for it: a waiting read (or a
     *        subclass constructor waiting on m_data_cv) wakes and gives up.
     *        Safe from any thread; the handler stays valid.
     */
    void abort();

    /**
     * @brief Abort every live stream handler in the process -- for quitting,
     *        when a stalled station must not hold up the threads being joined.
     */
    static void abortAll();

    /**
     * @brief For quitting: abort every live stream handler, and refuse to
     *        make new ones from now on, so a load that starts during
     *        shutdown cannot hold up the threads being joined.
     */
    static void shutdown();

    // How far behind the reader the buffer keeps data, for backward seeks.
    static constexpr size_t HISTORY_BYTES = 256 * 1024;
    // How far ahead of the reader the producer buffers before it waits.
    static constexpr size_t READ_AHEAD_BYTES = 1024 * 1024;
    // The furthest a seek may move ahead of the data received so far.
    static constexpr size_t FORWARD_SEEK_BYTES = 1024 * 1024;
    // Titles kept for getStreamTitleAt(); older ones are dropped.
    static constexpr size_t MAX_TITLES = 16;

protected:
    /**
     * @brief Register the handler and join the current abort group.
     * @throws InvalidMediaException if the current group has been aborted:
     *         the load this handler is for has been given up already.
     */
    LiveStreamIOHandler();

    /** @brief Start runWorker() on the worker thread. */
    void startWorker();

    /**
     * @brief Stop the worker: abort(), then join it. Safe to call more than
     *        once. A subclass must call it before its own members go -- in its
     *        destructor, and before throwing from its constructor -- since the
     *        worker runs the subclass's code.
     */
    void stopWorker();

    /**
     * @brief The producer, run on the worker thread until the stream ends,
     *        fails or is aborted. It must call finishWorker() when done.
     */
    virtual void runWorker() = 0;

    /**
     * @brief Hand audio to the reader. Waits while the reader is
     *        READ_AHEAD_BYTES or more behind, and drops data that has fallen
     *        more than HISTORY_BYTES behind it. Counts as activity for
     *        stalledFor().
     * @param data Audio bytes.
     * @param len Their count.
     * @return false if aborted while waiting.
     */
    bool appendAudio(const char* data, size_t len);

    /**
     * @brief Add audio without waiting for room. Caller holds m_buf_mutex.
     *        For the few bytes a producer finds it was wrong to hold back.
     * @param data Audio bytes.
     * @param len Their count.
     */
    void insertAudio_unlocked(const char* data, size_t len);

    /**
     * @brief Record @p title as the one playing from @p at -- by default the
     *        current end of the audio (the very first from 0) -- unless it
     *        repeats the last.
     * @param title The title, decoded to UTF-8.
     * @param at Where it starts; -1 for the current end. Never placed before
     *        the last title recorded.
     */
    void recordTitle(const std::string& title, filesize_t at = -1);

    /**
     * @brief The producer has finished: record whether it failed and why, and
     *        wake everything waiting on the buffer.
     * @param failed Whether it ended in error (an abort is not one).
     * @param error_text Why, when it failed.
     */
    void finishWorker(bool failed, const std::string& error_text);

    /**
     * @brief Note that data arrived, for stalledFor().
     */
    void noteActivity();

    /**
     * @brief Whether nothing has arrived for @p seconds while the producer
     *        was not waiting on the reader (which is not the server's fault).
     * @param seconds The limit.
     * @return true if stalled that long.
     */
    bool stalledFor(int seconds) const;

    /**
     * @brief Wait up to @p seconds on the worker thread, waking at once if
     *        the handler is aborted.
     * @param seconds How long.
     * @return false if aborted.
     */
    bool waitOrAbort(double seconds);

    /**
     * @brief Have @p curl open its sockets through this handler, which keeps
     *        them so abort() can shut them down: a transfer waiting on a
     *        silent server then ends at once, not at curl's next progress
     *        tick, about a second later.
     * @param curl The transfer about to be performed.
     */
    void trackSockets(CURL* curl);

    /**
     * @brief GET @p url to completion, aborting when the handler is: a
     *        playlist, a segment, a picture. Same TLS and redirect rules as
     *        the streaming GET, a REQUEST_SECONDS limit, sockets tracked.
     * @param url What to fetch.
     * @param body Receives the body.
     * @param content_type Receives the Content-Type (lower case, no
     *        parameters), when not null.
     * @param effective_url Receives the URL after redirects, when not null.
     * @param error Receives why it failed.
     * @param max_bytes A body longer than this fails; 0 for no limit.
     * @return true for a 2xx answer.
     */
    bool fetch(const std::string& url, std::string& body, std::string* content_type,
               std::string* effective_url, std::string& error, size_t max_bytes = 0);

    // Seconds a fetch() may take in all.
    static constexpr long REQUEST_SECONDS = 30;

    /**
     * @brief Total audio bytes received so far. Caller holds m_buf_mutex.
     * @return The offset just past the newest byte.
     */
    filesize_t audioEnd_unlocked() const;

    /// Guards the buffer and what the producer shares with the reader.
    mutable std::mutex m_buf_mutex;
    std::condition_variable m_data_cv;   // readers (and subclass constructors) wait here
    std::condition_variable m_space_cv;  // the producer waits here for room

    bool m_finished = false;          // producer over (end, error or abort)
    bool m_failed = false;            // producer ended with an error
    std::string m_error_text;         // why, for exceptions and the log

    std::atomic<bool> m_abort{false};

    /** @brief Set by shutdown(): constructors refuse from then on. */
    static std::atomic<bool> s_shut_down;

    /**
     * @brief libcurl CURLOPT_OPENSOCKETFUNCTION: open the socket and keep it.
     * @param self The handler.
     * @param purpose What it is for.
     * @param address Where it goes.
     * @return The socket, or CURL_SOCKET_BAD.
     */
    static curl_socket_t openSocketCallback(void* self, curlsocktype purpose, struct curl_sockaddr* address);

    /**
     * @brief libcurl CURLOPT_CLOSESOCKETFUNCTION: forget the socket, close it.
     * @param self The handler.
     * @param socket The socket.
     * @return 0.
     */
    static int closeSocketCallback(void* self, curl_socket_t socket);

    /** @brief Guards m_sockets. */
    std::mutex m_sockets_mutex;
    /** @brief The sockets curl has open for this handler, for abort() to shut down. */
    std::set<curl_socket_t> m_sockets;

private:
    /**
     * @brief Copy audio from the buffer, waiting for the producer when the
     *        reader has caught up. fread semantics: fewer than @p count
     *        elements only at the end, on error, or once closed or aborted.
     * @param buffer Destination.
     * @param size Element size in bytes.
     * @param count Elements wanted.
     * @return Whole elements read.
     */
    size_t read_unlocked(void* buffer, size_t size, size_t count) override;

    /**
     * @brief Move the reader within what the buffer can serve: back to the
     *        oldest byte kept, forward to FORWARD_SEEK_BYTES past the newest.
     *        SEEK_END always fails.
     * @param offset Offset relative to @p whence.
     * @param whence SEEK_SET or SEEK_CUR.
     * @return 0 on success, -1 (error ESPIPE) when out of reach.
     */
    int seek_unlocked(filesize_t offset, int whence) override;

    /**
     * @brief The reader's position in the audio.
     * @return The position, or -1 once closed.
     */
    filesize_t tell_unlocked() override;

    /**
     * @brief Mark the handler closed; close() has stopped the producer.
     * @return Always 0.
     */
    int close_unlocked() override;

    std::vector<uint8_t> m_data;      // bytes [m_data_start, m_data_start + size)
    filesize_t m_data_start = 0;      // stream offset of m_data[0]
    filesize_t m_read_pos = 0;        // reader's stream offset

    // Titles announced, with the audio offset each starts at; newest last.
    std::deque<std::pair<filesize_t, std::string>> m_titles;
    uint64_t m_title_count = 0;

    std::atomic<bool> m_waiting_for_reader{false};
    std::atomic<int64_t> m_last_data_ms{0};  // steady-clock ms of last activity

    std::thread m_worker;
    std::shared_ptr<StreamAbortGroup> m_group;
};

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3

#endif // LIVESTREAMIOHANDLER_H

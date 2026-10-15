/*
 * LiveStreamIOHandler.cpp - base for I/O handlers that read internet radio
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace IO {
namespace HTTP {

namespace {

/**
 * @brief The steady clock in milliseconds, for the stall timer.
 * @return Milliseconds since the steady clock's (arbitrary) epoch.
 */
int64_t liveSteadyMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

/// The abort group live stream handlers created on this thread join.
thread_local std::shared_ptr<StreamAbortGroup> t_current_group;

/**
 * @brief The mutex guarding allLiveHandlers().
 * @return It.
 */
std::mutex& allLiveHandlersMutex()
{
    static std::mutex m;
    return m;
}

/**
 * @brief Every live stream handler alive in the process, for abortAll().
 * @return The set.
 */
std::set<LiveStreamIOHandler*>& allLiveHandlers()
{
    static std::set<LiveStreamIOHandler*> handlers;
    return handlers;
}

} // namespace

/**
 * @brief Make @p group the current one on this thread for the scope.
 * @param group The group, or null.
 */
StreamAbortGroup::Scope::Scope(std::shared_ptr<StreamAbortGroup> group)
    : m_previous(std::move(t_current_group))
{
    t_current_group = std::move(group);
}

/**
 * @brief Put back the group that was current before.
 */
StreamAbortGroup::Scope::~Scope()
{
    t_current_group = std::move(m_previous);
}

/**
 * @brief The group current on this thread.
 * @return It, or null.
 */
std::shared_ptr<StreamAbortGroup> StreamAbortGroup::current()
{
    return t_current_group;
}

/**
 * @brief Abort the members and refuse later ones; a detached group ignores it.
 */
void StreamAbortGroup::abort()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_detached) return;
    m_aborted = true;
    for (LiveStreamIOHandler* handler : m_members) {
        handler->abort();
    }
}

/**
 * @brief Forget the members: they belong to a stream that is playing now.
 */
void StreamAbortGroup::detach()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_detached = true;
    m_members.clear();
}

/**
 * @brief Whether abort() reached the group before it was detached.
 * @return true if aborted.
 */
bool StreamAbortGroup::isAborted() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_aborted;
}

/**
 * @brief Take a new connection in, unless the group has been aborted. A
 *        detached group admits it without tracking it.
 * @param handler The connection.
 * @return false if aborted.
 */
bool StreamAbortGroup::join(LiveStreamIOHandler* handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_aborted) return false;
    if (!m_detached) m_members.insert(handler);
    return true;
}

/**
 * @brief Let a departing connection go.
 * @param handler The connection.
 */
void StreamAbortGroup::leave(LiveStreamIOHandler* handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_members.erase(handler);
}

/**
 * @brief Register, and join the current abort group unless it has given up.
 * @throws InvalidMediaException if the current group has been aborted.
 */
LiveStreamIOHandler::LiveStreamIOHandler()
{
    if (s_shut_down) {
        throw InvalidMediaException("Cannot open stream: shutting down");
    }
    m_group = StreamAbortGroup::current();
    if (m_group && !m_group->join(this)) {
        m_group.reset();
        throw InvalidMediaException("Cannot open stream: aborted");
    }
    {
        std::lock_guard<std::mutex> lock(allLiveHandlersMutex());
        allLiveHandlers().insert(this);
        /*
         * shutdown() came between the check above and here: it has walked
         * the registry already, so abort ourselves.
         */
        if (s_shut_down) m_abort = true;
    }
    m_last_data_ms = liveSteadyMs();
}

/**
 * @brief Out of the group and the registry first, so neither can reach a
 *        handler being torn down; then stop the worker if still running.
 */
LiveStreamIOHandler::~LiveStreamIOHandler()
{
    if (m_group) m_group->leave(this);
    {
        std::lock_guard<std::mutex> lock(allLiveHandlersMutex());
        allLiveHandlers().erase(this);
    }
    stopWorker();
}

/**
 * @brief Run the subclass's producer on a thread of its own.
 */
void LiveStreamIOHandler::startWorker()
{
    m_worker = std::thread([this] { runWorker(); });
}

/**
 * @brief Abort, then join the worker.
 */
void LiveStreamIOHandler::stopWorker()
{
    abort();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

/**
 * @brief Flag the abort and wake the producer (waiting for room) and any
 *        reader or constructor (waiting for data). Does not wait.
 */
void LiveStreamIOHandler::abort()
{
    m_abort = true;
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    m_space_cv.notify_all();
    m_data_cv.notify_all();
}

/**
 * @brief Abort every live stream handler.
 */
void LiveStreamIOHandler::abortAll()
{
    std::lock_guard<std::mutex> lock(allLiveHandlersMutex());
    for (LiveStreamIOHandler* handler : allLiveHandlers()) {
        handler->abort();
    }
}

/**
 * @brief Refuse new handlers, then abort every one there is. The flag is
 *        set first, so a handler made meanwhile either refuses or is
 *        registered in time to be aborted.
 */
void LiveStreamIOHandler::shutdown()
{
    s_shut_down = true;
    abortAll();
}

/** @brief Set by shutdown(): no new live stream may start. */
std::atomic<bool> LiveStreamIOHandler::s_shut_down{false};

/**
 * @brief Wait for room, trim the history, append, wake readers.
 * @param data Audio bytes.
 * @param len Their count.
 * @return false if aborted while waiting.
 */
bool LiveStreamIOHandler::appendAudio(const char* data, size_t len)
{
    noteActivity();
    std::unique_lock<std::mutex> lock(m_buf_mutex);
    auto behind = [this] {
        return m_data_start + static_cast<filesize_t>(m_data.size()) - m_read_pos;
    };
    if (behind() >= static_cast<filesize_t>(READ_AHEAD_BYTES)) {
        m_waiting_for_reader = true;
        m_space_cv.wait(lock, [&] {
            return m_abort || behind() < static_cast<filesize_t>(READ_AHEAD_BYTES);
        });
        m_waiting_for_reader = false;
        // Waiting on the reader is not a stall.
        noteActivity();
    }
    if (m_abort) return false;

    /*
     * Drop what is older than HISTORY_BYTES behind the reader, in large steps
     * so the front of the vector is not shuffled for every chunk. A reader
     * that has sought ahead of the data (past a large ID3v2 tag, say) counts
     * as being at the end: measured from beyond it, the drop would exceed
     * what the buffer holds.
     */
    const filesize_t keep_from = std::min(m_read_pos, audioEnd_unlocked()) - static_cast<filesize_t>(HISTORY_BYTES);
    if (keep_from - m_data_start >= static_cast<filesize_t>(HISTORY_BYTES)) {
        const size_t drop = static_cast<size_t>(keep_from - m_data_start);
        m_data.erase(m_data.begin(), m_data.begin() + static_cast<std::ptrdiff_t>(drop));
        m_data_start += static_cast<filesize_t>(drop);
    }
    insertAudio_unlocked(data, len);
    return true;
}

/**
 * @brief Append without waiting; caller holds m_buf_mutex.
 * @param data Audio bytes.
 * @param len Their count.
 */
void LiveStreamIOHandler::insertAudio_unlocked(const char* data, size_t len)
{
    m_data.insert(m_data.end(), data, data + len);
    m_data_cv.notify_all();
}

/**
 * @brief Record a title at @p at (default: the current end of the audio, or
 *        0 for the first), unless it repeats the last, keeping at most
 *        MAX_TITLES.
 * @param title The title.
 * @param where Its start, or -1.
 */
void LiveStreamIOHandler::recordTitle(const std::string& title, filesize_t where)
{
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    if (!m_titles.empty() && m_titles.back().second == title) return;
    // The audio received so far is what plays before this title. The first
    // title usually comes only after some audio, which is the same song: it
    // applies from the start.
    filesize_t at = where >= 0 ? where
                  : m_titles.empty() && m_title_count == 0 ? 0 : audioEnd_unlocked();
    if (!m_titles.empty() && at < m_titles.back().first) {
        at = m_titles.back().first;
    }
    m_titles.emplace_back(at, title);
    if (m_titles.size() > MAX_TITLES) m_titles.pop_front();
    ++m_title_count;
    Debug::log("http", "LiveStreamIOHandler: title at ", at, ": '", title, "'");
}

/**
 * @brief Mark the producer finished and wake everyone.
 * @param failed Whether it failed.
 * @param error_text Why.
 */
void LiveStreamIOHandler::finishWorker(bool failed, const std::string& error_text)
{
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    if (failed && !m_abort) {
        m_failed = true;
        m_error_text = error_text;
    }
    m_finished = true;
    m_data_cv.notify_all();
    m_space_cv.notify_all();
}

/**
 * @brief Restart the stall clock.
 */
void LiveStreamIOHandler::noteActivity()
{
    m_last_data_ms = liveSteadyMs();
}

/**
 * @brief Whether the stall clock has run past @p seconds while not waiting
 *        on the reader.
 * @param seconds The limit.
 * @return true if stalled.
 */
bool LiveStreamIOHandler::stalledFor(int seconds) const
{
    if (m_waiting_for_reader) return false;
    return liveSteadyMs() - m_last_data_ms > static_cast<int64_t>(seconds) * 1000;
}

/**
 * @brief Sleep on the space condition (which abort() signals), so an abort
 *        ends the wait at once.
 * @param seconds How long.
 * @return false if aborted.
 */
bool LiveStreamIOHandler::waitOrAbort(double seconds)
{
    std::unique_lock<std::mutex> lock(m_buf_mutex);
    m_space_cv.wait_for(lock, std::chrono::milliseconds(static_cast<int64_t>(seconds * 1000)),
                        [this] { return m_abort.load(); });
    return !m_abort;
}

/**
 * @brief The offset just past the newest byte; caller holds m_buf_mutex.
 * @return It.
 */
filesize_t LiveStreamIOHandler::audioEnd_unlocked() const
{
    return m_data_start + static_cast<filesize_t>(m_data.size());
}

/**
 * @brief Copy audio from the buffer, waiting for the producer when the
 *        reader has caught up, until the request is filled, the producer ends
 *        or the handler is stopped. A producer that failed sets error EIO.
 * @param buffer Destination.
 * @param size Element size.
 * @param count Elements wanted.
 * @return Whole elements read.
 */
size_t LiveStreamIOHandler::read_unlocked(void* buffer, size_t size, size_t count)
{
    updateErrorState(0);
    if (m_closed) {
        updateErrorState(EBADF);
        return 0;
    }
    if (!buffer) {
        updateErrorState(EINVAL);
        return 0;
    }
    if (size == 0 || count == 0) return 0;
    if (count > std::numeric_limits<size_t>::max() / size) {
        updateErrorState(EINVAL);
        return 0;
    }

    const size_t wanted = size * count;
    uint8_t* out = static_cast<uint8_t*>(buffer);
    size_t got = 0;

    std::unique_lock<std::mutex> lock(m_buf_mutex);
    while (got < wanted) {
        const filesize_t end = audioEnd_unlocked();
        if (m_read_pos < end) {
            const size_t offset = static_cast<size_t>(m_read_pos - m_data_start);
            const size_t n = std::min(wanted - got, m_data.size() - offset);
            std::memcpy(out + got, m_data.data() + offset, n);
            got += n;
            m_read_pos += static_cast<filesize_t>(n);
            m_space_cv.notify_all();
            continue;
        }
        if (m_finished || m_abort) {
            if (m_failed) updateErrorState(EIO, m_error_text);
            updateEofState(true);
            break;
        }
        m_data_cv.wait(lock, [this] {
            return m_finished || m_abort || m_read_pos < audioEnd_unlocked();
        });
    }
    m_position = m_read_pos;
    return got / size;
}

/**
 * @brief Move the reader within [oldest kept byte, newest + FORWARD_SEEK_BYTES].
 *        A target past what has arrived is reached by later reads waiting.
 * @param offset Offset from @p whence.
 * @param whence SEEK_SET or SEEK_CUR; SEEK_END is refused.
 * @return 0, or -1 with error ESPIPE.
 */
int LiveStreamIOHandler::seek_unlocked(filesize_t offset, int whence)
{
    updateErrorState(0);
    if (m_closed) {
        updateErrorState(EBADF);
        return -1;
    }

    std::lock_guard<std::mutex> lock(m_buf_mutex);
    filesize_t target;
    switch (whence) {
        case SEEK_SET: target = offset; break;
        case SEEK_CUR:
            // Out of filesize_t's range is out of the window anyway.
            if ((offset > 0 && m_read_pos > std::numeric_limits<filesize_t>::max() - offset) ||
                (offset < 0 && m_read_pos < std::numeric_limits<filesize_t>::min() - offset)) {
                updateErrorState(ESPIPE);
                return -1;
            }
            target = m_read_pos + offset;
            break;
        default:
            // No end to seek from.
            updateErrorState(ESPIPE);
            return -1;
    }
    const filesize_t end = audioEnd_unlocked();
    if (target < m_data_start || target > end + static_cast<filesize_t>(FORWARD_SEEK_BYTES)) {
        Debug::log("http", "LiveStreamIOHandler: seek to ", target, " refused (buffer holds ",
                   m_data_start, "-", end, ")");
        updateErrorState(ESPIPE);
        return -1;
    }
    m_read_pos = target;
    m_position = target;
    // Past what has arrived is not the end: later reads wait for it.
    updateEofState(m_finished && target >= end);
    m_space_cv.notify_all();
    return 0;
}

/**
 * @brief The reader's offset into the audio.
 * @return The offset, or -1 (error EBADF) once closed.
 */
filesize_t LiveStreamIOHandler::tell_unlocked()
{
    if (m_closed) {
        updateErrorState(EBADF);
        return -1;
    }
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    return m_read_pos;
}

/**
 * @brief Stop the producer, which wakes a waiting read so it releases the
 *        operation lock, then close through the base class.
 * @return What IOHandler::close() returns.
 */
int LiveStreamIOHandler::close()
{
    stopWorker();
    return IOHandler::close();
}

/**
 * @brief Mark the handler closed and at EOF; the producer is already stopped.
 * @return 0.
 */
int LiveStreamIOHandler::close_unlocked()
{
    updateClosedState(true);
    updateEofState(true);
    return 0;
}

/**
 * @brief Closed, or the producer has ended and the reader has had every byte.
 * @return true at the end of the stream.
 */
bool LiveStreamIOHandler::eof()
{
    if (m_closed) return true;
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    /*
     * Aborted counts as over at once, as read() treats it: the producer may
     * not have said it finished yet.
     */
    return (m_finished || m_abort) && m_read_pos >= audioEnd_unlocked();
}

/**
 * @brief A stream's size is unknown.
 * @return -1, always.
 */
filesize_t LiveStreamIOHandler::getFileSize()
{
    return -1;
}

/**
 * @brief These handlers read internet radio.
 * @return true.
 */
bool LiveStreamIOHandler::isLiveStream() const
{
    return true;
}

/**
 * @brief The newest recorded title that starts at or before @p offset.
 * @param offset Audio offset, as tell() counts it.
 * @return That title, or "" if none starts that early.
 */
std::string LiveStreamIOHandler::getStreamTitleAt(filesize_t offset) const
{
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    for (auto it = m_titles.rbegin(); it != m_titles.rend(); ++it) {
        if (it->first <= offset) return it->second;
    }
    return std::string();
}

/**
 * @brief Titles recorded since the stream opened, dropped ones included.
 * @return The count.
 */
uint64_t LiveStreamIOHandler::getStreamTitleCount() const
{
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    return m_title_count;
}

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3

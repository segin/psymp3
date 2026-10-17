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

/** @brief Where a fetch() puts its body, and how much it may hold. */
struct FetchSink {
    std::string* body;   ///< the body so far
    size_t max_bytes;    ///< 0: no limit
    bool too_large;      ///< set when the limit was passed
};

/**
 * @brief libcurl write callback: append to the sink's body, refusing past
 *        its limit (which ends the transfer).
 * @param data Bytes.
 * @param size Always 1.
 * @param nmemb Their count.
 * @param out The FetchSink.
 * @return Bytes taken; fewer ends the transfer.
 */
size_t fetchWrite(char* data, size_t size, size_t nmemb, void* out)
{
    auto* sink = static_cast<FetchSink*>(out);
    const size_t len = size * nmemb;
    if (sink->max_bytes != 0 && sink->body->size() + len > sink->max_bytes) {
        sink->too_large = true;
        return 0;
    }
    sink->body->append(data, len);
    return len;
}

/**
 * @brief libcurl progress callback: stop when the handler is aborted.
 * @param abort_flag The handler's abort flag.
 * @return Non-zero to abort.
 */
int fetchAbortCheck(void* abort_flag, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return static_cast<const std::atomic<bool>*>(abort_flag)->load() ? 1 : 0;
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
    // The producer starts the artwork thread, so it is joined after.
    if (m_art_worker.joinable()) {
        m_art_worker.join();
    }
}

/**
 * @brief Flag the abort and wake the producer (waiting for room) and any
 *        reader or constructor (waiting for data). Does not wait.
 */
void LiveStreamIOHandler::abort()
{
    m_abort = true;
    {
        /*
         * Wake a transfer waiting on the network now: shutting its sockets
         * down makes their waits return, where curl would otherwise notice
         * the abort only at its next progress tick, a second away on a
         * silent connection. They stay open -- curl closes them.
         */
        std::lock_guard<std::mutex> lock(m_sockets_mutex);
        for (curl_socket_t socket : m_sockets) {
#ifdef _WIN32
            ::shutdown(socket, SD_BOTH);
#else
            ::shutdown(socket, SHUT_RDWR);
#endif
        }
    }
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    m_space_cv.notify_all();
    m_data_cv.notify_all();
    m_art_cv.notify_all();
}

/**
 * @brief Route @p curl's socket opening and closing through this handler.
 * @param curl The transfer.
 */
void LiveStreamIOHandler::trackSockets(CURL* curl)
{
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, &LiveStreamIOHandler::openSocketCallback);
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, this);
    curl_easy_setopt(curl, CURLOPT_CLOSESOCKETFUNCTION, &LiveStreamIOHandler::closeSocketCallback);
    curl_easy_setopt(curl, CURLOPT_CLOSESOCKETDATA, this);
}

/**
 * @brief Open a socket as curl asks, and keep it for abort(). One opened
 *        after the abort is shut down at once.
 * @param self The handler.
 * @param purpose What it is for (unused: every socket is tracked).
 * @param address Family, type and protocol to open.
 * @return The socket, or CURL_SOCKET_BAD.
 */
curl_socket_t LiveStreamIOHandler::openSocketCallback(void* self, curlsocktype, struct curl_sockaddr* address)
{
    auto* handler = static_cast<LiveStreamIOHandler*>(self);
    const curl_socket_t socket = ::socket(address->family, address->socktype, address->protocol);
    if (socket == CURL_SOCKET_BAD) return socket;
    std::lock_guard<std::mutex> lock(handler->m_sockets_mutex);
    handler->m_sockets.insert(socket);
    if (handler->m_abort) {
#ifdef _WIN32
        ::shutdown(socket, SD_BOTH);
#else
        ::shutdown(socket, SHUT_RDWR);
#endif
    }
    return socket;
}

/**
 * @brief Forget and close a socket curl is done with.
 * @param self The handler.
 * @param socket The socket.
 * @return 0.
 */
int LiveStreamIOHandler::closeSocketCallback(void* self, curl_socket_t socket)
{
    auto* handler = static_cast<LiveStreamIOHandler*>(self);
    {
        std::lock_guard<std::mutex> lock(handler->m_sockets_mutex);
        handler->m_sockets.erase(socket);
    }
#ifdef _WIN32
    return ::closesocket(socket);
#else
    return ::close(socket);
#endif
}

/**
 * @brief One complete GET with the same TLS and redirect rules as the other
 *        HTTP handlers, a REQUEST_SECONDS limit, an abort check and tracked
 *        sockets.
 * @param url URL.
 * @param body Body.
 * @param content_type Content-Type out, or null.
 * @param effective_url URL after redirects out, or null.
 * @param error Why it failed.
 * @param max_bytes Longest body taken; 0 for any.
 * @return true for 2xx.
 */
bool LiveStreamIOHandler::fetch(const std::string& url, std::string& body, std::string* content_type,
                                std::string* effective_url, std::string& error, size_t max_bytes)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        error = "curl_easy_init failed";
        return false;
    }
    FetchSink sink{&body, max_bytes, false};
    char error_buffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &fetchWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    trackSockets(curl); // so abort() ends a wait on a silent server at once
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &fetchAbortCheck);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &m_abort);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, REQUEST_SECONDS);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
#if defined(_WIN32) && LIBCURL_VERSION_NUM >= 0x074700 /* 7.71.0 */
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
#endif
#if LIBCURL_VERSION_NUM >= 0x075500 /* 7.85.0 */
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl, CURLOPT_USERAGENT, PSYMP3_USER_AGENT);

    const CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (content_type) {
        char* type = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &type);
        std::string t = type ? type : "";
        t = t.substr(0, t.find(';'));
        const size_t b = t.find_first_not_of(" \t");
        t = b == std::string::npos ? std::string() : t.substr(b, t.find_last_not_of(" \t") - b + 1);
        std::transform(t.begin(), t.end(), t.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        *content_type = t;
    }
    if (effective_url) {
        char* eff = nullptr;
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &eff);
        *effective_url = eff ? eff : "";
    }
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        error = m_abort ? std::string("aborted")
              : sink.too_large ? "larger than " + std::to_string(max_bytes) + " bytes"
              : error_buffer[0] ? std::string(error_buffer) : std::string(curl_easy_strerror(rc));
        return false;
    }
    if (status < 200 || status >= 300) {
        error = "server answered " + std::to_string(status);
        return false;
    }
    /*
     * Not noteActivity(): a playlist that answers but never grows is a stall.
     * Audio arriving is activity, which appendAudio() notes.
     */
    return true;
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
    // A connection parked for reuse must not outlive the registry at exit.
    discardParkedHandlers();
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
 * @brief Place a picture: the current end by default, except that the
 *        stream's first applies from where its first title does (from the
 *        start, as that title does), never before the last title or picture.
 * @param at The requested offset, or -1.
 * @return The offset to use.
 */
filesize_t LiveStreamIOHandler::placeMetadata_unlocked(filesize_t at) const
{
    if (at < 0) {
        at = !m_pictures.empty() || m_title_count > 1 ? audioEnd_unlocked()
           : m_titles.empty() ? 0 : m_titles.back().first;
    }
    if (!m_titles.empty() && at < m_titles.back().first) at = m_titles.back().first;
    if (!m_pictures.empty() && at < m_pictures.back().at) at = m_pictures.back().at;
    return at;
}

/**
 * @brief Whether a title starts in (@p at, @p upto].
 * @param at A picture's offset.
 * @param upto The furthest offset that matters.
 * @return true if one does.
 */
bool LiveStreamIOHandler::titleStartsAfter_unlocked(filesize_t at, filesize_t upto) const
{
    for (auto it = m_titles.rbegin(); it != m_titles.rend(); ++it) {
        if (it->first <= at) return false;
        if (it->first <= upto) return true;
    }
    return false;
}

/**
 * @brief Keep @p entry, and at most MAX_PICTURES in all.
 * @param entry The entry.
 */
void LiveStreamIOHandler::pushPicture_unlocked(PictureEntry entry)
{
    m_pictures.push_back(std::move(entry));
    if (m_pictures.size() > MAX_PICTURES) m_pictures.pop_front();
}

/**
 * @brief Record a picture sent whole, unless it is the one already in
 *        effect for the latest song.
 * @param picture The picture, or null for none.
 * @param where Its start, or -1.
 */
void LiveStreamIOHandler::recordPicture(std::shared_ptr<const PsyMP3::Tag::Picture> picture, filesize_t where)
{
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    const filesize_t at = placeMetadata_unlocked(where);
    if (!m_pictures.empty() && !titleStartsAfter_unlocked(m_pictures.back().at, at)) {
        const PictureEntry& last = m_pictures.back();
        const bool same = last.url.empty() &&
                          (last.picture == picture ||
                           (last.picture && picture && last.picture->data == picture->data));
        if (same) return; // sent again with each segment, say
    }
    pushPicture_unlocked(PictureEntry{at, std::string(), std::move(picture)});
    Debug::log("http", "LiveStreamIOHandler: picture at ", at);
}

/**
 * @brief Record a picture by URL: from the cache when it has been fetched,
 *        otherwise queued for the artwork thread (started on first use).
 * @param url The URL.
 * @param where Its start, or -1.
 */
void LiveStreamIOHandler::recordArtworkUrl(const std::string& url, filesize_t where)
{
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    const filesize_t at = placeMetadata_unlocked(where);
    if (!m_pictures.empty() && m_pictures.back().url == url && !titleStartsAfter_unlocked(m_pictures.back().at, at)) {
        return; // the same picture, still for the same song
    }
    PictureEntry entry{at, url, nullptr};
    bool cached = false;
    for (const auto& known : m_art_cache) {
        if (known.first == url) {
            entry.picture = known.second;
            cached = true;
            break;
        }
    }
    pushPicture_unlocked(std::move(entry));
    Debug::log("http", "LiveStreamIOHandler: picture at ", at, " from ", url, cached ? " (known)" : "");
    if (cached || m_abort) return;
    if (std::find(m_art_queue.begin(), m_art_queue.end(), url) == m_art_queue.end()) {
        m_art_queue.push_back(url);
    }
    if (!m_art_worker.joinable()) {
        m_art_worker = std::thread([this] { runArtworkWorker(); });
    }
    m_art_cv.notify_all();
}

/**
 * @brief Fetch each queued URL in turn, outside the lock; keep what it gave
 *        in the cache and in every entry waiting for it. Ends on abort.
 */
void LiveStreamIOHandler::runArtworkWorker()
{
    std::unique_lock<std::mutex> lock(m_buf_mutex);
    for (;;) {
        m_art_cv.wait(lock, [this] { return m_abort || !m_art_queue.empty(); });
        if (m_abort) return;
        const std::string url = m_art_queue.front();
        lock.unlock();
        std::string body;
        std::string error;
        std::shared_ptr<const PsyMP3::Tag::Picture> picture;
        if (fetch(url, body, nullptr, nullptr, error, MAX_PICTURE_BYTES)) {
            picture = pictureFromBytes(body);
            if (!picture) error = "not an image";
        }
        Debug::log("http", "LiveStreamIOHandler: picture ", url, picture ? " fetched" : " refused: " + error);
        lock.lock();
        m_art_queue.pop_front();
        if (m_abort) return; // an aborted fetch says nothing of the URL
        m_art_cache.emplace_back(url, picture);
        if (m_art_cache.size() > ARTWORK_CACHE_SIZE) m_art_cache.pop_front();
        for (PictureEntry& entry : m_pictures) {
            if (entry.url == url) entry.picture = picture;
        }
    }
}

/**
 * @brief A front-cover picture of @p bytes, typed by their own header.
 * @param bytes The data.
 * @return It, or null when they are no image.
 */
std::shared_ptr<const PsyMP3::Tag::Picture> LiveStreamIOHandler::pictureFromBytes(const std::string& bytes)
{
    auto picture = std::make_shared<PsyMP3::Tag::Picture>();
    picture->data.assign(bytes.begin(), bytes.end());
    picture->mime_type = PsyMP3::Tag::ImageUtils::sniffMimeType(picture->data);
    if (picture->mime_type.empty()) return nullptr;
    picture->type = PsyMP3::Tag::PictureType::FrontCover;
    PsyMP3::Tag::ImageUtils::extractDimensions(*picture);
    return picture;
}

/**
 * @brief The newest picture at or before @p offset, unless a title has
 *        started since it.
 * @param offset Offset into the audio.
 * @return The picture, or null.
 */
std::shared_ptr<const PsyMP3::Tag::Picture> LiveStreamIOHandler::getStreamPictureAt(filesize_t offset) const
{
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    for (auto it = m_pictures.rbegin(); it != m_pictures.rend(); ++it) {
        if (it->at <= offset) {
            return titleStartsAfter_unlocked(it->at, offset) ? nullptr : it->picture;
        }
    }
    return nullptr;
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

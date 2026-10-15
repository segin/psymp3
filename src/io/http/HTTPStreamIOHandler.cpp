/*
 * HTTPStreamIOHandler.cpp - I/O handler for HTTP streams of no fixed length
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
int64_t steadyMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

/**
 * @brief Lower-case the ASCII letters of a header name or value. Bytes
 *        outside A-Z are left alone, so no locale is involved.
 * @param s The text.
 * @return @p s with A-Z turned into a-z.
 */
std::string toLower(std::string s)
{
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

/**
 * @brief Strip leading and trailing spaces, tabs and line endings.
 * @param s The text.
 * @return @p s without surrounding whitespace; "" if it was all whitespace.
 */
std::string trim(const std::string& s)
{
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

} // namespace

/**
 * @brief Start the transfer on the worker thread and wait for the response
 *        head; throw if there is none or it is an error status.
 * @param url The http(s) URL to stream.
 * @throws InvalidMediaException on connection failure, an error status, or
 *         no response before the worker's timeouts.
 */
HTTPStreamIOHandler::HTTPStreamIOHandler(const std::string& url)
    : m_url(url)
{
    Debug::log("http", "HTTPStreamIOHandler: connecting to ", url);
    m_last_data_ms = steadyMs();
    m_worker = std::thread(&HTTPStreamIOHandler::runTransfer, this);

    // Wait for the response head. The worker's own stall and connect
    // timeouts bound this: it always ends up with headers or finished.
    std::unique_lock<std::mutex> lock(m_buf_mutex);
    m_data_cv.wait(lock, [this] { return m_headers_ready || m_finished; });

    std::string error;
    if (!m_headers_ready) {
        error = m_error_text.empty() ? std::string("no response") : m_error_text;
    } else if (m_status >= 400) {
        error = "server answered " + std::to_string(m_status);
    }
    const int status = m_status;
    lock.unlock();
    if (!error.empty()) {
        stopTransfer();
        Debug::log("http", "HTTPStreamIOHandler: ", url, " failed: ", error);
        throw InvalidMediaException("Cannot open " + url + ": " + error);
    }
    Debug::log("http", "HTTPStreamIOHandler: ", url, " status ", status,
               ", type '", getHeader("content-type"), "', length ", m_content_length,
               m_icy ? ", ICY" : "");
}

/**
 * @brief Stop the transfer and join the worker thread.
 */
HTTPStreamIOHandler::~HTTPStreamIOHandler()
{
    stopTransfer();
}

/**
 * @brief Flag the abort, wake the worker (waiting for room) and any reader
 *        (waiting for data), and join the worker. libcurl notices the flag at
 *        its next callback, at most about a second later on an idle line.
 */
void HTTPStreamIOHandler::stopTransfer()
{
    m_abort = true;
    {
        std::lock_guard<std::mutex> lock(m_buf_mutex);
        m_space_cv.notify_all();
        m_data_cv.notify_all();
    }
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

/**
 * @brief Worker thread: set up one libcurl GET (redirects followed, HTTP/0.9
 *        allowed for SHOUTcast v1, ICY metadata requested, no overall
 *        timeout) and run it to its end, then record how it ended and wake
 *        the reader and the constructor.
 */
void HTTPStreamIOHandler::runTransfer()
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::lock_guard<std::mutex> lock(m_buf_mutex);
        m_failed = true;
        m_finished = true;
        m_error_text = "curl_easy_init failed";
        m_data_cv.notify_all();
        return;
    }

    char error_buffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &HTTPStreamIOHandler::headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &HTTPStreamIOHandler::writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &HTTPStreamIOHandler::progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    // No overall CURLOPT_TIMEOUT: the body is meant to go on indefinitely.
    // A dead connection is caught by the stall check in onProgress().
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(CONNECT_SECONDS));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
#if defined(_WIN32) && LIBCURL_VERSION_NUM >= 0x074700 /* 7.71.0 */
    // As in HTTPClient: OpenSSL has no CA bundle on Windows; use the system store.
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
#endif
#if LIBCURL_VERSION_NUM >= 0x075500 /* 7.85.0 */
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
#if LIBCURL_VERSION_NUM >= 0x074000 /* 7.64.0 */
    // SHOUTcast v1 answers "ICY 200 OK", which libcurl only lets through as
    // an HTTP/0.9 body; consumeIcyV1Header() then takes the headers off it.
    curl_easy_setopt(curl, CURLOPT_HTTP09_ALLOWED, 1L);
#endif
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PsyMP3/3.0");
    // Ask for the song titles. A server that has them answers with an
    // icy-metaint header and interleaves them with the audio; deliverBody()
    // takes them out again. One that has none ignores the request.
    struct curl_slist* request_headers = curl_slist_append(nullptr, "Icy-MetaData: 1");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_headers);

    const CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(request_headers);

    std::lock_guard<std::mutex> lock(m_buf_mutex);
    if (rc != CURLE_OK && !m_abort) {
        m_failed = true;
        m_error_text = error_buffer[0] ? std::string(error_buffer) : std::string(curl_easy_strerror(rc));
        Debug::log("http", "HTTPStreamIOHandler: ", m_url, " transfer ended: ", m_error_text);
    } else {
        Debug::log("http", "HTTPStreamIOHandler: ", m_url, " transfer ended after ",
                   m_data_start + static_cast<filesize_t>(m_data.size()), " bytes");
    }
    // An HTTP/0.9 reply too short to hold a whole ICY header block still has
    // to release the constructor.
    if (!m_headers_ready && !m_failed && !m_abort) {
        if (!m_icy_v1_header.empty()) {
            m_data.insert(m_data.end(), m_icy_v1_header.begin(), m_icy_v1_header.end());
            m_icy_v1_header.clear();
        }
        markHeadersReady_unlocked();
    }
    m_finished = true;
    m_data_cv.notify_all();
}

/**
 * @brief libcurl header callback: pass the line to onHeader().
 * @param data The header line.
 * @param size Always 1.
 * @param nmemb Its length.
 * @param self The handler.
 * @return What onHeader() returns.
 */
size_t HTTPStreamIOHandler::headerCallback(char* data, size_t size, size_t nmemb, void* self)
{
    return static_cast<HTTPStreamIOHandler*>(self)->onHeader(data, size * nmemb);
}

/**
 * @brief libcurl write callback: pass the body bytes to onBody().
 * @param data Body bytes.
 * @param size Always 1.
 * @param nmemb Their count.
 * @param self The handler.
 * @return What onBody() returns.
 */
size_t HTTPStreamIOHandler::writeCallback(char* data, size_t size, size_t nmemb, void* self)
{
    return static_cast<HTTPStreamIOHandler*>(self)->onBody(data, size * nmemb);
}

/**
 * @brief libcurl progress callback: ask onProgress() whether to go on.
 * @param self The handler.
 * @return 0 to continue, 1 to abort.
 */
int HTTPStreamIOHandler::progressCallback(void* self, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return static_cast<HTTPStreamIOHandler*>(self)->onProgress() ? 0 : 1;
}

/**
 * @brief Take one header line: a status line resets the headers and records
 *        the status; any other line is stored by addHeaderLine_unlocked().
 * @param data The line, line ending included.
 * @param len Its length.
 * @return @p len, or 0 when aborting.
 */
size_t HTTPStreamIOHandler::onHeader(const char* data, size_t len)
{
    if (m_abort) return 0;
    const std::string line(data, len);
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    if (line.compare(0, 5, "HTTP/") == 0) {
        // A new response begins (the first, or the next after a redirect):
        // only the last one's headers describe the body.
        m_headers.clear();
        const size_t sp = line.find(' ');
        m_status = (sp == std::string::npos) ? 0 : std::atoi(line.c_str() + sp + 1);
    } else {
        addHeaderLine_unlocked(line);
    }
    return len;
}

/**
 * @brief Store a "Name: value" line under its trimmed, lower-cased name; a
 *        later line of the same name replaces an earlier one. Caller holds
 *        m_buf_mutex.
 * @param line The header line.
 */
void HTTPStreamIOHandler::addHeaderLine_unlocked(const std::string& line)
{
    const size_t colon = line.find(':');
    if (colon == std::string::npos) return;
    const std::string name = toLower(trim(line.substr(0, colon)));
    if (name.empty()) return;
    m_headers[name] = trim(line.substr(colon + 1));
}

/**
 * @brief Settle what the headers say -- Content-Length, whether the reply is
 *        ICY, the icy-metaint framing -- set the metadata state to start
 *        with audio, and wake the constructor. Once only. Caller holds
 *        m_buf_mutex.
 */
void HTTPStreamIOHandler::markHeadersReady_unlocked()
{
    if (m_headers_ready) return;
    m_content_length = -1;
    auto it = m_headers.find("content-length");
    if (it != m_headers.end()) {
        char* end = nullptr;
        const long long v = std::strtoll(it->second.c_str(), &end, 10);
        if (end != it->second.c_str() && v >= 0) {
            m_content_length = v;
        }
    }
    for (const auto& h : m_headers) {
        if (h.first.compare(0, 4, "icy-") == 0) {
            m_icy = true;
            break;
        }
    }
    m_metaint = 0;
    it = m_headers.find("icy-metaint");
    if (it != m_headers.end()) {
        char* end = nullptr;
        const long long v = std::strtoll(it->second.c_str(), &end, 10);
        if (end != it->second.c_str() && v > 0 && v <= MAX_METAINT) {
            m_metaint = v;
        } else {
            Debug::log("http", "HTTPStreamIOHandler: ignoring icy-metaint '", it->second, "'");
        }
    }
    m_audio_left = m_metaint;
    m_meta_left = -1;
    m_headers_ready = true;
    m_data_cv.notify_all();
}

/**
 * @brief Take a chunk of body: while an HTTP/0.9 reply's ICY head is still
 *        coming, collect it; once the head is known, pass the rest on to
 *        deliverBody(). Resets the stall timer.
 * @param data Body bytes.
 * @param len Their count.
 * @return @p len, or 0 to abort (stopping, a bad ICY head, or aborted
 *         while waiting for the reader).
 */
size_t HTTPStreamIOHandler::onBody(const char* data, size_t len)
{
    if (m_abort) return 0;
    m_last_data_ms = steadyMs();

    size_t used = 0;
    {
        std::lock_guard<std::mutex> lock(m_buf_mutex);
        // Status 0 means no HTTP status line: an HTTP/0.9 reply, which is how
        // a SHOUTcast v1 "ICY 200 OK" header block reaches us.
        if (!m_headers_ready && m_status == 0 && !m_icy_v1_done) {
            used = consumeIcyV1Header(data, len);
            if (!m_icy_v1_done) {
                return m_failed ? 0 : len; // block not finished yet
            }
        }
        if (!m_headers_ready) {
            markHeadersReady_unlocked();
        }
    }
    if (used < len && !deliverBody(data + used, len - used)) {
        return 0;
    }
    return len;
}

/**
 * @brief Walk the icy-metaint framing (audio, length byte, metadata, audio,
 *        ...) across chunk boundaries: audio to appendBody(), each complete
 *        metadata block to onMetadataBlock(). Without icy-metaint, all of it
 *        is audio. Worker thread only.
 * @param data Body bytes after any ICY v1 head.
 * @param len Their count.
 * @return false if aborted while waiting for the reader.
 */
bool HTTPStreamIOHandler::deliverBody(const char* data, size_t len)
{
    if (m_metaint == 0) {
        return appendBody(data, len);
    }
    while (len > 0) {
        if (m_audio_left > 0) {
            const size_t n = static_cast<size_t>(std::min<int64_t>(m_audio_left, static_cast<int64_t>(len)));
            if (!appendBody(data, n)) return false;
            data += n;
            len -= n;
            m_audio_left -= static_cast<int64_t>(n);
            continue;
        }
        if (m_meta_left < 0) {
            // The length byte, in units of 16 bytes; 0 means no block this time.
            m_meta_left = static_cast<int64_t>(static_cast<uint8_t>(*data)) * 16;
            ++data;
            --len;
            m_meta_block.clear();
        } else {
            const size_t n = static_cast<size_t>(std::min<int64_t>(m_meta_left, static_cast<int64_t>(len)));
            m_meta_block.append(data, n);
            data += n;
            len -= n;
            m_meta_left -= static_cast<int64_t>(n);
        }
        if (m_meta_left == 0) {
            if (!m_meta_block.empty()) onMetadataBlock(m_meta_block);
            m_meta_left = -1;
            m_audio_left = m_metaint;
        }
    }
    return true;
}

/**
 * @brief Decode a block's StreamTitle and, if it differs from the last one,
 *        record it at the current end of the audio (the first title at 0),
 *        keeping at most MAX_TITLES.
 * @param block The metadata block, NUL padding included.
 */
void HTTPStreamIOHandler::onMetadataBlock(const std::string& block)
{
    std::string raw;
    if (!parseStreamTitle(block, raw)) return;
    const std::string title = decodeIcyText(raw);

    std::lock_guard<std::mutex> lock(m_buf_mutex);
    if (!m_titles.empty() && m_titles.back().second == title) return;
    // The audio received so far is what plays before this title. The first
    // title comes only after the first icy-metaint bytes of audio, which are
    // the same song: it applies from the start.
    const filesize_t at = m_titles.empty() && m_title_count == 0
        ? 0 : m_data_start + static_cast<filesize_t>(m_data.size());
    m_titles.emplace_back(at, title);
    if (m_titles.size() > MAX_TITLES) m_titles.pop_front();
    ++m_title_count;
    Debug::log("http", "HTTPStreamIOHandler: StreamTitle at ", at, ": '", title, "'");
}

/**
 * @brief Extract the raw StreamTitle value: from after "StreamTitle='" to the
 *        next "';", or failing that the last quote, the padding or the end.
 * @param block A metadata block.
 * @param title Receives the value, undecoded.
 * @return true if the block has a StreamTitle.
 */
bool HTTPStreamIOHandler::parseStreamTitle(const std::string& block, std::string& title)
{
    static const std::string key = "StreamTitle='";
    const size_t start = block.find(key);
    if (start == std::string::npos) return false;
    const size_t from = start + key.size();
    size_t end = block.find("';", from);
    if (end == std::string::npos) {
        // Unterminated: up to the last quote, or to the padding.
        end = block.rfind('\'');
        if (end == std::string::npos || end < from) {
            end = block.find('\0', from);
            if (end == std::string::npos) end = block.size();
        }
    }
    title = block.substr(from, end - from);
    return true;
}

/**
 * @brief Text from an ICY server as UTF-8: kept if it is valid UTF-8,
 *        otherwise converted from Latin-1.
 * @param raw The bytes as sent.
 * @return UTF-8 text.
 */
std::string HTTPStreamIOHandler::decodeIcyText(const std::string& raw)
{
    if (Core::Utility::UTF8Util::isValid(raw)) return raw;
    return Core::Utility::UTF8Util::fromLatin1(raw);
}

/**
 * @brief The icy-name header, decoded.
 * @return The station's name, or "".
 */
std::string HTTPStreamIOHandler::getStationName() const
{
    return decodeIcyText(getHeader("icy-name"));
}

/**
 * @brief The newest recorded title that starts at or before @p offset.
 * @param offset Audio offset, as tell() counts it.
 * @return That title, or "" if none starts that early.
 */
std::string HTTPStreamIOHandler::getStreamTitleAt(filesize_t offset) const
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
uint64_t HTTPStreamIOHandler::getStreamTitleCount() const
{
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    return m_title_count;
}

/**
 * @brief Gather an HTTP/0.9 reply's first bytes until it is clear whether
 *        they are an "ICY nnn" head; if so, parse the head up to its blank
 *        line (CRLF or bare LF) into the status and headers, and if not,
 *        hand the bytes on as body. Caller holds m_buf_mutex.
 * @param data This chunk.
 * @param len Its length.
 * @return Bytes of this chunk that were head; the rest is body.
 */
size_t HTTPStreamIOHandler::consumeIcyV1Header(const char* data, size_t len)
{
    // Collect until the blank line that ends the block. Decide early whether
    // this is ICY at all: a plain HTTP/0.9 body is passed on untouched.
    const size_t before = m_icy_v1_header.size();
    m_icy_v1_header.append(data, len);
    if (m_icy_v1_header.size() >= 4 && m_icy_v1_header.compare(0, 4, "ICY ") != 0) {
        // Not ICY: everything collected so far is body.
        m_data.insert(m_data.end(), m_icy_v1_header.begin(), m_icy_v1_header.begin() + before);
        m_icy_v1_header.clear();
        m_icy_v1_done = true;
        return 0;
    }
    size_t end = m_icy_v1_header.find("\r\n\r\n");
    size_t term = 4;
    if (end == std::string::npos) {
        end = m_icy_v1_header.find("\n\n");
        term = 2;
    }
    if (end == std::string::npos) {
        if (m_icy_v1_header.size() > MAX_ICY_HEADER_BYTES) {
            m_failed = true;
            m_error_text = "ICY header block too long";
        }
        return len;
    }

    const std::string block = m_icy_v1_header.substr(0, end);
    const size_t block_end = end + term;  // offset of the first body byte
    m_icy_v1_header.clear();
    m_icy_v1_done = true;

    size_t pos = 0;
    bool first = true;
    while (pos <= block.size()) {
        size_t nl = block.find('\n', pos);
        if (nl == std::string::npos) nl = block.size();
        const std::string line = trim(block.substr(pos, nl - pos));
        if (first) {
            // "ICY 200 OK"
            m_status = std::atoi(line.c_str() + 4);
            first = false;
        } else {
            addHeaderLine_unlocked(line);
        }
        pos = nl + 1;
    }
    Debug::log("http", "HTTPStreamIOHandler: SHOUTcast v1 reply, status ", m_status);
    // Bytes of this chunk past the block are body. block_end counts from the
    // start of everything collected, of which `before` came in earlier calls.
    return block_end - before;
}

/**
 * @brief Add audio to the buffer: first wait while the reader is
 *        READ_AHEAD_BYTES behind, then trim what lies more than
 *        HISTORY_BYTES behind it (in large steps), append, and wake readers.
 * @param data Audio bytes.
 * @param len Their count.
 * @return false if aborted while waiting.
 */
bool HTTPStreamIOHandler::appendBody(const char* data, size_t len)
{
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
        m_last_data_ms = steadyMs();
    }
    if (m_abort) return false;

    // Drop what is older than HISTORY_BYTES behind the reader, in large steps
    // so the front of the vector is not shuffled for every chunk.
    const filesize_t keep_from = m_read_pos - static_cast<filesize_t>(HISTORY_BYTES);
    if (keep_from - m_data_start >= static_cast<filesize_t>(HISTORY_BYTES)) {
        const size_t drop = static_cast<size_t>(keep_from - m_data_start);
        m_data.erase(m_data.begin(), m_data.begin() + static_cast<std::ptrdiff_t>(drop));
        m_data_start += static_cast<filesize_t>(drop);
    }
    m_data.insert(m_data.end(), data, data + len);
    m_data_cv.notify_all();
    return true;
}

/**
 * @brief Decide whether the transfer goes on: not when stopping, and not
 *        after STALL_SECONDS without a byte unless the worker was waiting on
 *        the reader, which is not the server's fault.
 * @return true to continue.
 */
bool HTTPStreamIOHandler::onProgress()
{
    if (m_abort) return false;
    if (m_waiting_for_reader) return true;
    if (steadyMs() - m_last_data_ms > static_cast<int64_t>(STALL_SECONDS) * 1000) {
        std::lock_guard<std::mutex> lock(m_buf_mutex);
        m_error_text = "no data for " + std::to_string(STALL_SECONDS) + " seconds";
        Debug::log("http", "HTTPStreamIOHandler: ", m_url, ": ", m_error_text);
        return false;
    }
    return true;
}

/**
 * @brief Copy audio from the buffer, waiting for the worker when the reader
 *        has caught up, until the request is filled, the transfer ends or
 *        the handler is stopped. A transfer that failed sets error EIO.
 * @param buffer Destination.
 * @param size Element size.
 * @param count Elements wanted.
 * @return Whole elements read.
 */
size_t HTTPStreamIOHandler::read_unlocked(void* buffer, size_t size, size_t count)
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
        const filesize_t end = m_data_start + static_cast<filesize_t>(m_data.size());
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
            return m_finished || m_abort ||
                   m_read_pos < m_data_start + static_cast<filesize_t>(m_data.size());
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
int HTTPStreamIOHandler::seek_unlocked(filesize_t offset, int whence)
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
        case SEEK_CUR: target = m_read_pos + offset; break;
        default:
            // No end to seek from.
            updateErrorState(ESPIPE);
            return -1;
    }
    const filesize_t end = m_data_start + static_cast<filesize_t>(m_data.size());
    if (target < m_data_start || target > end + static_cast<filesize_t>(FORWARD_SEEK_BYTES)) {
        Debug::log("http", "HTTPStreamIOHandler: seek to ", target, " refused (buffer holds ",
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
filesize_t HTTPStreamIOHandler::tell_unlocked()
{
    if (m_closed) {
        updateErrorState(EBADF);
        return -1;
    }
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    return m_read_pos;
}

/**
 * @brief Stop the transfer, which wakes a waiting read so it releases the
 *        operation lock, then close through the base class.
 * @return What IOHandler::close() returns.
 */
int HTTPStreamIOHandler::close()
{
    // A read may be waiting for data with the operation lock held; stop the
    // transfer first so it wakes and lets go.
    stopTransfer();
    return IOHandler::close();
}

/**
 * @brief Mark the handler closed and at EOF; the transfer is already stopped.
 * @return 0.
 */
int HTTPStreamIOHandler::close_unlocked()
{
    updateClosedState(true);
    updateEofState(true);
    return 0;
}

/**
 * @brief Closed, or the transfer has ended and the reader has had every byte.
 * @return true at the end of the stream.
 */
bool HTTPStreamIOHandler::eof()
{
    if (m_closed) return true;
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    return m_finished && m_read_pos >= m_data_start + static_cast<filesize_t>(m_data.size());
}

/**
 * @brief A stream's size is unknown.
 * @return -1, always.
 */
filesize_t HTTPStreamIOHandler::getFileSize()
{
    // A stream has no size, even once it has ended.
    return -1;
}

/**
 * @brief Look a response header up by name, ignoring case.
 * @param name Header name.
 * @return Its value as sent (not decoded), or "".
 */
std::string HTTPStreamIOHandler::getHeader(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    auto it = m_headers.find(toLower(name));
    return it == m_headers.end() ? std::string() : it->second;
}

/**
 * @brief Content-Type stripped of parameters and lower-cased.
 * @return E.g. "audio/mpeg", or "" when the server sent none.
 */
std::string HTTPStreamIOHandler::getMimeType() const
{
    std::string type = getHeader("content-type");
    const size_t semi = type.find(';');
    if (semi != std::string::npos) type.resize(semi);
    return toLower(trim(type));
}

namespace {

/**
 * @brief URLs recently found to be files of known length, so that opening one
 *        again (loading a track opens its URL several times: format probe,
 *        demuxer, tags) skips the GET that tells a file from a stream.
 *
 * Entries expire after FILE_URL_TTL, in case the URL starts serving something
 * else; the map is emptied when it reaches FILE_URL_LIMIT entries.
 */
class KnownFileURLs {
public:
    /**
     * @brief Whether @p url was found to be a file within the last FILE_URL_TTL.
     * @param url The URL as it will be requested.
     * @return true to skip the check and open it with HTTPIOHandler.
     */
    static bool contains(const std::string& url)
    {
        std::lock_guard<std::mutex> lock(mutex());
        auto& urls = map();
        auto it = urls.find(url);
        if (it == urls.end()) return false;
        if (std::chrono::steady_clock::now() - it->second > FILE_URL_TTL) {
            urls.erase(it);
            return false;
        }
        return true;
    }

    /**
     * @brief Record that @p url serves a file of known length.
     * @param url The URL.
     */
    static void add(const std::string& url)
    {
        std::lock_guard<std::mutex> lock(mutex());
        auto& urls = map();
        if (urls.size() >= FILE_URL_LIMIT) urls.clear();
        urls[url] = std::chrono::steady_clock::now();
    }

private:
    static constexpr std::chrono::minutes FILE_URL_TTL{10};
    static constexpr size_t FILE_URL_LIMIT = 256;

    /**
     * @brief The URL map, created on first use.
     * @return URL to the time it was found to be a file.
     */
    static std::map<std::string, std::chrono::steady_clock::time_point>& map()
    {
        static std::map<std::string, std::chrono::steady_clock::time_point> urls;
        return urls;
    }

    /**
     * @brief The mutex guarding map(), created on first use.
     * @return The mutex.
     */
    static std::mutex& mutex()
    {
        static std::mutex m;
        return m;
    }
};

} // namespace

/**
 * @brief Open an http(s) URL as a stream or a file: URLs known to be files go
 *        straight to HTTPIOHandler; otherwise one GET decides, a live reply
 *        keeping the HTTPStreamIOHandler that made it.
 * @param url The URL.
 * @return The handler.
 * @throws InvalidMediaException if the URL cannot be opened.
 */
std::unique_ptr<IOHandler> openHTTPIOHandler(const std::string& url)
{
    if (KnownFileURLs::contains(url)) {
        return std::make_unique<HTTPIOHandler>(url);
    }
    auto stream = std::make_unique<HTTPStreamIOHandler>(url);
    if (stream->isLive()) {
        Debug::log("http", "openHTTPIOHandler: ", url, " is a stream");
        return stream;
    }
    // A file of known length: drop this connection and let HTTPIOHandler
    // fetch it with range requests, so it can be seeked.
    const int64_t length = stream->getContentLength();
    stream.reset();
    KnownFileURLs::add(url);
    Debug::log("http", "openHTTPIOHandler: ", url, " is a file of ", length, " bytes");
    return std::make_unique<HTTPIOHandler>(url);
}

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3

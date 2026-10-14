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

HTTPStreamIOHandler::~HTTPStreamIOHandler()
{
    stopTransfer();
}

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

    const CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

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

size_t HTTPStreamIOHandler::headerCallback(char* data, size_t size, size_t nmemb, void* self)
{
    return static_cast<HTTPStreamIOHandler*>(self)->onHeader(data, size * nmemb);
}

size_t HTTPStreamIOHandler::writeCallback(char* data, size_t size, size_t nmemb, void* self)
{
    return static_cast<HTTPStreamIOHandler*>(self)->onBody(data, size * nmemb);
}

int HTTPStreamIOHandler::progressCallback(void* self, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return static_cast<HTTPStreamIOHandler*>(self)->onProgress() ? 0 : 1;
}

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

void HTTPStreamIOHandler::addHeaderLine_unlocked(const std::string& line)
{
    const size_t colon = line.find(':');
    if (colon == std::string::npos) return;
    const std::string name = toLower(trim(line.substr(0, colon)));
    if (name.empty()) return;
    m_headers[name] = trim(line.substr(colon + 1));
}

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
    m_headers_ready = true;
    m_data_cv.notify_all();
}

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
    if (used < len && !appendBody(data + used, len - used)) {
        return 0;
    }
    return len;
}

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

filesize_t HTTPStreamIOHandler::tell_unlocked()
{
    if (m_closed) {
        updateErrorState(EBADF);
        return -1;
    }
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    return m_read_pos;
}

int HTTPStreamIOHandler::close()
{
    // A read may be waiting for data with the operation lock held; stop the
    // transfer first so it wakes and lets go.
    stopTransfer();
    return IOHandler::close();
}

int HTTPStreamIOHandler::close_unlocked()
{
    updateClosedState(true);
    updateEofState(true);
    return 0;
}

bool HTTPStreamIOHandler::eof()
{
    if (m_closed) return true;
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    return m_finished && m_read_pos >= m_data_start + static_cast<filesize_t>(m_data.size());
}

filesize_t HTTPStreamIOHandler::getFileSize()
{
    // A stream has no size, even once it has ended.
    return -1;
}

std::string HTTPStreamIOHandler::getHeader(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    auto it = m_headers.find(toLower(name));
    return it == m_headers.end() ? std::string() : it->second;
}

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

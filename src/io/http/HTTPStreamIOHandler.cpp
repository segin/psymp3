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
    startWorker();

    // Wait for the response head. The worker's own stall and connect
    // timeouts bound this: it always ends up with headers or finished. An
    // abort() ends it at once.
    std::unique_lock<std::mutex> lock(m_buf_mutex);
    m_data_cv.wait(lock, [this] { return m_headers_ready || m_finished || m_abort; });

    std::string error;
    if (m_abort) {
        error = "aborted";
    } else if (!m_headers_ready) {
        error = m_error_text.empty() ? std::string("no response") : m_error_text;
    } else if (m_status >= 400) {
        error = "server answered " + std::to_string(m_status);
    }
    const int status = m_status;
    lock.unlock();
    if (!error.empty()) {
        // The worker runs this class's code, so it must stop before this
        // class's members go; the base's destructor then unregisters.
        stopWorker();
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
    // The worker runs this class's code: stop it while this class is whole.
    stopWorker();
}

/**
 * @brief The producer: set up one libcurl GET (redirects followed, HTTP/0.9
 *        allowed for SHOUTcast v1, ICY metadata requested, no overall
 *        timeout) and run it to its end, then record how it ended and wake
 *        the reader and the constructor.
 */
void HTTPStreamIOHandler::runWorker()
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        finishWorker(true, "curl_easy_init failed");
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

    const bool failed = rc != CURLE_OK && !m_abort;
    std::string error_text;
    {
        std::lock_guard<std::mutex> lock(m_buf_mutex);
        if (failed) {
            error_text = !m_error_text.empty() ? m_error_text
                       : error_buffer[0] ? std::string(error_buffer) : std::string(curl_easy_strerror(rc));
            Debug::log("http", "HTTPStreamIOHandler: ", m_url, " transfer ended: ", error_text);
        } else {
            Debug::log("http", "HTTPStreamIOHandler: ", m_url, " transfer ended after ",
                       audioEnd_unlocked(), " bytes");
        }
        // An HTTP/0.9 reply too short to hold a whole ICY header block still
        // has to release the constructor.
        if (!m_headers_ready && !failed && !m_failed && !m_abort) {
            if (!m_icy_v1_header.empty()) {
                insertAudio_unlocked(m_icy_v1_header.data(), m_icy_v1_header.size());
                m_icy_v1_header.clear();
            }
            markHeadersReady_unlocked();
        }
    }
    finishWorker(failed, error_text);
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
    noteActivity();

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
 *        ...) across chunk boundaries: audio to appendAudio(), each complete
 *        metadata block to onMetadataBlock(). Without icy-metaint, all of it
 *        is audio. Worker thread only.
 * @param data Body bytes after any ICY v1 head.
 * @param len Their count.
 * @return false if aborted while waiting for the reader.
 */
bool HTTPStreamIOHandler::deliverBody(const char* data, size_t len)
{
    if (m_metaint == 0) {
        return appendAudio(data, len);
    }
    while (len > 0) {
        if (m_audio_left > 0) {
            const size_t n = static_cast<size_t>(std::min<int64_t>(m_audio_left, static_cast<int64_t>(len)));
            if (!appendAudio(data, n)) return false;
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
 * @brief Decode a block's StreamTitle and record it (recordTitle() places
 *        it, and ignores a repeat).
 * @param block The metadata block, NUL padding included.
 */
void HTTPStreamIOHandler::onMetadataBlock(const std::string& block)
{
    std::string raw;
    if (!parseStreamTitle(block, raw)) return;
    recordTitle(decodeIcyText(raw));
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
 * @brief Take a StreamTitle apart: iHeart's key="value" attributes (title in
 *        text=, kind in song_spot=) after an optional "Artist -" head, or
 *        else plain "Artist - Title".
 * @param stream_title Decoded title.
 * @return Artist, title and whether it is music.
 */
HTTPStreamIOHandler::TitleParts HTTPStreamIOHandler::splitStreamTitle(const std::string& stream_title)
{
    TitleParts parts;
    const std::string s = trim(stream_title);

    // Attributes begin at the first key=" whose key is a run of letters,
    // digits and underscores at the start or after a space.
    size_t attrs = std::string::npos;
    for (size_t eq = s.find("=\""); eq != std::string::npos; eq = s.find("=\"", eq + 1)) {
        size_t key = eq;
        while (key > 0 && (std::isalnum(static_cast<unsigned char>(s[key - 1])) || s[key - 1] == '_')) {
            --key;
        }
        if (key < eq && (key == 0 || s[key - 1] == ' ')) {
            attrs = key;
            break;
        }
    }

    if (attrs == std::string::npos) {
        const size_t dash = s.find(" - ");
        if (dash == std::string::npos) {
            parts.title = s;
        } else {
            parts.artist = trim(s.substr(0, dash));
            parts.title = trim(s.substr(dash + 3));
        }
        return parts;
    }

    // key="value" pairs; a backslash escapes the next character.
    std::map<std::string, std::string> values;
    size_t pos = attrs;
    while (pos < s.size()) {
        const size_t eq = s.find("=\"", pos);
        if (eq == std::string::npos) break;
        const std::string key = trim(s.substr(pos, eq - pos));
        std::string value;
        size_t i = eq + 2;
        for (; i < s.size() && s[i] != '"'; ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) ++i;
            value += s[i];
        }
        values[key] = value;
        pos = i + 1;
    }

    // What comes before the attributes is "Artist -", or just "-".
    std::string head = trim(s.substr(0, attrs));
    if (!head.empty() && head.back() == '-') {
        head.pop_back();
        head = trim(head);
    }
    parts.artist = head;
    parts.title = trim(values["text"]);
    if (parts.title.empty() && !parts.artist.empty()) {
        std::swap(parts.title, parts.artist);
    }
    auto spot = values.find("song_spot");
    parts.is_music = spot == values.end() || spot->second.empty() || spot->second == "M";
    return parts;
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
        insertAudio_unlocked(m_icy_v1_header.data(), before);
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
 * @brief Decide whether the transfer goes on: not when stopping, and not
 *        after STALL_SECONDS without a byte unless the worker was waiting on
 *        the reader, which is not the server's fault.
 * @return true to continue.
 */
bool HTTPStreamIOHandler::onProgress()
{
    if (m_abort) return false;
    if (stalledFor(STALL_SECONDS)) {
        std::lock_guard<std::mutex> lock(m_buf_mutex);
        m_error_text = "no data for " + std::to_string(STALL_SECONDS) + " seconds";
        Debug::log("http", "HTTPStreamIOHandler: ", m_url, ": ", m_error_text);
        return false;
    }
    return true;
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

namespace {

/// Playlists that point at playlists are followed this deep, no further.
constexpr int MAX_PLAYLIST_HOPS = 4;

/// Most bytes read from a playlist response before giving up on it.
constexpr size_t MAX_PLAYLIST_BYTES = 256 * 1024;

/**
 * @brief The first entry of a plain M3U playlist: its first line that is
 *        neither empty nor a comment.
 * @param text The playlist.
 * @return The entry as written, or "".
 */
std::string firstPlaylistEntry(const std::string& text)
{
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = trim(text.substr(pos, nl - pos));
        pos = nl + 1;
        if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        if (!line.empty() && line[0] != '#') return line;
    }
    return std::string();
}

/**
 * @brief openHTTPIOHandler(), counting the playlists followed to get here.
 * @param url The URL.
 * @param hops Playlists already followed.
 * @return The handler.
 */
std::unique_ptr<IOHandler> openHTTPIOHandlerFollowing(const std::string& url, int hops);

} // namespace

/**
 * @brief Open an http(s) URL as a stream, an HLS station, or a file: URLs
 *        known to be files go straight to HTTPIOHandler; otherwise one GET
 *        decides.
 * @param url The URL.
 * @return The handler.
 * @throws InvalidMediaException if the URL cannot be opened.
 */
std::unique_ptr<IOHandler> openHTTPIOHandler(const std::string& url)
{
    return openHTTPIOHandlerFollowing(url, 0);
}

namespace {

/**
 * @brief The work of openHTTPIOHandler(). A live reply keeps the
 *        HTTPStreamIOHandler that made it; a playlist is read whole -- an HLS
 *        one goes to HLSIOHandler, a plain M3U (what radio sites hand out as
 *        "listen" links) has its first entry opened in turn; a file of known
 *        length goes to HTTPIOHandler.
 * @param url The URL.
 * @param hops Playlists already followed.
 * @return The handler.
 * @throws InvalidMediaException if the URL cannot be opened.
 */
std::unique_ptr<IOHandler> openHTTPIOHandlerFollowing(const std::string& url, int hops)
{
    if (KnownFileURLs::contains(url)) {
        return std::make_unique<HTTPIOHandler>(url);
    }
    auto stream = std::make_unique<HTTPStreamIOHandler>(url);
    if (HLSIOHandler::looksLikeHLS(stream->getMimeType(), url)) {
        std::string text;
        char buf[8192];
        while (text.size() < MAX_PLAYLIST_BYTES) {
            const size_t n = stream->read(buf, 1, sizeof buf);
            if (n == 0) break;
            text.append(buf, n);
        }
        stream.reset();
        if (text.find("#EXT-X-") != std::string::npos) {
            Debug::log("http", "openHTTPIOHandler: ", url, " is an HLS playlist");
            return std::make_unique<HLSIOHandler>(url);
        }
        const std::string entry = firstPlaylistEntry(text);
        if (entry.empty()) {
            throw InvalidMediaException("Cannot open " + url + ": the playlist is empty");
        }
        if (hops >= MAX_PLAYLIST_HOPS) {
            throw InvalidMediaException("Cannot open " + url + ": playlists nested too deep");
        }
        const std::string next = HLSIOHandler::resolveUrl(url, entry);
        Debug::log("http", "openHTTPIOHandler: ", url, " is a playlist; opening its first entry ", next);
        return openHTTPIOHandlerFollowing(next, hops + 1);
    }
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

} // namespace

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3

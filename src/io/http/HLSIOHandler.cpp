/*
 * HLSIOHandler.cpp - I/O handler for HTTP Live Streaming radio
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
 * @brief Strip surrounding spaces, tabs and line endings.
 * @param s The text.
 * @return It, trimmed.
 */
std::string hlsTrim(const std::string& s)
{
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

/**
 * @brief Lower-case ASCII letters.
 * @param s The text.
 * @return It, lower-cased.
 */
std::string hlsLower(std::string s)
{
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

/**
 * @brief Read an HLS attribute list (KEY=VALUE,KEY="quoted, value",...).
 *        Quoted values keep their commas; backslash-escaped quotes inside
 *        them (iHeart writes url="song_spot=\"M\" ...") are unescaped.
 * @param text The list.
 * @return Keys (upper- or lower-case as written) to values.
 */
std::map<std::string, std::string> hlsAttributes(const std::string& text)
{
    std::map<std::string, std::string> out;
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t eq = text.find('=', pos);
        if (eq == std::string::npos) break;
        const std::string key = hlsTrim(text.substr(pos, eq - pos));
        std::string value;
        size_t i = eq + 1;
        if (i < text.size() && text[i] == '"') {
            for (++i; i < text.size() && text[i] != '"'; ++i) {
                if (text[i] == '\\' && i + 1 < text.size()) ++i;
                value += text[i];
            }
            ++i; // closing quote
            const size_t comma = text.find(',', i);
            pos = comma == std::string::npos ? text.size() : comma + 1;
        } else {
            const size_t comma = text.find(',', i);
            value = hlsTrim(text.substr(i, comma == std::string::npos ? std::string::npos : comma - i));
            pos = comma == std::string::npos ? text.size() : comma + 1;
        }
        if (!key.empty()) out[key] = value;
    }
    return out;
}

/**
 * @brief The song an #EXTINF line names, in a form splitStreamTitle() reads:
 *        "Artist - Title", or iHeart's attributes rewritten as
 *        'Artist - text="Title" song_spot="X"' so that station spots are
 *        known for what they are.
 * @param info What follows "#EXTINF:" (duration, then the title part).
 * @return The title, or "" when the line names none.
 */
std::string hlsTitleFromExtinf(const std::string& info)
{
    const size_t comma = info.find(',');
    if (comma == std::string::npos) return std::string();
    const std::string rest = hlsTrim(info.substr(comma + 1));
    if (rest.empty()) return std::string();
    if (rest.find("=\"") == std::string::npos) {
        return rest; // plain "Artist - Title"
    }
    auto attrs = hlsAttributes(rest);
    const std::string title = attrs["title"];
    const std::string artist = attrs["artist"];
    if (title.empty() && artist.empty()) return std::string();
    // iHeart puts the kind of spot inside url="song_spot=\"M\" ...".
    std::string spot;
    const std::string& url = attrs["url"];
    const size_t s = url.find("song_spot=\"");
    if (s != std::string::npos) {
        const size_t from = s + 11;
        const size_t to = url.find('"', from);
        if (to != std::string::npos) spot = url.substr(from, to - from);
    }
    std::string escaped;
    for (char c : title) {
        if (c == '"' || c == '\\') escaped += '\\';
        escaped += c;
    }
    std::string out = artist + " - text=\"" + escaped + "\"";
    if (!spot.empty()) out += " song_spot=\"" + spot + "\"";
    return out;
}

} // namespace

/**
 * @brief Read the playlist line by line: tags, and URI lines after
 *        #EXT-X-STREAM-INF (variants) or #EXTINF (segments).
 * @param text The playlist.
 * @return What it describes.
 */
HLSIOHandler::Playlist HLSIOHandler::parsePlaylist(const std::string& text)
{
    Playlist pl;
    uint64_t sequence = 0;
    bool pending_variant = false;
    bool pending_segment = false;
    Variant variant;
    Segment segment;

    size_t pos = 0;
    bool first_line = true;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = hlsTrim(text.substr(pos, nl - pos));
        pos = nl + 1;
        if (first_line) {
            // A UTF-8 byte order mark may come first.
            if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
            if (line != "#EXTM3U") return pl;
            pl.valid = true;
            first_line = false;
            continue;
        }
        if (line.empty()) continue;
        if (line[0] == '#') {
            if (line.compare(0, 18, "#EXT-X-STREAM-INF:") == 0) {
                auto attrs = hlsAttributes(line.substr(18));
                variant = Variant();
                variant.bandwidth = std::strtoull(attrs["BANDWIDTH"].c_str(), nullptr, 10);
                variant.codecs = attrs["CODECS"];
                pending_variant = true;
            } else if (line.compare(0, 8, "#EXTINF:") == 0) {
                segment = Segment();
                segment.duration = std::strtod(line.c_str() + 8, nullptr);
                segment.title = hlsTitleFromExtinf(line.substr(8));
                pending_segment = true;
            } else if (line.compare(0, 22, "#EXT-X-TARGETDURATION:") == 0) {
                pl.target_duration = std::strtod(line.c_str() + 22, nullptr);
            } else if (line.compare(0, 22, "#EXT-X-MEDIA-SEQUENCE:") == 0) {
                sequence = std::strtoull(line.c_str() + 22, nullptr, 10);
            } else if (line == "#EXT-X-ENDLIST") {
                pl.ended = true;
            } else if (line.compare(0, 11, "#EXT-X-KEY:") == 0) {
                if (hlsAttributes(line.substr(11))["METHOD"] != "NONE") {
                    pl.unsupported = true;
                    pl.unsupported_reason = "encrypted segments";
                }
            } else if (line.compare(0, 11, "#EXT-X-MAP:") == 0) {
                pl.unsupported = true;
                pl.unsupported_reason = "fragmented MP4 segments";
            }
            continue;
        }
        // A URI line.
        if (pending_variant) {
            variant.uri = line;
            pl.variants.push_back(variant);
            pending_variant = false;
        } else if (pending_segment) {
            segment.uri = line;
            segment.sequence = sequence++;
            pl.segments.push_back(segment);
            pending_segment = false;
        }
    }
    return pl;
}

/**
 * @brief Pick the audio variant with the most bandwidth.
 * @param variants The variants.
 * @return Index of the choice.
 */
size_t HLSIOHandler::chooseVariant(const std::vector<Variant>& variants)
{
    size_t best = variants.size();
    for (size_t i = 0; i < variants.size(); ++i) {
        const std::string codecs = hlsLower(variants[i].codecs);
        const bool video = codecs.find("avc") != std::string::npos ||
                           codecs.find("hvc") != std::string::npos ||
                           codecs.find("hev") != std::string::npos;
        if (video) continue;
        if (best == variants.size() || variants[i].bandwidth > variants[best].bandwidth) {
            best = i;
        }
    }
    return best == variants.size() ? 0 : best;
}

/**
 * @brief Resolve a playlist URI: absolute as is; "//host/..." takes the
 *        base's scheme; "/path" the base's scheme and host; anything else is
 *        relative to the base's directory.
 * @param base Playlist URL.
 * @param reference URI as written.
 * @return Absolute URL.
 */
std::string HLSIOHandler::resolveUrl(const std::string& base, const std::string& reference)
{
    /*
     * Absolute when it starts with a scheme (RFC 3986 §3.1): a letter, then
     * letters, digits, "+", "-" or ".", then ":" -- before any "/", "?" or
     * "#", so a URL inside a query ("seg.aac?u=https://x") is not mistaken
     * for one.
     */
    const size_t colon = reference.find(':');
    if (colon != std::string::npos && colon > 0 && colon < reference.find_first_of("/?#") &&
        std::isalpha(static_cast<unsigned char>(reference[0]))) {
        bool scheme = true;
        for (size_t i = 1; i < colon && scheme; ++i) {
            const unsigned char c = static_cast<unsigned char>(reference[i]);
            scheme = std::isalnum(c) || c == '+' || c == '-' || c == '.';
        }
        if (scheme) return reference;
    }
    const size_t scheme_end = base.find("://");
    if (scheme_end == std::string::npos) return reference;
    if (reference.compare(0, 2, "//") == 0) {
        return base.substr(0, scheme_end + 1) + reference;
    }
    // The origin ends where the authority does: at the path, query or fragment.
    const size_t auth_end = std::min(base.find_first_of("/?#", scheme_end + 3), base.size());
    const std::string origin = base.substr(0, auth_end);
    const size_t path_end = std::min(base.find_first_of("?#", auth_end), base.size());
    const std::string path = base.substr(auth_end, path_end - auth_end);
    if (reference.empty()) {
        return base.substr(0, base.find('#'));
    }
    if (reference[0] == '/') {
        return origin + reference;
    }
    if (reference[0] == '?') {
        // The base's path, with this query instead (RFC 3986 §5.2.2).
        return origin + (path.empty() ? std::string("/") : path) + reference;
    }
    if (reference[0] == '#') {
        return base.substr(0, base.find('#')) + reference;
    }
    // Relative: the base's directory, without its query.
    const size_t slash = path.rfind('/');
    return origin + (slash == std::string::npos ? std::string("/") : path.substr(0, slash + 1)) + reference;
}

/**
 * @brief Drop each ID3v2 tag (10-byte header, synchsafe size, optional
 *        footer) at the start of a segment.
 * @param data The segment.
 */
void HLSIOHandler::stripLeadingID3(std::string& data)
{
    while (data.size() >= 10 && data.compare(0, 3, "ID3") == 0) {
        const auto b = [&data](size_t i) { return static_cast<uint64_t>(static_cast<uint8_t>(data[i]) & 0x7F); };
        uint64_t size = 10 + ((b(6) << 21) | (b(7) << 14) | (b(8) << 7) | b(9));
        if (static_cast<uint8_t>(data[5]) & 0x10) size += 10;
        if (size > data.size()) {
            data.clear();
            return;
        }
        data.erase(0, static_cast<size_t>(size));
    }
}

/**
 * @brief HLS by Content-Type, or by a path ending .m3u8.
 * @param content_type Lower-case type.
 * @param url The URL.
 * @return true if it looks like HLS.
 */
bool HLSIOHandler::looksLikeHLS(const std::string& content_type, const std::string& url)
{
    if (content_type == "application/vnd.apple.mpegurl" || content_type == "application/x-mpegurl" ||
        content_type == "audio/mpegurl" || content_type == "audio/x-mpegurl") {
        return true;
    }
    std::string path = hlsLower(url.substr(0, url.find_first_of("?#")));
    return path.size() >= 5 && path.compare(path.size() - 5, 5, ".m3u8") == 0;
}

/**
 * @brief Fetch the playlist, settle on a media playlist, start the producer
 *        and wait for the first audio.
 * @param url The playlist URL.
 */
HLSIOHandler::HLSIOHandler(const std::string& url)
    : m_url(url)
{
    Debug::log("http", "HLSIOHandler: opening ", url);
    std::string body, effective, error;
    if (!fetch(url, body, nullptr, &effective, error)) {
        throw InvalidMediaException("Cannot open " + url + ": " + error);
    }
    Playlist pl = parsePlaylist(body);
    if (!pl.valid) {
        throw InvalidMediaException("Cannot open " + url + ": not an HLS playlist");
    }
    m_media_url = effective.empty() ? url : effective;
    if (!pl.variants.empty()) {
        const Variant& v = pl.variants[chooseVariant(pl.variants)];
        m_media_url = resolveUrl(m_media_url, v.uri);
        Debug::log("http", "HLSIOHandler: variant ", v.bandwidth, " bit/s '", v.codecs, "': ", m_media_url);
        body.clear();
        if (!fetch(m_media_url, body, nullptr, &effective, error)) {
            throw InvalidMediaException("Cannot open " + url + ": media playlist: " + error);
        }
        if (!effective.empty()) m_media_url = effective;
        pl = parsePlaylist(body);
    }
    if (!pl.valid || pl.segments.empty()) {
        throw InvalidMediaException("Cannot open " + url + ": the playlist lists no segments");
    }
    if (pl.unsupported) {
        throw InvalidMediaException("Cannot open " + url + ": HLS with " + pl.unsupported_reason +
                                    " is not supported");
    }
    const std::string first = hlsLower(pl.segments.front().uri.substr(0, pl.segments.front().uri.find('?')));
    if (first.size() >= 3 && (first.compare(first.size() - 3, 3, ".ts") == 0 ||
                              (first.size() >= 4 && first.compare(first.size() - 4, 4, ".m4s") == 0))) {
        throw InvalidMediaException("Cannot open " + url +
                                    ": HLS in MPEG-TS or MP4 segments is not supported");
    }

    startWorker();
    std::unique_lock<std::mutex> lock(m_buf_mutex);
    m_data_cv.wait(lock, [this] { return m_first_audio || m_finished || m_abort.load(); });
    const bool ok = m_first_audio && !m_abort;
    const std::string why = m_abort ? std::string("aborted") : m_error_text;
    lock.unlock();
    if (!ok) {
        stopWorker();
        throw InvalidMediaException("Cannot open " + url + ": " + (why.empty() ? "no audio" : why));
    }
    Debug::log("http", "HLSIOHandler: playing ", m_media_url, " as ", getMimeType());
}

/**
 * @brief Stop the producer while this class is whole.
 */
HLSIOHandler::~HLSIOHandler()
{
    stopWorker();
}

/**
 * @brief The segments' media type.
 * @return It.
 */
std::string HLSIOHandler::getMimeType() const
{
    std::lock_guard<std::mutex> lock(m_buf_mutex);
    return m_mime_type;
}

/**
 * @brief Download segments in sequence, reloading the media playlist for
 *        more, until it ends, fails or the handler is aborted.
 */
void HLSIOHandler::runWorker()
{
    bool started = false;
    uint64_t next_sequence = 0;
    int reload_failures = 0;
    std::string body, error, effective, content_type;
    /** @brief The segment last played (or skipped), to find our place by. */
    std::string last_uri;

    while (!m_abort) {
        body.clear();
        if (!fetch(m_media_url, body, nullptr, &effective, error)) {
            if (m_abort) break;
            if (++reload_failures >= MAX_RELOAD_FAILURES) {
                finishWorker(true, "media playlist: " + error);
                return;
            }
            if (!waitOrAbort(2.0)) break;
            continue;
        }
        reload_failures = 0;
        const Playlist pl = parsePlaylist(body);
        if (!pl.valid || pl.unsupported) {
            finishWorker(true, pl.unsupported ? pl.unsupported_reason : std::string("bad media playlist"));
            return;
        }
        if (pl.segments.empty()) {
            if (pl.ended) break;
            if (!waitOrAbort(std::max(1.0, pl.target_duration / 2))) break;
            continue;
        }

        /*
         * Start near the live edge; after that, carry on where we were --
         * unless the playlist has moved past us, in which case catch up.
         */
        const uint64_t first_seq = pl.segments.front().sequence;
        const uint64_t last_seq = pl.segments.back().sequence;
        bool restart = !started || next_sequence < first_seq;
        if (started && !restart && !last_uri.empty()) {
            /*
             * Where we are is the segment last played, which the numbers
             * normally find -- but a packager that restarts numbers from 0
             * again, and a window without EXT-X-MEDIA-SEQUENCE numbers every
             * reload from 0. Follow the segment itself when the numbers
             * disagree, and start over at the live edge when it has gone and
             * the numbers are all behind us.
             */
            const Segment* at_number = nullptr;
            const Segment* at_uri = nullptr;
            for (const Segment& seg : pl.segments) {
                if (seg.sequence + 1 == next_sequence) at_number = &seg;
                if (seg.uri == last_uri) at_uri = &seg;
            }
            if (!(at_number && at_number->uri == last_uri)) {
                if (at_uri) {
                    next_sequence = at_uri->sequence + 1;
                } else if (next_sequence > last_seq + 1) {
                    Debug::log("http", "HLSIOHandler: the playlist started its numbering over; "
                               "going to the live edge");
                    restart = true;
                }
            }
        }
        if (restart) {
            const size_t n = pl.segments.size();
            const size_t from = pl.ended || n <= START_SEGMENTS_FROM_END ? 0 : n - START_SEGMENTS_FROM_END;
            if (started && next_sequence < first_seq) {
                Debug::log("http", "HLSIOHandler: fell behind the playlist; skipping to the live edge");
            }
            next_sequence = pl.segments[from].sequence;
            started = true;
        }

        bool any = false;
        for (const Segment& seg : pl.segments) {
            if (m_abort) break;
            if (seg.sequence < next_sequence) continue;
            std::string data;
            const std::string seg_url = resolveUrl(effective.empty() ? m_media_url : effective, seg.uri);
            if (!fetch(seg_url, data, &content_type, nullptr, error)) {
                if (m_abort) break;
                Debug::log("http", "HLSIOHandler: segment ", seg.sequence, " failed: ", error, "; skipping it");
                next_sequence = seg.sequence + 1;
                last_uri = seg.uri;
                continue;
            }
            stripLeadingID3(data);
            {
                std::lock_guard<std::mutex> lock(m_buf_mutex);
                if (m_mime_type.empty()) {
                    std::string type = content_type;
                    if (type.empty() || type == "application/octet-stream") {
                        const std::string path = hlsLower(seg.uri.substr(0, seg.uri.find('?')));
                        type = path.size() >= 4 && path.compare(path.size() - 4, 4, ".mp3") == 0
                             ? "audio/mpeg" : "audio/aac";
                    }
                    m_mime_type = type;
                }
            }
            if (!seg.title.empty()) {
                recordTitle(seg.title);
            }
            if (!data.empty() && !appendAudio(data.data(), data.size())) break;
            {
                std::lock_guard<std::mutex> lock(m_buf_mutex);
                if (!m_first_audio) {
                    m_first_audio = true;
                    m_data_cv.notify_all();
                }
            }
            next_sequence = seg.sequence + 1;
            last_uri = seg.uri;
            any = true;
        }
        if (m_abort) break;
        if (pl.ended && next_sequence > last_seq) break;
        // Nothing new: wait about half a target duration before asking again.
        if (!any && !waitOrAbort(std::max(1.0, pl.target_duration / 2))) break;
    }
    finishWorker(false, std::string());
}

/**
 * @brief Collect body bytes into a std::string.
 * @param data Bytes.
 * @param size Always 1.
 * @param nmemb Count.
 * @param out The string.
 * @return Bytes taken.
 */
size_t HLSIOHandler::appendToString(char* data, size_t size, size_t nmemb, void* out)
{
    static_cast<std::string*>(out)->append(data, size * nmemb);
    return size * nmemb;
}

/**
 * @brief Abort a request when the handler is aborted.
 * @param self The handler.
 * @return 1 to abort.
 */
int HLSIOHandler::abortCheck(void* self, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return static_cast<HLSIOHandler*>(self)->m_abort ? 1 : 0;
}

/**
 * @brief One complete GET with the same TLS and redirect rules as the other
 *        HTTP handlers, a REQUEST_SECONDS limit, and an abort check.
 * @param url URL.
 * @param body Body.
 * @param content_type Content-Type out, or null.
 * @param effective_url URL after redirects out, or null.
 * @param error Why it failed.
 * @return true for 2xx.
 */
bool HLSIOHandler::fetch(const std::string& url, std::string& body, std::string* content_type,
                         std::string* effective_url, std::string& error)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        error = "curl_easy_init failed";
        return false;
    }
    char error_buffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &HLSIOHandler::appendToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &HLSIOHandler::abortCheck);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
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
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PsyMP3/3.0");

    const CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (content_type) {
        char* type = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &type);
        std::string t = type ? type : "";
        t = t.substr(0, t.find(';'));
        *content_type = hlsLower(hlsTrim(t));
    }
    if (effective_url) {
        char* eff = nullptr;
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &eff);
        *effective_url = eff ? eff : "";
    }
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        error = m_abort ? std::string("aborted")
              : error_buffer[0] ? std::string(error_buffer) : std::string(curl_easy_strerror(rc));
        return false;
    }
    if (status < 200 || status >= 300) {
        error = "server answered " + std::to_string(status);
        return false;
    }
    noteActivity();
    return true;
}

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3

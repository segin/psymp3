/*
 * test_hls_io_handler.cpp - HLSIOHandler: playlist parsing and a local station
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The playlists are modelled on what iHeart's HLS stations serve: a master
 * playlist with one AAC variant, and media playlists whose #EXTINF lines
 * carry title="...", artist="..." and the spot kind inside url="...". The
 * end-to-end test serves them from 127.0.0.1, with packed-audio segments
 * that each begin with an ID3 tag, as real ones do.
 */

#include "psymp3.h"
#include "test_framework.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using PsyMP3::IO::HTTP::HLSIOHandler;

namespace {

/** @brief Parsing master and media playlists. */
class PlaylistParseTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    PlaylistParseTest() : TestCase("HLS playlists: master variants, media segments, iHeart titles") {}
protected:
    /** @brief Parse both kinds and check every field read from them. */
    void runTest() override {
        const auto master = HLSIOHandler::parsePlaylist(
            "#EXTM3U\n"
            "#EXT-X-STREAM-INF:BANDWIDTH=24000,CODECS=\"mp4a.40.2\"\n"
            "http://example.com/zc7001/low/playlist.m3u8\n"
            "#EXT-X-STREAM-INF:BANDWIDTH=64000,CODECS=\"mp4a.40.2\"\n"
            "high/playlist.m3u8\n"
            "#EXT-X-STREAM-INF:BANDWIDTH=900000,CODECS=\"avc1.4d401f,mp4a.40.2\"\n"
            "video.m3u8\n");
        ASSERT_TRUE(master.valid, "master is M3U");
        ASSERT_EQUALS(size_t(3), master.variants.size(), "three variants");
        ASSERT_EQUALS(uint64_t(64000), master.variants[1].bandwidth, "bandwidth read");
        ASSERT_EQUALS(std::string("mp4a.40.2"), master.variants[1].codecs, "quoted codecs read");
        ASSERT_EQUALS(size_t(1), HLSIOHandler::chooseVariant(master.variants),
                      "the best audio-only variant, not the video one");

        const auto media = HLSIOHandler::parsePlaylist(
            "#EXTM3U\r\n"
            "#EXT-X-TARGETDURATION:10\r\n"
            "#EXT-X-MEDIA-SEQUENCE:179490653\r\n"
            "#EXT-X-DISCONTINUITY\r\n"
            "#EXTINF:10,title=\"Evil Woman\",artist=\"ELECTRIC LIGHT ORCHESTRA\",url=\"song_spot=\\\"M\\\" "
            "spotInstanceId=\\\"-1\\\" length=\\\"00:04:09\\\" \"\r\n"
            "http://cdn.example.com/main/179490653.aac?rj-org=x\r\n"
            "#EXTINF:10,title=\"Fly In\",artist=\"\",url=\"song_spot=\\\"T\\\" \"\r\n"
            "179490654.aac\r\n"
            "#EXTINF:9.98,Plain Artist - Plain Song\r\n"
            "179490655.aac\r\n");
        ASSERT_TRUE(media.valid, "media is M3U, with CRLF line ends");
        ASSERT_EQUALS(size_t(3), media.segments.size(), "three segments");
        ASSERT_EQUALS(10.0, media.target_duration, "target duration");
        ASSERT_EQUALS(uint64_t(179490653), media.segments[0].sequence, "first sequence number");
        ASSERT_EQUALS(uint64_t(179490655), media.segments[2].sequence, "sequence numbers count up");
        ASSERT_FALSE(media.ended, "a live playlist has no ENDLIST");

        auto parts = PsyMP3::IO::HTTP::HTTPStreamIOHandler::splitStreamTitle(media.segments[0].title);
        ASSERT_EQUALS(std::string("ELECTRIC LIGHT ORCHESTRA"), parts.artist, "iHeart EXTINF: artist");
        ASSERT_EQUALS(std::string("Evil Woman"), parts.title, "iHeart EXTINF: title");
        ASSERT_TRUE(parts.is_music, "song_spot M is music");
        parts = PsyMP3::IO::HTTP::HTTPStreamIOHandler::splitStreamTitle(media.segments[1].title);
        ASSERT_EQUALS(std::string("Fly In"), parts.title, "a spot's title");
        ASSERT_FALSE(parts.is_music, "song_spot T is not music");
        parts = PsyMP3::IO::HTTP::HTTPStreamIOHandler::splitStreamTitle(media.segments[2].title);
        ASSERT_EQUALS(std::string("Plain Artist"), parts.artist, "plain EXTINF: artist");
        ASSERT_EQUALS(std::string("Plain Song"), parts.title, "plain EXTINF: title");

        ASSERT_FALSE(HLSIOHandler::parsePlaylist("http://a/b.mp3\n").valid, "no #EXTM3U: not a playlist");
        const auto keyed = HLSIOHandler::parsePlaylist(
            "#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI=\"k\"\n#EXTINF:10,\na.aac\n");
        ASSERT_TRUE(keyed.unsupported, "encrypted segments are refused");
        const auto fmp4 = HLSIOHandler::parsePlaylist(
            "#EXTM3U\n#EXT-X-MAP:URI=\"init.mp4\"\n#EXTINF:10,\na.m4s\n");
        ASSERT_TRUE(fmp4.unsupported, "fragmented MP4 is refused");
        // RFC 8216 quoted strings have no escapes: a backslash is the title's own.
        const auto slashed = HLSIOHandler::parsePlaylist(
            "#EXTM3U\n#EXTINF:10,title=\"AC\\DC Song\",artist=\"AC\\DC\"\na.aac\n");
        parts = PsyMP3::IO::HTTP::HTTPStreamIOHandler::splitStreamTitle(slashed.segments[0].title);
        ASSERT_EQUALS(std::string("AC\\DC Song"), parts.title, "a backslash in a title is kept");
        ASSERT_EQUALS(std::string("AC\\DC"), parts.artist, "and in an artist");
    }
};

/** @brief URL resolution, ID3 stripping and HLS recognition. */
class HelpersTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    HelpersTest() : TestCase("HLS helpers: URL resolution, ID3 stripping, detection") {}
protected:
    /** @brief Check each helper on the cases playlists use. */
    void runTest() override {
        const std::string base = "http://h.example.com/zc7001/7_1/playlist.m3u8?tok=1";
        ASSERT_EQUALS(std::string("http://other/x.aac"), HLSIOHandler::resolveUrl(base, "http://other/x.aac"),
                      "absolute kept");
        ASSERT_EQUALS(std::string("http://h.example.com/zc7001/7_1/main/1.aac"),
                      HLSIOHandler::resolveUrl(base, "main/1.aac"), "relative to the playlist's directory");
        ASSERT_EQUALS(std::string("http://h.example.com/seg/1.aac"),
                      HLSIOHandler::resolveUrl(base, "/seg/1.aac"), "root-relative");
        ASSERT_EQUALS(std::string("http://cdn.example.com/1.aac"),
                      HLSIOHandler::resolveUrl(base, "//cdn.example.com/1.aac"), "scheme-relative");
        // A URL inside the query does not make the reference absolute.
        ASSERT_EQUALS(std::string("http://h.example.com/zc7001/7_1/seg1.aac?u=https://x/y"),
                      HLSIOHandler::resolveUrl(base, "seg1.aac?u=https://x/y"), "relative, URL in its query");
        // A base with no path: its query is not a directory.
        ASSERT_EQUALS(std::string("https://h/s.aac"), HLSIOHandler::resolveUrl("https://h?x=1", "s.aac"),
                      "relative to a base with a query and no path");
        ASSERT_EQUALS(std::string("https://h/s.aac"), HLSIOHandler::resolveUrl("https://h?x=1", "/s.aac"),
                      "root-relative to a base with a query and no path");
        ASSERT_EQUALS(std::string("http://h.example.com/zc7001/7_1/playlist.m3u8?v=2"),
                      HLSIOHandler::resolveUrl(base, "?v=2"), "a query-only reference keeps the file");
        ASSERT_EQUALS(std::string("HTTPS://up.example.com/a"), HLSIOHandler::resolveUrl(base, "HTTPS://up.example.com/a"),
                      "an upper-case scheme is a scheme");

        // A 10-byte header with synchsafe size 5, then 5 bytes, twice, then audio.
        std::string seg = std::string("ID3\x04\x00\x00\x00\x00\x00\x05", 10) + "PRIVx" +
                          std::string("ID3\x04\x00\x00\x00\x00\x00\x02", 10) + "zz" + "AUDIO";
        HLSIOHandler::stripLeadingID3(seg);
        ASSERT_EQUALS(std::string("AUDIO"), seg, "both leading tags removed");
        std::string plain = "\xFF\xF1rest";
        HLSIOHandler::stripLeadingID3(plain);
        ASSERT_EQUALS(std::string("\xFF\xF1rest"), plain, "no tag: unchanged");

        ASSERT_TRUE(HLSIOHandler::looksLikeHLS("application/vnd.apple.mpegurl", "http://a/x"), "by type");
        ASSERT_TRUE(HLSIOHandler::looksLikeHLS("audio/x-mpegurl", "http://a/x"), "by legacy type");
        ASSERT_TRUE(HLSIOHandler::looksLikeHLS("", "http://a/hls.M3U8?x=1"), "by name, any case, query ignored");
        ASSERT_FALSE(HLSIOHandler::looksLikeHLS("audio/aac", "http://a/zc7001"), "an AAC stream is not");
    }
};

#ifndef _WIN32

/**
 * @brief A small HTTP server on 127.0.0.1 answering any number of requests,
 *        each from a table of paths.
 */
class PathServer {
public:
    /** @brief One canned answer. */
    struct Reply {
        int status = 200;          ///< HTTP status
        std::string type;          ///< Content-Type
        std::string body;          ///< body
    };

    /**
     * @brief Listen on a free port and serve until destroyed.
     * @param replies Path (with query) to answer; others get 404.
     */
    explicit PathServer(std::map<std::string, Reply> replies) : m_replies(std::move(replies))
    {
        m_listen = ::socket(AF_INET, SOCK_STREAM, 0);
        int one = 1;
        ::setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ::bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
        ::listen(m_listen, 16);
        socklen_t len = sizeof addr;
        ::getsockname(m_listen, reinterpret_cast<sockaddr*>(&addr), &len);
        m_port = ntohs(addr.sin_port);
        m_thread = std::thread([this] { acceptLoop(); });
    }

    /** @brief Stop accepting and wait for the server thread. */
    ~PathServer()
    {
        ::shutdown(m_listen, SHUT_RDWR);
        ::close(m_listen);
        if (m_thread.joinable()) m_thread.join();
    }

    /**
     * @brief The base URL of this server.
     * @return http://127.0.0.1:<port>
     */
    std::string base() const { return "http://127.0.0.1:" + std::to_string(m_port); }

    /**
     * @brief How many times a path was asked for.
     * @param path The path.
     * @return The count.
     */
    int hits(const std::string& path) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_hits.find(path);
        return it == m_hits.end() ? 0 : it->second;
    }

    /**
     * @brief Change or add an answer while serving, as a live station's
     *        playlist changes.
     * @param path The path.
     * @param reply What to answer from now on.
     */
    void setReply(const std::string& path, Reply reply)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_replies[path] = std::move(reply);
    }

private:
    /** @brief Accept connections and answer each on the server thread. */
    void acceptLoop()
    {
        for (;;) {
            const int fd = ::accept(m_listen, nullptr, nullptr);
            if (fd < 0) return;
            answer(fd);
            ::close(fd);
        }
    }

    /**
     * @brief Read one request and send its canned reply.
     * @param fd The connection.
     */
    void answer(int fd)
    {
        std::string request;
        char buf[2048];
        while (request.find("\r\n\r\n") == std::string::npos) {
            const ssize_t n = ::recv(fd, buf, sizeof buf, 0);
            if (n <= 0) return;
            request.append(buf, static_cast<size_t>(n));
        }
        const size_t sp1 = request.find(' ');
        const size_t sp2 = request.find(' ', sp1 + 1);
        const std::string path = request.substr(sp1 + 1, sp2 - sp1 - 1);
        Reply reply;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ++m_hits[path];
            auto it = m_replies.find(path);
            if (it == m_replies.end()) {
                reply.status = 404;
            } else {
                reply = it->second;
            }
        }
        std::string head = "HTTP/1.1 " + std::to_string(reply.status) + " X\r\nContent-Length: " +
                           std::to_string(reply.body.size()) + "\r\nConnection: close\r\n";
        if (!reply.type.empty()) head += "Content-Type: " + reply.type + "\r\n";
        const std::string all = head + "\r\n" + reply.body;
        size_t sent = 0;
        while (sent < all.size()) {
            const ssize_t n = ::send(fd, all.data() + sent, all.size() - sent, 0);
            if (n <= 0) return;
            sent += static_cast<size_t>(n);
        }
    }

    int m_listen = -1;
    int m_port = 0;
    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::map<std::string, Reply> m_replies;
    std::map<std::string, int> m_hits;
};

/**
 * @brief Recognisable audio bytes for segment @p n.
 * @param n Segment number.
 * @return 3000 bytes that differ between segments.
 */
std::string segmentAudio(int n)
{
    std::string s(3000, '\0');
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<char>((i * 13 + static_cast<size_t>(n) * 7) & 0x7F);
    return s;
}

/**
 * @brief A segment as served: an ID3 tag with a PRIV frame's worth of bytes,
 *        then the audio.
 * @param n Segment number.
 * @return Its bytes.
 */
std::string servedSegment(int n)
{
    return std::string("ID3\x04\x00\x00\x00\x00\x00\x14", 10) + std::string(20, 'p') + segmentAudio(n);
}

/** @brief A finished (ENDLIST) station played start to end through a master playlist. */
class LocalStationTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    LocalStationTest() : TestCase("HLS end to end: master, media, segments, titles") {}
protected:
    /**
     * @brief Serve a master playlist, a relative media playlist and four
     *        segments; open through openHTTPIOHandler(); check the audio, the
     *        titles and the MIME type.
     */
    void runTest() override {
        std::map<std::string, PathServer::Reply> replies;
        replies["/live/hls.m3u8"] = {200, "application/vnd.apple.mpegurl",
            "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=64000,CODECS=\"mp4a.40.2\"\nv/playlist.m3u8\n"};
        std::string media = "#EXTM3U\n#EXT-X-TARGETDURATION:2\n#EXT-X-MEDIA-SEQUENCE:7\n";
        for (int n = 0; n < 4; ++n) {
            media += "#EXTINF:2,title=\"Song " + std::to_string(n / 2) + "\",artist=\"Band\",url=\"song_spot=\\\"M\\\"\"\n";
            media += "seg" + std::to_string(n) + ".aac\n";
            replies["/live/v/seg" + std::to_string(n) + ".aac"] = {200, "audio/aac", servedSegment(n)};
        }
        media += "#EXT-X-ENDLIST\n";
        replies["/live/v/playlist.m3u8"] = {200, "application/vnd.apple.mpegurl", media};
        PathServer server(replies);

        auto handler = PsyMP3::IO::HTTP::openHTTPIOHandler(server.base() + "/live/hls.m3u8");
        ASSERT_NOT_NULL(handler.get(), "opened");
        ASSERT_TRUE(handler->isLiveStream(), "live");
        ASSERT_EQUALS(std::string("audio/aac"), handler->getMimeType(), "segment type");

        std::string got;
        char buf[1000];
        for (;;) {
            const size_t n = handler->read(buf, 1, sizeof buf);
            if (n == 0) break;
            got.append(buf, n);
        }
        std::string expected;
        for (int n = 0; n < 4; ++n) expected += segmentAudio(n);
        ASSERT_EQUALS(expected.size(), got.size(), "all four segments, without their ID3 tags");
        ASSERT_TRUE(got == expected, "in order, unchanged");

        const auto first = PsyMP3::IO::HTTP::HTTPStreamIOHandler::splitStreamTitle(handler->getStreamTitleAt(0));
        ASSERT_EQUALS(std::string("Song 0"), first.title, "title of the first segments");
        ASSERT_EQUALS(std::string("Band"), first.artist, "artist");
        const auto later = PsyMP3::IO::HTTP::HTTPStreamIOHandler::splitStreamTitle(handler->getStreamTitleAt(6000));
        ASSERT_EQUALS(std::string("Song 1"), later.title, "the title changes where segment 2 starts");
        ASSERT_EQUALS(1, server.hits("/live/v/seg0.aac"), "each segment fetched once");
    }
};

/**
 * @brief Read exactly @p n bytes, waiting as a live reader does.
 * @param handler The stream.
 * @param n How many.
 * @return What was read (shorter only if the stream ended).
 */
std::string readExactly(PsyMP3::IO::IOHandler& handler, size_t n)
{
    std::string got(n, '\0');
    const size_t r = handler.read(&got[0], 1, n);
    got.resize(r);
    return got;
}

/**
 * @brief Read until the stream ends.
 * @param handler The stream.
 * @return Everything left.
 */
std::string readRest(PsyMP3::IO::IOHandler& handler)
{
    std::string got;
    char buf[1000];
    for (;;) {
        const size_t n = handler.read(buf, 1, sizeof buf);
        if (n == 0) break;
        got.append(buf, n);
    }
    return got;
}

/**
 * @brief A media playlist of the given segments.
 * @param sequence Its EXT-X-MEDIA-SEQUENCE, or -1 to leave the tag out.
 * @param segs The segment numbers, named seg<n>.aac.
 * @param ended Whether to end it with EXT-X-ENDLIST.
 * @return The playlist text.
 */
std::string mediaPlaylist(long sequence, const std::vector<int>& segs, bool ended)
{
    std::string text = "#EXTM3U\n#EXT-X-TARGETDURATION:2\n";
    if (sequence >= 0) text += "#EXT-X-MEDIA-SEQUENCE:" + std::to_string(sequence) + "\n";
    for (int n : segs) text += "#EXTINF:2,\nseg" + std::to_string(n) + ".aac\n";
    if (ended) text += "#EXT-X-ENDLIST\n";
    return text;
}

/**
 * @brief A station whose packager restarts and numbers its segments from 0
 *        again: the player used to skip every new segment as already played
 *        and go silent for good.
 */
class SequenceRestartTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    SequenceRestartTest() : TestCase("HLS: a station that starts its numbering over plays on") {}
protected:
    /** @brief Play segments 5000-5002, then a playlist numbered from 0. */
    void runTest() override {
        std::map<std::string, PathServer::Reply> replies;
        for (int n = 0; n < 6; ++n) {
            replies["/r/seg" + std::to_string(n) + ".aac"] = {200, "audio/aac", servedSegment(n)};
        }
        replies["/r/live.m3u8"] = {200, "application/vnd.apple.mpegurl", mediaPlaylist(5000, {0, 1, 2}, false)};
        PathServer server(replies);
        auto handler = PsyMP3::IO::HTTP::openHTTPIOHandler(server.base() + "/r/live.m3u8");
        ASSERT_NOT_NULL(handler.get(), "opened");
        const std::string before = readExactly(*handler, 3 * 3000);
        ASSERT_TRUE(before == segmentAudio(0) + segmentAudio(1) + segmentAudio(2), "the first window");
        server.setReply("/r/live.m3u8", {200, "application/vnd.apple.mpegurl", mediaPlaylist(0, {3, 4}, true)});
        const std::string after = readRest(*handler);
        ASSERT_TRUE(after == segmentAudio(3) + segmentAudio(4), "the restarted numbering is played");
    }
};

/**
 * @brief A sliding window that never says EXT-X-MEDIA-SEQUENCE, so every
 *        reload numbers its segments from 0: the player follows the
 *        segments themselves rather than their numbers.
 */
class UnnumberedWindowTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    UnnumberedWindowTest() : TestCase("HLS: a window without a media sequence moves on") {}
protected:
    /** @brief Play 0-2, then a window of 1-3 that also numbers from 0. */
    void runTest() override {
        std::map<std::string, PathServer::Reply> replies;
        for (int n = 0; n < 6; ++n) {
            replies["/u/seg" + std::to_string(n) + ".aac"] = {200, "audio/aac", servedSegment(n)};
        }
        replies["/u/live.m3u8"] = {200, "application/vnd.apple.mpegurl", mediaPlaylist(-1, {0, 1, 2}, false)};
        PathServer server(replies);
        auto handler = PsyMP3::IO::HTTP::openHTTPIOHandler(server.base() + "/u/live.m3u8");
        ASSERT_NOT_NULL(handler.get(), "opened");
        const std::string before = readExactly(*handler, 3 * 3000);
        ASSERT_TRUE(before == segmentAudio(0) + segmentAudio(1) + segmentAudio(2), "the first window");
        server.setReply("/u/live.m3u8", {200, "application/vnd.apple.mpegurl", mediaPlaylist(-1, {1, 2, 3}, true)});
        const std::string after = readRest(*handler);
        ASSERT_TRUE(after == segmentAudio(3), "only the new segment follows");
        ASSERT_EQUALS(1, server.hits("/u/seg1.aac"), "nothing is fetched twice");
    }
};

/** @brief A plain M3U "listen" link pointing at a stream. */
class PlainPlaylistTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    PlainPlaylistTest() : TestCase("A plain .m3u link opens its first entry") {}
protected:
    /** @brief Serve an audio/x-mpegurl list naming an HLS playlist; follow it. */
    void runTest() override {
        std::map<std::string, PathServer::Reply> replies;
        replies["/listen.m3u"] = {200, "audio/x-mpegurl", "#EXTM3U\n#EXTINF:-1,Station\n/hls/p.m3u8\n"};
        replies["/hls/p.m3u8"] = {200, "application/vnd.apple.mpegurl",
            "#EXTM3U\n#EXT-X-TARGETDURATION:2\n#EXTINF:2,\ns.aac\n#EXT-X-ENDLIST\n"};
        replies["/hls/s.aac"] = {200, "audio/aac", servedSegment(9)};
        PathServer server(replies);

        auto handler = PsyMP3::IO::HTTP::openHTTPIOHandler(server.base() + "/listen.m3u");
        std::string got;
        char buf[1000];
        for (;;) {
            const size_t n = handler->read(buf, 1, sizeof buf);
            if (n == 0) break;
            got.append(buf, n);
        }
        ASSERT_TRUE(got == segmentAudio(9), "the entry's stream plays");
    }
};

#endif // !_WIN32

} // namespace

/**
 * @brief Run the HLS tests; the entry test_http_suite calls.
 * @return Number of failed tests.
 */
int test_hls_io_handler_main()
{
#ifndef _WIN32
    ::signal(SIGPIPE, SIG_IGN);
#endif
    TestFramework::TestSuite suite("HLSIOHandler Tests");
    suite.addTest(std::make_unique<PlaylistParseTest>());
    suite.addTest(std::make_unique<HelpersTest>());
#ifndef _WIN32
    suite.addTest(std::make_unique<LocalStationTest>());
    suite.addTest(std::make_unique<PlainPlaylistTest>());
    suite.addTest(std::make_unique<SequenceRestartTest>());
    suite.addTest(std::make_unique<UnnumberedWindowTest>());
#endif
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}

/*
 * DiscordPresence.cpp - Discord Rich Presence ("Listening to PsyMP3") client
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifndef _WIN32
#include <sys/un.h>
#endif

namespace PsyMP3 {
namespace Discord {

// PsyMP3's Discord application ID (Developer Portal -> General Information ->
// "Application ID", a numeric snowflake - NOT the Ed25519 public key). The ID
// is public, like the Last.fm API key. Empty = integration dormant.
// Overridable with the PSYMP3_DISCORD_CLIENT_ID environment variable for
// testing against a personal application.
static const char kApplicationId[] = "1533216209661726720";

// How often to retry connecting while Discord isn't running.
static constexpr int kReconnectSeconds = 15;

// IPC opcodes (the framed-JSON local RPC protocol).
static constexpr uint32_t kOpHandshake = 0;
static constexpr uint32_t kOpFrame = 1;
static constexpr uint32_t kOpClose = 2;

// Discord rejects activity strings outside 2..128 bytes. Truncate on a UTF-8
// codepoint boundary; pad a 1-byte string (untagged single-letter filename)
// rather than have the whole activity refused.
static std::string clampActivityString(const std::string& s)
{
    std::string out = s;
    if (out.size() > 128) {
        size_t cut = 128;
        while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) {
            --cut; // never split a multi-byte codepoint
        }
        out.resize(cut);
    }
    if (out.size() == 1) {
        out += ' ';
    }
    return out;
}

DiscordPresence::DiscordPresence()
{
    m_client_id = kApplicationId;
    if (const char* env_id = getenv("PSYMP3_DISCORD_CLIENT_ID")) {
        if (env_id[0] != '\0') {
            m_client_id = env_id;
        }
    }
    m_worker = std::thread(&DiscordPresence::workerLoop, this);
    DEBUG_LOG_LAZY("discord", "DiscordPresence worker started (client id ",
                   m_client_id.empty() ? "unset - dormant" : m_client_id, ")");
}

DiscordPresence::~DiscordPresence()
{
    // Ask the worker to clear the visible presence on its way out, then join.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Activity gone;
        queue_unlocked(gone);
        m_shutdown = true;
    }
    m_cv.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
    ipcClose();
}

void DiscordPresence::setEnabled(bool on)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_enabled == on) return;
        m_enabled = on;
        if (!on) {
            Activity gone;
            queue_unlocked(gone); // clear before the worker disconnects
        } else if (m_current.visible) {
            queue_unlocked(m_current); // re-show what was playing
        }
    }
    m_cv.notify_one();
}

void DiscordPresence::setNowPlaying(const std::string& artist, const std::string& title,
                                    const std::string& album, unsigned int length_s,
                                    unsigned int position_s, const std::string& release_mbid,
                                    const std::string& lookup_artist)
{
    Activity a;
    a.visible = true;
    // A fully untagged file arrives with empty strings; details is the one
    // field Discord requires, so it always gets a value.
    a.title = clampActivityString(title.empty() ? "Unknown Track" : title);
    a.artist = clampActivityString(artist);
    a.album = clampActivityString(album);
    a.lookup_artist = lookup_artist.empty() ? artist : lookup_artist;
    // Client-side progress bar: start anchored so "now" is at position_s,
    // end at the track's length. Length 0 (streams) = no bar.
    if (length_s > 0 && position_s <= length_s) {
        long long now_ms = static_cast<long long>(time(nullptr)) * 1000;
        a.start_ms = now_ms - static_cast<long long>(position_s) * 1000;
        a.end_ms = a.start_ms + static_cast<long long>(length_s) * 1000;
    }
    if (PsyMP3::LastFM::LastFM::isValidMBID(release_mbid)) {
        a.art_url = "https://coverartarchive.org/release/" + release_mbid + "/front-250";
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        queue_unlocked(a);
    }
    m_cv.notify_one();
}

void DiscordPresence::setPaused()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_current.visible && !m_has_pending) {
            return; // nothing showing
        }
        Activity a = m_has_pending ? m_pending : m_current;
        a.paused = true;
        a.start_ms = a.end_ms = 0; // no progress bar while paused
        queue_unlocked(a);
    }
    m_cv.notify_one();
}

void DiscordPresence::clear()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Activity gone;
        queue_unlocked(gone);
    }
    m_cv.notify_one();
}

void DiscordPresence::queue_unlocked(const Activity& a)
{
    m_pending = a;
    m_has_pending = true;
}

void DiscordPresence::workerLoop()
{
    System::setThisThreadName("discord-rpc");

    if (m_client_id.empty()) {
        // No application ID compiled in or provided: sleep until shutdown.
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_shutdown.load(); });
        return;
    }

    bool connected = false;
    while (!m_shutdown) {
        Activity to_send;
        bool have_work = false;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (connected) {
                m_cv.wait(lock, [this] { return m_has_pending || m_shutdown.load(); });
            } else {
                // Retry cadence while Discord isn't reachable. Only bother
                // when enabled and there is (or was) something to show.
                m_cv.wait_for(lock, std::chrono::seconds(kReconnectSeconds), [this] {
                    return m_has_pending || m_shutdown.load();
                });
            }
            if (m_shutdown && !m_has_pending) break;
            if (m_has_pending) {
                to_send = m_pending;
                m_has_pending = false;
                have_work = true;
            } else if (!connected && m_enabled && m_current.visible) {
                to_send = m_current; // reconnect attempt: restore the presence
                have_work = true;
            }
        }

        if (!m_enabled) {
            // Disabled: only a queued CLEAR passes through (so the visible
            // presence vanishes); anything else waits, and an idle
            // connection is dropped.
            if (!have_work || to_send.visible) {
                if (connected && !have_work) { ipcClose(); connected = false; }
                continue;
            }
        }

        if (!connected) {
            connected = ipcConnect();
            if (!connected) {
                if (m_shutdown) break;
                continue; // retry after the wait_for cadence
            }
        }

        if (have_work) {
            if (to_send.visible) {
                resolveArtwork(to_send); // network lookup, worker thread only
            }
            if (sendActivity(to_send)) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_current = to_send;
            } else {
                DEBUG_LOG_LAZY("discord", "Send failed - dropping connection");
                ipcClose();
                connected = false;
                // Keep the state for the reconnect re-send.
                std::lock_guard<std::mutex> lock(m_mutex);
                if (to_send.visible) m_current = to_send;
            }
        }
        if (m_shutdown) break;
    }
}

bool DiscordPresence::sendActivity(const Activity& a)
{
    ipcDrain(); // discard any queued responses/events before writing

    std::string payload;
    payload.reserve(512);
    payload += "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
#ifdef _WIN32
    payload += std::to_string(static_cast<long>(GetCurrentProcessId()));
#else
    payload += std::to_string(static_cast<long>(getpid()));
#endif
    if (a.visible) {
        payload += ",\"activity\":{\"type\":2";
        // Experiment: the compact member-list line normally renders the
        // registered application name; if the client honors a payload name,
        // it shows the artist there instead ("Listening to <artist>").
        if (!a.artist.empty()) {
            payload += ",\"name\":\"" + jsonEscape(a.artist) + "\"";
        }
        payload += ",\"details\":\"" + jsonEscape(a.title) + "\"";
        if (!a.artist.empty()) {
            std::string state = a.artist;
            if (a.paused) state += " (paused)";
            payload += ",\"state\":\"" + jsonEscape(clampActivityString(state)) + "\"";
        } else if (a.paused) {
            payload += ",\"state\":\"(paused)\"";
        }
        if (a.start_ms > 0 && a.end_ms > a.start_ms) {
            payload += ",\"timestamps\":{\"start\":" + std::to_string(a.start_ms) +
                       ",\"end\":" + std::to_string(a.end_ms) + "}";
        }
        // Assets only when there is artwork: large_text is the image's
        // tooltip, so without an image it has nowhere to appear. Untagged or
        // MBID-less tracks simply present without art (the client falls back
        // to the application icon). With artwork, the PsyMP3 logo rides
        // along as the corner badge - the app branding the artist-as-name
        // override displaced from the card header.
        if (!a.art_url.empty()) {
            payload += ",\"assets\":{\"large_image\":\"" + jsonEscape(a.art_url) + "\"";
            if (!a.album.empty()) {
                payload += ",\"large_text\":\"" + jsonEscape(a.album) + "\"";
            }
            payload += ",\"small_image\":\"https://raw.githubusercontent.com/segin/psymp3/master/res/psymp3_icon3_nobg_128x128.png\"";
            payload += ",\"small_text\":\"PsyMP3\"";
            payload += "}";
        }
        payload += "}";
    }
    payload += "},\"nonce\":\"" + std::to_string(++m_nonce) + "\"}";

    if (!ipcSend(kOpFrame, payload)) {
        return false;
    }
    // Log Discord's verdict: a rejected payload (e.g. a field the client's
    // validator refuses) comes back as an ERROR response, which is the only
    // way to see that the presence silently failed to apply.
    std::string reply;
    if (ipcReadFrame(reply, 1000)) {
        DEBUG_LOG_LAZY("discord", "Response: ", reply);
    }
    DEBUG_LOG_LAZY("discord", a.visible ? "Presence updated" : "Presence cleared");
    return true;
}

void DiscordPresence::resolveArtwork(Activity& a)
{
    if (!a.art_url.empty() || a.album.empty()) {
        return; // tagged art wins; no album = nothing to search for
    }
    const std::string& artist = a.lookup_artist.empty() ? a.artist : a.lookup_artist;
    if (artist.empty()) {
        return;
    }

    // One-entry memo: pause/seek/resume re-send the same track, and each
    // would otherwise hit MusicBrainz again. Deliberately NOT a persistent
    // cache - a new album is a fresh query every time.
    const std::string key = artist + "\n" + a.album;
    if (key == m_memo_key) {
        if (!m_memo_mbid.empty()) {
            a.art_url = "https://coverartarchive.org/release-group/" + m_memo_mbid +
                        "/front-250";
        }
        return;
    }

    // MusicBrainz API etiquette: at most one request per second.
    auto now = std::chrono::steady_clock::now();
    auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_mb_query);
    if (m_last_mb_query.time_since_epoch().count() != 0 && since.count() < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 - since.count()));
        if (m_shutdown) return;
    }
    m_last_mb_query = std::chrono::steady_clock::now();

    // Lucene query; embedded quotes would break the phrase syntax.
    auto strip_quotes = [](std::string v) {
        v.erase(std::remove(v.begin(), v.end(), '"'), v.end());
        return v;
    };
    std::string query = "artist:\"" + strip_quotes(artist) + "\" AND releasegroup:\"" +
                        strip_quotes(a.album) + "\"";
    std::string url = "https://musicbrainz.org/ws/2/release-group/?limit=1&fmt=json&query=" +
                      PsyMP3::IO::HTTP::HTTPClient::urlEncode(query);

    DEBUG_LOG_LAZY("discord", "MusicBrainz release-group lookup: ", artist, " / ", a.album);
    PsyMP3::IO::HTTP::HTTPClient::Response response = PsyMP3::IO::HTTP::HTTPClient::get(
        url, {{"User-Agent", "PsyMP3/" PSYMP3_VERSION " ( segin2005@gmail.com )"}}, 5);

    m_memo_key = key;
    m_memo_mbid.clear();
    if (response.success) {
        // First group id in the result: "release-groups":[{"id":"<uuid>",...
        size_t groups = response.body.find("\"release-groups\"");
        if (groups != std::string::npos) {
            size_t idpos = response.body.find("\"id\":\"", groups);
            if (idpos != std::string::npos && idpos + 7 + 36 <= response.body.size()) {
                std::string mbid = response.body.substr(idpos + 6, 36);
                if (PsyMP3::LastFM::LastFM::isValidMBID(mbid)) {
                    m_memo_mbid = mbid;
                }
            }
        }
        DEBUG_LOG_LAZY("discord", "MusicBrainz lookup result: ",
                       m_memo_mbid.empty() ? "no match" : m_memo_mbid);
    } else {
        DEBUG_LOG_LAZY("discord", "MusicBrainz lookup failed: ", response.statusMessage);
    }

    if (!m_memo_mbid.empty()) {
        a.art_url = "https://coverartarchive.org/release-group/" + m_memo_mbid +
                    "/front-250";
    }
}

std::string DiscordPresence::jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c); // UTF-8 passes through
                }
        }
    }
    return out;
}

// ============================================================================
// IPC transport
// ============================================================================

bool DiscordPresence::ipcSend(uint32_t opcode, const std::string& payload)
{
    // Frame: two little-endian u32s (opcode, length), then the JSON bytes.
    std::string frame;
    frame.resize(8);
    uint32_t len = static_cast<uint32_t>(payload.size());
    frame[0] = static_cast<char>(opcode & 0xFF);
    frame[1] = static_cast<char>((opcode >> 8) & 0xFF);
    frame[2] = static_cast<char>((opcode >> 16) & 0xFF);
    frame[3] = static_cast<char>((opcode >> 24) & 0xFF);
    frame[4] = static_cast<char>(len & 0xFF);
    frame[5] = static_cast<char>((len >> 8) & 0xFF);
    frame[6] = static_cast<char>((len >> 16) & 0xFF);
    frame[7] = static_cast<char>((len >> 24) & 0xFF);
    frame += payload;

#ifdef _WIN32
    if (!m_pipe || m_pipe == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    return WriteFile(static_cast<HANDLE>(m_pipe), frame.data(),
                     static_cast<DWORD>(frame.size()), &written, nullptr) &&
           written == frame.size();
#else
    if (m_fd < 0) return false;
    size_t off = 0;
    while (off < frame.size()) {
        ssize_t n = ::send(m_fd, frame.data() + off, frame.size() - off, 0);
        if (n <= 0) return false;
        off += static_cast<size_t>(n);
    }
    return true;
#endif
}

bool DiscordPresence::ipcReadFrame(std::string& payload_out, int timeout_ms)
{
#ifdef _WIN32
    if (!m_pipe || m_pipe == INVALID_HANDLE_VALUE) return false;
    // Wait for the header to be available (Discord replies promptly).
    for (int waited = 0; waited < timeout_ms; waited += 50) {
        DWORD avail = 0;
        if (!PeekNamedPipe(static_cast<HANDLE>(m_pipe), nullptr, 0, nullptr, &avail, nullptr))
            return false;
        if (avail >= 8) break;
        Sleep(50);
    }
    uint8_t hdr[8];
    DWORD got = 0;
    if (!ReadFile(static_cast<HANDLE>(m_pipe), hdr, 8, &got, nullptr) || got != 8)
        return false;
    uint32_t len = hdr[4] | (hdr[5] << 8) | (hdr[6] << 16)
                 | (static_cast<uint32_t>(hdr[7]) << 24);
    if (len > 1024 * 1024) return false;
    payload_out.resize(len);
    DWORD off = 0;
    while (off < len) {
        if (!ReadFile(static_cast<HANDLE>(m_pipe), &payload_out[off], len - off, &got, nullptr) || got == 0)
            return false;
        off += got;
    }
    return true;
#else
    if (m_fd < 0) return false;
    struct pollfd pfd = {m_fd, POLLIN, 0};
    if (poll(&pfd, 1, timeout_ms) <= 0 || !(pfd.revents & POLLIN)) return false;
    uint8_t hdr[8];
    size_t off = 0;
    while (off < 8) {
        ssize_t n = ::recv(m_fd, hdr + off, 8 - off, 0);
        if (n <= 0) return false;
        off += static_cast<size_t>(n);
    }
    uint32_t len = hdr[4] | (hdr[5] << 8) | (hdr[6] << 16)
                 | (static_cast<uint32_t>(hdr[7]) << 24);
    if (len > 1024 * 1024) return false;
    payload_out.resize(len);
    off = 0;
    while (off < len) {
        ssize_t n = ::recv(m_fd, &payload_out[off], len - off, 0);
        if (n <= 0) return false;
        off += static_cast<size_t>(n);
    }
    return true;
#endif
}

void DiscordPresence::ipcDrain()
{
#ifdef _WIN32
    if (!m_pipe || m_pipe == INVALID_HANDLE_VALUE) return;
    DWORD avail = 0;
    std::string sink;
    while (PeekNamedPipe(static_cast<HANDLE>(m_pipe), nullptr, 0, nullptr, &avail, nullptr) && avail >= 8) {
        if (!ipcReadFrame(sink, 0)) break;
    }
#else
    if (m_fd < 0) return;
    std::string sink;
    struct pollfd pfd = {m_fd, POLLIN, 0};
    while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        if (!ipcReadFrame(sink, 0)) break;
    }
#endif
}

bool DiscordPresence::ipcConnect()
{
#ifdef _WIN32
    for (int i = 0; i < 10; ++i) {
        std::string path = "\\\\.\\pipe\\discord-ipc-" + std::to_string(i);
        HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                               nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;
        m_pipe = h;
        std::string handshake = "{\"v\":1,\"client_id\":\"" + jsonEscape(m_client_id) + "\"}";
        std::string reply;
        if (ipcSend(kOpHandshake, handshake) && ipcReadFrame(reply, 5000) &&
            reply.find("READY") != std::string::npos) {
            DEBUG_LOG_LAZY("discord", "Connected to Discord via ", path);
            return true;
        }
        ipcClose();
    }
    return false;
#else
    // Candidate base dirs, then sandboxed client subpaths within each.
    std::vector<std::string> bases;
    if (const char* xdg = getenv("XDG_RUNTIME_DIR")) bases.push_back(xdg);
    if (const char* tmp = getenv("TMPDIR")) bases.push_back(tmp);
    bases.push_back("/tmp");
    static const char* kSubdirs[] = {"", "app/com.discordapp.Discord/", "snap.discord/"};

    for (const std::string& base : bases) {
        for (const char* sub : kSubdirs) {
            for (int i = 0; i < 10; ++i) {
                std::string path = base + "/" + sub + "discord-ipc-" + std::to_string(i);
                int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
                if (fd < 0) return false;
                struct sockaddr_un addr;
                memset(&addr, 0, sizeof(addr));
                addr.sun_family = AF_UNIX;
                if (path.size() >= sizeof(addr.sun_path)) { ::close(fd); continue; }
                strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
                if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
                    ::close(fd);
                    continue;
                }
                m_fd = fd;
                std::string handshake = "{\"v\":1,\"client_id\":\"" + jsonEscape(m_client_id) + "\"}";
                std::string reply;
                if (ipcSend(kOpHandshake, handshake) && ipcReadFrame(reply, 5000) &&
                    reply.find("READY") != std::string::npos) {
                    DEBUG_LOG_LAZY("discord", "Connected to Discord via ", path);
                    return true;
                }
                ipcClose();
            }
        }
    }
    return false;
#endif
}

void DiscordPresence::ipcClose()
{
#ifdef _WIN32
    if (m_pipe && m_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(m_pipe));
    }
    m_pipe = nullptr;
#else
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
}

} // namespace Discord
} // namespace PsyMP3

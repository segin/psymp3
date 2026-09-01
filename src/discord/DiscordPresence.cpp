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

#ifndef _WIN32
// Never let a write to a vanished Discord raise SIGPIPE: its default
// disposition terminates the process. Linux and the BSDs spell the
// per-call suppression MSG_NOSIGNAL; macOS/older BSD use the SO_NOSIGPIPE
// socket option instead (applied in ipcConnect).
#ifdef MSG_NOSIGNAL
static constexpr int kSendFlags = MSG_NOSIGNAL;
#else
static constexpr int kSendFlags = 0;
#endif
#endif

// IPC opcodes (the framed-JSON local RPC protocol).
static constexpr uint32_t kOpHandshake = 0;
static constexpr uint32_t kOpFrame = 1;
static constexpr uint32_t kOpClose = 2;
static constexpr uint32_t kOpPing = 3;
static constexpr uint32_t kOpPong = 4;

// MusicBrainz relevance score (0-100) below which its best hit is treated as
// no match rather than as artwork.
static constexpr long kMinMatchScore = 85;

// Appended to the artist while paused. Sized here so the clamp can reserve
// room for it instead of truncating it back off.
static const char kPausedSuffix[] = " (paused)";

// Characters, not bytes: every byte that is not a UTF-8 continuation byte
// starts a new codepoint.
static size_t utf8Length(const std::string& s)
{
    size_t n = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) ++n;
    }
    return n;
}

// Discord rejects activity strings shorter than 2 characters or longer than
// 128. Truncate on a UTF-8 codepoint boundary (bytes are the conservative
// bound for the maximum), and pad a too-short string rather than have the
// whole activity refused.
static std::string clampActivityString(const std::string& s, size_t max_bytes = 128)
{
    std::string out = s;
    if (out.size() > max_bytes) {
        size_t cut = max_bytes;
        while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) {
            --cut; // never split a multi-byte codepoint
        }
        out.resize(cut);
    }
    // The minimum is two CHARACTERS. Testing out.size() measured bytes, so a
    // title of a single non-ASCII codepoint ("↺", 3 bytes) sailed past the
    // check as if it were long enough and Discord refused the activity.
    while (!out.empty() && utf8Length(out) < 2) {
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
        } else if (m_last_visible.visible) {
            // m_last_visible, not m_current: m_current tracks the last state
            // the worker acted on, which after a disable is the CLEAR, so
            // re-enabling while paused used to restore nothing at all.
            queue_unlocked(m_last_visible); // re-show what was playing
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
    if (a.visible) {
        // Remembered so re-enabling can restore what was on screen: m_current
        // follows the CLEAR that disabling queues, so it cannot serve here.
        m_last_visible = a;
        if (!m_enabled) {
            // Dropping visible updates while disabled is not just an
            // optimisation: letting one through would overwrite the CLEAR
            // queued by the disable, and the presence would stay on screen.
            return;
        }
    }
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
    // Whether Discord is currently showing a presence for us. Worker-only, so
    // it needs no lock; used to avoid dialling up Discord purely to clear a
    // presence that was never displayed.
    bool displayed = false;
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
                // Record the DESIRED state here, not after a successful send.
                // Recording it only on success meant a failed connect threw
                // the activity away entirely -- and since the reconnect
                // re-send below is gated on m_current.visible, starting
                // PsyMP3 before Discord left the presence permanently blank
                // until the user happened to pause, seek or change track.
                // Assigning at dequeue also keeps ordering honest: a newer
                // update queued while we are out of the lock sends next and
                // overwrites this, instead of a stale value being written
                // back after the send window.
                m_current = to_send;
            } else if (!connected && m_enabled && m_current.visible) {
                to_send = m_current; // reconnect attempt: restore the presence
                have_work = true;
            }
        }

        if (!m_enabled) {
            // Disabled: a queued CLEAR still goes out so a visible presence
            // vanishes; everything else is dropped. Closing here is what
            // actually releases the socket when the user turns the
            // integration off -- the old `connected && !have_work` guard
            // could not fire on the disable path, because disabling queues a
            // CLEAR and so always arrived with have_work set.
            if (!have_work || to_send.visible || !displayed) {
                // !displayed: nothing is on screen, so there is nothing to
                // clear and no reason to dial up a fresh RPC session (which
                // is what made quitting connect to Discord while disabled).
                if (connected) { ipcClose(); connected = false; }
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
            // m_current already holds this activity (recorded at dequeue), so
            // neither path writes it back: doing so on success could clobber
            // a newer update queued during the send window, and doing so only
            // for visible activities on failure meant a failed CLEAR left the
            // old track in m_current and republished it on reconnect.
            if (sendActivity(to_send)) {
                displayed = to_send.visible;
            } else {
                DEBUG_LOG_LAZY("discord", "Send failed - dropping connection");
                ipcClose();
                connected = false;
            }
            // Having delivered the CLEAR that disabling queued, let go of the
            // socket rather than sitting on an idle connection to a service
            // the user has switched off.
            if (!m_enabled && connected) { ipcClose(); connected = false; }
        }
        if (m_shutdown) break;
    }
}

bool DiscordPresence::sendActivity(const Activity& a)
{
    // Also the liveness check: a false return means Discord closed the
    // socket, so writing would hit a dead peer.
    if (!ipcDrain()) {
        DEBUG_LOG_LAZY("discord", "Peer closed the connection before send");
        return false;
    }

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
            // Reserve room for the marker BEFORE appending it: the artist is
            // already clamped to 128 bytes, so appending first and clamping
            // after simply cut the marker away again for long artist names.
            std::string state = a.artist;
            if (a.paused) {
                state = clampActivityString(a.artist, 128 - (sizeof(kPausedSuffix) - 1));
                state += kPausedSuffix;
            }
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
    // Discord's verdict actually decides the outcome: a rejected payload (a
    // field the client's validator refuses) comes back as an ERROR event, and
    // reporting that as success meant the presence silently failed to apply
    // while the log said it had been updated.
    std::string reply;
    uint32_t op = kOpFrame;
    if (!ipcReadFrame(reply, 1000, &op)) {
        DEBUG_LOG_LAZY("discord", "No reply to SET_ACTIVITY - assuming dead connection");
        return false;
    }
    if (op == kOpClose) {
        DEBUG_LOG_LAZY("discord", "Discord closed the connection: ", reply);
        return false;
    }
    if (op == kOpPing) {
        // Keep-alive interleaved with our reply: answer it and take the next
        // frame as the actual response.
        ipcSend(kOpPong, reply);
        if (!ipcReadFrame(reply, 1000, &op)) return false;
    }
    if (reply.find("\"evt\":\"ERROR\"") != std::string::npos) {
        DEBUG_LOG_LAZY("discord", "Discord rejected the activity: ", reply);
        return false;
    }
    DEBUG_LOG_LAZY("discord", a.visible ? "Presence updated" : "Presence cleared");
    return true;
}

bool DiscordPresence::waitOrShutdown(std::chrono::milliseconds d)
{
    // Returns false if shutdown was requested during the wait. Plain
    // sleep_for() here made quit sit for seconds with the window still up:
    // the worker has to finish its nap before it can see m_shutdown.
    std::unique_lock<std::mutex> lock(m_mutex);
    return !m_cv.wait_for(lock, d, [this] { return m_shutdown.load(); });
}

void DiscordPresence::resolveArtwork(Activity& a)
{
    if (m_shutdown) {
        return; // quitting: never start a network lookup the join must await
    }
    if (!a.art_url.empty()) {
        // Art from a tagged MUSICBRAINZ_ALBUMID: trust it, but confirm the
        // image actually exists. Not every release has a front cover in the
        // Cover Art Archive, and taking the tag's word for it meant the
        // best-tagged files were the ones that ended up with no artwork --
        // and no album tooltip, which rides on the image.
        auto probe = PsyMP3::IO::HTTP::HTTPClient::head(
            a.art_url, {{"User-Agent", "PsyMP3/" PSYMP3_VERSION " ( segin2005@gmail.com )"}}, 5);
        if (probe.success) {
            return;
        }
        DEBUG_LOG_LAZY("discord", "Tagged release has no front cover (",
                       probe.statusCode, "); falling back to release-group search");
        a.art_url.clear();
    }
    if (a.album.empty()) {
        return; // no album = nothing to search for
    }
    const std::string& artist = a.lookup_artist.empty() ? a.artist : a.lookup_artist;
    if (artist.empty()) {
        return;
    }

    // One-entry memo: pause/seek/resume re-send the same track, and each
    // would otherwise hit MusicBrainz again. Deliberately NOT a persistent
    // cache - a new album is a fresh query every time.
    const std::string key = artist + "\n" + a.album;
    if (key == m_memo_key &&
        (m_memo_expiry == std::chrono::steady_clock::time_point{} ||
         std::chrono::steady_clock::now() < m_memo_expiry)) {
        // A definitive answer has no expiry; a transient failure gets a short
        // one so the album can recover its artwork later in the session.
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
        if (!waitOrShutdown(std::chrono::milliseconds(1000 - since.count()))) return;
    }
    m_last_mb_query = std::chrono::steady_clock::now();

    // Lucene phrase term. Stripping only the quote left backslashes in place,
    // and a trailing one escapes the closing quote: the query then fails to
    // parse and the empty result was cached as a definitive "no artwork".
    // Clamped too -- a pathological tag would otherwise become a multi-megabyte
    // request to musicbrainz.org.
    auto lucene_phrase = [](const std::string& v) {
        static constexpr size_t kMaxTerm = 200;
        std::string in = v;
        if (in.size() > kMaxTerm) {
            size_t cut = kMaxTerm;
            while (cut > 0 && (static_cast<unsigned char>(in[cut]) & 0xC0) == 0x80) --cut;
            in.resize(cut);
        }
        std::string out;
        out.reserve(in.size() + 8);
        for (char c : in) {
            if (c == '\\' || c == '"') out += '\\';
            out += c;
        }
        return out;
    };
    std::string query = "artist:\"" + lucene_phrase(artist) + "\" AND releasegroup:\"" +
                        lucene_phrase(a.album) + "\"";
    std::string url = "https://musicbrainz.org/ws/2/release-group/?limit=1&fmt=json&query=" +
                      PsyMP3::IO::HTTP::HTTPClient::urlEncode(query);

    DEBUG_LOG_LAZY("discord", "MusicBrainz release-group lookup: ", artist, " / ", a.album);

    // MusicBrainz is routinely briefly overloaded (HTTP 503 "server busy").
    // Retry a couple of times, and memoize only definitive answers — a 200
    // (match or genuine no-match) or a 4xx query rejection. A transport
    // failure or 5xx must NOT be memoized: presence updates are sparse
    // (track change, pause, seek), so a memoized flake would cost the whole
    // track its artwork AND the album tooltip that rides on it.
    PsyMP3::IO::HTTP::HTTPClient::Response response;
    bool definitive = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (m_shutdown) return; // don't start another 5s request while quitting
        if (attempt > 0) {
            if (!waitOrShutdown(std::chrono::milliseconds(1200))) return;
            m_last_mb_query = std::chrono::steady_clock::now();
        }
        response = PsyMP3::IO::HTTP::HTTPClient::get(
            url, {{"User-Agent", "PsyMP3/" PSYMP3_VERSION " ( segin2005@gmail.com )"}}, 5);
        // 408 (timeout), 425 (too early) and 429 (rate limited) are 4xx but
        // say nothing about whether the album exists; treating them as
        // definitive permanently suppressed artwork for that album.
        const bool retryable_4xx = response.statusCode == 408 ||
                                   response.statusCode == 425 ||
                                   response.statusCode == 429;
        if (response.success ||
            (response.statusCode >= 400 && response.statusCode < 500 && !retryable_4xx)) {
            definitive = true;
            break;
        }
        DEBUG_LOG_LAZY("discord", "MusicBrainz lookup attempt ", attempt + 1,
                       " failed: ", response.statusMessage);
    }
    if (!definitive) {
        // Remember the failure briefly. Not remembering it at all meant every
        // pause, seek and resume on the same track re-ran the whole 3-request
        // retry loop, stalling the presence worker each time.
        DEBUG_LOG_LAZY("discord",
                       "MusicBrainz lookup transient failure; backing off briefly");
        m_memo_key = key;
        m_memo_mbid.clear();
        m_memo_expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        return;
    }

    m_memo_key = key;
    m_memo_mbid.clear();
    m_memo_expiry = {}; // definitive: hold for the rest of the session
    if (response.success) {
        // First group id in the result: "release-groups":[{"id":"<uuid>",...
        size_t groups = response.body.find("\"release-groups\"");
        if (groups != std::string::npos) {
            size_t idpos = response.body.find("\"id\":\"", groups);
            if (idpos != std::string::npos && idpos + 7 + 36 <= response.body.size()) {
                std::string mbid = response.body.substr(idpos + 6, 36);
                // MusicBrainz always returns its best hit, however poor: with
                // limit=1 a misspelled or obscure album still yields SOME
                // release group, and accepting it unconditionally put
                // confidently wrong cover art on screen. Its relevance score
                // (0-100) is the guard.
                long score = 0;
                size_t spos = response.body.find("\"score\":", groups);
                if (spos != std::string::npos) {
                    score = strtol(response.body.c_str() + spos + 8, nullptr, 10);
                }
                if (score < kMinMatchScore) {
                    DEBUG_LOG_LAZY("discord", "MusicBrainz best match scored ", score,
                                   " (< ", kMinMatchScore, ") - treating as no match");
                } else if (PsyMP3::LastFM::LastFM::isValidMBID(mbid)) {
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
    // MSG_NOSIGNAL: writing to a stream socket whose peer has gone (the user
    // quit Discord) raises SIGPIPE, and SIGPIPE's default disposition kills
    // the process -- from this worker thread, taking the player down mid-song.
    // Nothing here installs a SIGPIPE handler, so the flag is the protection.
    // EINTR is a retry, not a dead connection.
    size_t off = 0;
    while (off < frame.size()) {
        ssize_t n = ::send(m_fd, frame.data() + off, frame.size() - off, kSendFlags);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        off += static_cast<size_t>(n);
    }
    return true;
#endif
}

bool DiscordPresence::ipcReadFrame(std::string& payload_out, int timeout_ms, uint32_t* opcode_out)
{
#ifdef _WIN32
    if (!m_pipe || m_pipe == INVALID_HANDLE_VALUE) return false;
    // Wait for the header to be available (Discord replies promptly). Falling
    // out of this loop used to drop into a BLOCKING ReadFile, so an expired
    // timeout blocked anyway; bail instead.
    bool header_ready = false;
    for (int waited = 0; waited < timeout_ms; waited += 50) {
        DWORD avail = 0;
        if (!PeekNamedPipe(static_cast<HANDLE>(m_pipe), nullptr, 0, nullptr, &avail, nullptr))
            return false;
        if (avail >= 8) { header_ready = true; break; }
        Sleep(50);
    }
    if (!header_ready) return false;
    uint8_t hdr[8];
    DWORD got = 0;
    if (!ReadFile(static_cast<HANDLE>(m_pipe), hdr, 8, &got, nullptr) || got != 8)
        return false;
    uint32_t op = hdr[0] | (hdr[1] << 8) | (hdr[2] << 16)
                | (static_cast<uint32_t>(hdr[3]) << 24);
    uint32_t len = hdr[4] | (hdr[5] << 8) | (hdr[6] << 16)
                 | (static_cast<uint32_t>(hdr[7]) << 24);
    if (opcode_out) *opcode_out = op;
    if (len > 1024 * 1024) {
        // Header consumed: the stream cannot resynchronise. Close it.
        DEBUG_LOG_LAZY("discord", "Oversized IPC frame (", len, " bytes) - closing");
        ipcClose();
        return false;
    }
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
    // Every read is bounded by one deadline. Previously only the first poll()
    // was timed: once a header arrived, the recv() loops blocked indefinitely,
    // so a peer that sent a partial frame and stalled hung the worker (and
    // therefore quit) forever.
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    auto recv_exact = [this, deadline](void* buf, size_t want) -> bool {
        size_t off = 0;
        while (off < want) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) return false;
            auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            struct pollfd pfd = {m_fd, POLLIN, 0};
            int rc = poll(&pfd, 1, static_cast<int>(left.count()));
            if (rc < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (rc == 0) return false; // timed out
            if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) return false;
            ssize_t n = ::recv(m_fd, static_cast<char*>(buf) + off, want - off, 0);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) return false; // 0 = peer closed
            off += static_cast<size_t>(n);
        }
        return true;
    };

    uint8_t hdr[8];
    if (!recv_exact(hdr, 8)) return false;
    uint32_t op = hdr[0] | (hdr[1] << 8) | (hdr[2] << 16)
                | (static_cast<uint32_t>(hdr[3]) << 24);
    uint32_t len = hdr[4] | (hdr[5] << 8) | (hdr[6] << 16)
                 | (static_cast<uint32_t>(hdr[7]) << 24);
    if (opcode_out) *opcode_out = op;
    if (len > 1024 * 1024) {
        // The header is already consumed, so the stream can never resynchronise
        // -- every later read would be misframed. Drop the connection instead
        // of leaving a permanently desynchronised socket in place.
        DEBUG_LOG_LAZY("discord", "Oversized IPC frame (", len, " bytes) - closing");
        ipcClose();
        return false;
    }
    payload_out.resize(len);
    if (len > 0 && !recv_exact(&payload_out[0], len)) return false;
    return true;
#endif
}

bool DiscordPresence::ipcDrain()
{
#ifdef _WIN32
    if (!m_pipe || m_pipe == INVALID_HANDLE_VALUE) return false;
    DWORD avail = 0;
    std::string sink;
    while (PeekNamedPipe(static_cast<HANDLE>(m_pipe), nullptr, 0, nullptr, &avail, nullptr) && avail >= 8) {
        if (!ipcReadFrame(sink, 0)) return false;
    }
    return true;
#else
    if (m_fd < 0) return false;
    std::string sink;
    for (;;) {
        struct pollfd pfd = {m_fd, POLLIN, 0};
        int rc = poll(&pfd, 1, 0);
        if (rc < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (rc == 0) return true; // nothing queued, connection still good
        // A closed peer reports POLLHUP (and POLLIN with a 0-byte read).
        // Reporting that here is what stops the caller writing into a dead
        // socket, which is where SIGPIPE came from.
        if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) return false;
        if (!(pfd.revents & POLLIN)) return true;
        uint32_t op = kOpFrame;
        if (!ipcReadFrame(sink, 0, &op)) return false;
        if (op == kOpClose) return false;
        if (op == kOpPing) ipcSend(kOpPong, sink); // Discord drops unanswered pings
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
        if (h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY) {
            // The pipe exists and Discord IS running; every instance is just
            // busy this instant. Treating that as "not installed" cost a full
            // reconnect cycle, so wait briefly for a free instance instead.
            if (WaitNamedPipeA(path.c_str(), 500)) {
                h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                nullptr, OPEN_EXISTING, 0, nullptr);
            }
        }
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
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
                // Platforms without MSG_NOSIGNAL suppress SIGPIPE per-socket.
                { int on = 1; ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)); }
#endif
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

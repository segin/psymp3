/*
 * DiscordPresence.h - Discord Rich Presence ("Listening to PsyMP3") client
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
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

#ifndef DISCORDPRESENCE_H
#define DISCORDPRESENCE_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Discord {

/**
 * @brief Discord Rich Presence client ("Listening to PsyMP3").
 *
 * Talks the framed-JSON IPC protocol to the local Discord desktop client
 * (unix socket discord-ipc-N on POSIX, named pipe \\.\pipe\discord-ipc-N on
 * Windows). Presence is an activity of type 2 (Listening): track title,
 * artist, album, a client-rendered progress bar from start/end timestamps,
 * and cover art from the Cover Art Archive when the track carries a
 * MusicBrainz release ID.
 *
 * Threading follows the LastFM worker model: public setters record a
 * last-write-wins pending state under the mutex and notify; a single worker
 * thread owns all socket I/O, connecting with a retry cadence while Discord
 * is not running and re-sending the current state after a reconnect.
 * Destruction joins the worker (clearing the presence first, best-effort).
 */
class DiscordPresence {
public:
    DiscordPresence();
    ~DiscordPresence();

    DiscordPresence(const DiscordPresence&) = delete;
    DiscordPresence& operator=(const DiscordPresence&) = delete;

    /**
     * @brief Enable/disable the integration. Disabling clears any visible
     * presence and closes the connection; enabling (re)starts connecting.
     */
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }

    /**
     * @brief Show a playing track. Position/length in seconds (length 0 =
     * unknown: no progress bar). release_mbid, when a well-formed UUID,
     * selects Cover Art Archive artwork.
     */
    void setNowPlaying(const std::string& artist, const std::string& title,
                       const std::string& album, unsigned int length_s,
                       unsigned int position_s, const std::string& release_mbid,
                       const std::string& lookup_artist = "");

    /**
     * @brief Keep the current track visible but drop the progress bar
     * (paused). No-op if nothing was playing.
     */
    void setPaused();

    /** @brief Remove the presence entirely (stopped). */
    void clear();

private:
    struct Activity {
        bool visible = false;   // false: clear the presence
        bool paused = false;
        std::string title;
        std::string artist;
        std::string album;
        std::string art_url;    // empty: no artwork (yet)
        std::string lookup_artist; // primary artist, for the MusicBrainz search
        long long start_ms = 0; // unix ms; 0 = no timestamps
        long long end_ms = 0;
    };

    void workerLoop();
    // Worker-thread only: when a visible activity has no artwork, search
    // MusicBrainz for the RELEASE GROUP by artist+album and point art_url at
    // the Cover Art Archive. The release group is the album as a whole rather
    // than one pressing of it: a search pinned to a single release picks an
    // arbitrary one of the (often dozens of) regional editions, and whichever
    // it lands on may be the one nobody uploaded art for -- e.g. "Nine Track
    // Mind" has 27 releases and the first hit, the JP pressing, is the only
    // one of the top seven with no front cover. CAA's release-group endpoint
    // serves the art of a representative release that has some.
    // One-entry memo of the current album (seek/pause re-sends), >=1s between
    // queries; no persistent cache.
    bool waitOrShutdown(std::chrono::milliseconds d);
    void resolveArtwork(Activity& a);
    void queue_unlocked(const Activity& a);

    // IPC (worker thread only)
    bool ipcConnect();
    void ipcClose();
    bool ipcSend(uint32_t opcode, const std::string& payload);
    bool ipcReadFrame(std::string& payload_out, int timeout_ms);
    bool ipcDrain();
    bool sendActivity(const Activity& a);

    static std::string jsonEscape(const std::string& s);

    std::string m_client_id;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_worker;
    std::atomic<bool> m_shutdown = false;
    std::atomic<bool> m_enabled = false;
    bool m_has_pending = false;
    Activity m_pending;   // last-write-wins
    Activity m_current;   // re-sent after reconnect
    Activity m_last_visible; // last visible activity, restored on re-enable
    uint64_t m_nonce = 0;
    // resolveArtwork() state (worker-thread only, no locking)
    std::string m_memo_key;
    std::string m_memo_mbid;   // release-group MBID, empty when the search missed
    std::chrono::steady_clock::time_point m_last_mb_query{};

#ifdef _WIN32
    void* m_pipe = nullptr; // HANDLE; INVALID_HANDLE_VALUE/nullptr = closed
#else
    int m_fd = -1;
#endif
};

} // namespace Discord
} // namespace PsyMP3

#endif // DISCORDPRESENCE_H

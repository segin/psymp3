/*
 * LastFM.h - Last.fm audioscrobbler implementation
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef LASTFM_H
#define LASTFM_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace LastFM {

/**
 * @brief Last.fm scrobbler client using the Web Services API 2.0
 *
 * Authenticates with auth.getMobileSession (username + password over HTTPS,
 * signed with the application's API key/shared secret) and submits plays via
 * track.scrobble / track.updateNowPlaying. The session key Last.fm issues has
 * an indefinite lifetime and is persisted to lastfm.conf, so the password is
 * only needed until the first successful authentication.
 *
 * Provides scrobbling functionality with XML-based local caching for failed
 * submissions. Implements background batch processing without limits.
 *
 * Threading Safety (Requirements 7.1, 7.2, 7.4):
 * This class follows the public/private lock pattern for thread safety.
 * All public methods that access shared state acquire locks and call
 * corresponding _unlocked private implementations.
 * 
 * Lock acquisition order (to prevent deadlocks):
 * 1. m_scrobble_mutex - protects:
 *    - m_scrobbles queue (enqueue/dequeue operations)
 *    - m_backoff_seconds (exponential backoff state)
 *    - Cache file operations (saveScrobbles_unlocked)
 * 
 * Thread-safe public methods:
 * - scrobbleTrack() - adds to queue, notifies submission thread
 * - getCachedScrobbleCount() - returns queue size
 * - forceSubmission() - triggers immediate submission
 * - saveScrobbles() - persists queue to cache file
 * 
 * Thread-safe internal operations:
 * - submissionThreadLoop() - background thread, acquires lock for queue access
 * - submitSavedScrobbles() - acquires lock for batch extraction
 * 
 * Non-locking methods (read immutable config or perform network I/O):
 * - isConfigured() - reads config set at construction
 * - setNowPlaying(), unsetNowPlaying() - network I/O only
 * - submitScrobble() - network I/O only
 * 
 * Graceful shutdown (Requirements 7.3):
 * - Destructor sets m_shutdown flag and notifies condition variable
 * - Submission thread exits cleanly
 * - Pending scrobbles are saved to cache before destruction
 */
/**
 * @brief Request to update now-playing status
 */
struct NowPlayingRequest {
    std::string artist;
    std::string title;
    std::string album;
    int length;
    bool is_clear;  // true to clear now-playing, false to set it
    
    NowPlayingRequest() : length(0), is_clear(true) {}
    NowPlayingRequest(const std::string& a, const std::string& t, const std::string& al, int len)
        : artist(a), title(t), album(al), length(len), is_clear(false) {}
};

class LastFM {
private:
    std::queue<Scrobble> m_scrobbles;
    std::queue<NowPlayingRequest> m_nowplaying_requests;
    std::string m_session_key;    // Web Services 2.0 session key (indefinite lifetime, persisted to lastfm.conf)
    std::string m_username;
    std::string m_password;       // Plaintext password, required by auth.getMobileSession. Held only until a session key exists; cleansed on destruction.
    std::string m_config_file;
    std::string m_cache_file;

    // Background submission thread
    std::thread m_submission_thread;
    mutable std::mutex m_scrobble_mutex;
    std::condition_variable m_submission_cv;
    std::atomic<bool> m_shutdown = false;
    std::atomic<bool> m_submission_active = false;
    std::atomic<bool> m_has_nowplaying_request = false;  // Fast check without lock
    int m_handshake_attempts = 0;
    bool m_handshake_permanently_failed = false;
    
    // Exponential backoff state (Requirements 4.3)
    int m_backoff_seconds = 0;
    static constexpr int INITIAL_BACKOFF_SECONDS = 60;  // Start at 1 minute
    static constexpr int MAX_BACKOFF_SECONDS = 3600;    // Cap at 1 hour
    
    // Backoff management methods (called with lock held)
    void resetBackoff_unlocked();
    void increaseBackoff_unlocked();
    
    // Web Services API 2.0 plumbing.
    // A call's result: `ok` when Last.fm answered with status="ok"; otherwise
    // error_code is the Last.fm error number (0 for a transport-level failure)
    // and message describes what went wrong.
    struct WsResponse {
        bool ok = false;
        int error_code = 0;
        std::string message;
        std::string body;
    };
    static std::string apiSignature(const std::map<std::string, std::string>& params);
    static WsResponse wsCall(std::map<std::string, std::string> params, int timeout_seconds);
    bool authenticate();          // auth.getMobileSession -> m_session_key
    void persistSessionKey();     // rewrite lastfm.conf's session_key= line

    // Configuration and cache management.
    // lastfm.conf holds username= / password= (user- or dialog-written) plus
    // the app-written session_key=. Credential edits go through the
    // Settings dialog; the app itself only ever rewrites the session key.
    void readConfig();
    std::string getSessionKey();
    void loadScrobbles();  // Called only from constructor (single-threaded)
    void saveScrobbles();  // Public wrapper acquires lock
    void saveScrobbles_unlocked();  // Assumes lock is held
    
    // Network operations (no lock needed - pure network I/O)
    bool submitScrobble(const std::string& artist, const std::string& title, 
                       const std::string& album, int length, time_t timestamp);
    
    // Background thread functions
    void submissionThreadLoop();
    void submitSavedScrobbles();
    void processNowPlayingRequests();  // Process pending now-playing requests
    bool submitNowPlayingRequest(const NowPlayingRequest& request);  // Actually perform HTTP POST
    bool expireNowPlaying(int timeout_seconds);  // cancel the visible now-playing status

    // Last successfully-submitted now-playing track, so a clear request (or
    // shutdown) can expire it. Guarded by m_scrobble_mutex.
    std::string m_np_artist;
    std::string m_np_title;
    bool m_np_active = false;

    // Queue access helpers (assumes lock is held)
    size_t getQueueSize_unlocked() const;
    bool isQueueEmpty_unlocked() const;
    
    // Scrobble queue operations (assumes lock is held)
    void scrobbleTrack_unlocked(Scrobble&& scrobble);
    
public:
    /**
     * @brief URL encode a string for use in Last.fm API requests
     * Follows RFC 3986 unreserved character set.
     * @param input The string to encode
     * @return URL-encoded string
     */
    static std::string urlEncode(const std::string& input);

private:
    /**
     * @brief Hash a string using MD5 for protocol compatibility
     * Labeled to distinguish from secure hashing (SEC-02)
     * @param input The string to hash
     * @return Lowercase hexadecimal MD5 hash
     */
    static std::string protocolMD5(const std::string& input);
    
public:
    LastFM();
    ~LastFM();
    
    /**
     * @brief Set the now playing track on Last.fm
     * @param track The track currently playing
     * @return true if successful, false otherwise
     */
    bool setNowPlaying(const track& track);
    
    /**
     * @brief Clear the now playing status on Last.fm
     * Used when pausing or stopping playback
     * @return true if successful, false otherwise
     */
    bool unsetNowPlaying();
    
    /**
     * @brief Add a track to the scrobble queue
     * @param track The track to scrobble
     * @return true if added successfully, false otherwise
     */
    bool scrobbleTrack(const track& track);
    
    /**
     * @brief Get the number of cached scrobbles waiting to be submitted
     * @return Number of cached scrobbles
     */
    size_t getCachedScrobbleCount() const;
    
    /**
     * @brief Force immediate submission of all cached scrobbles
     */
    void forceSubmission();
    
    /**
     * @brief Check if Last.fm is properly configured
     * @return true if username and password are set
     */
    bool isConfigured() const;

    /**
     * @brief The configured Last.fm username (empty when unconfigured)
     */
    std::string getUsername() const;

    /**
     * @brief One-shot credential check via auth.getMobileSession.
     *
     * Performs a stateless Web Services 2.0 authentication with the given
     * credentials (the issued session key is discarded) and returns a
     * human-readable status string ("Authenticated OK",
     * "Rejected: bad username/password", ...). Blocking (network, up to ~10s)
     * - call from a worker thread, never the UI thread.
     */
    static std::string testCredentials(const std::string& username,
                                       const std::string& password);
};

} // namespace LastFM
} // namespace PsyMP3

#endif // LASTFM_H

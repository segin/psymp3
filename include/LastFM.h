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

#include "scrobble.h"

/**
 * @brief Last.fm audioscrobbler client using the legacy 1.2 submissions API
 * 
 * Provides scrobbling functionality with XML-based local caching for failed
 * submissions. Implements background batch processing without limits.
 */
class LastFM {
private:
    std::queue<Scrobble> m_scrobbles;
    std::string m_session_key;
    std::string m_username;
    std::string m_password;
    std::string m_config_file;
    std::string m_cache_file;
    
    // API endpoints - supports multiple hosts for redundancy
    std::array<std::string, 3> m_api_hosts = {
        "post.audioscrobbler.com",
        "post2.audioscrobbler.com", 
        "submissions.last.fm"
    };
    std::array<int, 3> m_api_ports = {80, 80, 80};
    
    // Submission URLs (obtained from handshake response)
    std::string m_submission_url;
    std::string m_nowplaying_url;
    
    // Background submission thread
    std::thread m_submission_thread;
    mutable std::mutex m_scrobble_mutex;
    std::condition_variable m_submission_cv;
    std::atomic<bool> m_shutdown = false;
    std::atomic<bool> m_submission_active = false;
    
    // Configuration and cache management
    void readConfig();
    void writeConfig();
    std::string getSessionKey();
    void loadScrobbles();
    void saveScrobbles();
    
    // Network operations
    bool submitScrobble(const std::string& artist, const std::string& title, 
                       const std::string& album, int length, time_t timestamp);
    
    // Background thread functions
    void submissionThreadLoop();
    void submitSavedScrobbles();
    bool performHandshake(int host_index);
    
    // URL encoding and utilities
    std::string urlEncode(const std::string& input);
    std::string md5Hash(const std::string& input);
    
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
};

#endif // LASTFM_H
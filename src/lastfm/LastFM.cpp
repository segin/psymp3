/*
 * LastFM.cpp - Last.fm audioscrobbler implementation
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <openssl/crypto.h>
#include <openssl/crypto.h>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace PsyMP3 {
namespace LastFM {

LastFM::LastFM() :
    m_config_file(System::getStoragePath().to8Bit(true) + "/lastfm.conf"),
    m_cache_file(System::getStoragePath().to8Bit(true) + "/scrobble_cache.xml")
{
    DEBUG_LOG_LAZY("lastfm", "Initializing Last.fm scrobbler");
    DEBUG_LOG_LAZY("lastfm", "Config file: ", m_config_file);
    DEBUG_LOG_LAZY("lastfm", "Cache file: ", m_cache_file);
    
    readConfig();
    loadScrobbles();
    
    // Start background submission thread
    m_submission_thread = std::thread(&LastFM::submissionThreadLoop, this);
    DEBUG_LOG_LAZY("lastfm", "Background submission thread started");
}

LastFM::~LastFM()
{
    // Signal shutdown and wait for thread to finish (Requirements 7.3)
    m_shutdown = true;
    m_submission_cv.notify_all();
    
    if (m_submission_thread.joinable()) {
        m_submission_thread.join();
    }
    
    // Save any remaining scrobbles to cache (Requirements 7.3)
    // Use public saveScrobbles() which acquires lock for thread safety
    saveScrobbles();
    writeConfig();
    
    DEBUG_LOG_LAZY("lastfm", "LastFM shutdown complete, pending scrobbles saved");
}

void LastFM::readConfig()
{
    DEBUG_LOG_LAZY("lastfm", "Reading configuration from ", m_config_file);
    std::ifstream config(m_config_file);
    if (!config.is_open()) {
        DEBUG_LOG_LAZY("lastfm", "Config file not found - Last.fm not configured");
        return;
    }
    
    std::string line;
    while (std::getline(config, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        
        std::string key = line.substr(0, equals);
        std::string value = line.substr(equals + 1);
        
        if (key == "username") {
            m_username = value;
            DEBUG_LOG_LAZY("lastfm", "Username loaded: ", m_username);
        } else if (key == "password") {
            // Legacy password entry - migrate to hash
            if (!value.empty()) {
                m_password_hash = protocolMD5(value);
                DEBUG_LOG_LAZY("lastfm", "Legacy password loaded and migrated to hash");
                // Securely clear the plain-text password from memory (Requirements 7.5, Security Directive)
                OPENSSL_cleanse(&value[0], value.length());
                OPENSSL_cleanse(&line[0], line.length());
            }
        } else if (key == "password_hash") {
            m_password_hash = value;
            DEBUG_LOG_LAZY("lastfm", "Password hash loaded");
        } else if (key == "session_key") {
            m_session_key = value;
            DEBUG_LOG_LAZY("lastfm", "Session key loaded");
        } else if (key == "now_playing_url") {
            m_nowplaying_url = value;
            DEBUG_LOG_LAZY("lastfm", "Now playing URL loaded: ", m_nowplaying_url);
        } else if (key == "submission_url") {
            m_submission_url = value;
            DEBUG_LOG_LAZY("lastfm", "Submission URL loaded: ", m_submission_url);
        }
    }
    
    if (isConfigured()) {
        DEBUG_LOG_LAZY("lastfm", "Configuration complete - scrobbling enabled");
    } else {
        DEBUG_LOG_LAZY("lastfm", "Missing username or password hash - scrobbling disabled");
    }
}

void LastFM::writeConfig()
{
    System::createStoragePath();

#ifndef _WIN32
    // Set umask to ensure file is created with 0600 permissions
    mode_t old_mask = umask(0077);
#endif

    std::ofstream config(m_config_file);

#ifndef _WIN32
    umask(old_mask);
#endif

    if (!config.is_open()) {
        DEBUG_LOG_LAZY("lastfm", "Failed to write config file: ", m_config_file);
        return;
    }
    
    config << "# Last.fm configuration\n";
    config << "username=" << m_username << "\n";
    // password_hash is not persisted for security reasons
    config << "session_key=" << m_session_key << "\n";
    config << "now_playing_url=" << m_nowplaying_url << "\n";
    config << "submission_url=" << m_submission_url << "\n";
}

std::string LastFM::getSessionKey()
{
    if (!m_session_key.empty() && !m_nowplaying_url.empty() && !m_submission_url.empty()) {
        return m_session_key;
    }
    
    if (m_handshake_permanently_failed) {
        return "";
    }

    // Try to get session key from each host
    for (int i = 0; i < 3; ++i) {
        if (performHandshake(i)) {
            m_handshake_attempts = 0; // Reset on success
            writeConfig(); // Save the session key
            return m_session_key;
        }
    }
    
    m_handshake_attempts++;
    DEBUG_LOG_LAZY("lastfm", "Failed to obtain session key from all hosts. Attempt #", m_handshake_attempts);

    if (m_handshake_attempts >= 3) {
        DEBUG_LOG_LAZY("lastfm", "Exceeded handshake retry limit. Disabling for this session.");
        m_handshake_permanently_failed = true;
    }

    return "";
}

bool LastFM::performHandshake(int host_index)
{
    if (m_username.empty() || m_password_hash.empty()) {
        DEBUG_LOG_LAZY("lastfm", "Username or password hash not configured");
        return false;
    }
    
    // Generate timestamp and auth token (double MD5 as per API spec)
    // Use cached password hash to avoid redundant computation (Requirements 1.3)
    time_t timestamp = time(nullptr);
    
    std::string auth_token = protocolMD5(m_password_hash + std::to_string(timestamp));
    
    // Build handshake URL using efficient string concatenation (Requirements 2.3)
    // Use HTTPS for security (SEC-04)
    std::string url;
    url.reserve(256);  // Pre-allocate reasonable size for URL
    
    url += "https://";
    url += m_api_hosts[host_index];
    url += ":";
    url += std::to_string(m_api_ports[host_index]);
    url += "/?hs=true&p=1.2.1&c=psy&v=3.0&u=";
    url += HTTPClient::urlEncode(m_username);
    url += "&t=";
    url += std::to_string(timestamp);
    url += "&a=";
    url += auth_token;
    
    DEBUG_LOG_LAZY("lastfm", "Performing handshake with ", m_api_hosts[host_index]);
    
    HTTPClient::Response response = HTTPClient::get(url, {{"Host", m_api_hosts[host_index]}}, 10); // 10 second timeout
    
    if (!response.success) {
        DEBUG_LOG_LAZY("lastfm", "Handshake failed - ", response.statusMessage);
        return false;
    }
    
    // Parse handshake response
    std::istringstream responseStream(response.body);
    std::string status;
    std::getline(responseStream, status);
    
    if (status.substr(0, 2) == "OK") {
        std::string sessionKey, nowPlayingUrl, submissionUrl;
        std::getline(responseStream, sessionKey);
        std::getline(responseStream, nowPlayingUrl);
        std::getline(responseStream, submissionUrl);
        
        if (!sessionKey.empty() && !submissionUrl.empty()) {
            m_session_key = sessionKey;
            m_nowplaying_url = nowPlayingUrl;
            m_submission_url = submissionUrl;
            DEBUG_LOG_LAZY("lastfm", "Handshake successful");
            DEBUG_LOG_LAZY("lastfm", "Now Playing URL: ", m_nowplaying_url);
            DEBUG_LOG_LAZY("lastfm", "Submission URL: ", m_submission_url);
            return true;
        }
    } else if (status.substr(0, 6) == "FAILED") {
        DEBUG_LOG_LAZY("lastfm", "Handshake failed - ", status);
    } else {
        DEBUG_LOG_LAZY("lastfm", "Unexpected handshake response: ", status);
    }
    
    return false;
}

void LastFM::loadScrobbles()
{
    std::ifstream cache(m_cache_file);
    if (!cache.is_open()) {
        return;
    }
    
    // Read entire file content
    std::string content((std::istreambuf_iterator<char>(cache)),
                        std::istreambuf_iterator<char>());
    
    if (content.empty()) {
        return;
    }
    
    try {
        XMLUtil::Element root = XMLUtil::parseXML(content);
        
        // Find all scrobble elements
        auto scrobbleElements = XMLUtil::findChildren(root, "scrobble");
        
        for (const XMLUtil::Element* scrobbleElement : scrobbleElements) {
            std::string scrobbleXML = XMLUtil::generateXML(*scrobbleElement);
            
            try {
                Scrobble scrobble = Scrobble::fromXML(scrobbleXML);
                m_scrobbles.push(scrobble);
            } catch (const std::exception& e) {
                DEBUG_LOG_LAZY("lastfm", "Failed to parse cached scrobble: ", e.what());
            }
        }
        
        DEBUG_LOG_LAZY("lastfm", "Loaded ", m_scrobbles.size(), " cached scrobbles");
        
    } catch (const std::exception& e) {
        DEBUG_LOG_LAZY("lastfm", "Failed to parse scrobble cache XML: ", e.what());
    }
}

void LastFM::saveScrobbles()
{
    std::lock_guard<std::mutex> lock(m_scrobble_mutex);
    saveScrobbles_unlocked();
}

void LastFM::saveScrobbles_unlocked()
{
    // Assumes m_scrobble_mutex is held by caller
    if (m_scrobbles.empty()) {
        // Remove cache file if no scrobbles
        std::remove(m_cache_file.c_str());
        return;
    }
    
    System::createStoragePath();
    std::ofstream cache(m_cache_file);
    if (!cache.is_open()) {
        DEBUG_LOG_LAZY("lastfm", "Failed to write cache file: ", m_cache_file);
        return;
    }
    
    // Create root element for scrobbles collection
    XMLUtil::Element root("scrobbles");
    
    // Pre-allocate children vector to reduce allocations (Requirements 5.2)
    root.children.reserve(m_scrobbles.size());
    
    // Reuse string buffer across iterations to minimize allocations (Requirements 5.2)
    std::string scrobbleXML;
    scrobbleXML.reserve(512);  // Pre-allocate reasonable size for scrobble XML
    
    // Create a copy of the queue to iterate through
    std::queue<Scrobble> temp_queue = m_scrobbles;
    while (!temp_queue.empty()) {
        // Parse the scrobble's XML and add it as a child element
        scrobbleXML = temp_queue.front().toXML();  // Reuse buffer
        try {
            XMLUtil::Element scrobbleElement = XMLUtil::parseXML(scrobbleXML);
            root.children.push_back(scrobbleElement);
        } catch (const std::exception& e) {
            DEBUG_LOG_LAZY("lastfm", "Failed to serialize scrobble: ", e.what());
        }
        temp_queue.pop();
    }
    
    // Write XML with declaration
    cache << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    cache << XMLUtil::generateXML(root) << "\n";
    
    DEBUG_LOG_LAZY("lastfm", "Saved ", m_scrobbles.size(), " scrobbles to cache");
}

void LastFM::submissionThreadLoop()
{
    System::setThisThreadName("lastfm-submission");
    
    while (!m_shutdown) {
        std::unique_lock<std::mutex> lock(m_scrobble_mutex);
        
        // If handshake has permanently failed, wait only for shutdown to avoid busy-wait
        if (m_handshake_permanently_failed) {
            m_submission_cv.wait(lock, [this] { return m_shutdown.load(); });
            break;
        }
        
        // Wait for scrobbles, now-playing requests, or shutdown, with backoff timeout (Requirements 4.1, 4.4)
        if (m_backoff_seconds > 0) {
            // If in backoff, wait with timeout instead of indefinitely
            auto timeout = std::chrono::seconds(m_backoff_seconds);
            m_submission_cv.wait_for(lock, timeout, [this] { 
                return !m_scrobbles.empty() || !m_nowplaying_requests.empty() || m_shutdown || m_handshake_permanently_failed; 
            });
        } else {
            // Normal wait when not in backoff
            m_submission_cv.wait(lock, [this] { 
                return !m_scrobbles.empty() || !m_nowplaying_requests.empty() || m_shutdown || m_handshake_permanently_failed; 
            });
        }
        
        if (m_shutdown || m_handshake_permanently_failed) break;
        
        // Process now-playing requests first (they're more time-sensitive)
        if (!m_nowplaying_requests.empty()) {
            m_submission_active = true;
            lock.unlock();
            
            processNowPlayingRequests();
            
            lock.lock();
            m_submission_active = false;
        }
        
        // Process scrobble submissions
        if (!m_scrobbles.empty() && !m_submission_active) {
            m_submission_active = true;
            lock.unlock();
            
            submitSavedScrobbles();
            
            lock.lock();
            m_submission_active = false;
        }
    }
}


void LastFM::resetBackoff_unlocked()
{
    m_backoff_seconds = 0;
    DEBUG_LOG_LAZY("lastfm", "Backoff reset - normal submission resumed");
}

void LastFM::increaseBackoff_unlocked()
{
    if (m_backoff_seconds == 0) {
        m_backoff_seconds = INITIAL_BACKOFF_SECONDS;
    } else {
        m_backoff_seconds = std::min(m_backoff_seconds * 2, MAX_BACKOFF_SECONDS);
    }
    DEBUG_LOG_LAZY("lastfm", "Backoff increased to ", m_backoff_seconds, " seconds");
}

size_t LastFM::getQueueSize_unlocked() const
{
    return m_scrobbles.size();
}

bool LastFM::isQueueEmpty_unlocked() const
{
    return m_scrobbles.empty();
}

void LastFM::submitSavedScrobbles()
{
    if ((m_session_key.empty() || m_submission_url.empty()) && getSessionKey().empty()) {
        DEBUG_LOG_LAZY("lastfm", "Cannot submit scrobbles without valid session key and submission URL");
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        increaseBackoff_unlocked();  // Apply backoff on failure (Requirements 4.3)
        return;
    }
    
    // Submit scrobbles in batches (limited to 5 per request as per user requirement)
    // API spec allows up to 50, but user requested max 5
    const int batch_size = 5;
    int submitted = 0;
    
    // Collect scrobbles to submit while holding lock, then release lock for network I/O
    // This prevents holding the lock during potentially slow network operations
    std::vector<Scrobble> to_submit;
    {
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        while (!isQueueEmpty_unlocked() && static_cast<int>(to_submit.size()) < batch_size) {
            to_submit.push_back(m_scrobbles.front());
            m_scrobbles.pop();
        }
    }
    
    // Submit without holding lock (network I/O can be slow)
    std::vector<Scrobble> failed_scrobbles;
    for (const auto& scrobble : to_submit) {
        bool success = submitScrobble(scrobble.getArtistStr(), scrobble.getTitleStr(), 
                                     scrobble.getAlbumStr(), scrobble.GetLen(), 
                                     scrobble.getTimestamp());
        
        if (success) {
            submitted++;
        } else {
            DEBUG_LOG_LAZY("lastfm", "Failed to submit scrobble, keeping in cache");
            // Keep this and all remaining scrobbles for retry
            failed_scrobbles.push_back(scrobble);
            break; // Don't try more if this one failed
        }
    }
    
    // Re-queue failed scrobbles and any remaining ones we didn't try
    {
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        
        // Add back failed scrobbles to front of queue (they should be retried first)
        // We need to rebuild the queue with failed items at front
        if (!failed_scrobbles.empty()) {
            std::queue<Scrobble> new_queue;
            for (auto& s : failed_scrobbles) {
                new_queue.push(std::move(s));
            }
            // Add remaining items from to_submit that weren't tried
            size_t failed_idx = failed_scrobbles.size();
            for (size_t i = submitted + failed_idx; i < to_submit.size(); ++i) {
                new_queue.push(std::move(to_submit[i]));
            }
            // Add existing queue items
            while (!m_scrobbles.empty()) {
                new_queue.push(std::move(m_scrobbles.front()));
                m_scrobbles.pop();
            }
            m_scrobbles = std::move(new_queue);
            increaseBackoff_unlocked();  // Apply backoff on failure (Requirements 4.3)
        }
        
        if (submitted > 0) {
            DEBUG_LOG_LAZY("lastfm", "Successfully submitted ", submitted, " scrobbles");
            resetBackoff_unlocked();  // Reset backoff on successful submission (Requirements 4.3)
            saveScrobbles_unlocked(); // Update cache
        }
    }
}

bool LastFM::submitScrobble(const std::string& artist, const std::string& title, 
                           const std::string& album, int length, time_t timestamp)
{
    if (m_session_key.empty()) {
        DEBUG_LOG_LAZY("lastfm", "No session key available for scrobble submission");
        return false;
    }
    
    // Build POST data for Last.fm 1.2 scrobble API
    // Use string concatenation with reserve() instead of ostringstream (Requirements 2.1, 2.4)
    std::string postData;
    postData.reserve(512);  // Pre-allocate reasonable size for POST data
    
    postData += "s=";
    postData += HTTPClient::urlEncode(m_session_key);
    postData += "&a[0]=";
    postData += HTTPClient::urlEncode(artist);
    postData += "&t[0]=";
    postData += HTTPClient::urlEncode(title);
    postData += "&i[0]=";
    postData += std::to_string(timestamp);
    postData += "&o[0]=P";  // Source: P = chosen by user
    postData += "&r[0]=";   // Rating (empty)
    postData += "&l[0]=";
    postData += std::to_string(length);
    postData += "&b[0]=";
    postData += HTTPClient::urlEncode(album);
    postData += "&n[0]=";   // Track number (empty)
    postData += "&m[0]=";
    postData += protocolMD5(artist + title);  // MusicBrainz ID (using hash as fallback)
    
    // Use submission URL from handshake response
    if (m_submission_url.empty()) {
        DEBUG_LOG_LAZY("lastfm", "No submission URL available");
        return false;
    }
    
    HTTPClient::Response response = HTTPClient::post(m_submission_url, postData, 
                                                    "application/x-www-form-urlencoded", {}, 10);
        
    if (response.success) {
        std::istringstream responseStream(response.body);
        std::string status;
        std::getline(responseStream, status);
        
        if (status == "OK") {
            DEBUG_LOG_LAZY("lastfm", "Scrobble submitted successfully: ", artist, " - ", title);
            return true;
        } else if (status.substr(0, 6) == "FAILED") {
            DEBUG_LOG_LAZY("lastfm", "Scrobble submission failed - ", status);
            // Clear session for authentication failures
            if (status.find("BADAUTH") != std::string::npos) {
                m_session_key.clear(); // Force re-authentication
                m_submission_url.clear();
                m_nowplaying_url.clear();
            }
        } else {
            DEBUG_LOG_LAZY("lastfm", "Unexpected scrobble response: ", status);
        }
    } else {
        DEBUG_LOG_LAZY("lastfm", "HTTP error during scrobble submission: ", response.statusMessage);
    }
    
    return false;
}


bool LastFM::setNowPlaying(const track& track)
{
    if (!isConfigured()) {
        DEBUG_LOG_LAZY("lastfm", "Cannot set now playing - not configured");
        return false;
    }
    
    DEBUG_LOG_LAZY("lastfm", "Queueing now playing: ", track.GetArtist().to8Bit(true), " - ", track.GetTitle().to8Bit(true));
    
    // Create a now-playing request and queue it for background processing
    NowPlayingRequest request(
        track.GetArtist().to8Bit(true),
        track.GetTitle().to8Bit(true),
        track.GetAlbum().to8Bit(true),
        track.GetLen()
    );
    
    {
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        m_nowplaying_requests.push(std::move(request));
    }
    
    // Notify the background thread (outside lock to avoid holding lock during notification)
    m_submission_cv.notify_one();
    
    return true;  // Request queued successfully
}

bool LastFM::unsetNowPlaying()
{
    if (!isConfigured()) {
        DEBUG_LOG_LAZY("lastfm", "Cannot unset now playing - not configured");
        return false;
    }
    
    DEBUG_LOG_LAZY("lastfm", "Queueing clear now playing request");
    
    // Create a "clear" request (is_clear=true)
    NowPlayingRequest request;  // Default constructor sets is_clear=true
    
    {
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        m_nowplaying_requests.push(std::move(request));
    }
    
    // Notify the background thread
    m_submission_cv.notify_one();
    
    return true;  // Request queued successfully
}

void LastFM::processNowPlayingRequests()
{
    // Process all pending now-playing requests
    // This is called from the background thread with lock NOT held
    
    while (!m_shutdown) {
        NowPlayingRequest request;
        
        {
            std::lock_guard<std::mutex> lock(m_scrobble_mutex);
            if (m_nowplaying_requests.empty()) {
                break;
            }
            request = std::move(m_nowplaying_requests.front());
            m_nowplaying_requests.pop();
        }
        
        // Submit the request (network I/O, no lock held)
        submitNowPlayingRequest(request);
    }
}

bool LastFM::submitNowPlayingRequest(const NowPlayingRequest& request)
{
    // Ensure we have valid session and now playing URL
    if ((m_session_key.empty() || m_nowplaying_url.empty()) && getSessionKey().empty()) {
        DEBUG_LOG_LAZY("lastfm", "Cannot submit now playing without valid session key and now playing URL");
        return false;
    }
    
    // Build POST data for Last.fm 1.2 now playing API
    std::string postData;
    postData.reserve(512);
    
    postData += "s=";
    postData += HTTPClient::urlEncode(m_session_key);
    postData += "&a=";
    postData += HTTPClient::urlEncode(request.artist);
    postData += "&t=";
    postData += HTTPClient::urlEncode(request.title);
    postData += "&b=";
    postData += HTTPClient::urlEncode(request.album);
    postData += "&l=";
    postData += std::to_string(request.length);
    postData += "&n=";  // Track number (empty)
    postData += "&m=";
    postData += protocolMD5(request.artist + request.title);  // MusicBrainz ID fallback
    
    if (m_nowplaying_url.empty()) {
        DEBUG_LOG_LAZY("lastfm", "No now playing URL available");
        return false;
    }
    
    // Use shorter timeout (5 seconds) since this is background and non-critical
    HTTPClient::Response response = HTTPClient::post(m_nowplaying_url, postData, 
                                                    "application/x-www-form-urlencoded", {}, 5);
        
    if (response.success) {
        std::istringstream responseStream(response.body);
        std::string status;
        std::getline(responseStream, status);
        
        if (status == "OK") {
            if (request.is_clear) {
                DEBUG_LOG_LAZY("lastfm", "Now playing status cleared successfully");
            } else {
                DEBUG_LOG_LAZY("lastfm", "Now playing submitted successfully: ", request.artist, " - ", request.title);
            }
            return true;
        } else if (status.substr(0, 6) == "FAILED") {
            DEBUG_LOG_LAZY("lastfm", "Now playing submission failed - ", status);
            // Clear session for authentication failures
            if (status.find("BADAUTH") != std::string::npos) {
                m_session_key.clear();
                m_submission_url.clear();
                m_nowplaying_url.clear();
            }
        } else {
            DEBUG_LOG_LAZY("lastfm", "Unexpected now playing response: ", status);
        }
    } else {
        DEBUG_LOG_LAZY("lastfm", "HTTP error during now playing submission: ", response.statusMessage);
    }
    
    return false;
}


void LastFM::scrobbleTrack_unlocked(Scrobble&& scrobble)
{
    // Assumes m_scrobble_mutex is held by caller (Requirements 7.2)
    m_scrobbles.push(std::move(scrobble));  // Use move semantics to avoid copy
}

bool LastFM::scrobbleTrack(const track& track)
{
    if (!isConfigured()) {
        DEBUG_LOG_LAZY("lastfm", "Cannot scrobble - not configured");
        return false;
    }
    
    // Add to queue for background submission using move semantics (Requirements 5.1)
    Scrobble scrobble(track);
    
    {
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        scrobbleTrack_unlocked(std::move(scrobble));  // Use _unlocked variant (Requirements 7.2)
    }
    
    DEBUG_LOG_LAZY("lastfm", "Added scrobble to queue: ", track.GetArtist().to8Bit(true), " - ", track.GetTitle().to8Bit(true));
    
    // Notify submission thread (outside lock to avoid holding lock during notification)
    m_submission_cv.notify_one();
    
    return true;
}

size_t LastFM::getCachedScrobbleCount() const
{
    std::lock_guard<std::mutex> lock(m_scrobble_mutex);
    return getQueueSize_unlocked();
}

void LastFM::forceSubmission()
{
    {
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        if (isQueueEmpty_unlocked()) {
            return;
        }
    }
    m_submission_cv.notify_one();
}

bool LastFM::isConfigured() const
{
    return !m_username.empty() && !m_password_hash.empty();
}

std::string LastFM::urlEncode(const std::string& input)
{
    return HTTPClient::urlEncode(input);
}

std::string LastFM::protocolMD5(const std::string& input)
{
    // Optimized MD5 implementation for protocol compatibility.
    // Labeled to distinguish from secure hashing. (SEC-02)
    static constexpr char hex_chars[] = "0123456789abcdef";
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    
    if (EVP_Digest(input.c_str(), input.length(), hash, &hash_len, EVP_md5(), nullptr)) {
        
        // Pre-allocate output string (MD5 is 16 bytes = 32 hex chars)
        std::string result;
        result.reserve(hash_len * 2);
        
        // Convert bytes to hex using lookup table and bit shifting
        for (unsigned int i = 0; i < hash_len; i++) {
            result += hex_chars[(hash[i] >> 4) & 0x0F];
            result += hex_chars[hash[i] & 0x0F];
        }
        
        // Securely clear the hash from memory (Requirements 7.5, Security Directive)
        OPENSSL_cleanse(hash, sizeof(hash));

        return result;
    }
    
    return "";
}


} // namespace LastFM
} // namespace PsyMP3

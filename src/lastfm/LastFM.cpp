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

// Strip a trailing carriage return so CRLF-terminated lines parse the same as
// LF-terminated ones; std::getline only consumes the '\n'.
static inline std::string& chompCR(std::string& s)
{
    if (!s.empty() && s.back() == '\r') {
        s.pop_back();
    }
    return s;
}

// PsyMP3's registered Last.fm API application (Web Services 2.0).
static const char kApiKey[]    = "4bdeb98d813cdc75f69bfb9d9dd5e1b4";
static const char kApiSecret[] = "62c5e84649bf6d52dafd3d96cd1f63a7";
static const char kApiRoot[]   = "https://ws.audioscrobbler.com/2.0/";

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
    // Signal shutdown and wait for thread to finish (Requirements 7.3). The
    // store must happen under m_scrobble_mutex — the same mutex the submission
    // thread evaluates its wait predicate under — or a store+notify landing
    // between the waiter's predicate check and its block is lost and join()
    // hangs forever (no further notify arrives once submissions have stopped).
    {
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        m_shutdown = true;
    }
    m_submission_cv.notify_all();
    
    if (m_submission_thread.joinable()) {
        m_submission_thread.join();
    }
    
    // Save any remaining scrobbles to cache (Requirements 7.3)
    // Use public saveScrobbles() which acquires lock for thread safety
    saveScrobbles();
    // The session key was persisted to lastfm.conf the moment authentication
    // succeeded (persistSessionKey), so nothing further to write here.

    // Cancel any still-visible now-playing status so the profile doesn't
    // advertise a track for minutes after the player exited. Short timeout:
    // shutdown must not hang on a dead network.
    expireNowPlaying(3);

    // Securely clear credentials from memory (CWE-312)
    if (!m_password.empty()) {
        OPENSSL_cleanse(&m_password[0], m_password.length());
    }
    if (!m_session_key.empty()) {
        OPENSSL_cleanse(&m_session_key[0], m_session_key.length());
    }

    DEBUG_LOG_LAZY("lastfm", "LastFM shutdown complete, pending scrobbles saved");
}

void LastFM::readConfig()
{
    DEBUG_LOG_LAZY("lastfm", "Reading configuration from ", m_config_file);
    std::ifstream config(System::pathFromUtf8(m_config_file));
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
            // auth.getMobileSession needs the plaintext password (the API 2.0
            // signature scheme has no hashed-credential variant). It is kept in
            // memory only until the first successful authentication persists a
            // session key, and is cleansed on destruction.
            if (!value.empty()) {
                m_password = value;
                OPENSSL_cleanse(&value[0], value.length());
                OPENSSL_cleanse(&line[0], line.length());
                DEBUG_LOG_LAZY("lastfm", "Password loaded");
            }
        } else if (key == "password_hash") {
            // Written by ancient builds for the retired 1.2.1 submissions
            // protocol. The Web Services API cannot authenticate with an MD5
            // hash — the user must re-enter their password (Settings ->
            // Last.fm Credentials...).
            DEBUG_LOG_LAZY("lastfm", "Ignoring legacy password_hash - re-enter the password to use the new Last.fm API");
        } else if (key == "session_key") {
            // Issued by auth.getMobileSession; valid indefinitely unless the
            // user revokes the application, so reusing it across runs is the
            // supported (and recommended) flow.
            m_session_key = value;
            DEBUG_LOG_LAZY("lastfm", "Session key loaded");
        }
    }
    
    if (isConfigured()) {
        DEBUG_LOG_LAZY("lastfm", "Configuration complete - scrobbling enabled");
    } else {
        DEBUG_LOG_LAZY("lastfm", "Missing username or password hash - scrobbling disabled");
    }
}

std::string LastFM::apiSignature(const std::map<std::string, std::string>& params)
{
    // Web Services 2.0 signing: concatenate <name><value> pairs sorted by
    // parameter name (std::map iterates sorted), append the shared secret,
    // MD5 the lot. Values go in RAW (not URL-encoded).
    std::string sig_data;
    for (const auto& kv : params) {
        sig_data += kv.first;
        sig_data += kv.second;
    }
    sig_data += kApiSecret;
    std::string sig = protocolMD5(sig_data);
    OPENSSL_cleanse(&sig_data[0], sig_data.length()); // may contain the password
    return sig;
}

LastFM::WsResponse LastFM::wsCall(std::map<std::string, std::string> params, int timeout_seconds)
{
    params["api_key"] = kApiKey;
    const std::string api_sig = apiSignature(params);

    std::string post_data;
    post_data.reserve(512);
    for (const auto& kv : params) {
        post_data += urlEncode(kv.first);
        post_data += '=';
        post_data += urlEncode(kv.second);
        post_data += '&';
    }
    post_data += "api_sig=";
    post_data += api_sig;

    HTTPClient::Response response = HTTPClient::post(
        kApiRoot, post_data, "application/x-www-form-urlencoded", {}, timeout_seconds);
    OPENSSL_cleanse(&post_data[0], post_data.length()); // may contain the password
    for (auto& kv : params) {
        if (!kv.second.empty()) {
            OPENSSL_cleanse(&kv.second[0], kv.second.length());
        }
    }

    WsResponse out;
    out.body = response.body;
    // Last.fm reports API errors with a 4xx status AND an <lfm> body, so parse
    // the body whenever there is one; only a body-less failure is transport.
    pugi::xml_document doc;
    if (doc.load_buffer(response.body.data(), response.body.size())) {
        pugi::xml_node lfm = doc.child("lfm");
        if (lfm) {
            if (std::string(lfm.attribute("status").value()) == "ok") {
                out.ok = true;
                return out;
            }
            pugi::xml_node error = lfm.child("error");
            out.error_code = error.attribute("code").as_int();
            out.message = error.child_value();
            if (out.message.empty()) {
                out.message = "malformed error response";
            }
            return out;
        }
    }
    out.message = response.success
        ? "unexpected response from Last.fm"
        : "Network error: " + response.statusMessage;
    return out;
}

bool LastFM::authenticate()
{
    if (m_username.empty() || m_password.empty()) {
        DEBUG_LOG_LAZY("lastfm", "No password available to authenticate with");
        return false;
    }

    DEBUG_LOG_LAZY("lastfm", "Requesting mobile session for ", m_username);
    WsResponse response = wsCall({{"method", "auth.getMobileSession"},
                                  {"username", m_username},
                                  {"password", m_password}}, 10);

    if (response.ok) {
        // auth.getMobileSession: <lfm status="ok"><session>...<key>K</key>...
        pugi::xml_document doc;
        std::string key;
        if (doc.load_buffer(response.body.data(), response.body.size())) {
            key = doc.child("lfm").child("session").child("key").child_value();
        }
        if (!key.empty()) {
            m_session_key = key;
            persistSessionKey();
            // The password has served its purpose; the session key never
            // expires, so drop the plaintext from memory now.
            OPENSSL_cleanse(&m_password[0], m_password.length());
            m_password.clear();
            DEBUG_LOG_LAZY("lastfm", "Authenticated; session key persisted");
            return true;
        }
        DEBUG_LOG_LAZY("lastfm", "auth.getMobileSession OK but no session key in response");
        return false;
    }

    DEBUG_LOG_LAZY("lastfm", "auth.getMobileSession failed (code ", response.error_code,
                   "): ", response.message);
    // 4 = bad credentials, 10 = invalid API key, 26 = API key suspended —
    // retrying any of these with the same inputs can never succeed.
    if (response.error_code == 4 || response.error_code == 10 || response.error_code == 26) {
        m_handshake_permanently_failed = true;
    }
    return false;
}

void LastFM::persistSessionKey()
{
    // Rewrite lastfm.conf with the fresh session_key= line, preserving every
    // other line (username=, password=, user comments) verbatim.
    std::vector<std::string> kept;
    {
        std::ifstream in(System::pathFromUtf8(m_config_file));
        std::string line;
        while (std::getline(in, line)) {
            chompCR(line);
            if (line.rfind("session_key=", 0) == 0) continue;
            kept.push_back(line);
        }
    }

    System::createStoragePath();

#ifndef _WIN32
    // Credentials file: 0600, same as the scrobble cache.
    mode_t old_mask = umask(0077);
#endif
    std::ofstream out(System::pathFromUtf8(m_config_file), std::ios::trunc);
#ifndef _WIN32
    umask(old_mask);
#endif

    if (!out.is_open()) {
        DEBUG_LOG_LAZY("lastfm", "Failed to persist session key to ", m_config_file);
        return;
    }
    for (const auto& line : kept) {
        out << line << "\n";
    }
    if (!m_session_key.empty()) {
        out << "session_key=" << m_session_key << "\n";
    }
}

std::string LastFM::getSessionKey()
{
    if (!m_session_key.empty()) {
        return m_session_key;
    }

    if (m_handshake_permanently_failed) {
        return "";
    }

    if (authenticate()) {
        m_handshake_attempts = 0; // Reset on success
        return m_session_key;
    }

    m_handshake_attempts++;
    DEBUG_LOG_LAZY("lastfm", "Failed to obtain session key. Attempt #", m_handshake_attempts);

    if (m_handshake_attempts >= 3) {
        DEBUG_LOG_LAZY("lastfm", "Exceeded authentication retry limit. Disabling for this session.");
        m_handshake_permanently_failed = true;
    }

    return "";
}

void LastFM::loadScrobbles()
{
    std::ifstream cache(System::pathFromUtf8(m_cache_file));
    if (!cache.is_open()) {
        return;
    }
    
    // Read entire file content
    std::string content((std::istreambuf_iterator<char>(cache)),
                        std::istreambuf_iterator<char>());
    
    if (content.empty()) {
        return;
    }
    
    pugi::xml_document doc;
    pugi::xml_parse_result parsed = doc.load_buffer(content.data(), content.size());
    if (!parsed) {
        DEBUG_LOG_LAZY("lastfm", "Failed to parse scrobble cache XML: ", parsed.description());
        return;
    }

    for (pugi::xml_node node : doc.child("scrobbles").children("scrobble")) {
        Scrobble scrobble = Scrobble::fromXMLNode(node);
        // fromXMLNode returns an empty sentinel (blank fields, timestamp 0)
        // for unusable entries. Don't queue that — it would be submitted to
        // Last.fm as a garbage scrobble.
        if (scrobble.getTimestamp() > 0 &&
            !scrobble.getArtistStr().empty() &&
            !scrobble.getTitleStr().empty()) {
            m_scrobbles.push(scrobble);
        } else {
            DEBUG_LOG_LAZY("lastfm", "Skipping invalid/unparseable cached scrobble");
        }
    }

    DEBUG_LOG_LAZY("lastfm", "Loaded ", m_scrobbles.size(), " cached scrobbles");
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
        std::error_code remove_ec;
        std::filesystem::remove(System::pathFromUtf8(m_cache_file), remove_ec);
        return;
    }
    
    System::createStoragePath();

#ifndef _WIN32
    // Listening history is private; create the cache 0600 like the config file.
    mode_t old_mask = umask(0077);
#endif

    std::ofstream cache(System::pathFromUtf8(m_cache_file));

#ifndef _WIN32
    umask(old_mask);
#endif

    if (!cache.is_open()) {
        DEBUG_LOG_LAZY("lastfm", "Failed to write cache file: ", m_cache_file);
        return;
    }
    
    pugi::xml_document doc;
    pugi::xml_node decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";
    pugi::xml_node root = doc.append_child("scrobbles");

    // Create a copy of the queue to iterate through
    std::queue<Scrobble> temp_queue = m_scrobbles;
    while (!temp_queue.empty()) {
        temp_queue.front().appendXML(root);
        temp_queue.pop();
    }

    doc.save(cache, "  ");

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
            // If in backoff, sleep the full timeout and only wake early for
            // shutdown / permanent failure. Waking as soon as scrobbles (or
            // now-playing requests) are queued would defeat the backoff
            // entirely, since those are precisely what we are delaying a retry
            // of after a server failure.
            auto timeout = std::chrono::seconds(m_backoff_seconds);
            m_submission_cv.wait_for(lock, timeout, [this] {
                return m_shutdown.load() || m_handshake_permanently_failed;
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
    if (getSessionKey().empty()) {
        DEBUG_LOG_LAZY("lastfm", "Cannot submit scrobbles without a valid session key");
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
                                     scrobble.getTimestamp(), scrobble.getMusicBrainzID());
        
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
            // Only reset backoff on a fully-successful batch. On a partial
            // failure, increaseBackoff_unlocked() above just applied a backoff;
            // resetting it here would cancel it and retry immediately.
            if (failed_scrobbles.empty()) {
                resetBackoff_unlocked();  // Reset backoff on success (Requirements 4.3)
            }
            saveScrobbles_unlocked(); // Update cache
        }
    }
}

bool LastFM::submitScrobble(const std::string& artist, const std::string& title,
                           const std::string& album, int length, time_t timestamp,
                           const std::string& mbid)
{
    if (m_session_key.empty()) {
        DEBUG_LOG_LAZY("lastfm", "No session key available for scrobble submission");
        return false;
    }

    std::map<std::string, std::string> params = {
        {"method", "track.scrobble"},
        {"artist", artist},
        {"track", title},
        {"timestamp", std::to_string(timestamp)},
        {"sk", m_session_key},
    };
    if (!album.empty()) {
        params["album"] = album;
    }
    if (length > 0) {
        params["duration"] = std::to_string(length);
    }
    if (isValidMBID(mbid)) {
        params["mbid"] = mbid;
    }

    WsResponse response = wsCall(std::move(params), 10);

    if (response.ok) {
        // status="ok" covers "accepted" AND "ignored" (e.g. track too short):
        // an ignored scrobble was received and judged — resubmitting it can
        // only ever be ignored again, so both count as done.
        DEBUG_LOG_LAZY("lastfm", "Scrobble submitted successfully: ", artist, " - ", title);
        return true;
    }

    DEBUG_LOG_LAZY("lastfm", "Scrobble submission failed (code ", response.error_code,
                   "): ", response.message);
    if (response.error_code == 9) {
        // Invalid session key — the user revoked the application. Drop the
        // stored key so the next attempt re-authenticates (which needs the
        // password; if we no longer hold one, authenticate() will say so).
        m_session_key.clear();
        persistSessionKey();
    }
    return false;
}


bool LastFM::setNowPlaying(const track& track)
{
    if (!isConfigured()) {
        DEBUG_LOG_LAZY("lastfm", "Cannot set now playing - not configured");
        return false;
    }
    
    DEBUG_LOG_LAZY("lastfm", "Queueing now playing: ", track.GetScrobbleArtist().to8Bit(true),
                   " - ", track.GetTitle().to8Bit(true),
                   " (mbid: ", track.GetMusicBrainzID().to8Bit(true), ")");
    
    // Create a now-playing request and queue it for background processing.
    // Same artist policy as scrobbles: first artist of a multi-valued credit.
    NowPlayingRequest request(
        track.GetScrobbleArtist().to8Bit(true),
        track.GetTitle().to8Bit(true),
        track.GetAlbum().to8Bit(true),
        track.GetLen(),
        track.GetMusicBrainzID().to8Bit(true)
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
    if (request.is_clear) {
        return expireNowPlaying(5);
    }

    if (getSessionKey().empty()) {
        DEBUG_LOG_LAZY("lastfm", "Cannot submit now playing without a valid session key");
        return false;
    }

    std::map<std::string, std::string> params = {
        {"method", "track.updateNowPlaying"},
        {"artist", request.artist},
        {"track", request.title},
        {"sk", m_session_key},
    };
    if (!request.album.empty()) {
        params["album"] = request.album;
    }
    if (request.length > 0) {
        params["duration"] = std::to_string(request.length);
    }
    if (isValidMBID(request.mbid)) {
        params["mbid"] = request.mbid;
    }

    // Use shorter timeout (5 seconds) since this is background and non-critical
    WsResponse response = wsCall(std::move(params), 5);

    if (response.ok) {
        DEBUG_LOG_LAZY("lastfm", "Now playing submitted successfully: ", request.artist, " - ", request.title);
        {
            std::lock_guard<std::mutex> lock(m_scrobble_mutex);
            m_np_artist = request.artist;
            m_np_title = request.title;
            m_np_active = true;
        }
        return true;
    }

    DEBUG_LOG_LAZY("lastfm", "Now playing submission failed (code ", response.error_code,
                   "): ", response.message);
    return false;
}

bool LastFM::expireNowPlaying(int timeout_seconds)
{
    std::string artist, title;
    {
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        if (!m_np_active) {
            return true; // nothing showing, nothing to cancel
        }
        artist = m_np_artist;
        title = m_np_title;
    }

    if (m_session_key.empty()) {
        return false; // don't start an authentication round just to clear
    }

    // The Web Services API has no removeNowPlaying; the status is displayed
    // until `duration` runs out. Cancel it by re-submitting the SAME track
    // with duration=1, which replaces the live status with one that expires
    // immediately instead of lingering for the track's remaining length.
    WsResponse response = wsCall({{"method", "track.updateNowPlaying"},
                                  {"artist", artist},
                                  {"track", title},
                                  {"duration", "1"},
                                  {"sk", m_session_key}}, timeout_seconds);

    if (response.ok) {
        DEBUG_LOG_LAZY("lastfm", "Now playing status cancelled");
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        m_np_active = false;
        return true;
    }

    DEBUG_LOG_LAZY("lastfm", "Failed to cancel now playing (code ", response.error_code,
                   "): ", response.message);
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

bool LastFM::isValidMBID(const std::string& mbid)
{
    // MusicBrainz IDs are UUIDs: 8-4-4-4-12 hex groups, 36 chars total. The
    // old 1.2.1 submitter used to stuff an MD5-of-artist+title in this field;
    // validating here guarantees nothing like that ever reaches the API again.
    if (mbid.length() != 36) {
        return false;
    }
    for (size_t i = 0; i < mbid.length(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (mbid[i] != '-') {
                return false;
            }
        } else if (!std::isxdigit(static_cast<unsigned char>(mbid[i]))) {
            return false;
        }
    }
    return true;
}

bool LastFM::isConfigured() const
{
    // A persisted session key alone is enough — the password is only needed
    // to obtain one.
    return !m_username.empty() && (!m_password.empty() || !m_session_key.empty());
}

std::string LastFM::getUsername() const
{
    return m_username;
}

std::string LastFM::testCredentials(const std::string& username,
                                    const std::string& password)
{
    if (username.empty() || password.empty()) {
        return "Enter a username and password first";
    }

    // A stateless auth.getMobileSession round-trip: the issued session key is
    // discarded, so nothing here touches the live scrobbler's session.
    WsResponse response = wsCall({{"method", "auth.getMobileSession"},
                                  {"username", username},
                                  {"password", password}}, 10);

    if (response.ok) {
        return "Authenticated OK";
    }

    switch (response.error_code) {
        case 4:
            return "Rejected: bad username/password";
        case 10:
        case 26:
            return "Rejected: API key problem (" + response.message + ")";
        case 29:
            return "Rejected: rate limited, try again later";
        case 11:
        case 16:
            return "Last.fm is temporarily unavailable, try again later";
        case 0:
            return response.message; // transport-level: "Network error: ..."
        default:
            return "Failed: " + response.message +
                   " (code " + std::to_string(response.error_code) + ")";
    }
}

std::string LastFM::urlEncode(const std::string& input)
{
    if (input.empty()) {
        return "";
    }

    std::string output;
    output.reserve(input.length() * 3);

    static const char hex_chars[] = "0123456789ABCDEF";

    for (unsigned char c : input) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            output += c;
        } else {
            output += '%';
            output += hex_chars[(c >> 4) & 0x0F];
            output += hex_chars[c & 0x0F];
        }
    }
    return output;
}

std::string LastFM::protocolMD5(const std::string& input)
{
    // OVERRIDE: The Last.fm Submissions Protocol version 1.x strictly depends on
    // the use of the MD5 algorithm and does NOT provide provision for the use
    // of alternative hashsum algorithms.
    // Optimized MD5 implementation for protocol compatibility.
    // Labeled to distinguish from secure hashing. (SEC-02)
    // lgtm[cpp/weak-cryptographic-algorithm]
    static constexpr char hex_chars[] = "0123456789abcdef";
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    
    // NOLINTNEXTLINE(cert-msc50-cpp, cert-msc68-cpp)
    if (EVP_Digest(input.c_str(), input.length(), hash, &hash_len, EVP_md5(), nullptr)) {
        
        // Pre-allocate output string (MD5 is 16 bytes = 32 hex chars)
        std::string result;
        result.reserve(hash_len * 2);
        
        // Convert bytes to hex using lookup table and bit shifting
        for (unsigned int i = 0; i < hash_len; i++) {
            result += hex_chars[(hash[i] >> 4) & 0x0F];
            result += hex_chars[hash[i] & 0x0F];
        }
        
        // Securely clear the raw hash from stack memory
        OPENSSL_cleanse(hash, sizeof(hash));

        return result;
    }
    
    return "";
}


} // namespace LastFM
} // namespace PsyMP3

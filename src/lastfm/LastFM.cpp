/*
 * LastFM.cpp - Last.fm audioscrobbler implementation
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace LastFM {

LastFM::LastFM() :
    m_config_file(System::getStoragePath().to8Bit(true) + "/lastfm.conf"),
    m_cache_file(System::getStoragePath().to8Bit(true) + "/scrobble_cache.xml")
{
    Debug::log("lastfm", "Initializing Last.fm scrobbler");
    Debug::log("lastfm", "Config file: ", m_config_file);
    Debug::log("lastfm", "Cache file: ", m_cache_file);
    
    readConfig();
    loadScrobbles();
    
    // Start background submission thread
    m_submission_thread = std::thread(&LastFM::submissionThreadLoop, this);
    Debug::log("lastfm", "Background submission thread started");
}

LastFM::~LastFM()
{
    // Signal shutdown and wait for thread to finish
    m_shutdown = true;
    m_submission_cv.notify_all();
    
    if (m_submission_thread.joinable()) {
        m_submission_thread.join();
    }
    
    // Save any remaining scrobbles to cache
    saveScrobbles();
    writeConfig();
}

void LastFM::readConfig()
{
    Debug::log("lastfm", "Reading configuration from ", m_config_file);
    std::ifstream config(m_config_file);
    if (!config.is_open()) {
        Debug::log("lastfm", "Config file not found - Last.fm not configured");
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
            Debug::log("lastfm", "Username loaded: ", m_username);
        } else if (key == "password") {
            m_password = value;
            Debug::log("lastfm", "Password loaded (", m_password.length(), " characters)");
        } else if (key == "session_key") {
            m_session_key = value;
            Debug::log("lastfm", "Session key loaded: ", m_session_key.substr(0, 8), "...");
        } else if (key == "now_playing_url") {
            m_nowplaying_url = value;
            Debug::log("lastfm", "Now playing URL loaded: ", m_nowplaying_url);
        } else if (key == "submission_url") {
            m_submission_url = value;
            Debug::log("lastfm", "Submission URL loaded: ", m_submission_url);
        }
    }
    
    if (isConfigured()) {
        // Cache the password hash to avoid redundant computation (Requirements 1.3)
        m_password_hash = md5Hash(m_password);
        Debug::log("lastfm", "Password hash cached for auth token generation");
        Debug::log("lastfm", "Configuration complete - scrobbling enabled");
    } else {
        Debug::log("lastfm", "Missing username or password - scrobbling disabled");
    }
}

void LastFM::writeConfig()
{
    System::createStoragePath();
    std::ofstream config(m_config_file);
    if (!config.is_open()) {
        Debug::log("lastfm", "Failed to write config file: ", m_config_file);
        return;
    }
    
    config << "# Last.fm configuration\n";
    config << "username=" << m_username << "\n";
    config << "password=" << m_password << "\n";
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
    Debug::log("lastfm", "Failed to obtain session key from all hosts. Attempt #", m_handshake_attempts);

    if (m_handshake_attempts >= 3) {
        Debug::log("lastfm", "Exceeded handshake retry limit. Disabling for this session.");
        m_handshake_permanently_failed = true;
    }

    return "";
}

bool LastFM::performHandshake(int host_index)
{
    if (m_username.empty() || m_password.empty()) {
        Debug::log("lastfm", "Username or password not configured");
        return false;
    }
    
    // Generate timestamp and auth token (double MD5 as per API spec)
    // Use cached password hash to avoid redundant computation (Requirements 1.3)
    time_t timestamp = time(nullptr);
    
    // Ensure password hash is cached (in case config was modified)
    if (m_password_hash.empty()) {
        m_password_hash = md5Hash(m_password);
    }
    
    std::string auth_token = md5Hash(m_password_hash + std::to_string(timestamp));
    
    // Build handshake URL (use root path as per API spec)
    std::string url = "http://" + m_api_hosts[host_index] + ":" + std::to_string(m_api_ports[host_index]) + 
                     "/?hs=true&p=1.2.1&c=psy&v=3.0&u=" + HTTPClient::urlEncode(m_username) + 
                     "&t=" + std::to_string(timestamp) + "&a=" + auth_token;
    
    Debug::log("lastfm", "Performing handshake with ", m_api_hosts[host_index]);
    
    HTTPClient::Response response = HTTPClient::get(url, {{"Host", m_api_hosts[host_index]}}, 10); // 10 second timeout
    
    if (!response.success) {
        Debug::log("lastfm", "Handshake failed - ", response.statusMessage);
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
            Debug::log("lastfm", "Handshake successful. Session Key: ", m_session_key);
            Debug::log("lastfm", "Now Playing URL: ", m_nowplaying_url);
            Debug::log("lastfm", "Submission URL: ", m_submission_url);
            return true;
        }
    } else if (status.substr(0, 6) == "FAILED") {
        Debug::log("lastfm", "Handshake failed - ", status);
    } else {
        Debug::log("lastfm", "Unexpected handshake response: ", status);
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
                Debug::log("lastfm", "Failed to parse cached scrobble: ", e.what());
            }
        }
        
        Debug::log("lastfm", "Loaded ", m_scrobbles.size(), " cached scrobbles");
        
    } catch (const std::exception& e) {
        Debug::log("lastfm", "Failed to parse scrobble cache XML: ", e.what());
    }
}

void LastFM::saveScrobbles()
{
    if (m_scrobbles.empty()) {
        // Remove cache file if no scrobbles
        std::remove(m_cache_file.c_str());
        return;
    }
    
    System::createStoragePath();
    std::ofstream cache(m_cache_file);
    if (!cache.is_open()) {
        Debug::log("lastfm", "Failed to write cache file: ", m_cache_file);
        return;
    }
    
    // Create root element for scrobbles collection
    XMLUtil::Element root("scrobbles");
    
    // Create a copy of the queue to iterate through
    std::queue<Scrobble> temp_queue = m_scrobbles;
    while (!temp_queue.empty()) {
        // Parse the scrobble's XML and add it as a child element
        std::string scrobbleXML = temp_queue.front().toXML();
        try {
            XMLUtil::Element scrobbleElement = XMLUtil::parseXML(scrobbleXML);
            root.children.push_back(scrobbleElement);
        } catch (const std::exception& e) {
            Debug::log("lastfm", "Failed to serialize scrobble: ", e.what());
        }
        temp_queue.pop();
    }
    
    // Write XML with declaration
    cache << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    cache << XMLUtil::generateXML(root) << "\n";
    
    Debug::log("lastfm", "Saved ", m_scrobbles.size(), " scrobbles to cache");
}

void LastFM::submissionThreadLoop()
{
    System::setThisThreadName("lastfm-submission");
    
    while (!m_shutdown) {
        std::unique_lock<std::mutex> lock(m_scrobble_mutex);
        m_submission_cv.wait(lock, [this] { 
            return (!m_scrobbles.empty() && !m_handshake_permanently_failed) || m_shutdown; 
        });
        
        if (m_shutdown) break;
        
        if (!m_scrobbles.empty() && !m_submission_active) {
            m_submission_active = true;
            lock.unlock();
            
            submitSavedScrobbles();
            
            lock.lock();
            m_submission_active = false;
        }
    }
}

void LastFM::submitSavedScrobbles()
{
    if ((m_session_key.empty() || m_submission_url.empty()) && getSessionKey().empty()) {
        Debug::log("lastfm", "Cannot submit scrobbles without valid session key and submission URL");
        return;
    }
    
    // Submit scrobbles in batches (limited to 5 per request as per user requirement)
    // API spec allows up to 50, but user requested max 5
    const int batch_size = 5;
    int submitted = 0;
    
    std::lock_guard<std::mutex> lock(m_scrobble_mutex);
    while (!m_scrobbles.empty() && submitted < batch_size) {
        const Scrobble& scrobble = m_scrobbles.front();
        
        // Submit scrobble using current session URL
        bool success = submitScrobble(scrobble.getArtistStr(), scrobble.getTitleStr(), 
                                     scrobble.getAlbumStr(), scrobble.GetLen(), 
                                     scrobble.getTimestamp());
        
        if (success) {
            m_scrobbles.pop();
            submitted++;
        } else {
            Debug::log("lastfm", "Failed to submit scrobble, keeping in cache");
            break; // Don't try more if this one failed
        }
    }
    
    if (submitted > 0) {
        Debug::log("lastfm", "Successfully submitted ", submitted, " scrobbles");
        saveScrobbles(); // Update cache
    }
}

bool LastFM::submitScrobble(const std::string& artist, const std::string& title, 
                           const std::string& album, int length, time_t timestamp)
{
    if (m_session_key.empty()) {
        Debug::log("lastfm", "No session key available for scrobble submission");
        return false;
    }
    
    // Build POST data for Last.fm 1.2 scrobble API
    std::ostringstream postData;
    postData << "s=" << HTTPClient::urlEncode(m_session_key);
    postData << "&a[0]=" << HTTPClient::urlEncode(artist);
    postData << "&t[0]=" << HTTPClient::urlEncode(title);
    postData << "&i[0]=" << timestamp;
    postData << "&o[0]=P"; // Source: P = chosen by user
    postData << "&r[0]="; // Rating (empty)
    postData << "&l[0]=" << length;
    postData << "&b[0]=" << HTTPClient::urlEncode(album);
    postData << "&n[0]="; // Track number (empty)
    postData << "&m[0]=" << md5Hash(artist + title); // MusicBrainz ID (using hash as fallback)
    
    // Use submission URL from handshake response
    if (m_submission_url.empty()) {
        Debug::log("lastfm", "No submission URL available");
        return false;
    }
    
    HTTPClient::Response response = HTTPClient::post(m_submission_url, postData.str(), 
                                                    "application/x-www-form-urlencoded", {}, 10);
        
    if (response.success) {
        std::istringstream responseStream(response.body);
        std::string status;
        std::getline(responseStream, status);
        
        if (status == "OK") {
            Debug::log("lastfm", "Scrobble submitted successfully: ", artist, " - ", title);
            return true;
        } else if (status.substr(0, 6) == "FAILED") {
            Debug::log("lastfm", "Scrobble submission failed - ", status);
            // Clear session for authentication failures
            if (status.find("BADAUTH") != std::string::npos) {
                m_session_key.clear(); // Force re-authentication
                m_submission_url.clear();
                m_nowplaying_url.clear();
            }
        } else {
            Debug::log("lastfm", "Unexpected scrobble response: ", status);
        }
    } else {
        Debug::log("lastfm", "HTTP error during scrobble submission: ", response.statusMessage);
    }
    
    return false;
}


bool LastFM::setNowPlaying(const track& track)
{
    if (!isConfigured()) {
        Debug::log("lastfm", "Cannot set now playing - not configured");
        return false;
    }
    
    Debug::log("lastfm", "Setting now playing: ", track.GetArtist().to8Bit(true), " - ", track.GetTitle().to8Bit(true));
    
    // Ensure we have valid session and now playing URL
    if ((m_session_key.empty() || m_nowplaying_url.empty()) && getSessionKey().empty()) {
        Debug::log("lastfm", "Cannot set now playing without valid session key and now playing URL");
        return false;
    }
    
    // Build POST data for Last.fm 1.2 now playing API
    std::ostringstream postData;
    postData << "s=" << HTTPClient::urlEncode(m_session_key);
    postData << "&a=" << HTTPClient::urlEncode(track.GetArtist().to8Bit(true));
    postData << "&t=" << HTTPClient::urlEncode(track.GetTitle().to8Bit(true));
    postData << "&b=" << HTTPClient::urlEncode(track.GetAlbum().to8Bit(true));
    postData << "&l=" << track.GetLen();
    postData << "&n="; // Track number (empty - not available in track class)
    postData << "&m=" << md5Hash(track.GetArtist().to8Bit(true) + track.GetTitle().to8Bit(true)); // MusicBrainz ID fallback
    
    // Use now playing URL from handshake response
    if (m_nowplaying_url.empty()) {
        Debug::log("lastfm", "No now playing URL available");
        return false;
    }
    
    HTTPClient::Response response = HTTPClient::post(m_nowplaying_url, postData.str(), 
                                                    "application/x-www-form-urlencoded", {}, 10);
        
    if (response.success) {
        std::istringstream responseStream(response.body);
        std::string status;
        std::getline(responseStream, status);
        
        if (status == "OK") {
            Debug::log("lastfm", "Now playing submitted successfully: ", track.GetArtist().to8Bit(true), " - ", track.GetTitle().to8Bit(true));
            return true;
        } else if (status.substr(0, 6) == "FAILED") {
            Debug::log("lastfm", "Now playing submission failed - ", status);
            // Clear session for authentication failures
            if (status.find("BADAUTH") != std::string::npos) {
                m_session_key.clear(); // Force re-authentication
                m_submission_url.clear();
                m_nowplaying_url.clear();
            }
        } else {
            Debug::log("lastfm", "Unexpected now playing response: ", status);
        }
    } else {
        Debug::log("lastfm", "HTTP error during now playing submission: ", response.statusMessage);
    }
    
    return false;
}

bool LastFM::unsetNowPlaying()
{
    if (!isConfigured()) {
        Debug::log("lastfm", "Cannot unset now playing - not configured");
        return false;
    }
    
    Debug::log("lastfm", "Clearing now playing status");
    
    // Ensure we have valid session and now playing URL
    if ((m_session_key.empty() || m_nowplaying_url.empty()) && getSessionKey().empty()) {
        Debug::log("lastfm", "Cannot unset now playing without valid session key and now playing URL");
        return false;
    }
    
    // Build POST data with only session key (empty now playing)
    std::ostringstream postData;
    postData << "s=" << HTTPClient::urlEncode(m_session_key) << "&a=&t=";
    
    // Use now playing URL from handshake response
    if (m_nowplaying_url.empty()) {
        Debug::log("lastfm", "No now playing URL available");
        return false;
    }
    
    HTTPClient::Response response = HTTPClient::post(m_nowplaying_url, postData.str(), 
                                                    "application/x-www-form-urlencoded", {}, 10);
        
    if (response.success) {
        std::istringstream responseStream(response.body);
        std::string status;
        std::getline(responseStream, status);
        
        if (status == "OK") {
            Debug::log("lastfm", "Now playing status cleared successfully");
            return true;
        } else if (status.substr(0, 6) == "FAILED") {
            Debug::log("lastfm", "Failed to clear now playing status - ", status);
            // Clear session for authentication failures
            if (status.find("BADAUTH") != std::string::npos) {
                m_session_key.clear(); // Force re-authentication
                m_submission_url.clear();
                m_nowplaying_url.clear();
            }
        } else {
            Debug::log("lastfm", "Unexpected response when clearing now playing: ", status);
        }
    } else {
        Debug::log("lastfm", "HTTP error when clearing now playing: ", response.statusMessage);
    }
    
    return false;
}

bool LastFM::scrobbleTrack(const track& track)
{
    if (!isConfigured()) {
        Debug::log("lastfm", "Cannot scrobble - not configured");
        return false;
    }
    
    // Add to queue for background submission
    Scrobble scrobble(track);
    
    std::lock_guard<std::mutex> lock(m_scrobble_mutex);
    m_scrobbles.push(scrobble);
    
    Debug::log("lastfm", "Added scrobble to queue: ", track.GetArtist().to8Bit(true), " - ", track.GetTitle().to8Bit(true));
    Debug::log("lastfm", "Queue size: ", m_scrobbles.size());
    
    // Notify submission thread
    m_submission_cv.notify_one();
    
    return true;
}

size_t LastFM::getCachedScrobbleCount() const
{
    std::lock_guard<std::mutex> lock(m_scrobble_mutex);
    return m_scrobbles.size();
}

void LastFM::forceSubmission()
{
    if (!m_scrobbles.empty()) {
        m_submission_cv.notify_one();
    }
}

bool LastFM::isConfigured() const
{
    return !m_username.empty() && !m_password.empty();
}

std::string LastFM::urlEncode(const std::string& input)
{
    return HTTPClient::urlEncode(input);
}

std::string LastFM::md5Hash(const std::string& input)
{
    // Optimized MD5 implementation using lookup table for hex conversion
    // instead of iostream formatting (Requirements 1.1, 1.2)
    static constexpr char hex_chars[] = "0123456789abcdef";
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    
    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) &&
        EVP_DigestUpdate(ctx, input.c_str(), input.length()) &&
        EVP_DigestFinal_ex(ctx, hash, &hash_len)) {
        
        // Pre-allocate output string (MD5 is always 16 bytes = 32 hex chars)
        std::string result;
        result.reserve(32);
        
        // Convert bytes to hex using lookup table and bit shifting
        for (unsigned int i = 0; i < hash_len; i++) {
            result += hex_chars[(hash[i] >> 4) & 0x0F];
            result += hex_chars[hash[i] & 0x0F];
        }
        
        EVP_MD_CTX_free(ctx);
        return result;
    }
    
    EVP_MD_CTX_free(ctx);
    return "";
}

} // namespace LastFM
} // namespace PsyMP3

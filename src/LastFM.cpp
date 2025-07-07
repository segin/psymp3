/*
 * LastFM.cpp - Last.fm audioscrobbler implementation
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

LastFM::LastFM() :
    m_config_file(System::getStoragePath().to8Bit(true) + "/lastfm.conf"),
    m_cache_file(System::getStoragePath().to8Bit(true) + "/scrobble_cache.xml")
{
    readConfig();
    loadScrobbles();
    
    // Start background submission thread
    m_submission_thread = std::thread(&LastFM::submissionThreadLoop, this);
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
    std::ifstream config(m_config_file);
    if (!config.is_open()) {
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
        } else if (key == "password") {
            m_password = value;
        } else if (key == "session_key") {
            m_session_key = value;
        }
    }
}

void LastFM::writeConfig()
{
    System::createStoragePath();
    std::ofstream config(m_config_file);
    if (!config.is_open()) {
        std::cerr << "LastFM: Failed to write config file: " << m_config_file << std::endl;
        return;
    }
    
    config << "# Last.fm configuration\n";
    config << "username=" << m_username << "\n";
    config << "password=" << m_password << "\n";
    config << "session_key=" << m_session_key << "\n";
}

std::string LastFM::getSessionKey()
{
    if (!m_session_key.empty()) {
        return m_session_key;
    }
    
    // Try to get session key from each host
    for (int i = 0; i < 3; ++i) {
        if (performHandshake(i)) {
            writeConfig(); // Save the session key
            return m_session_key;
        }
    }
    
    std::cerr << "LastFM: Failed to obtain session key from all hosts" << std::endl;
    return "";
}

bool LastFM::performHandshake(int host_index)
{
    if (m_username.empty() || m_password.empty()) {
        std::cerr << "LastFM: Username or password not configured" << std::endl;
        return false;
    }
    
    // Generate timestamp and auth token (double MD5 as per API spec)
    time_t timestamp = time(nullptr);
    std::string password_hash = md5Hash(m_password);
    std::string auth_token = md5Hash(password_hash + std::to_string(timestamp));
    
    // Build handshake URL (use root path as per API spec)
    std::string url = "http://" + m_api_hosts[host_index] + ":" + std::to_string(m_api_ports[host_index]) + 
                     "/?hs=true&p=1.2.1&c=psy&v=3.0&u=" + HTTPClient::urlEncode(m_username) + 
                     "&t=" + std::to_string(timestamp) + "&a=" + auth_token;
    
    std::cout << "LastFM: Performing handshake with " << m_api_hosts[host_index] << std::endl;
    
    HTTPClient::Response response = HTTPClient::get(url, {}, 10); // 10 second timeout
    
    if (!response.success) {
        std::cerr << "LastFM: Handshake failed - " << response.statusMessage << std::endl;
        return false;
    }
    
    // Parse handshake response
    std::istringstream responseStream(response.body);
    std::string status;
    std::getline(responseStream, status);
    
    if (status == "OK") {
        std::string sessionKey, nowPlayingUrl, submissionUrl;
        std::getline(responseStream, sessionKey);
        std::getline(responseStream, nowPlayingUrl);
        std::getline(responseStream, submissionUrl);
        
        if (!sessionKey.empty() && !submissionUrl.empty()) {
            m_session_key = sessionKey;
            m_nowplaying_url = nowPlayingUrl;
            m_submission_url = submissionUrl;
            std::cout << "LastFM: Handshake successful, session key and URLs obtained" << std::endl;
            return true;
        }
    } else if (status.substr(0, 6) == "FAILED") {
        std::cerr << "LastFM: Handshake failed - " << status << std::endl;
    } else {
        std::cerr << "LastFM: Unexpected handshake response: " << status << std::endl;
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
                std::cerr << "LastFM: Failed to parse cached scrobble: " << e.what() << std::endl;
            }
        }
        
        std::cout << "LastFM: Loaded " << m_scrobbles.size() << " cached scrobbles" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "LastFM: Failed to parse scrobble cache XML: " << e.what() << std::endl;
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
        std::cerr << "LastFM: Failed to write cache file: " << m_cache_file << std::endl;
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
            std::cerr << "LastFM: Failed to serialize scrobble: " << e.what() << std::endl;
        }
        temp_queue.pop();
    }
    
    // Write XML with declaration
    cache << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    cache << XMLUtil::generateXML(root) << "\n";
    
    std::cout << "LastFM: Saved " << m_scrobbles.size() << " scrobbles to cache" << std::endl;
}

void LastFM::submissionThreadLoop()
{
    System::setThisThreadName("lastfm-submission");
    
    while (!m_shutdown) {
        std::unique_lock<std::mutex> lock(m_scrobble_mutex);
        m_submission_cv.wait(lock, [this] { 
            return !m_scrobbles.empty() || m_shutdown; 
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
        std::cerr << "LastFM: Cannot submit scrobbles without valid session key and submission URL" << std::endl;
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
            std::cerr << "LastFM: Failed to submit scrobble, keeping in cache" << std::endl;
            break; // Don't try more if this one failed
        }
    }
    
    if (submitted > 0) {
        std::cout << "LastFM: Successfully submitted " << submitted << " scrobbles" << std::endl;
        saveScrobbles(); // Update cache
    }
}

bool LastFM::submitScrobble(const std::string& artist, const std::string& title, 
                           const std::string& album, int length, time_t timestamp)
{
    if (m_session_key.empty()) {
        std::cerr << "LastFM: No session key available for scrobble submission" << std::endl;
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
        std::cerr << "LastFM: No submission URL available" << std::endl;
        return false;
    }
    
    HTTPClient::Response response = HTTPClient::post(m_submission_url, postData.str(), 
                                                    "application/x-www-form-urlencoded", {}, 10);
        
    if (response.success) {
        std::istringstream responseStream(response.body);
        std::string status;
        std::getline(responseStream, status);
        
        if (status == "OK") {
            std::cout << "LastFM: Scrobble submitted successfully: " << artist << " - " << title << std::endl;
            return true;
        } else if (status.substr(0, 6) == "FAILED") {
            std::cerr << "LastFM: Scrobble submission failed - " << status << std::endl;
            // Clear session for authentication failures
            if (status.find("BADAUTH") != std::string::npos) {
                m_session_key.clear(); // Force re-authentication
                m_submission_url.clear();
                m_nowplaying_url.clear();
            }
        } else {
            std::cerr << "LastFM: Unexpected scrobble response: " << status << std::endl;
        }
    } else {
        std::cerr << "LastFM: HTTP error during scrobble submission: " << response.statusMessage << std::endl;
    }
    
    return false;
}


bool LastFM::setNowPlaying(const track& track)
{
    if (!isConfigured()) {
        return false;
    }
    
    // TODO: Implement now playing submission
    std::cout << "LastFM: Would set now playing: " << track.GetArtist() << " - " << track.GetTitle() << std::endl;
    return true;
}

bool LastFM::scrobbleTrack(const track& track)
{
    if (!isConfigured()) {
        return false;
    }
    
    // Add to queue for background submission
    Scrobble scrobble(track);
    
    std::lock_guard<std::mutex> lock(m_scrobble_mutex);
    m_scrobbles.push(scrobble);
    
    std::cout << "LastFM: Added scrobble to queue: " << track.GetArtist() << " - " << track.GetTitle() << std::endl;
    
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
    // Proper MD5 implementation using modern OpenSSL EVP interface
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    
    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) &&
        EVP_DigestUpdate(ctx, input.c_str(), input.length()) &&
        EVP_DigestFinal_ex(ctx, hash, &hash_len)) {
        
        std::ostringstream hexHash;
        for (unsigned int i = 0; i < hash_len; i++) {
            hexHash << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(hash[i]);
        }
        
        EVP_MD_CTX_free(ctx);
        return hexHash.str();
    }
    
    EVP_MD_CTX_free(ctx);
    return "";
}
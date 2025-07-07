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
    
    // Generate timestamp and auth token
    time_t timestamp = time(nullptr);
    std::string auth_token = md5Hash(m_password + std::to_string(timestamp));
    
    // Build handshake URL
    std::string url = "http://" + m_api_hosts[host_index] + ":" + std::to_string(m_api_ports[host_index]) + 
                     m_api_paths[host_index] + "?hs=true&p=1.2.1&c=psy&v=3.0&u=" + urlEncode(m_username) + 
                     "&t=" + std::to_string(timestamp) + "&a=" + auth_token;
    
    // TODO: Implement HTTP client to perform handshake
    // For now, return false to indicate we need HTTP client implementation
    std::cerr << "LastFM: HTTP client not yet implemented for handshake" << std::endl;
    return false;
}

void LastFM::loadScrobbles()
{
    std::ifstream cache(m_cache_file);
    if (!cache.is_open()) {
        return;
    }
    
    std::string line;
    std::string current_scrobble;
    bool in_scrobble = false;
    
    while (std::getline(cache, line)) {
        if (line.find("<scrobble>") != std::string::npos) {
            in_scrobble = true;
            current_scrobble = line + "\n";
        } else if (line.find("</scrobble>") != std::string::npos) {
            current_scrobble += line;
            in_scrobble = false;
            
            try {
                Scrobble scrobble = Scrobble::fromXML(current_scrobble);
                m_scrobbles.push(scrobble);
            } catch (const std::exception& e) {
                std::cerr << "LastFM: Failed to parse cached scrobble: " << e.what() << std::endl;
            }
            
            current_scrobble.clear();
        } else if (in_scrobble) {
            current_scrobble += line + "\n";
        }
    }
    
    std::cout << "LastFM: Loaded " << m_scrobbles.size() << " cached scrobbles" << std::endl;
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
    
    cache << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    cache << "<scrobbles>\n";
    
    // Create a copy of the queue to iterate through
    std::queue<Scrobble> temp_queue = m_scrobbles;
    while (!temp_queue.empty()) {
        cache << "  " << temp_queue.front().toXML() << "\n";
        temp_queue.pop();
    }
    
    cache << "</scrobbles>\n";
    
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
    if (m_session_key.empty() && getSessionKey().empty()) {
        std::cerr << "LastFM: Cannot submit scrobbles without valid session key" << std::endl;
        return;
    }
    
    // Submit scrobbles in batches (max 5 per request as requested)
    const int batch_size = 5;
    int submitted = 0;
    
    std::lock_guard<std::mutex> lock(m_scrobble_mutex);
    while (!m_scrobbles.empty() && submitted < batch_size) {
        const Scrobble& scrobble = m_scrobbles.front();
        
        // Try submitting to each host until successful
        bool success = false;
        for (int i = 0; i < 3 && !success; ++i) {
            success = submitScrobble(scrobble.getArtistStr(), scrobble.getTitleStr(), 
                                   scrobble.getAlbumStr(), scrobble.GetLen(), 
                                   scrobble.getTimestamp());
        }
        
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
    // TODO: Implement HTTP POST for actual submission
    // This is a placeholder that always returns false
    std::cerr << "LastFM: HTTP client not yet implemented for scrobble submission" << std::endl;
    return false;
}

std::string LastFM::submitData(const std::string& data, int host_index)
{
    // TODO: Implement HTTP client
    return "";
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
    std::ostringstream encoded;
    for (char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << '%' << std::uppercase << std::hex << (unsigned char)c;
        }
    }
    return encoded.str();
}

std::string LastFM::md5Hash(const std::string& input)
{
    // TODO: Implement MD5 hashing
    // For now, return a placeholder
    return "placeholder_md5_hash";
}
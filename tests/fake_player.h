/*
 * fake_player.h - Minimal fake Player class for MPRIS testing
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FAKE_PLAYER_H
#define FAKE_PLAYER_H

#include <string>
#include <atomic>
#include <mutex>

// Minimal fake Player class for MPRIS testing
class Player {
public:
    Player() 
        : m_is_playing(false)
        , m_is_paused(false)
        , m_position_ms(0)
        , m_track_length_ms(180000) // 3 minutes default
    {}
    
    virtual ~Player() = default;
    
    // Player control methods
    virtual bool play() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_is_playing = true;
        m_is_paused = false;
        return true;
    }
    
    virtual bool pause() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_is_paused = true;
        m_is_playing = false;
        return true;
    }
    
    virtual bool stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_is_playing = false;
        m_is_paused = false;
        m_position_ms = 0;
        return true;
    }
    
    virtual bool playPause() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_is_playing) {
            m_is_paused = true;
            m_is_playing = false;
        } else {
            m_is_playing = true;
            m_is_paused = false;
        }
        return true;
    }
    
    virtual void nextTrack() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_position_ms = 0;
        // Simulate track change
    }
    
    virtual void prevTrack() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_position_ms = 0;
        // Simulate track change
    }
    
    virtual void seekTo(unsigned long position_ms) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (position_ms <= m_track_length_ms) {
            m_position_ms = position_ms;
        }
    }
    
    // State query methods
    bool isPlaying() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_is_playing;
    }
    
    bool isPaused() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_is_paused;
    }
    
    unsigned long getPosition() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_position_ms;
    }
    
    unsigned long getTrackLength() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_track_length_ms;
    }
    
    // Static method for user event synthesis (required by MethodHandler)
    static void synthesizeUserEvent(int event_type, void* param1, void* param2) {
        // Fake implementation - just ignore the event
        (void)event_type; (void)param1; (void)param2;
    }
    
private:
    mutable std::mutex m_mutex;
    bool m_is_playing;
    bool m_is_paused;
    unsigned long m_position_ms;
    unsigned long m_track_length_ms;
};

// Constants that might be needed by MethodHandler
#define QUIT_APPLICATION 1

#endif // FAKE_PLAYER_H
/*
 * mock_player.cpp - Mock Player class implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "mock_player.h"

namespace TestFramework {

// MockPlayer implementation

MockPlayer::MockPlayer(const Config& config) 
    : m_config(config), m_state(PlayerState::Stopped) {
    
    // Initialize with default track
    m_current_track = TrackInfo("Unknown Artist", "Unknown Title", "Unknown Album");
    m_current_track.track_id = "/test/track/1";
    m_duration_us = 180000000; // 3 minutes default
    
    m_simulated_time = std::chrono::system_clock::now();
    
    // Initialize atomic variables from config
    m_error_simulation_enabled.store(config.enable_error_simulation);
    m_error_rate.store(config.error_rate);
    m_thread_safety_testing.store(config.thread_safety_testing);
}

MockPlayer::~MockPlayer() = default;

bool MockPlayer::play() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return play_unlocked();
}

bool MockPlayer::play_unlocked() {
    recordLockAcquisition_unlocked();
    
    if (shouldSimulateError_unlocked("play")) {
        updateStatistics_unlocked("play", false);
        return false;
    }
    
    PlayerState old_state = m_state;
    m_state = PlayerState::Playing;
    
    if (m_config.simulate_state_changes && old_state != m_state) {
        simulateDelay_unlocked(m_config.state_change_delay);
        notifyStateChange_unlocked(old_state, m_state);
    }
    
    updateStatistics_unlocked("play", true);
    return true;
}

bool MockPlayer::pause() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return pause_unlocked();
}

bool MockPlayer::pause_unlocked() {
    recordLockAcquisition_unlocked();
    
    if (shouldSimulateError_unlocked("pause")) {
        updateStatistics_unlocked("pause", false);
        return false;
    }
    
    PlayerState old_state = m_state;
    m_state = PlayerState::Paused;
    
    if (m_config.simulate_state_changes && old_state != m_state) {
        simulateDelay_unlocked(m_config.state_change_delay);
        notifyStateChange_unlocked(old_state, m_state);
    }
    
    updateStatistics_unlocked("pause", true);
    return true;
}

bool MockPlayer::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return stop_unlocked();
}

bool MockPlayer::stop_unlocked() {
    recordLockAcquisition_unlocked();
    
    if (shouldSimulateError_unlocked("stop")) {
        updateStatistics_unlocked("stop", false);
        return false;
    }
    
    PlayerState old_state = m_state;
    uint64_t old_position = m_position_us;
    
    m_state = PlayerState::Stopped;
    m_position_us = 0;
    
    if (m_config.simulate_state_changes && old_state != m_state) {
        simulateDelay_unlocked(m_config.state_change_delay);
        notifyStateChange_unlocked(old_state, m_state);
    }
    
    if (old_position != m_position_us) {
        notifyPositionChange_unlocked(old_position, m_position_us);
    }
    
    updateStatistics_unlocked("stop", true);
    return true;
}

bool MockPlayer::playPause() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state == PlayerState::Playing) {
        return pause_unlocked();
    } else {
        return play_unlocked();
    }
}

void MockPlayer::nextTrack() {
    std::lock_guard<std::mutex> lock(m_mutex);
    nextTrack_unlocked();
}

void MockPlayer::nextTrack_unlocked() {
    recordLockAcquisition_unlocked();
    
    if (shouldSimulateError_unlocked("next")) {
        updateStatistics_unlocked("next", false);
        return;
    }
    
    if (!m_playlist.empty() && m_current_track_index < m_playlist.size() - 1) {
        TrackInfo old_track = m_current_track;
        m_current_track_index++;
        m_current_track = m_playlist[m_current_track_index];
        m_position_us = 0;
        
        if (m_config.simulate_track_changes) {
            simulateDelay_unlocked(m_config.track_change_delay);
            notifyTrackChange_unlocked(old_track, m_current_track);
        }
    }
    
    updateStatistics_unlocked("next", true);
}

void MockPlayer::prevTrack() {
    std::lock_guard<std::mutex> lock(m_mutex);
    prevTrack_unlocked();
}

void MockPlayer::prevTrack_unlocked() {
    recordLockAcquisition_unlocked();
    
    if (shouldSimulateError_unlocked("prev")) {
        updateStatistics_unlocked("prev", false);
        return;
    }
    
    if (!m_playlist.empty() && m_current_track_index > 0) {
        TrackInfo old_track = m_current_track;
        m_current_track_index--;
        m_current_track = m_playlist[m_current_track_index];
        m_position_us = 0;
        
        if (m_config.simulate_track_changes) {
            simulateDelay_unlocked(m_config.track_change_delay);
            notifyTrackChange_unlocked(old_track, m_current_track);
        }
    }
    
    updateStatistics_unlocked("prev", true);
}

void MockPlayer::seekTo(uint64_t position_us) {
    std::lock_guard<std::mutex> lock(m_mutex);
    seekTo_unlocked(position_us);
}

void MockPlayer::seekTo_unlocked(uint64_t position_us) {
    recordLockAcquisition_unlocked();
    
    if (shouldSimulateError_unlocked("seek")) {
        updateStatistics_unlocked("seek", false);
        return;
    }
    
    // Clamp position to valid range
    uint64_t old_position = m_position_us;
    m_position_us = std::min(position_us, m_duration_us);
    
    if (m_config.simulate_seeking && old_position != m_position_us) {
        simulateDelay_unlocked(m_config.seek_delay);
        notifyPositionChange_unlocked(old_position, m_position_us);
    }
    
    updateStatistics_unlocked("seek", true);
}

PlayerState MockPlayer::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getState_unlocked();
}

PlayerState MockPlayer::getState_unlocked() const {
    return m_state;
}

uint64_t MockPlayer::getPosition() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getPosition_unlocked();
}

uint64_t MockPlayer::getPosition_unlocked() const {
    return m_position_us;
}

MockPlayer::TrackInfo MockPlayer::getCurrentTrack() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getCurrentTrack_unlocked();
}

MockPlayer::TrackInfo MockPlayer::getCurrentTrack_unlocked() const {
    return m_current_track;
}

uint64_t MockPlayer::getDuration() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getDuration_unlocked();
}

uint64_t MockPlayer::getDuration_unlocked() const {
    return m_duration_us;
}

bool MockPlayer::isPlaying() const {
    return getState() == PlayerState::Playing;
}

bool MockPlayer::isPaused() const {
    return getState() == PlayerState::Paused;
}

bool MockPlayer::isStopped() const {
    return getState() == PlayerState::Stopped;
}

void MockPlayer::setState(PlayerState state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    setState_unlocked(state);
}

void MockPlayer::setState_unlocked(PlayerState state) {
    PlayerState old_state = m_state;
    m_state = state;
    
    if (old_state != m_state) {
        notifyStateChange_unlocked(old_state, m_state);
    }
}

void MockPlayer::setPosition(uint64_t position_us) {
    std::lock_guard<std::mutex> lock(m_mutex);
    setPosition_unlocked(position_us);
}

void MockPlayer::setPosition_unlocked(uint64_t position_us) {
    uint64_t old_position = m_position_us;
    m_position_us = position_us;
    
    if (old_position != m_position_us) {
        notifyPositionChange_unlocked(old_position, m_position_us);
    }
}

void MockPlayer::setCurrentTrack(const TrackInfo& track) {
    std::lock_guard<std::mutex> lock(m_mutex);
    setCurrentTrack_unlocked(track);
}

void MockPlayer::setCurrentTrack_unlocked(const TrackInfo& track) {
    TrackInfo old_track = m_current_track;
    m_current_track = track;
    
    notifyTrackChange_unlocked(old_track, m_current_track);
}

void MockPlayer::setDuration(uint64_t duration_us) {
    std::lock_guard<std::mutex> lock(m_mutex);
    setDuration_unlocked(duration_us);
}

void MockPlayer::setDuration_unlocked(uint64_t duration_us) {
    m_duration_us = duration_us;
    
    // Clamp current position if it exceeds new duration
    if (m_position_us > m_duration_us) {
        uint64_t old_position = m_position_us;
        m_position_us = m_duration_us;
        notifyPositionChange_unlocked(old_position, m_position_us);
    }
}

void MockPlayer::setPlaylist(const std::vector<TrackInfo>& tracks) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_playlist = tracks;
    m_current_track_index = 0;
    
    if (!m_playlist.empty()) {
        setCurrentTrack_unlocked(m_playlist[0]);
    }
}

std::vector<MockPlayer::TrackInfo> MockPlayer::getPlaylist() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_playlist;
}

size_t MockPlayer::getCurrentTrackIndex() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current_track_index;
}

void MockPlayer::setCurrentTrackIndex(size_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (index < m_playlist.size()) {
        TrackInfo old_track = m_current_track;
        m_current_track_index = index;
        m_current_track = m_playlist[index];
        m_position_us = 0;
        
        notifyTrackChange_unlocked(old_track, m_current_track);
    }
}

void MockPlayer::setStateChangeCallback(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state_change_callback = callback;
}

void MockPlayer::setPositionChangeCallback(PositionChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_position_change_callback = callback;
}

void MockPlayer::setTrackChangeCallback(TrackChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_track_change_callback = callback;
}

void MockPlayer::setOperationCallback(OperationCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_operation_callback = callback;
}

void MockPlayer::enableErrorSimulation(bool enable) {
    m_error_simulation_enabled.store(enable);
}

void MockPlayer::setErrorRate(double rate) {
    m_error_rate.store(rate);
}

void MockPlayer::injectError(const std::string& operation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_injected_errors.push_back(operation);
}

void MockPlayer::clearInjectedErrors() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_injected_errors.clear();
}

MockPlayer::Statistics MockPlayer::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statistics;
}

void MockPlayer::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statistics = Statistics{};
}

void MockPlayer::enableThreadSafetyTesting(bool enable) {
    m_thread_safety_testing.store(enable);
}

bool MockPlayer::isThreadSafetyTestingEnabled() const {
    return m_thread_safety_testing.load();
}

bool MockPlayer::validateState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Basic state validation
    if (m_position_us > m_duration_us) {
        return false;
    }
    
    if (m_current_track_index >= m_playlist.size() && !m_playlist.empty()) {
        return false;
    }
    
    return true;
}

std::string MockPlayer::getValidationError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_position_us > m_duration_us) {
        return "Position exceeds duration";
    }
    
    if (m_current_track_index >= m_playlist.size() && !m_playlist.empty()) {
        return "Current track index out of bounds";
    }
    
    return "";
}

const MockPlayer::Config& MockPlayer::getConfig() const {
    return m_config;
}

void MockPlayer::updateConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void MockPlayer::setSimulatedTime(std::chrono::system_clock::time_point time) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_simulated_time = time;
    m_use_simulated_time = true;
}

std::chrono::system_clock::time_point MockPlayer::getSimulatedTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getCurrentTime_unlocked();
}

void MockPlayer::advanceSimulatedTime(std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_use_simulated_time) {
        m_simulated_time += duration;
    }
}

void MockPlayer::performBatchOperations(const std::vector<std::string>& operations) {
    for (const auto& op : operations) {
        if (op == "play") {
            play();
        } else if (op == "pause") {
            pause();
        } else if (op == "stop") {
            stop();
        } else if (op == "next") {
            nextTrack();
        } else if (op == "prev") {
            prevTrack();
        } else if (op.substr(0, 4) == "seek") {
            // Parse seek position from operation string
            size_t pos = op.find(':');
            if (pos != std::string::npos) {
                uint64_t position = std::stoull(op.substr(pos + 1));
                seekTo(position);
            }
        }
    }
}

void MockPlayer::simulatePlaybackSession(std::chrono::milliseconds duration) {
    auto start_time = getCurrentTime_unlocked();
    auto end_time = start_time + duration;
    
    play();
    
    while (getCurrentTime_unlocked() < end_time) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Simulate position advancement
        if (m_state == PlayerState::Playing) {
            uint64_t new_position = m_position_us + 100000; // 100ms
            if (new_position < m_duration_us) {
                setPosition(new_position);
            } else {
                nextTrack();
            }
        }
    }
}

void MockPlayer::testLockAcquisition(const std::string& operation) {
    auto start = std::chrono::high_resolution_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        recordLockAcquisition_unlocked();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    m_last_lock_acquisition_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
}

std::chrono::microseconds MockPlayer::getLastLockAcquisitionTime() const {
    return m_last_lock_acquisition_time;
}

size_t MockPlayer::getLockContentionCount() const {
    return m_lock_contention_count.load();
}

// Private methods

bool MockPlayer::shouldSimulateError_unlocked(const std::string& operation) const {
    // Check for injected errors first
    auto it = std::find(m_injected_errors.begin(), m_injected_errors.end(), operation);
    if (it != m_injected_errors.end()) {
        return true;
    }
    
    // Check global error simulation
    if (!m_error_simulation_enabled.load()) {
        return false;
    }
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    return dist(gen) < m_error_rate.load();
}

void MockPlayer::updateStatistics_unlocked(const std::string& operation, bool success) {
    m_statistics.last_operation_time = getCurrentTime_unlocked();
    
    if (operation == "play") {
        m_statistics.play_calls++;
    } else if (operation == "pause") {
        m_statistics.pause_calls++;
    } else if (operation == "stop") {
        m_statistics.stop_calls++;
    } else if (operation == "next") {
        m_statistics.next_calls++;
    } else if (operation == "prev") {
        m_statistics.prev_calls++;
    } else if (operation == "seek") {
        m_statistics.seek_calls++;
    }
    
    if (!success) {
        m_statistics.errors_injected++;
    }
}

void MockPlayer::notifyStateChange_unlocked(PlayerState old_state, PlayerState new_state) {
    m_statistics.state_changes++;
    
    if (m_state_change_callback) {
        // Note: In a real implementation, we'd call this without holding the lock
        // to prevent deadlocks, but for testing purposes we accept this risk
        m_state_change_callback(old_state, new_state);
    }
}

void MockPlayer::notifyPositionChange_unlocked(uint64_t old_position, uint64_t new_position) {
    m_statistics.position_changes++;
    
    if (m_position_change_callback) {
        m_position_change_callback(old_position, new_position);
    }
}

void MockPlayer::notifyTrackChange_unlocked(const TrackInfo& old_track, const TrackInfo& new_track) {
    m_statistics.track_changes++;
    
    if (m_track_change_callback) {
        m_track_change_callback(old_track, new_track);
    }
}

void MockPlayer::notifyOperation_unlocked(const std::string& operation, bool success) {
    if (m_operation_callback) {
        m_operation_callback(operation, success);
    }
}

void MockPlayer::simulateDelay_unlocked(std::chrono::milliseconds delay) {
    if (delay.count() > 0) {
        std::this_thread::sleep_for(delay);
    }
}

std::chrono::system_clock::time_point MockPlayer::getCurrentTime_unlocked() const {
    if (m_use_simulated_time) {
        return m_simulated_time;
    } else {
        return std::chrono::system_clock::now();
    }
}

void MockPlayer::recordLockAcquisition_unlocked() {
    if (m_thread_safety_testing.load()) {
        // Simple contention detection - if we had to wait, increment counter
        // This is a simplified approach for testing purposes
        static thread_local auto last_acquisition = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - last_acquisition);
        
        if (elapsed > std::chrono::microseconds{100}) {
            m_lock_contention_count.fetch_add(1);
        }
        
        last_acquisition = now;
    }
}

// MockPlayerFactory implementation

std::unique_ptr<MockPlayer> MockPlayerFactory::createBasicPlayer() {
    MockPlayer::Config config;
    config.simulate_state_changes = true;
    config.simulate_seeking = true;
    config.simulate_track_changes = true;
    config.enable_error_simulation = false;
    
    return std::make_unique<MockPlayer>(config);
}

std::unique_ptr<MockPlayer> MockPlayerFactory::createThreadSafetyTestPlayer() {
    MockPlayer::Config config;
    config.thread_safety_testing = true;
    config.state_change_delay = std::chrono::milliseconds{1};
    config.seek_delay = std::chrono::milliseconds{1};
    config.track_change_delay = std::chrono::milliseconds{1};
    
    return std::make_unique<MockPlayer>(config);
}

std::unique_ptr<MockPlayer> MockPlayerFactory::createErrorSimulationPlayer(double error_rate) {
    MockPlayer::Config config;
    config.enable_error_simulation = true;
    config.error_rate = error_rate;
    
    return std::make_unique<MockPlayer>(config);
}

std::unique_ptr<MockPlayer> MockPlayerFactory::createPlayerWithPlaylist(
    const std::vector<MockPlayer::TrackInfo>& tracks) {
    
    auto player = createBasicPlayer();
    player->setPlaylist(tracks);
    return player;
}

std::unique_ptr<MockPlayer> MockPlayerFactory::createPerformanceTestPlayer() {
    MockPlayer::Config config;
    config.simulate_state_changes = false;
    config.simulate_seeking = false;
    config.simulate_track_changes = false;
    config.state_change_delay = std::chrono::milliseconds{0};
    config.seek_delay = std::chrono::milliseconds{0};
    config.track_change_delay = std::chrono::milliseconds{0};
    
    return std::make_unique<MockPlayer>(config);
}

std::unique_ptr<MockPlayer> MockPlayerFactory::createRealisticPlayer() {
    MockPlayer::Config config;
    config.simulate_state_changes = true;
    config.state_change_delay = std::chrono::milliseconds{50};
    config.simulate_seeking = true;
    config.seek_delay = std::chrono::milliseconds{25};
    config.simulate_track_changes = true;
    config.track_change_delay = std::chrono::milliseconds{100};
    config.enable_error_simulation = true;
    config.error_rate = 0.02; // 2% error rate
    
    return std::make_unique<MockPlayer>(config);
}

} // namespace TestFramework
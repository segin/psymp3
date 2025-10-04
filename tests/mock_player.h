/*
 * mock_player.h - Mock Player class for MPRIS testing isolation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef MOCK_PLAYER_H
#define MOCK_PLAYER_H

#ifdef HAVE_DBUS

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>

// Forward declaration to avoid including player.h
enum class PlayerState;

namespace TestFramework {

/**
 * @brief Mock Player class for testing MPRIS integration in isolation
 * 
 * This class provides a controllable Player implementation that can simulate
 * various player states and behaviors without requiring the full Player
 * infrastructure. It follows the same threading safety patterns as the real
 * Player class.
 */
class MockPlayer {
public:
    /**
     * @brief Configuration for mock player behavior
     */
    struct Config {
        bool simulate_state_changes;
        std::chrono::milliseconds state_change_delay;
        bool simulate_seeking;
        std::chrono::milliseconds seek_delay;
        bool simulate_track_changes;
        std::chrono::milliseconds track_change_delay;
        bool enable_error_simulation;
        double error_rate; // 10% error rate for operations
        bool thread_safety_testing;
        
        Config() : simulate_state_changes(true), state_change_delay(100),
                   simulate_seeking(true), seek_delay(50),
                   simulate_track_changes(true), track_change_delay(200),
                   enable_error_simulation(false), error_rate(0.1),
                   thread_safety_testing(false) {}
    };
    
    /**
     * @brief Track information structure
     */
    struct TrackInfo {
        std::string artist;
        std::string title;
        std::string album;
        std::string track_id;
        uint64_t duration_us = 0;
        std::string art_url;
        
        TrackInfo() = default;
        TrackInfo(const std::string& artist, const std::string& title, const std::string& album)
            : artist(artist), title(title), album(album) {}
    };
    
    /**
     * @brief Callback function types for monitoring player operations
     */
    using StateChangeCallback = std::function<void(PlayerState old_state, PlayerState new_state)>;
    using PositionChangeCallback = std::function<void(uint64_t old_position, uint64_t new_position)>;
    using TrackChangeCallback = std::function<void(const TrackInfo& old_track, const TrackInfo& new_track)>;
    using OperationCallback = std::function<void(const std::string& operation, bool success)>;
    
    MockPlayer(const Config& config = Config());
    ~MockPlayer();
    
    // Player control methods (mimic real Player interface)
    bool play();
    bool pause();
    bool stop();
    bool playPause();
    void nextTrack();
    void prevTrack();
    void seekTo(uint64_t position_us);
    
    // State access methods
    PlayerState getState() const;
    uint64_t getPosition() const;
    TrackInfo getCurrentTrack() const;
    uint64_t getDuration() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;
    
    // Mock-specific control methods
    void setState(PlayerState state);
    void setPosition(uint64_t position_us);
    void setCurrentTrack(const TrackInfo& track);
    void setDuration(uint64_t duration_us);
    
    // Playlist simulation
    void setPlaylist(const std::vector<TrackInfo>& tracks);
    std::vector<TrackInfo> getPlaylist() const;
    size_t getCurrentTrackIndex() const;
    void setCurrentTrackIndex(size_t index);
    
    // Callback registration
    void setStateChangeCallback(StateChangeCallback callback);
    void setPositionChangeCallback(PositionChangeCallback callback);
    void setTrackChangeCallback(TrackChangeCallback callback);
    void setOperationCallback(OperationCallback callback);
    
    // Error simulation
    void enableErrorSimulation(bool enable);
    void setErrorRate(double rate);
    void injectError(const std::string& operation);
    void clearInjectedErrors();
    
    // Statistics and monitoring
    struct Statistics {
        size_t play_calls = 0;
        size_t pause_calls = 0;
        size_t stop_calls = 0;
        size_t next_calls = 0;
        size_t prev_calls = 0;
        size_t seek_calls = 0;
        size_t state_changes = 0;
        size_t position_changes = 0;
        size_t track_changes = 0;
        size_t errors_injected = 0;
        std::chrono::system_clock::time_point last_operation_time;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();
    
    // Threading safety testing
    void enableThreadSafetyTesting(bool enable);
    bool isThreadSafetyTestingEnabled() const;
    
    // Validation utilities
    bool validateState() const;
    std::string getValidationError() const;
    
    // Configuration access
    const Config& getConfig() const;
    void updateConfig(const Config& config);
    
    // Time simulation (for testing time-based operations)
    void setSimulatedTime(std::chrono::system_clock::time_point time);
    std::chrono::system_clock::time_point getSimulatedTime() const;
    void advanceSimulatedTime(std::chrono::milliseconds duration);
    
    // Batch operations for testing
    void performBatchOperations(const std::vector<std::string>& operations);
    void simulatePlaybackSession(std::chrono::milliseconds duration);
    
    // Lock testing utilities (for verifying threading safety patterns)
    void testLockAcquisition(const std::string& operation);
    std::chrono::microseconds getLastLockAcquisitionTime() const;
    size_t getLockContentionCount() const;

private:
    Config m_config;
    mutable std::mutex m_mutex;
    
    // Player state
    PlayerState m_state;
    uint64_t m_position_us = 0;
    TrackInfo m_current_track;
    uint64_t m_duration_us = 0;
    
    // Playlist
    std::vector<TrackInfo> m_playlist;
    size_t m_current_track_index = 0;
    
    // Callbacks
    StateChangeCallback m_state_change_callback;
    PositionChangeCallback m_position_change_callback;
    TrackChangeCallback m_track_change_callback;
    OperationCallback m_operation_callback;
    
    // Error simulation
    std::atomic<bool> m_error_simulation_enabled{false};
    std::atomic<double> m_error_rate{0.1};
    std::vector<std::string> m_injected_errors;
    
    // Statistics
    mutable Statistics m_statistics;
    
    // Threading safety testing
    std::atomic<bool> m_thread_safety_testing{false};
    std::chrono::microseconds m_last_lock_acquisition_time{0};
    std::atomic<size_t> m_lock_contention_count{0};
    
    // Time simulation
    std::chrono::system_clock::time_point m_simulated_time;
    bool m_use_simulated_time = false;
    
    // Private implementation methods (following threading safety pattern)
    bool play_unlocked();
    bool pause_unlocked();
    bool stop_unlocked();
    bool playPause_unlocked();
    void nextTrack_unlocked();
    void prevTrack_unlocked();
    void seekTo_unlocked(uint64_t position_us);
    
    PlayerState getState_unlocked() const;
    uint64_t getPosition_unlocked() const;
    TrackInfo getCurrentTrack_unlocked() const;
    uint64_t getDuration_unlocked() const;
    
    void setState_unlocked(PlayerState state);
    void setPosition_unlocked(uint64_t position_us);
    void setCurrentTrack_unlocked(const TrackInfo& track);
    void setDuration_unlocked(uint64_t duration_us);
    
    // Internal utilities
    bool shouldSimulateError_unlocked(const std::string& operation) const;
    void updateStatistics_unlocked(const std::string& operation, bool success);
    void notifyStateChange_unlocked(PlayerState old_state, PlayerState new_state);
    void notifyPositionChange_unlocked(uint64_t old_position, uint64_t new_position);
    void notifyTrackChange_unlocked(const TrackInfo& old_track, const TrackInfo& new_track);
    void notifyOperation_unlocked(const std::string& operation, bool success);
    
    void simulateDelay_unlocked(std::chrono::milliseconds delay);
    std::chrono::system_clock::time_point getCurrentTime_unlocked() const;
    
    // Lock timing for threading safety testing
    void recordLockAcquisition_unlocked();
};

/**
 * @brief Factory for creating pre-configured mock players
 */
class MockPlayerFactory {
public:
    /**
     * @brief Create a basic mock player with default configuration
     */
    static std::unique_ptr<MockPlayer> createBasicPlayer();
    
    /**
     * @brief Create a mock player configured for threading safety testing
     */
    static std::unique_ptr<MockPlayer> createThreadSafetyTestPlayer();
    
    /**
     * @brief Create a mock player with error simulation enabled
     */
    static std::unique_ptr<MockPlayer> createErrorSimulationPlayer(double error_rate = 0.1);
    
    /**
     * @brief Create a mock player with a pre-loaded playlist
     */
    static std::unique_ptr<MockPlayer> createPlayerWithPlaylist(const std::vector<MockPlayer::TrackInfo>& tracks);
    
    /**
     * @brief Create a mock player for performance testing
     */
    static std::unique_ptr<MockPlayer> createPerformanceTestPlayer();
    
    /**
     * @brief Create a mock player that simulates real-world behavior
     */
    static std::unique_ptr<MockPlayer> createRealisticPlayer();
};

/**
 * @brief Test scenarios for mock player validation
 */
class MockPlayerTestScenarios {
public:
    /**
     * @brief Run basic functionality test
     */
    static bool testBasicFunctionality(MockPlayer& player);
    
    /**
     * @brief Run state transition test
     */
    static bool testStateTransitions(MockPlayer& player);
    
    /**
     * @brief Run seeking functionality test
     */
    static bool testSeeking(MockPlayer& player);
    
    /**
     * @brief Run playlist navigation test
     */
    static bool testPlaylistNavigation(MockPlayer& player);
    
    /**
     * @brief Run error handling test
     */
    static bool testErrorHandling(MockPlayer& player);
    
    /**
     * @brief Run threading safety test
     */
    static bool testThreadingSafety(MockPlayer& player, size_t num_threads = 4);
    
    /**
     * @brief Run performance test
     */
    static bool testPerformance(MockPlayer& player, size_t num_operations = 1000);
    
    /**
     * @brief Run comprehensive validation test
     */
    static bool runAllTests(MockPlayer& player);
};

} // namespace TestFramework

#endif // HAVE_DBUS
#endif // MOCK_PLAYER_H
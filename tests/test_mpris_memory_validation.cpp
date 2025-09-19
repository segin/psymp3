/*
 * test_mpris_memory_validation.cpp - MPRIS memory usage and leak detection test
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS

#include <dbus/dbus.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

/**
 * @brief Memory usage tracker for monitoring MPRIS memory consumption
 */
class MemoryTracker {
public:
    struct MemoryStats {
        size_t virtual_memory_kb = 0;
        size_t resident_memory_kb = 0;
        size_t shared_memory_kb = 0;
        size_t heap_size_kb = 0;
        size_t stack_size_kb = 0;
    };
    
    MemoryStats getCurrentStats() {
        MemoryStats stats;
        
        // Read from /proc/self/status
        std::ifstream status_file("/proc/self/status");
        if (!status_file.is_open()) {
            return stats;
        }
        
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.find("VmSize:") == 0) {
                stats.virtual_memory_kb = extractMemoryValue(line);
            } else if (line.find("VmRSS:") == 0) {
                stats.resident_memory_kb = extractMemoryValue(line);
            } else if (line.find("RssAnon:") == 0) {
                stats.heap_size_kb = extractMemoryValue(line);
            } else if (line.find("VmStk:") == 0) {
                stats.stack_size_kb = extractMemoryValue(line);
            }
        }
        
        return stats;
    }
    
    void startTracking() {
        m_baseline_stats = getCurrentStats();
        m_tracking_active = true;
        m_peak_stats = m_baseline_stats;
        
        // Start monitoring thread
        m_monitor_thread = std::thread([this]() {
            while (m_tracking_active.load()) {
                auto current_stats = getCurrentStats();
                
                // Update peak values
                m_peak_stats.virtual_memory_kb = std::max(m_peak_stats.virtual_memory_kb, current_stats.virtual_memory_kb);
                m_peak_stats.resident_memory_kb = std::max(m_peak_stats.resident_memory_kb, current_stats.resident_memory_kb);
                m_peak_stats.heap_size_kb = std::max(m_peak_stats.heap_size_kb, current_stats.heap_size_kb);
                m_peak_stats.stack_size_kb = std::max(m_peak_stats.stack_size_kb, current_stats.stack_size_kb);
                
                // Store sample for analysis
                {
                    std::lock_guard<std::mutex> lock(m_samples_mutex);
                    m_memory_samples.push_back(current_stats);
                    
                    // Keep only last 1000 samples
                    if (m_memory_samples.size() > 1000) {
                        m_memory_samples.erase(m_memory_samples.begin());
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }
    
    void stopTracking() {
        m_tracking_active.store(false);
        if (m_monitor_thread.joinable()) {
            m_monitor_thread.join();
        }
        
        m_final_stats = getCurrentStats();
    }
    
    MemoryStats getBaselineStats() const { return m_baseline_stats; }
    MemoryStats getPeakStats() const { return m_peak_stats; }
    MemoryStats getFinalStats() const { return m_final_stats; }
    
    bool hasMemoryLeak() const {
        // Consider it a leak if final memory is significantly higher than baseline
        const double leak_threshold = 1.1; // 10% increase
        
        return (m_final_stats.resident_memory_kb > m_baseline_stats.resident_memory_kb * leak_threshold) ||
               (m_final_stats.heap_size_kb > m_baseline_stats.heap_size_kb * leak_threshold);
    }
    
    size_t getMemoryGrowth() const {
        return m_final_stats.resident_memory_kb - m_baseline_stats.resident_memory_kb;
    }
    
    std::vector<MemoryStats> getMemorySamples() const {
        std::lock_guard<std::mutex> lock(m_samples_mutex);
        return m_memory_samples;
    }

private:
    MemoryStats m_baseline_stats;
    MemoryStats m_peak_stats;
    MemoryStats m_final_stats;
    std::atomic<bool> m_tracking_active{false};
    std::thread m_monitor_thread;
    
    mutable std::mutex m_samples_mutex;
    std::vector<MemoryStats> m_memory_samples;
    
    size_t extractMemoryValue(const std::string& line) {
        std::istringstream iss(line);
        std::string token;
        
        // Skip the label (e.g., "VmSize:")
        iss >> token;
        
        // Get the value
        size_t value = 0;
        iss >> value;
        
        return value;
    }
};

/**
 * @brief MPRIS memory validation tester
 */
class MPRISMemoryValidator {
public:
    bool runAllTests() {
        std::cout << "Running MPRIS memory validation tests..." << std::endl;
        std::cout << "========================================" << std::endl;
        
        bool all_passed = true;
        
        // Test 1: Basic memory usage
        if (!testBasicMemoryUsage()) {
            std::cout << "✗ Basic memory usage test FAILED" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✓ Basic memory usage test PASSED" << std::endl;
        }
        
        // Test 2: Memory leak detection
        if (!testMemoryLeakDetection()) {
            std::cout << "✗ Memory leak detection test FAILED" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✓ Memory leak detection test PASSED" << std::endl;
        }
        
        // Test 3: Memory usage under load
        if (!testMemoryUnderLoad()) {
            std::cout << "✗ Memory under load test FAILED" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✓ Memory under load test PASSED" << std::endl;
        }
        
        // Test 4: Resource cleanup validation
        if (!testResourceCleanup()) {
            std::cout << "✗ Resource cleanup test FAILED" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✓ Resource cleanup test PASSED" << std::endl;
        }
        
        return all_passed;
    }

private:
    bool testBasicMemoryUsage() {
        std::cout << std::endl << "Testing basic memory usage..." << std::endl;
        
        MemoryTracker tracker;
        tracker.startTracking();
        
        auto baseline = tracker.getCurrentStats();
        std::cout << "Baseline memory usage:" << std::endl;
        printMemoryStats(baseline);
        
        // Create and initialize MPRIS
        MockPlayer mock_player;
        MPRISManager mpris_manager(reinterpret_cast<Player*>(&mock_player));
        
        auto init_result = mpris_manager.initialize();
        if (!init_result.isSuccess()) {
            std::cerr << "Failed to initialize MPRIS: " << init_result.getError() << std::endl;
            tracker.stopTracking();
            return false;
        }
        
        // Let it run for a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        auto after_init = tracker.getCurrentStats();
        std::cout << "Memory after MPRIS initialization:" << std::endl;
        printMemoryStats(after_init);
        
        // Perform some operations
        for (int i = 0; i < 100; ++i) {
            mpris_manager.updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
            mpris_manager.updateMetadata("Artist " + std::to_string(i), 
                                       "Title " + std::to_string(i), 
                                       "Album " + std::to_string(i));
            mpris_manager.updatePosition(i * 1000000);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        auto after_operations = tracker.getCurrentStats();
        std::cout << "Memory after operations:" << std::endl;
        printMemoryStats(after_operations);
        
        // Shutdown
        mpris_manager.shutdown();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        tracker.stopTracking();
        
        auto final_stats = tracker.getFinalStats();
        std::cout << "Final memory usage:" << std::endl;
        printMemoryStats(final_stats);
        
        // Check if memory usage is reasonable
        size_t memory_increase = after_init.resident_memory_kb - baseline.resident_memory_kb;
        std::cout << "Memory increase after initialization: " << memory_increase << " KB" << std::endl;
        
        // MPRIS should not use more than 5MB of additional memory
        const size_t max_acceptable_increase = 5 * 1024; // 5MB in KB
        
        if (memory_increase > max_acceptable_increase) {
            std::cerr << "MPRIS uses too much memory: " << memory_increase << " KB" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool testMemoryLeakDetection() {
        std::cout << std::endl << "Testing memory leak detection..." << std::endl;
        
        MemoryTracker tracker;
        tracker.startTracking();
        
        // Run multiple initialization/shutdown cycles
        const int num_cycles = 5;
        
        for (int cycle = 0; cycle < num_cycles; ++cycle) {
            std::cout << "Cycle " << (cycle + 1) << "/" << num_cycles << std::endl;
            
            MockPlayer mock_player;
            MPRISManager mpris_manager(reinterpret_cast<Player*>(&mock_player));
            
            auto init_result = mpris_manager.initialize();
            if (!init_result.isSuccess()) {
                std::cerr << "Failed to initialize MPRIS in cycle " << (cycle + 1) << std::endl;
                tracker.stopTracking();
                return false;
            }
            
            // Perform operations
            for (int i = 0; i < 50; ++i) {
                mpris_manager.updatePlaybackStatus(
                    (i % 3 == 0) ? MPRISTypes::PlaybackStatus::Playing :
                    (i % 3 == 1) ? MPRISTypes::PlaybackStatus::Paused :
                                   MPRISTypes::PlaybackStatus::Stopped
                );
                
                mpris_manager.updateMetadata("Test Artist", "Test Title", "Test Album");
                mpris_manager.updatePosition(i * 1000000);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            
            mpris_manager.shutdown();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        tracker.stopTracking();
        
        // Check for memory leaks
        bool has_leak = tracker.hasMemoryLeak();
        size_t memory_growth = tracker.getMemoryGrowth();
        
        std::cout << "Memory growth after " << num_cycles << " cycles: " << memory_growth << " KB" << std::endl;
        
        auto baseline = tracker.getBaselineStats();
        auto peak = tracker.getPeakStats();
        auto final = tracker.getFinalStats();
        
        std::cout << "Baseline: " << baseline.resident_memory_kb << " KB" << std::endl;
        std::cout << "Peak: " << peak.resident_memory_kb << " KB" << std::endl;
        std::cout << "Final: " << final.resident_memory_kb << " KB" << std::endl;
        
        if (has_leak) {
            std::cerr << "Memory leak detected!" << std::endl;
            return false;
        }
        
        // Allow for some memory growth (up to 1MB) due to system allocator behavior
        const size_t max_acceptable_growth = 1024; // 1MB in KB
        
        if (memory_growth > max_acceptable_growth) {
            std::cerr << "Excessive memory growth: " << memory_growth << " KB" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool testMemoryUnderLoad() {
        std::cout << std::endl << "Testing memory usage under load..." << std::endl;
        
        MemoryTracker tracker;
        tracker.startTracking();
        
        MockPlayer mock_player;
        MPRISManager mpris_manager(reinterpret_cast<Player*>(&mock_player));
        
        auto init_result = mpris_manager.initialize();
        if (!init_result.isSuccess()) {
            tracker.stopTracking();
            return false;
        }
        
        // Generate high load for 10 seconds
        std::atomic<bool> stop_load{false};
        std::vector<std::thread> load_threads;
        
        const int num_threads = 4;
        for (int t = 0; t < num_threads; ++t) {
            load_threads.emplace_back([&mpris_manager, &stop_load, t]() {
                int counter = 0;
                while (!stop_load.load()) {
                    mpris_manager.updatePlaybackStatus(
                        (counter % 3 == 0) ? MPRISTypes::PlaybackStatus::Playing :
                        (counter % 3 == 1) ? MPRISTypes::PlaybackStatus::Paused :
                                             MPRISTypes::PlaybackStatus::Stopped
                    );
                    
                    mpris_manager.updateMetadata(
                        "Artist " + std::to_string(t) + "_" + std::to_string(counter),
                        "Title " + std::to_string(t) + "_" + std::to_string(counter),
                        "Album " + std::to_string(t) + "_" + std::to_string(counter)
                    );
                    
                    mpris_manager.updatePosition(counter * 1000000);
                    
                    counter++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
        }
        
        // Let load run for 10 seconds
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        
        // Stop load
        stop_load.store(true);
        for (auto& thread : load_threads) {
            thread.join();
        }
        
        // Let system settle
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        mpris_manager.shutdown();
        tracker.stopTracking();
        
        // Analyze memory usage pattern
        auto samples = tracker.getMemorySamples();
        if (samples.size() < 10) {
            std::cerr << "Not enough memory samples collected" << std::endl;
            return false;
        }
        
        // Check for memory growth trend
        size_t first_quarter_avg = 0;
        size_t last_quarter_avg = 0;
        
        size_t quarter_size = samples.size() / 4;
        
        for (size_t i = 0; i < quarter_size; ++i) {
            first_quarter_avg += samples[i].resident_memory_kb;
        }
        first_quarter_avg /= quarter_size;
        
        for (size_t i = samples.size() - quarter_size; i < samples.size(); ++i) {
            last_quarter_avg += samples[i].resident_memory_kb;
        }
        last_quarter_avg /= quarter_size;
        
        std::cout << "First quarter average: " << first_quarter_avg << " KB" << std::endl;
        std::cout << "Last quarter average: " << last_quarter_avg << " KB" << std::endl;
        
        // Check if memory grew significantly during load
        double growth_ratio = static_cast<double>(last_quarter_avg) / first_quarter_avg;
        std::cout << "Memory growth ratio: " << growth_ratio << std::endl;
        
        // Memory should not grow by more than 50% under load
        if (growth_ratio > 1.5) {
            std::cerr << "Excessive memory growth under load: " << growth_ratio << "x" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool testResourceCleanup() {
        std::cout << std::endl << "Testing resource cleanup..." << std::endl;
        
        MemoryTracker tracker;
        tracker.startTracking();
        
        auto baseline = tracker.getCurrentStats();
        
        // Create and destroy multiple MPRIS instances
        for (int i = 0; i < 10; ++i) {
            MockPlayer mock_player;
            MPRISManager mpris_manager(reinterpret_cast<Player*>(&mock_player));
            
            auto init_result = mpris_manager.initialize();
            if (!init_result.isSuccess()) {
                std::cerr << "Failed to initialize MPRIS instance " << i << std::endl;
                tracker.stopTracking();
                return false;
            }
            
            // Perform some operations
            for (int j = 0; j < 20; ++j) {
                mpris_manager.updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
                mpris_manager.updateMetadata("Test", "Test", "Test");
                mpris_manager.updatePosition(j * 1000000);
            }
            
            mpris_manager.shutdown();
            
            // Force some cleanup time
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Let system settle
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        tracker.stopTracking();
        
        auto final = tracker.getFinalStats();
        
        std::cout << "Baseline memory: " << baseline.resident_memory_kb << " KB" << std::endl;
        std::cout << "Final memory: " << final.resident_memory_kb << " KB" << std::endl;
        
        size_t memory_difference = (final.resident_memory_kb > baseline.resident_memory_kb) ?
                                  (final.resident_memory_kb - baseline.resident_memory_kb) : 0;
        
        std::cout << "Memory difference: " << memory_difference << " KB" << std::endl;
        
        // Should not have significant memory increase after cleanup
        const size_t max_acceptable_difference = 512; // 512 KB
        
        if (memory_difference > max_acceptable_difference) {
            std::cerr << "Poor resource cleanup - memory not properly released" << std::endl;
            return false;
        }
        
        return true;
    }
    
    void printMemoryStats(const MemoryTracker::MemoryStats& stats) {
        std::cout << "  Virtual: " << stats.virtual_memory_kb << " KB" << std::endl;
        std::cout << "  Resident: " << stats.resident_memory_kb << " KB" << std::endl;
        std::cout << "  Heap: " << stats.heap_size_kb << " KB" << std::endl;
        std::cout << "  Stack: " << stats.stack_size_kb << " KB" << std::endl;
    }
};

// Mock Player class for testing
class MockPlayer {
public:
    MockPlayer() : state(PlayerState::Stopped) {}
    
    bool play() { 
        std::lock_guard<std::mutex> lock(m_mutex);
        state = PlayerState::Playing; 
        return true; 
    }
    
    bool pause() { 
        std::lock_guard<std::mutex> lock(m_mutex);
        state = PlayerState::Paused; 
        return true; 
    }
    
    bool stop() { 
        std::lock_guard<std::mutex> lock(m_mutex);
        state = PlayerState::Stopped; 
        return true; 
    }
    
    void nextTrack() {
        std::lock_guard<std::mutex> lock(m_mutex);
    }
    
    void prevTrack() {
        std::lock_guard<std::mutex> lock(m_mutex);
    }
    
    void seekTo(unsigned long pos) {
        std::lock_guard<std::mutex> lock(m_mutex);
    }
    
    PlayerState getState() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return state; 
    }
    
private:
    mutable std::mutex m_mutex;
    PlayerState state;
};

int main() {
    std::cout << "MPRIS Memory Validation Test" << std::endl;
    std::cout << "============================" << std::endl;
    
    // Check if we're running on Linux (required for /proc/self/status)
    std::ifstream status_file("/proc/self/status");
    if (!status_file.is_open()) {
        std::cout << "Memory validation test requires Linux /proc filesystem" << std::endl;
        std::cout << "Test skipped on this platform" << std::endl;
        return 0;
    }
    status_file.close();
    
    MPRISMemoryValidator validator;
    bool all_passed = validator.runAllTests();
    
    if (all_passed) {
        std::cout << std::endl << "✓ All MPRIS memory validation tests PASSED!" << std::endl;
        std::cout << "MPRIS system has good memory management and no detectable leaks." << std::endl;
        return 0;
    } else {
        std::cout << std::endl << "✗ Some MPRIS memory validation tests FAILED!" << std::endl;
        std::cout << "MPRIS system may have memory leaks or excessive memory usage." << std::endl;
        return 1;
    }
}

#else // !HAVE_DBUS

int main() {
    std::cout << "MPRIS memory validation test skipped (D-Bus not available)" << std::endl;
    return 0;
}

#endif // HAVE_DBUS
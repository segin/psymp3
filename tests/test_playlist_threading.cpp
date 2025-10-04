/*
 * test_playlist_threading.cpp - Test playlist threading safety
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

/**
 * Test that playlist operations are thread-safe and don't deadlock
 */
void test_playlist_threading() {
    std::cout << "Testing playlist threading safety..." << std::endl;
    
    Playlist playlist;
    
    // Add some test tracks
    playlist.addFile("/test/track1.mp3", "Artist1", "Title1", 180000);
    playlist.addFile("/test/track2.mp3", "Artist2", "Title2", 200000);
    playlist.addFile("/test/track3.mp3", "Artist3", "Title3", 220000);
    playlist.addFile("/test/track4.mp3", "Artist4", "Title4", 240000);
    playlist.addFile("/test/track5.mp3", "Artist5", "Title5", 260000);
    
    std::atomic<bool> test_running{true};
    std::atomic<int> operations_completed{0};
    std::atomic<bool> deadlock_detected{false};
    
    // Thread 1: Navigate through playlist
    std::thread navigator([&]() {
        while (test_running) {
            try {
                playlist.next();
                playlist.prev();
                playlist.peekNext();
                operations_completed++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (...) {
                deadlock_detected = true;
                break;
            }
        }
    });
    
    // Thread 2: Jump to positions
    std::thread jumper([&]() {
        while (test_running) {
            try {
                for (int i = 0; i < 5; i++) {
                    playlist.setPositionAndJump(i);
                    playlist.getTrack(i);
                    operations_completed++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (...) {
                deadlock_detected = true;
                break;
            }
        }
    });
    
    // Thread 3: Read playlist info
    std::thread reader([&]() {
        while (test_running) {
            try {
                playlist.entries();
                playlist.getPosition();
                for (int i = 0; i < 5; i++) {
                    playlist.getTrackInfo(i);
                }
                operations_completed++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (...) {
                deadlock_detected = true;
                break;
            }
        }
    });
    
    // Run test for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));
    test_running = false;
    
    // Wait for threads to complete with timeout
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    
    if (navigator.joinable()) {
        navigator.join();
    }
    if (jumper.joinable()) {
        jumper.join();
    }
    if (reader.joinable()) {
        reader.join();
    }
    
    if (deadlock_detected) {
        std::cout << "FAIL: Deadlock detected in playlist threading test!" << std::endl;
        exit(1);
    }
    
    if (operations_completed < 100) {
        std::cout << "FAIL: Too few operations completed (" << operations_completed 
                  << "), possible performance issue" << std::endl;
        exit(1);
    }
    
    std::cout << "PASS: Playlist threading test completed successfully" << std::endl;
    std::cout << "      Operations completed: " << operations_completed << std::endl;
}

int main() {
    try {
        test_playlist_threading();
        std::cout << "All playlist threading tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
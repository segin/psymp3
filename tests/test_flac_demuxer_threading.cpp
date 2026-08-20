/*
 * test_flac_demuxer_threading.cpp - Test FLAC demuxer thread safety
 * This file is part of PsyMP3.
 */

#include "psymp3.h"
#include "flac_test_data_utils.h"
#include <thread>
#include <vector>
#include <chrono>

/**
 * @brief Test FLAC demuxer thread safety with public/private lock pattern
 *
 * The container is deliberately never parsed: the point is that the public
 * accessors are safe to call concurrently even on a freshly constructed
 * demuxer. FileIOHandler throws on a missing file, so main() skips (77)
 * when no fixture is available rather than opening a bogus path.
 */
bool test_flac_demuxer_thread_safety(const std::string& fixture) {
    Debug::log("test", "Testing FLAC demuxer thread safety");

    auto handler = std::make_unique<FileIOHandler>(fixture);
    
    // Create FLAC demuxer
    FLACDemuxer demuxer(std::move(handler));
    
    // Test concurrent access to public methods
    std::vector<std::thread> threads;
    std::atomic<bool> test_running{true};
    std::atomic<int> operations_completed{0};
    
    // Thread 1: Repeatedly call getDuration()
    threads.emplace_back([&demuxer, &test_running, &operations_completed]() {
        while (test_running.load()) {
            demuxer.getDuration();
            operations_completed.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    });
    
    // Thread 2: Repeatedly call getPosition()
    threads.emplace_back([&demuxer, &test_running, &operations_completed]() {
        while (test_running.load()) {
            demuxer.getPosition();
            operations_completed.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    });
    
    // Thread 3: Repeatedly call getPosition() (demuxer uses getPosition, not getCurrentSample)
    threads.emplace_back([&demuxer, &test_running, &operations_completed]() {
        while (test_running.load()) {
            demuxer.getPosition();
            operations_completed.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    });
    
    // Thread 4: Repeatedly call isEOF()
    threads.emplace_back([&demuxer, &test_running, &operations_completed]() {
        while (test_running.load()) {
            demuxer.isEOF();
            operations_completed.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    });
    
    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop threads
    test_running.store(false);
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    int total_operations = operations_completed.load();
    Debug::log("test", "Thread safety test completed: ", total_operations, " operations");
    
    // If we get here without deadlocks or crashes, the test passed
    return total_operations > 0;
}

int main() {
    Debug::log("test", "Starting FLAC demuxer threading test");

    FLACTestDataUtils::validateTestDataAvailable("FLAC demuxer threading"); // exits 77 if none
    bool success = test_flac_demuxer_thread_safety(FLACTestDataUtils::findAvailableTestFile());
    
    if (success) {
        Debug::log("test", "FLAC demuxer threading test PASSED");
        return 0;
    } else {
        Debug::log("test", "FLAC demuxer threading test FAILED");
        return 1;
    }
}
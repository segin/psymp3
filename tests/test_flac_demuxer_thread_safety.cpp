/*
 * test_flac_demuxer_thread_safety.cpp - Thread safety tests for FLACDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// Test framework includes
#include "test_framework.h"
#include "test_framework_threading.h"

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

/**
 * @brief Test concurrent access to FLACDemuxer methods
 */
class FLACDemuxerThreadSafetyTest {
public:
    static bool testConcurrentReading() {
        Debug::log("test", "FLACDemuxerThreadSafetyTest::testConcurrentReading()");
        
        // Create a mock IOHandler for testing
        auto handler = std::make_unique<FileIOHandler>("test_file.flac");
        if (!handler || !handler->isOpen()) {
            Debug::log("test", "Cannot open test FLAC file, skipping thread safety test");
            return true; // Skip test if no file available
        }
        
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Parse container first
        if (!demuxer->parseContainer()) {
            Debug::log("test", "Failed to parse FLAC container");
            return false;
        }
        
        const int num_threads = 4;
        const int operations_per_thread = 100;
        std::atomic<int> successful_operations{0};
        std::atomic<int> failed_operations{0};
        std::vector<std::thread> threads;
        
        // Launch threads that perform concurrent reading
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&demuxer, &successful_operations, &failed_operations, operations_per_thread]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(1, 10);
                
                for (int j = 0; j < operations_per_thread; ++j) {
                    try {
                        // Perform various operations
                        auto chunk = demuxer->readChunk();
                        if (chunk.data.empty()) {
                            // EOF or error - this is expected
                        }
                        
                        auto position = demuxer->getPosition();
                        auto sample = demuxer->getCurrentSample();
                        auto duration = demuxer->getDuration();
                        auto eof = demuxer->isEOF();
                        
                        successful_operations++;
                        
                        // Add small random delay to increase chance of race conditions
                        std::this_thread::sleep_for(std::chrono::microseconds(dis(gen)));
                        
                    } catch (const std::exception& e) {
                        Debug::log("test", "Exception in thread: ", e.what());
                        failed_operations++;
                    }
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        Debug::log("test", "Concurrent reading test completed: ", 
                  successful_operations.load(), " successful, ", 
                  failed_operations.load(), " failed");
        
        // Test passes if we had no crashes and reasonable success rate
        return failed_operations.load() < (num_threads * operations_per_thread / 10); // Allow 10% failure rate
    }
    
    static bool testConcurrentSeeking() {
        Debug::log("test", "FLACDemuxerThreadSafetyTest::testConcurrentSeeking()");
        
        auto handler = std::make_unique<FileIOHandler>("test_file.flac");
        if (!handler || !handler->isOpen()) {
            Debug::log("test", "Cannot open test FLAC file, skipping seeking test");
            return true; // Skip test if no file available
        }
        
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        if (!demuxer->parseContainer()) {
            Debug::log("test", "Failed to parse FLAC container");
            return false;
        }
        
        uint64_t duration = demuxer->getDuration();
        if (duration == 0) {
            Debug::log("test", "Cannot determine duration, skipping seeking test");
            return true;
        }
        
        const int num_threads = 3;
        const int seeks_per_thread = 50;
        std::atomic<int> successful_seeks{0};
        std::atomic<int> failed_seeks{0};
        std::vector<std::thread> threads;
        
        // Launch threads that perform concurrent seeking
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&demuxer, &successful_seeks, &failed_seeks, seeks_per_thread, duration]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<uint64_t> dis(0, duration);
                
                for (int j = 0; j < seeks_per_thread; ++j) {
                    try {
                        uint64_t target_time = dis(gen);
                        bool seek_result = demuxer->seekTo(target_time);
                        
                        if (seek_result) {
                            successful_seeks++;
                        } else {
                            failed_seeks++;
                        }
                        
                        // Verify position is reasonable
                        uint64_t position = demuxer->getPosition();
                        if (position > duration + 1000) { // Allow 1 second tolerance
                            Debug::log("test", "Position out of bounds after seek: ", position);
                            failed_seeks++;
                        }
                        
                    } catch (const std::exception& e) {
                        Debug::log("test", "Exception during seek: ", e.what());
                        failed_seeks++;
                    }
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        Debug::log("test", "Concurrent seeking test completed: ", 
                  successful_seeks.load(), " successful, ", 
                  failed_seeks.load(), " failed");
        
        // Test passes if we had reasonable success rate and no crashes
        return failed_seeks.load() < (num_threads * seeks_per_thread / 5); // Allow 20% failure rate
    }
    
    static bool testConcurrentMetadataAccess() {
        Debug::log("test", "FLACDemuxerThreadSafetyTest::testConcurrentMetadataAccess()");
        
        auto handler = std::make_unique<FileIOHandler>("test_file.flac");
        if (!handler || !handler->isOpen()) {
            Debug::log("test", "Cannot open test FLAC file, skipping metadata test");
            return true; // Skip test if no file available
        }
        
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        if (!demuxer->parseContainer()) {
            Debug::log("test", "Failed to parse FLAC container");
            return false;
        }
        
        const int num_threads = 6;
        const int accesses_per_thread = 200;
        std::atomic<int> successful_accesses{0};
        std::atomic<int> failed_accesses{0};
        std::vector<std::thread> threads;
        
        // Launch threads that access metadata concurrently
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&demuxer, &successful_accesses, &failed_accesses, accesses_per_thread]() {
                for (int j = 0; j < accesses_per_thread; ++j) {
                    try {
                        // Access various metadata
                        auto streams = demuxer->getStreams();
                        auto stream_info = demuxer->getStreamInfo(1);
                        auto duration = demuxer->getDuration();
                        auto position = demuxer->getPosition();
                        auto sample = demuxer->getCurrentSample();
                        auto eof = demuxer->isEOF();
                        
                        successful_accesses++;
                        
                    } catch (const std::exception& e) {
                        Debug::log("test", "Exception during metadata access: ", e.what());
                        failed_accesses++;
                    }
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        Debug::log("test", "Concurrent metadata access test completed: ", 
                  successful_accesses.load(), " successful, ", 
                  failed_accesses.load(), " failed");
        
        // Test passes if we had no failures and no crashes
        return failed_accesses.load() == 0;
    }
    
    static bool testDestructorSafety() {
        Debug::log("test", "FLACDemuxerThreadSafetyTest::testDestructorSafety()");
        
        const int num_iterations = 10;
        
        for (int i = 0; i < num_iterations; ++i) {
            auto handler = std::make_unique<FileIOHandler>("test_file.flac");
            if (!handler || !handler->isOpen()) {
                Debug::log("test", "Cannot open test FLAC file, skipping destructor test");
                return true; // Skip test if no file available
            }
            
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            
            if (!demuxer->parseContainer()) {
                Debug::log("test", "Failed to parse FLAC container");
                return false;
            }
            
            std::atomic<bool> thread_should_stop{false};
            std::thread worker_thread([&demuxer, &thread_should_stop]() {
                while (!thread_should_stop.load()) {
                    try {
                        demuxer->readChunk();
                        demuxer->getPosition();
                        std::this_thread::sleep_for(std::chrono::microseconds(1));
                    } catch (...) {
                        // Expected when demuxer is destroyed
                        break;
                    }
                }
            });
            
            // Let thread run for a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Destroy demuxer while thread is running
            thread_should_stop.store(true);
            demuxer.reset();
            
            // Wait for thread to finish
            worker_thread.join();
        }
        
        Debug::log("test", "Destructor safety test completed successfully");
        return true;
    }
};

// Test registration
bool test_flac_demuxer_thread_safety_concurrent_reading() {
    return FLACDemuxerThreadSafetyTest::testConcurrentReading();
}

bool test_flac_demuxer_thread_safety_concurrent_seeking() {
    return FLACDemuxerThreadSafetyTest::testConcurrentSeeking();
}

bool test_flac_demuxer_thread_safety_metadata_access() {
    return FLACDemuxerThreadSafetyTest::testConcurrentMetadataAccess();
}

bool test_flac_demuxer_thread_safety_destructor() {
    return FLACDemuxerThreadSafetyTest::testDestructorSafety();
}
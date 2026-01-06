/*
 * test_ogg_thread_safety.cpp - Thread safety tests for OggDemuxer
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include <thread>
#include <atomic>
#include <chrono>

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << (message) \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

using namespace PsyMP3::Demuxer::Ogg;

bool testConcurrentGranuleOps() {
    std::cout << "Testing concurrent granule operations..." << std::endl;
    
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&errors, i]() {
            for (int j = 0; j < 1000; ++j) {
                int64_t a = i * 1000 + j;
                int64_t b = j * 100;
                
                int64_t sum = OggSeekingEngine::safeGranuleAdd(a, b);
                int64_t diff = OggSeekingEngine::safeGranuleSub(a, b);
                
                // Verify results
                if (sum != a + b) errors++;
                if (diff != a - b) errors++;
                
                // Test validity
                bool valid = OggSeekingEngine::isValidGranule(a);
                if (!valid) errors++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT(errors == 0, "Concurrent granule operations had errors");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testStreamManagerThreadSafety() {
    std::cout << "Testing OggStreamManager construction/destruction..." << std::endl;
    
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&errors, i]() {
            for (int j = 0; j < 100; ++j) {
                try {
                    OggStreamManager mgr(i * 1000 + j);
                    // Basic operations
                    mgr.getSerialNumber();
                    mgr.areHeadersComplete();
                    mgr.reset();
                } catch (...) {
                    errors++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT(errors == 0, "OggStreamManager thread operations had errors");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

int main() {
    std::cout << "Running OggDemuxer Thread Safety Tests..." << std::endl;
    std::cout << "==========================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    if (testConcurrentGranuleOps()) passed++; total++;
    if (testStreamManagerThreadSafety()) passed++; total++;
    
    std::cout << std::endl;
    if (passed == total) {
        std::cout << "All " << total << " tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << (total - passed) << " of " << total << " tests FAILED!" << std::endl;
        return 1;
    }
}

#else
int main() { return 0; }
#endif

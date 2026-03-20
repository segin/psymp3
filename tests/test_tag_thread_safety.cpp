/*
 * test_tag_thread_safety.cpp - Thread safety tests for Tag framework
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>
#endif

using namespace PsyMP3::Tag;
using namespace TestFramework;

// ============================================================================
// Helper functions to create test tags
// ============================================================================

/**
 * @brief Create a VorbisCommentTag with test data
 */
static std::unique_ptr<VorbisCommentTag> createTestVorbisTag() {
    std::map<std::string, std::vector<std::string>> fields;
    fields["TITLE"] = {"Test Title"};
    fields["ARTIST"] = {"Test Artist"};
    fields["ALBUM"] = {"Test Album"};
    fields["TRACKNUMBER"] = {"5"};
    fields["DATE"] = {"2024"};
    fields["GENRE"] = {"Rock"};
    fields["COMMENT"] = {"Test comment"};
    fields["COMPOSER"] = {"Test Composer"};
    fields["ALBUMARTIST"] = {"Test Album Artist"};
    fields["DISCNUMBER"] = {"1"};
    fields["DISCTOTAL"] = {"2"};
    fields["TRACKTOTAL"] = {"12"};
    
    return std::make_unique<VorbisCommentTag>("Test Encoder v1.0", fields);
}

/**
 * @brief Create a NullTag for testing
 */
static std::unique_ptr<NullTag> createTestNullTag() {
    return std::make_unique<NullTag>();
}

// ============================================================================
// Unit Tests for Thread Safety
// ============================================================================

// Test: Concurrent reads on VorbisCommentTag don't cause data races
class Test_VorbisCommentTag_ConcurrentReads : public TestCase {
public:
    Test_VorbisCommentTag_ConcurrentReads() 
        : TestCase("VorbisCommentTag_ConcurrentReads") {}
    
protected:
    void runTest() override {
        auto tag = createTestVorbisTag();
        
        std::atomic<bool> error_occurred{false};
        std::atomic<int> successful_reads{0};
        const int num_threads = 8;
        const int reads_per_thread = 100;
        
        std::vector<std::thread> threads;
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&tag, &error_occurred, &successful_reads, reads_per_thread, t]() {
                try {
                    for (int i = 0; i < reads_per_thread; ++i) {
                        // Read different fields based on iteration
                        std::string result;
                        switch ((t + i) % 12) {
                            case 0: result = tag->title(); break;
                            case 1: result = tag->artist(); break;
                            case 2: result = tag->album(); break;
                            case 3: result = tag->genre(); break;
                            case 4: result = tag->comment(); break;
                            case 5: result = tag->composer(); break;
                            case 6: result = tag->albumArtist(); break;
                            case 7: result = tag->getTag("TITLE"); break;
                            case 8: result = tag->formatName(); break;
                            case 9: { auto v = tag->year(); (void)v; } break;
                            case 10: { auto v = tag->track(); (void)v; } break;
                            case 11: { auto v = tag->isEmpty(); (void)v; } break;
                        }
                        successful_reads++;
                    }
                } catch (...) {
                    error_occurred = true;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_FALSE(error_occurred, "No exceptions should occur during concurrent reads");
        ASSERT_EQUALS(num_threads * reads_per_thread, successful_reads.load(), 
                      "All reads should complete successfully");
    }
};

// Test: Concurrent reads on NullTag don't cause data races
class Test_NullTag_ConcurrentReads : public TestCase {
public:
    Test_NullTag_ConcurrentReads() 
        : TestCase("NullTag_ConcurrentReads") {}
    
protected:
    void runTest() override {
        auto tag = createTestNullTag();
        
        std::atomic<bool> error_occurred{false};
        std::atomic<int> successful_reads{0};
        const int num_threads = 8;
        const int reads_per_thread = 100;
        
        std::vector<std::thread> threads;
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&tag, &error_occurred, &successful_reads, reads_per_thread]() {
                try {
                    for (int i = 0; i < reads_per_thread; ++i) {
                        // Call all accessor methods
                        (void)tag->title();
                        (void)tag->artist();
                        (void)tag->album();
                        (void)tag->isEmpty();
                        (void)tag->formatName();
                        (void)tag->pictureCount();
                        (void)tag->getAllTags();
                        successful_reads++;
                    }
                } catch (...) {
                    error_occurred = true;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_FALSE(error_occurred, "No exceptions should occur during concurrent reads");
        ASSERT_EQUALS(num_threads * reads_per_thread, successful_reads.load(), 
                      "All reads should complete successfully");
    }
};

// Test: Concurrent reads return consistent values
class Test_Tag_ConcurrentReadsReturnConsistentValues : public TestCase {
public:
    Test_Tag_ConcurrentReadsReturnConsistentValues() 
        : TestCase("Tag_ConcurrentReadsReturnConsistentValues") {}
    
protected:
    void runTest() override {
        auto tag = createTestVorbisTag();
        
        // Get expected values
        const std::string expected_title = tag->title();
        const std::string expected_artist = tag->artist();
        const std::string expected_album = tag->album();
        const uint32_t expected_year = tag->year();
        const uint32_t expected_track = tag->track();
        
        std::atomic<bool> inconsistency_found{false};
        const int num_threads = 8;
        const int reads_per_thread = 50;
        
        std::vector<std::thread> threads;
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                for (int i = 0; i < reads_per_thread; ++i) {
                    if (tag->title() != expected_title) inconsistency_found = true;
                    if (tag->artist() != expected_artist) inconsistency_found = true;
                    if (tag->album() != expected_album) inconsistency_found = true;
                    if (tag->year() != expected_year) inconsistency_found = true;
                    if (tag->track() != expected_track) inconsistency_found = true;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_FALSE(inconsistency_found, "All concurrent reads should return consistent values");
    }
};

// Test: High contention concurrent reads
class Test_Tag_HighContentionConcurrentReads : public TestCase {
public:
    Test_Tag_HighContentionConcurrentReads() 
        : TestCase("Tag_HighContentionConcurrentReads") {}
    
protected:
    void runTest() override {
        auto tag = createTestVorbisTag();
        
        std::atomic<bool> error_occurred{false};
        std::atomic<int> total_reads{0};
        const int num_threads = 16;
        const int reads_per_thread = 200;
        
        // Use a barrier to start all threads simultaneously for maximum contention
        std::atomic<int> ready_count{0};
        std::atomic<bool> start{false};
        
        std::vector<std::thread> threads;
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                ready_count++;
                while (!start) {
                    std::this_thread::yield();
                }
                
                try {
                    for (int i = 0; i < reads_per_thread; ++i) {
                        // Rapid-fire reads of all fields
                        (void)tag->title();
                        (void)tag->artist();
                        (void)tag->album();
                        (void)tag->year();
                        (void)tag->track();
                        (void)tag->genre();
                        (void)tag->comment();
                        (void)tag->getAllTags();
                        (void)tag->getTagValues("ARTIST");
                        (void)tag->hasTag("TITLE");
                        total_reads++;
                    }
                } catch (...) {
                    error_occurred = true;
                }
            });
        }
        
        // Wait for all threads to be ready
        while (ready_count < num_threads) {
            std::this_thread::yield();
        }
        
        // Start all threads simultaneously
        start = true;
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_FALSE(error_occurred, "No exceptions should occur during high contention reads");
        ASSERT_EQUALS(num_threads * reads_per_thread, total_reads.load(), 
                      "All reads should complete successfully");
    }
};

// Test: All Tag methods are const (compile-time verification)
class Test_Tag_AllMethodsAreConst : public TestCase {
public:
    Test_Tag_AllMethodsAreConst() 
        : TestCase("Tag_AllMethodsAreConst") {}
    
protected:
    void runTest() override {
        // This test verifies at compile time that all Tag methods can be called
        // on a const reference. If this compiles, the test passes.
        auto tag_ptr = createTestVorbisTag();
        const Tag& const_tag = *tag_ptr;
        
        // All these calls must compile for const Tag&
        (void)const_tag.title();
        (void)const_tag.artist();
        (void)const_tag.album();
        (void)const_tag.albumArtist();
        (void)const_tag.genre();
        (void)const_tag.year();
        (void)const_tag.track();
        (void)const_tag.trackTotal();
        (void)const_tag.disc();
        (void)const_tag.discTotal();
        (void)const_tag.comment();
        (void)const_tag.composer();
        (void)const_tag.getTag("TITLE");
        (void)const_tag.getTagValues("ARTIST");
        (void)const_tag.getAllTags();
        (void)const_tag.hasTag("ALBUM");
        (void)const_tag.pictureCount();
        (void)const_tag.getPicture(0);
        (void)const_tag.getFrontCover();
        (void)const_tag.isEmpty();
        (void)const_tag.formatName();
        
        ASSERT_TRUE(true, "All Tag methods are callable on const reference");
    }
};

// Test: No mutable state verification (behavioral test)
class Test_Tag_NoMutableStateModification : public TestCase {
public:
    Test_Tag_NoMutableStateModification() 
        : TestCase("Tag_NoMutableStateModification") {}
    
protected:
    void runTest() override {
        auto tag = createTestVorbisTag();
        
        // Get initial values
        std::string initial_title = tag->title();
        std::string initial_artist = tag->artist();
        uint32_t initial_year = tag->year();
        size_t initial_pic_count = tag->pictureCount();
        bool initial_empty = tag->isEmpty();
        
        // Call all accessor methods multiple times
        for (int i = 0; i < 100; ++i) {
            (void)tag->title();
            (void)tag->artist();
            (void)tag->album();
            (void)tag->getAllTags();
            (void)tag->getTagValues("ARTIST");
            (void)tag->hasTag("TITLE");
            (void)tag->pictureCount();
            (void)tag->getPicture(0);
            (void)tag->getFrontCover();
        }
        
        // Verify values haven't changed
        ASSERT_EQUALS(initial_title, tag->title(), "Title should not change after reads");
        ASSERT_EQUALS(initial_artist, tag->artist(), "Artist should not change after reads");
        ASSERT_EQUALS(initial_year, tag->year(), "Year should not change after reads");
        ASSERT_EQUALS(initial_pic_count, tag->pictureCount(), "Picture count should not change after reads");
        ASSERT_EQUALS(initial_empty, tag->isEmpty(), "isEmpty should not change after reads");
    }
};

// ============================================================================
// Property-Based Tests using RapidCheck
// ============================================================================

#ifdef HAVE_RAPIDCHECK

bool runTagThreadSafetyPropertyTests() {
    bool all_passed = true;
    
    std::cout << "Running RapidCheck property-based tests for Tag thread safety...\n\n";
    
    // ========================================================================
    // Property 12: Thread-Safe Concurrent Reads
    // **Validates: Requirements 9.1, 9.2**
    // Multiple threads can safely read from the same Tag instance without
    // synchronization, and all reads return consistent values.
    // ========================================================================
    
    std::cout << "  --- Property 12: Thread-Safe Concurrent Reads ---\n";
    
    // Property: Concurrent reads never throw exceptions
    std::cout << "  ConcurrentReadsNeverThrow: ";
    auto result1 = rc::check("Concurrent reads never throw exceptions", 
        [](int num_threads_raw, int reads_per_thread_raw) {
            // Constrain to reasonable values
            int num_threads = 2 + (std::abs(num_threads_raw) % 7);  // 2-8 threads
            int reads_per_thread = 10 + (std::abs(reads_per_thread_raw) % 41);  // 10-50 reads
            
            auto tag = createTestVorbisTag();
            std::atomic<bool> exception_thrown{false};
            
            std::vector<std::thread> threads;
            for (int t = 0; t < num_threads; ++t) {
                threads.emplace_back([&tag, &exception_thrown, reads_per_thread]() {
                    try {
                        for (int i = 0; i < reads_per_thread; ++i) {
                            (void)tag->title();
                            (void)tag->artist();
                            (void)tag->album();
                            (void)tag->year();
                            (void)tag->getAllTags();
                        }
                    } catch (...) {
                        exception_thrown = true;
                    }
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            RC_ASSERT(!exception_thrown);
        });
    if (!result1) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Concurrent reads return consistent values
    std::cout << "  ConcurrentReadsReturnConsistentValues: ";
    auto result2 = rc::check("Concurrent reads return consistent values", 
        [](int num_threads_raw) {
            int num_threads = 2 + (std::abs(num_threads_raw) % 7);  // 2-8 threads
            
            auto tag = createTestVorbisTag();
            const std::string expected_title = tag->title();
            const std::string expected_artist = tag->artist();
            const uint32_t expected_year = tag->year();
            
            std::atomic<bool> inconsistency{false};
            
            std::vector<std::thread> threads;
            for (int t = 0; t < num_threads; ++t) {
                threads.emplace_back([&]() {
                    for (int i = 0; i < 50; ++i) {
                        if (tag->title() != expected_title) inconsistency = true;
                        if (tag->artist() != expected_artist) inconsistency = true;
                        if (tag->year() != expected_year) inconsistency = true;
                    }
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            RC_ASSERT(!inconsistency);
        });
    if (!result2) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag is thread-safe
    std::cout << "  NullTagConcurrentReadsAreSafe: ";
    auto result3 = rc::check("NullTag concurrent reads are safe", 
        [](int num_threads_raw) {
            int num_threads = 2 + (std::abs(num_threads_raw) % 15);  // 2-16 threads
            
            auto tag = createTestNullTag();
            std::atomic<bool> exception_thrown{false};
            
            std::vector<std::thread> threads;
            for (int t = 0; t < num_threads; ++t) {
                threads.emplace_back([&tag, &exception_thrown]() {
                    try {
                        for (int i = 0; i < 100; ++i) {
                            (void)tag->title();
                            (void)tag->isEmpty();
                            (void)tag->formatName();
                            (void)tag->getAllTags();
                        }
                    } catch (...) {
                        exception_thrown = true;
                    }
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            RC_ASSERT(!exception_thrown);
        });
    if (!result3) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: getAllTags() returns consistent map under concurrent access
    std::cout << "  GetAllTagsConsistentUnderConcurrency: ";
    auto result4 = rc::check("getAllTags() returns consistent map under concurrency", 
        [](int num_threads_raw) {
            int num_threads = 2 + (std::abs(num_threads_raw) % 7);
            
            auto tag = createTestVorbisTag();
            const auto expected_tags = tag->getAllTags();
            
            std::atomic<bool> inconsistency{false};
            
            std::vector<std::thread> threads;
            for (int t = 0; t < num_threads; ++t) {
                threads.emplace_back([&]() {
                    for (int i = 0; i < 30; ++i) {
                        auto tags = tag->getAllTags();
                        if (tags.size() != expected_tags.size()) {
                            inconsistency = true;
                        }
                        for (const auto& [key, value] : expected_tags) {
                            auto it = tags.find(key);
                            if (it == tags.end() || it->second != value) {
                                inconsistency = true;
                            }
                        }
                    }
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            RC_ASSERT(!inconsistency);
        });
    if (!result4) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: getTagValues() returns consistent vector under concurrent access
    std::cout << "  GetTagValuesConsistentUnderConcurrency: ";
    auto result5 = rc::check("getTagValues() returns consistent vector under concurrency", 
        [](int num_threads_raw) {
            int num_threads = 2 + (std::abs(num_threads_raw) % 7);
            
            auto tag = createTestVorbisTag();
            const auto expected_values = tag->getTagValues("ARTIST");
            
            std::atomic<bool> inconsistency{false};
            
            std::vector<std::thread> threads;
            for (int t = 0; t < num_threads; ++t) {
                threads.emplace_back([&]() {
                    for (int i = 0; i < 50; ++i) {
                        auto values = tag->getTagValues("ARTIST");
                        if (values != expected_values) {
                            inconsistency = true;
                        }
                    }
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            RC_ASSERT(!inconsistency);
        });
    if (!result5) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Reads don't modify observable state
    std::cout << "  ReadsDoNotModifyObservableState: ";
    auto result6 = rc::check("Reads do not modify observable state", 
        [](int num_reads_raw) {
            int num_reads = 10 + (std::abs(num_reads_raw) % 91);  // 10-100 reads
            
            auto tag = createTestVorbisTag();
            
            // Capture initial state
            const std::string initial_title = tag->title();
            const std::string initial_artist = tag->artist();
            const uint32_t initial_year = tag->year();
            const size_t initial_pic_count = tag->pictureCount();
            const bool initial_empty = tag->isEmpty();
            const auto initial_all_tags = tag->getAllTags();
            
            // Perform many reads
            for (int i = 0; i < num_reads; ++i) {
                (void)tag->title();
                (void)tag->artist();
                (void)tag->album();
                (void)tag->getAllTags();
                (void)tag->getTagValues("TITLE");
                (void)tag->hasTag("ARTIST");
                (void)tag->pictureCount();
            }
            
            // Verify state hasn't changed
            RC_ASSERT(tag->title() == initial_title);
            RC_ASSERT(tag->artist() == initial_artist);
            RC_ASSERT(tag->year() == initial_year);
            RC_ASSERT(tag->pictureCount() == initial_pic_count);
            RC_ASSERT(tag->isEmpty() == initial_empty);
            RC_ASSERT(tag->getAllTags() == initial_all_tags);
        });
    if (!result6) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    std::cout << "\n";
    return all_passed;
}

#endif // HAVE_RAPIDCHECK

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== Tag Thread Safety Tests ===\n\n";
    
    bool all_passed = true;
    int tests_run = 0;
    int tests_passed = 0;
    
    // Run unit tests
    std::cout << "--- Unit Tests ---\n";
    
    auto runTest = [&](TestCase& test) {
        auto result = test.run();
        tests_run++;
        if (result.result == TestResult::PASSED) {
            tests_passed++;
            std::cout << "  " << result.name << ": PASSED\n";
        } else {
            all_passed = false;
            std::cout << "  " << result.name << ": FAILED - " << result.failure_message << "\n";
        }
    };
    
    {
        Test_VorbisCommentTag_ConcurrentReads test;
        runTest(test);
    }
    
    {
        Test_NullTag_ConcurrentReads test;
        runTest(test);
    }
    
    {
        Test_Tag_ConcurrentReadsReturnConsistentValues test;
        runTest(test);
    }
    
    {
        Test_Tag_HighContentionConcurrentReads test;
        runTest(test);
    }
    
    {
        Test_Tag_AllMethodsAreConst test;
        runTest(test);
    }
    
    {
        Test_Tag_NoMutableStateModification test;
        runTest(test);
    }
    
    std::cout << "\n";
    
#ifdef HAVE_RAPIDCHECK
    // Run property-based tests
    std::cout << "--- Property-Based Tests ---\n";
    if (!runTagThreadSafetyPropertyTests()) {
        all_passed = false;
    }
#else
    std::cout << "RapidCheck not available - skipping property-based tests\n\n";
#endif
    
    // Summary
    std::cout << "=== Test Summary ===\n";
    std::cout << "Tests run: " << tests_run << "\n";
    std::cout << "Tests passed: " << tests_passed << "\n";
    std::cout << "Tests failed: " << (tests_run - tests_passed) << "\n";
    
    if (all_passed && tests_run == tests_passed) {
        std::cout << "\nAll tests PASSED!\n";
        return 0;
    } else {
        std::cout << "\nSome tests FAILED!\n";
        return 1;
    }
}

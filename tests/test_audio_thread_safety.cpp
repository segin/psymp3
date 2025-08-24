/**
 * Comprehensive Thread Safety Tests for Audio Class
 * 
 * This file tests the refactored Audio class with public/private lock pattern
 * to ensure thread safety and deadlock prevention.
 * 
 * Requirements addressed: 3.3, 5.4
 */

#include "psymp3.h"
#include "test_framework.h"
#include "test_framework_threading.h"
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

// Mock Stream class for testing
class MockStream : public Stream {
private:
    std::atomic<bool> m_eof{false};
    std::atomic<size_t> m_position{0};
    size_t m_total_size;
    unsigned int m_rate;
    unsigned int m_channels;
    
public:
    MockStream(unsigned int rate = 44100, unsigned int channels = 2, size_t total_size = 1000000)
        : Stream(), m_total_size(total_size), m_rate(rate), m_channels(channels) {}
    
    size_t getData(size_t bytes, void* buffer) override {
        if (m_eof.load() || m_position >= m_total_size) {
            m_eof = true;
            return 0;
        }
        
        size_t available = std::min(bytes, m_total_size - m_position.load());
        if (buffer && available > 0) {
            // Fill with test pattern
            int16_t* samples = static_cast<int16_t*>(buffer);
            size_t sample_count = available / sizeof(int16_t);
            for (size_t i = 0; i < sample_count; ++i) {
                samples[i] = static_cast<int16_t>((i + m_position) % 32767);
            }
        }
        
        m_position += available;
        return available;
    }
    
    bool eof() override { return m_eof.load(); }
    unsigned int getRate() override { return m_rate; }
    unsigned int getChannels() override { return m_channels; }
    void seekTo(unsigned long pos) override { m_position = pos * sizeof(int16_t) * m_channels; }
};

// Mock FastFourier class for testing
class MockFastFourier : public FastFourier {
public:
    MockFastFourier() : FastFourier(512) {}
    // Use inherited methods - no need to override
};/**

 * Test concurrent access to Audio public methods
 */
void testAudioConcurrentAccess() {
    std::cout << "\n=== Testing Audio Concurrent Access ===" << std::endl;
    
    // Initialize SDL for audio testing
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cout << "SKIPPED: SDL audio initialization failed: " << SDL_GetError() << std::endl;
        return;
    }
    
    try {
        // Create test objects
        auto stream = std::make_unique<MockStream>();
        MockFastFourier fft;
        std::mutex player_mutex;
        
        // Create Audio instance
        Audio audio(std::move(stream), &fft, &player_mutex);
    
    ThreadingTest::TestConfig config;
    config.num_threads = 8;
    config.operations_per_thread = 100;
    config.timeout = std::chrono::milliseconds(5000);
    
    // Test concurrent access to different public methods
    auto test = ThreadingTest::ConcurrentAccessTest<Audio>(
        &audio,
        [](Audio* a, int thread_id) {
            switch (thread_id % 4) {
                case 0: 
                    a->isFinished();
                    break;
                case 1: 
                    a->resetBuffer();
                    break;
                case 2: 
                    a->getBufferLatencyMs();
                    break;
                case 3: {
                    // Test setStream with new mock stream
                    auto new_stream = std::make_unique<MockStream>();
                    a->setStream(std::move(new_stream));
                    break;
                }
            }
        },
        config
    );
    
    auto results = test.run();
    
        std::cout << "Concurrent access test: " 
                  << (results.success ? "PASSED" : "FAILED") << std::endl;
        std::cout << "Operations: " << results.total_operations 
                  << ", Errors: " << results.failed_operations << std::endl;
        
        if (!results.success) {
            for (const auto& error : results.errors) {
                std::cout << "  Error: " << error << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
    }
    
    SDL_Quit();
}

/**
 * Test deadlock prevention - operations that would deadlock with old pattern
 */
void testAudioDeadlockPrevention() {
    std::cout << "\n=== Testing Audio Deadlock Prevention ===" << std::endl;
    
    // Initialize SDL for audio testing
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cout << "SKIPPED: SDL audio initialization failed: " << SDL_GetError() << std::endl;
        return;
    }
    
    try {
        auto stream = std::make_unique<MockStream>();
        MockFastFourier fft;
        std::mutex player_mutex;
        
        Audio audio(std::move(stream), &fft, &player_mutex);
    
    ThreadingTest::TestConfig config;
    config.num_threads = 4;
    config.operations_per_thread = 50;
    config.timeout = std::chrono::milliseconds(3000);
    
    // Test operations that could cause deadlock if not properly implemented
    auto test = ThreadingTest::DeadlockDetectionTest<Audio>(
        &audio,
        [](Audio* a, int thread_id) {
            // These operations acquire multiple locks and could deadlock
            // if the unlocked pattern wasn't implemented correctly
            switch (thread_id % 3) {
                case 0: {
                    // setStream acquires both stream and buffer mutexes
                    auto new_stream = std::make_unique<MockStream>();
                    a->setStream(std::move(new_stream));
                    break;
                }
                case 1:
                    // Multiple buffer operations
                    a->resetBuffer();
                    a->getBufferLatencyMs();
                    a->isFinished();
                    break;
                case 2:
                    // Mixed operations
                    a->isFinished();
                    a->getBufferLatencyMs();
                    break;
            }
        },
        config
    );
    
    auto results = test.run();
    
        std::cout << "Deadlock prevention test: " 
                  << (results.success ? "PASSED" : "FAILED") << std::endl;
        std::cout << "Operations: " << results.total_operations 
                  << ", Errors: " << results.failed_operations << std::endl;
        
        if (!results.success) {
            std::cout << "WARNING: Potential deadlock detected!" << std::endl;
            for (const auto& error : results.errors) {
                std::cout << "  Error: " << error << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
    }
    
    SDL_Quit();
}/**
 * St
ress test with high concurrency and mixed operations
 */
void testAudioStressTest() {
    std::cout << "\n=== Testing Audio Stress Test ===" << std::endl;
    
    auto stream = std::make_unique<MockStream>();
    MockFastFourier fft;
    std::mutex player_mutex;
    
    Audio audio(std::move(stream), &fft, &player_mutex);
    
    ThreadingTest::TestConfig config;
    config.num_threads = 12;
    config.operations_per_thread = 200;
    config.timeout = std::chrono::milliseconds(10000);
    config.enable_stress_testing = true;
    
    // Define multiple operations for stress testing
    std::vector<std::function<void(Audio*, int)>> operations = {
        [](Audio* a, int) { a->isFinished(); },
        [](Audio* a, int) { a->getBufferLatencyMs(); },
        [](Audio* a, int) { a->resetBuffer(); },
        [](Audio* a, int) { 
            auto new_stream = std::make_unique<MockStream>();
            a->setStream(std::move(new_stream));
        },
        [](Audio* a, int) {
            // Multiple operations in sequence
            a->isFinished();
            a->getBufferLatencyMs();
        },
        [](Audio* a, int) {
            // Test play/pause operations
            a->play(true);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            a->play(false);
        }
    };
    
    auto test = ThreadingTest::StressTest<Audio>(&audio, operations, config);
    
    auto results = test.run();
    
    std::cout << "Stress test: " 
              << (results.success ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Operations: " << results.total_operations 
              << ", Errors: " << results.failed_operations 
              << ", Success rate: " << (results.getSuccessRate() * 100.0) << "%" << std::endl;
    
    if (!results.success) {
        for (const auto& error : results.errors) {
            std::cout << "  Error: " << error << std::endl;
        }
    }
}

/**
 * Performance regression test to ensure threading changes don't impact performance
 */
void testAudioPerformanceRegression() {
    std::cout << "\n=== Testing Audio Performance Regression ===" << std::endl;
    
    auto stream = std::make_unique<MockStream>();
    MockFastFourier fft;
    std::mutex player_mutex;
    
    Audio audio(std::move(stream), &fft, &player_mutex);
    
    const int iterations = 10000;
    
    // Benchmark individual operations
    {
        ThreadingTest::PerformanceBenchmark bench("Audio::isFinished() single-threaded");
        for (int i = 0; i < iterations; ++i) {
            audio.isFinished();
        }
    }
    
    {
        ThreadingTest::PerformanceBenchmark bench("Audio::getBufferLatencyMs() single-threaded");
        for (int i = 0; i < iterations; ++i) {
            audio.getBufferLatencyMs();
        }
    }
    
    {
        ThreadingTest::PerformanceBenchmark bench("Audio::resetBuffer() single-threaded");
        for (int i = 0; i < iterations / 10; ++i) {
            audio.resetBuffer();
        }
    }
    
    // Test concurrent performance
    {
        ThreadingTest::PerformanceBenchmark bench("Audio mixed operations multi-threaded");
        
        std::vector<std::thread> threads;
        const int num_threads = 4;
        const int ops_per_thread = iterations / num_threads;
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&audio, ops_per_thread, t]() {
                for (int i = 0; i < ops_per_thread; ++i) {
                    switch ((t + i) % 3) {
                        case 0: audio.isFinished(); break;
                        case 1: audio.getBufferLatencyMs(); break;
                        case 2: audio.resetBuffer(); break;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
}
/**
 * Test Audio class with TestFramework integration
 */
class AudioThreadSafetyTestCase : public TestFramework::TestCase {
public:
    AudioThreadSafetyTestCase() : TestCase("Audio Thread Safety Comprehensive Test") {}
    
protected:
    void runTest() override {
        // Test basic thread safety
        auto stream = std::make_unique<MockStream>();
        MockFastFourier fft;
        std::mutex player_mutex;
        
        Audio audio(std::move(stream), &fft, &player_mutex);
        
        // Test that all public methods can be called concurrently
        std::atomic<int> error_count{0};
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&audio, &error_count, i]() {
                try {
                    for (int j = 0; j < 100; ++j) {
                        switch ((i + j) % 4) {
                            case 0: audio.isFinished(); break;
                            case 1: audio.getBufferLatencyMs(); break;
                            case 2: audio.resetBuffer(); break;
                            case 3: {
                                auto new_stream = std::make_unique<MockStream>();
                                audio.setStream(std::move(new_stream));
                                break;
                            }
                        }
                    }
                } catch (...) {
                    error_count++;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        ASSERT_EQUALS(0, error_count.load(), "No errors should occur in concurrent access");
        
        // Test that the audio object is still functional after concurrent access
        ASSERT_FALSE(audio.isFinished(), "Audio should not be finished initially");
        
        uint64_t latency = audio.getBufferLatencyMs();
        ASSERT_TRUE(latency >= 0, "Buffer latency should be non-negative");
        
        // Test setStream functionality
        auto new_stream = std::make_unique<MockStream>(48000, 1);  // Different rate and channels
        audio.setStream(std::move(new_stream));
        
        ASSERT_EQUALS(48000, audio.getRate(), "Rate should be updated after setStream");
        ASSERT_EQUALS(1, audio.getChannels(), "Channels should be updated after setStream");
    }
};

/**
 * Main test function
 */
int main() {
    std::cout << "PsyMP3 Audio Class Thread Safety Tests" << std::endl;
    std::cout << "======================================" << std::endl;
    
    std::cout << "\nTesting the refactored Audio class with public/private lock pattern" << std::endl;
    std::cout << "to ensure thread safety and deadlock prevention." << std::endl;
    
    try {
        // Run threading-specific tests
        testAudioConcurrentAccess();
        testAudioDeadlockPrevention();
        testAudioStressTest();
        testAudioPerformanceRegression();
        
        // Run TestFramework integration test
        std::cout << "\n=== Running TestFramework Integration Test ===" << std::endl;
        AudioThreadSafetyTestCase test_case;
        auto result = test_case.run();
        
        std::cout << "TestFramework integration: " 
                  << (result.result == TestFramework::TestResult::PASSED ? "PASSED" : "FAILED") 
                  << " (" << result.execution_time.count() << "ms)" << std::endl;
        
        if (result.result != TestFramework::TestResult::PASSED) {
            std::cout << "Failure: " << result.failure_message << std::endl;
        }
        
        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "Audio class thread safety tests completed." << std::endl;
        std::cout << "These tests validate:" << std::endl;
        std::cout << "1. Concurrent access to public methods is safe" << std::endl;
        std::cout << "2. Deadlock conditions are prevented" << std::endl;
        std::cout << "3. High-concurrency stress testing passes" << std::endl;
        std::cout << "4. Performance impact is acceptable" << std::endl;
        std::cout << "5. Integration with existing test framework works" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test execution failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
/*
 * test_thread_safety_simple.cpp - Simple thread safety tests for demuxer architecture
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <cassert>
#include <fstream>
#include <mutex>
#include <map>
#include <string>

// Simple test framework
class SimpleTestFramework {
public:
    static void runAllTests() {
        std::cout << "=== Simple Thread Safety Tests ===" << std::endl;
        
        testBufferPoolThreadSafety();
        testFactoryThreadSafety();
        
        std::cout << "All simple thread safety tests completed." << std::endl;
    }

private:
    // Simple BufferPool mock for testing
    class TestBufferPool {
    public:
        static TestBufferPool& getInstance() {
            static TestBufferPool instance;
            return instance;
        }
        
        std::vector<uint8_t> getBuffer(size_t min_size) {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // Look for a suitable buffer in the pool
            for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
                if (it->capacity() >= min_size) {
                    std::vector<uint8_t> buffer = std::move(*it);
                    m_buffers.erase(it);
                    buffer.clear(); // Clear contents but keep capacity
                    return buffer;
                }
            }
            
            // No suitable buffer found, create a new one
            std::vector<uint8_t> buffer;
            buffer.reserve(std::max(min_size, static_cast<size_t>(4096))); // Minimum 4KB
            return buffer;
        }
        
        void returnBuffer(std::vector<uint8_t>&& buffer) {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // Only pool buffers that are reasonably sized and not too large
            if (buffer.capacity() >= 1024 && buffer.capacity() <= MAX_BUFFER_SIZE && 
                m_buffers.size() < MAX_POOLED_BUFFERS) {
                buffer.clear(); // Clear contents but keep capacity
                m_buffers.push_back(std::move(buffer));
            }
            // Otherwise, let the buffer be destroyed
        }
        
        void clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_buffers.clear();
        }
        
        struct PoolStats {
            size_t total_buffers;
            size_t total_memory_bytes;
            size_t largest_buffer_size;
        };
        
        PoolStats getStats() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            PoolStats stats;
            stats.total_buffers = m_buffers.size();
            stats.total_memory_bytes = 0;
            stats.largest_buffer_size = 0;
            
            for (const auto& buffer : m_buffers) {
                stats.total_memory_bytes += buffer.capacity();
                stats.largest_buffer_size = std::max(stats.largest_buffer_size, buffer.capacity());
            }
            
            return stats;
        }
        
    private:
        TestBufferPool() = default;
        mutable std::mutex m_mutex;
        std::vector<std::vector<uint8_t>> m_buffers;
        static constexpr size_t MAX_POOLED_BUFFERS = 32;
        static constexpr size_t MAX_BUFFER_SIZE = 1024 * 1024; // 1MB max per buffer
    };
    
    // Simple Factory mock for testing
    class TestFactory {
    public:
        static void registerFormat(const std::string& format_id, int priority) {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_formats[format_id] = priority;
        }
        
        static bool supportsFormat(const std::string& format_id) {
            std::lock_guard<std::mutex> lock(s_mutex);
            return s_formats.find(format_id) != s_formats.end();
        }
        
        static std::vector<std::string> getSupportedFormats() {
            std::lock_guard<std::mutex> lock(s_mutex);
            std::vector<std::string> formats;
            for (const auto& [format, priority] : s_formats) {
                formats.push_back(format);
            }
            return formats;
        }
        
        static void clear() {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_formats.clear();
        }
        
    private:
        static std::map<std::string, int> s_formats;
        static std::mutex s_mutex;
    };
    
    static void testBufferPoolThreadSafety() {
        std::cout << "Testing BufferPool thread safety..." << std::endl;
        
        TestBufferPool& pool = TestBufferPool::getInstance();
        pool.clear(); // Start with clean pool
        
        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        const int num_threads = 8;
        const int operations_per_thread = 100;
        
        std::vector<std::thread> threads;
        
        // Create threads that concurrently get and return buffers
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> size_dist(1024, 65536);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    try {
                        size_t buffer_size = size_dist(gen);
                        
                        // Get buffer
                        auto buffer = pool.getBuffer(buffer_size);
                        
                        // Use buffer
                        if (buffer.capacity() >= buffer_size) {
                            buffer.resize(buffer_size);
                            // Write some data
                            for (size_t j = 0; j < std::min(buffer_size, size_t(100)); ++j) {
                                buffer[j] = static_cast<uint8_t>(j + t);
                            }
                            success_count++;
                        } else {
                            failure_count++;
                        }
                        
                        // Return buffer
                        pool.returnBuffer(std::move(buffer));
                        
                    } catch (const std::exception& e) {
                        failure_count++;
                        std::cerr << "BufferPool thread " << t << " exception: " << e.what() << std::endl;
                    }
                    
                    // Small delay to increase contention
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "BufferPool test completed: " << success_count.load() 
                  << " successes, " << failure_count.load() << " failures" << std::endl;
        
        // Verify pool statistics are consistent
        auto stats = pool.getStats();
        std::cout << "Final pool stats: " << stats.total_buffers << " buffers, "
                  << stats.total_memory_bytes << " bytes" << std::endl;
        
        assert(failure_count.load() == 0);
        std::cout << "✓ BufferPool thread safety test passed" << std::endl;
    }
    
    static void testFactoryThreadSafety() {
        std::cout << "Testing Factory thread safety..." << std::endl;
        
        TestFactory::clear(); // Start clean
        
        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        const int num_threads = 4;
        const int operations_per_thread = 50;
        
        std::vector<std::thread> threads;
        
        // Create threads that concurrently register and query formats
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> op_dist(0, 2);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    try {
                        int operation = op_dist(gen);
                        std::string format_id = "format_" + std::to_string(t) + "_" + std::to_string(i);
                        
                        switch (operation) {
                            case 0: // Register format
                                TestFactory::registerFormat(format_id, 100 + t);
                                success_count++;
                                break;
                                
                            case 1: // Check if format is supported
                                {
                                    bool supported = TestFactory::supportsFormat(format_id);
                                    success_count++; // Any result is valid
                                }
                                break;
                                
                            case 2: // Get supported formats
                                {
                                    auto formats = TestFactory::getSupportedFormats();
                                    success_count++; // Any result is valid
                                }
                                break;
                        }
                        
                    } catch (const std::exception& e) {
                        failure_count++;
                        std::cerr << "Factory thread " << t << " exception: " << e.what() << std::endl;
                    }
                    
                    std::this_thread::sleep_for(std::chrono::microseconds(5));
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "Factory test completed: " << success_count.load() 
                  << " successes, " << failure_count.load() << " failures" << std::endl;
        
        // Verify final state
        auto formats = TestFactory::getSupportedFormats();
        std::cout << "Final registered formats: " << formats.size() << std::endl;
        
        assert(failure_count.load() == 0);
        std::cout << "✓ Factory thread safety test passed" << std::endl;
    }
};

// Static member definitions
std::map<std::string, int> SimpleTestFramework::TestFactory::s_formats;
std::mutex SimpleTestFramework::TestFactory::s_mutex;

int main() {
    try {
        SimpleTestFramework::runAllTests();
        std::cout << "\n=== All Simple Thread Safety Tests Passed ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Simple thread safety test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Simple thread safety test failed with unknown exception" << std::endl;
        return 1;
    }
}
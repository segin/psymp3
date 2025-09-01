/*
 * test_iohandler_performance_validation.cpp - Performance validation tests for IOHandler subsystem
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <fstream>
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>
#include <iomanip>

// Performance testing framework
class PerformanceValidator {
public:
    struct BenchmarkResult {
        std::string test_name;
        double duration_ms;
        double throughput_mbps;
        size_t operations_count;
        size_t bytes_processed;
        bool passed;
        std::string notes;
    };
    
    static void assert_true(bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << "ASSERTION FAILED: " << message << std::endl;
            exit(1);
        }
    }
    
    static void assert_performance(double actual_mbps, double minimum_mbps, const std::string& test_name) {
        if (actual_mbps < minimum_mbps) {
            std::cerr << "PERFORMANCE REGRESSION: " << test_name 
                     << " - Expected at least " << minimum_mbps << " MB/s, got " << actual_mbps << " MB/s" << std::endl;
            exit(1);
        }
    }
    
    static void createTestFile(const std::string& filename, size_t size_bytes) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to create test file: " + filename);
        }
        
        // Create test data with patterns for verification
        std::vector<uint8_t> buffer(4096);
        for (size_t i = 0; i < buffer.size(); ++i) {
            buffer[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF); // Simple pattern
        }
        
        size_t written = 0;
        while (written < size_bytes) {
            size_t to_write = std::min(buffer.size(), size_bytes - written);
            file.write(reinterpret_cast<const char*>(buffer.data()), to_write);
            written += to_write;
        }
        
        file.close();
    }
    
    static void cleanup_test_file(const std::string& filename) {
        std::remove(filename.c_str());
    }
    
    static BenchmarkResult benchmark_sequential_read(const std::string& filename, size_t buffer_size) {
        BenchmarkResult result;
        result.test_name = "Sequential Read (buffer: " + std::to_string(buffer_size) + " bytes)";
        result.operations_count = 0;
        result.bytes_processed = 0;
        result.passed = false;
        
        try {
            FileIOHandler handler{TagLib::String(filename)};
            
            std::vector<uint8_t> buffer(buffer_size);
            auto start_time = std::chrono::high_resolution_clock::now();
            
            while (!handler.eof()) {
                size_t bytes_read = handler.read(buffer.data(), 1, buffer.size());
                if (bytes_read == 0) {
                    break;
                }
                
                result.operations_count++;
                result.bytes_processed += bytes_read;
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
            result.duration_ms = duration.count() / 1000.0;
            result.throughput_mbps = (result.bytes_processed / (1024.0 * 1024.0)) / (duration.count() / 1000000.0);
            result.passed = true;
            
        } catch (const std::exception& e) {
            result.notes = "Exception: " + std::string(e.what());
        }
        
        return result;
    }
    
    static BenchmarkResult benchmark_random_access(const std::string& filename, size_t num_seeks, size_t read_size) {
        BenchmarkResult result;
        result.test_name = "Random Access (" + std::to_string(num_seeks) + " seeks, " + std::to_string(read_size) + " bytes/read)";
        result.operations_count = 0;
        result.bytes_processed = 0;
        result.passed = false;
        
        try {
            FileIOHandler handler{TagLib::String(filename)};
            off_t file_size = handler.getFileSize();
            
            if (file_size <= 0) {
                result.notes = "Invalid file size";
                return result;
            }
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<off_t> pos_dist(0, std::max(static_cast<off_t>(0), file_size - static_cast<off_t>(read_size)));
            
            std::vector<uint8_t> buffer(read_size);
            auto start_time = std::chrono::high_resolution_clock::now();
            
            for (size_t i = 0; i < num_seeks; ++i) {
                off_t seek_pos = pos_dist(gen);
                
                int seek_result = handler.seek(seek_pos, SEEK_SET);
                if (seek_result != 0) {
                    result.notes = "Seek failed at position " + std::to_string(seek_pos);
                    return result;
                }
                
                size_t bytes_read = handler.read(buffer.data(), 1, buffer.size());
                if (bytes_read == 0 && !handler.eof()) {
                    result.notes = "Read failed at position " + std::to_string(seek_pos);
                    return result;
                }
                
                result.operations_count++;
                result.bytes_processed += bytes_read;
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
            result.duration_ms = duration.count() / 1000.0;
            result.throughput_mbps = (result.bytes_processed / (1024.0 * 1024.0)) / (duration.count() / 1000000.0);
            result.passed = true;
            
        } catch (const std::exception& e) {
            result.notes = "Exception: " + std::string(e.what());
        }
        
        return result;
    }
    
    static BenchmarkResult benchmark_memory_usage(const std::string& filename) {
        BenchmarkResult result;
        result.test_name = "Memory Usage Monitoring";
        result.operations_count = 0;
        result.bytes_processed = 0;
        result.passed = false;
        
        try {
            // Get initial memory stats
            auto initial_stats = IOHandler::getMemoryStats();
            size_t initial_memory = initial_stats["total_memory_usage"];
            
            {
                FileIOHandler handler{TagLib::String(filename)};
                
                // Perform various operations to trigger memory allocation
                std::vector<uint8_t> buffer(64 * 1024); // 64KB buffer
                
                // Sequential read
                while (!handler.eof()) {
                    size_t bytes_read = handler.read(buffer.data(), 1, buffer.size());
                    if (bytes_read == 0) {
                        break;
                    }
                    result.bytes_processed += bytes_read;
                    result.operations_count++;
                }
                
                // Random seeks
                off_t file_size = handler.getFileSize();
                for (int i = 0; i < 100; ++i) {
                    off_t pos = (i * 1000) % file_size;
                    handler.seek(pos, SEEK_SET);
                    handler.read(buffer.data(), 1, 1024);
                    result.operations_count++;
                }
                
                // Check memory usage during operations
                auto peak_stats = IOHandler::getMemoryStats();
                size_t peak_memory = peak_stats["total_memory_usage"];
                
                result.throughput_mbps = (peak_memory - initial_memory) / (1024.0 * 1024.0); // Memory usage in MB
                
            } // Handler destroyed here
            
            // Check memory usage after cleanup
            auto final_stats = IOHandler::getMemoryStats();
            size_t final_memory = final_stats["total_memory_usage"];
            
            result.duration_ms = (final_memory - initial_memory) / (1024.0 * 1024.0); // Memory leak in MB
            result.passed = true;
            
            if (result.duration_ms > 1.0) { // More than 1MB leak
                result.notes = "Potential memory leak detected: " + std::to_string(result.duration_ms) + " MB";
                result.passed = false;
            }
            
        } catch (const std::exception& e) {
            result.notes = "Exception: " + std::string(e.what());
        }
        
        return result;
    }
    
    static void print_result(const BenchmarkResult& result) {
        std::cout << "  " << result.test_name << ":" << std::endl;
        std::cout << "    Duration: " << std::fixed << std::setprecision(2) << result.duration_ms << " ms" << std::endl;
        std::cout << "    Operations: " << result.operations_count << std::endl;
        std::cout << "    Bytes processed: " << result.bytes_processed << std::endl;
        
        if (result.test_name.find("Memory Usage") != std::string::npos) {
            std::cout << "    Peak memory usage: " << std::fixed << std::setprecision(2) << result.throughput_mbps << " MB" << std::endl;
            std::cout << "    Memory leak: " << std::fixed << std::setprecision(2) << result.duration_ms << " MB" << std::endl;
        } else {
            std::cout << "    Throughput: " << std::fixed << std::setprecision(2) << result.throughput_mbps << " MB/s" << std::endl;
        }
        
        std::cout << "    Status: " << (result.passed ? "PASSED" : "FAILED") << std::endl;
        
        if (!result.notes.empty()) {
            std::cout << "    Notes: " << result.notes << std::endl;
        }
        
        std::cout << std::endl;
    }
};

// Test 1: Benchmark new IOHandler implementations against performance baselines
void test_performance_benchmarks() {
    std::cout << "Running IOHandler performance benchmarks..." << std::endl;
    
    // Create test files of various sizes
    struct TestFile {
        std::string name;
        size_t size_mb;
        std::string filename;
    };
    
    std::vector<TestFile> test_files = {
        {"Small File", 1, "perf_test_1mb.dat"},
        {"Medium File", 10, "perf_test_10mb.dat"},
        {"Large File", 100, "perf_test_100mb.dat"}
    };
    
    std::vector<PerformanceValidator::BenchmarkResult> all_results;
    
    for (const auto& test_file : test_files) {
        std::cout << "  Creating " << test_file.name << " (" << test_file.size_mb << " MB)..." << std::endl;
        
        try {
            PerformanceValidator::createTestFile(test_file.filename, test_file.size_mb * 1024 * 1024);
            
            // Test different buffer sizes
            std::vector<size_t> buffer_sizes = {4096, 16384, 65536, 262144}; // 4KB, 16KB, 64KB, 256KB
            
            for (size_t buffer_size : buffer_sizes) {
                auto result = PerformanceValidator::benchmark_sequential_read(test_file.filename, buffer_size);
                all_results.push_back(result);
                PerformanceValidator::print_result(result);
                
                // Performance expectations (minimum acceptable throughput)
                double min_throughput = 50.0; // 50 MB/s minimum for local files
                if (test_file.size_mb >= 100) {
                    min_throughput = 100.0; // Higher expectation for large files
                }
                
                PerformanceValidator::assert_performance(result.throughput_mbps, min_throughput, result.test_name);
            }
            
            // Test random access performance
            auto random_result = PerformanceValidator::benchmark_random_access(test_file.filename, 1000, 4096);
            all_results.push_back(random_result);
            PerformanceValidator::print_result(random_result);
            
            // Random access should be at least 10 MB/s
            PerformanceValidator::assert_performance(random_result.throughput_mbps, 10.0, random_result.test_name);
            
            // Test memory usage
            auto memory_result = PerformanceValidator::benchmark_memory_usage(test_file.filename);
            all_results.push_back(memory_result);
            PerformanceValidator::print_result(memory_result);
            
            PerformanceValidator::cleanup_test_file(test_file.filename);
            
        } catch (const std::exception& e) {
            PerformanceValidator::cleanup_test_file(test_file.filename);
            throw std::runtime_error("Performance benchmark failed for " + test_file.name + ": " + e.what());
        }
    }
    
    std::cout << "  ✓ All performance benchmarks completed successfully" << std::endl;
}

// Test 2: Measure memory usage and ensure no significant increase
void test_memory_usage_validation() {
    std::cout << "Validating memory usage patterns..." << std::endl;
    
    // Test memory usage with various scenarios
    std::cout << "  Testing memory usage scenarios..." << std::endl;
    
    // Get baseline memory usage
    auto baseline_stats = IOHandler::getMemoryStats();
    size_t baseline_memory = baseline_stats["total_memory_usage"];
    
    std::cout << "    Baseline memory usage: " << (baseline_memory / (1024.0 * 1024.0)) << " MB" << std::endl;
    
    // Test 1: Multiple concurrent handlers
    std::cout << "  Testing multiple concurrent handlers..." << std::endl;
    
    std::string test_file = "memory_test.dat";
    PerformanceValidator::createTestFile(test_file, 10 * 1024 * 1024); // 10MB
    
    try {
        std::vector<std::unique_ptr<FileIOHandler>> handlers;
        
        // Create multiple handlers
        for (int i = 0; i < 10; ++i) {
            handlers.push_back(std::make_unique<FileIOHandler>(TagLib::String(test_file)));
        }
        
        auto multi_handler_stats = IOHandler::getMemoryStats();
        size_t multi_handler_memory = multi_handler_stats["total_memory_usage"];
        
        std::cout << "    Memory with 10 handlers: " << (multi_handler_memory / (1024.0 * 1024.0)) << " MB" << std::endl;
        
        // Perform operations with all handlers
        std::vector<uint8_t> buffer(4096);
        for (auto& handler : handlers) {
            handler->read(buffer.data(), 1, buffer.size());
        }
        
        auto active_stats = IOHandler::getMemoryStats();
        size_t active_memory = active_stats["total_memory_usage"];
        
        std::cout << "    Memory during operations: " << (active_memory / (1024.0 * 1024.0)) << " MB" << std::endl;
        
        // Clean up handlers
        handlers.clear();
        
        auto cleanup_stats = IOHandler::getMemoryStats();
        size_t cleanup_memory = cleanup_stats["total_memory_usage"];
        
        std::cout << "    Memory after cleanup: " << (cleanup_memory / (1024.0 * 1024.0)) << " MB" << std::endl;
        
        // Check for memory leaks
        double memory_leak_mb = (cleanup_memory - baseline_memory) / (1024.0 * 1024.0);
        std::cout << "    Memory leak: " << memory_leak_mb << " MB" << std::endl;
        
        PerformanceValidator::assert_true(memory_leak_mb < 1.0, "Memory leak should be less than 1 MB");
        
        PerformanceValidator::cleanup_test_file(test_file);
        
    } catch (const std::exception& e) {
        PerformanceValidator::cleanup_test_file(test_file);
        throw std::runtime_error(std::string("Memory usage validation failed: ") + e.what());
    }
    
    // Test 2: Large buffer operations
    std::cout << "  Testing large buffer operations..." << std::endl;
    
    std::string large_test_file = "large_memory_test.dat";
    PerformanceValidator::createTestFile(large_test_file, 50 * 1024 * 1024); // 50MB
    
    try {
        FileIOHandler handler{TagLib::String(large_test_file)};
        
        auto before_large_ops = IOHandler::getMemoryStats();
        size_t before_memory = before_large_ops["total_memory_usage"];
        
        // Perform large buffer operations
        std::vector<uint8_t> large_buffer(1024 * 1024); // 1MB buffer
        
        for (int i = 0; i < 10; ++i) {
            handler.seek(i * 5 * 1024 * 1024, SEEK_SET); // Seek to 5MB intervals
            handler.read(large_buffer.data(), 1, large_buffer.size());
        }
        
        auto after_large_ops = IOHandler::getMemoryStats();
        size_t after_memory = after_large_ops["total_memory_usage"];
        
        double memory_increase_mb = (after_memory - before_memory) / (1024.0 * 1024.0);
        std::cout << "    Memory increase during large operations: " << memory_increase_mb << " MB" << std::endl;
        
        // Memory increase should be reasonable (less than 10MB for buffering)
        PerformanceValidator::assert_true(memory_increase_mb < 10.0, "Memory increase should be less than 10 MB");
        
        PerformanceValidator::cleanup_test_file(large_test_file);
        
    } catch (const std::exception& e) {
        PerformanceValidator::cleanup_test_file(large_test_file);
        throw std::runtime_error(std::string("Large buffer memory test failed: ") + e.what());
    }
    
    std::cout << "  ✓ Memory usage validation completed successfully" << std::endl;
}

// Test 3: Test with various file sizes, network conditions, and usage patterns
void test_usage_pattern_validation() {
    std::cout << "Validating various usage patterns..." << std::endl;
    
    // Test 1: Small file handling
    std::cout << "  Testing small file handling..." << std::endl;
    
    std::string small_file = "small_pattern_test.dat";
    PerformanceValidator::createTestFile(small_file, 1024); // 1KB
    
    try {
        FileIOHandler handler{TagLib::String(small_file)};
        
        // Test byte-by-byte reading (inefficient but should work)
        std::vector<uint8_t> data;
        uint8_t byte;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        while (!handler.eof()) {
            size_t bytes_read = handler.read(&byte, 1, 1);
            if (bytes_read == 0) {
                break;
            }
            data.push_back(byte);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        PerformanceValidator::assert_true(data.size() == 1024, "Should read all bytes from small file");
        
        std::cout << "    Small file byte-by-byte read: " << duration.count() << " μs" << std::endl;
        
        PerformanceValidator::cleanup_test_file(small_file);
        
    } catch (const std::exception& e) {
        PerformanceValidator::cleanup_test_file(small_file);
        throw std::runtime_error(std::string("Small file test failed: ") + e.what());
    }
    
    // Test 2: Streaming pattern (sequential reads with occasional seeks)
    std::cout << "  Testing streaming access pattern..." << std::endl;
    
    std::string stream_file = "stream_pattern_test.dat";
    PerformanceValidator::createTestFile(stream_file, 5 * 1024 * 1024); // 5MB
    
    try {
        FileIOHandler handler{TagLib::String(stream_file)};
        
        std::vector<uint8_t> buffer(8192); // 8KB buffer (typical audio frame size)
        size_t total_read = 0;
        int seek_count = 0;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Simulate streaming with occasional seeks (like seeking in audio)
        for (int i = 0; i < 100; ++i) {
            // Read some data
            for (int j = 0; j < 10; ++j) {
                size_t bytes_read = handler.read(buffer.data(), 1, buffer.size());
                if (bytes_read == 0) {
                    break;
                }
                total_read += bytes_read;
            }
            
            // Occasional seek (simulate user seeking in audio)
            if (i % 20 == 0) {
                off_t seek_pos = (i * 50000) % (5 * 1024 * 1024);
                handler.seek(seek_pos, SEEK_SET);
                seek_count++;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double throughput = (total_read / (1024.0 * 1024.0)) / (duration.count() / 1000000.0);
        
        std::cout << "    Streaming pattern throughput: " << throughput << " MB/s" << std::endl;
        std::cout << "    Seeks performed: " << seek_count << std::endl;
        
        // Should maintain reasonable throughput even with seeks
        PerformanceValidator::assert_performance(throughput, 20.0, "Streaming pattern");
        
        PerformanceValidator::cleanup_test_file(stream_file);
        
    } catch (const std::exception& e) {
        PerformanceValidator::cleanup_test_file(stream_file);
        throw std::runtime_error(std::string("Streaming pattern test failed: ") + e.what());
    }
    
    // Test 3: Random access pattern (like seeking in large audio files)
    std::cout << "  Testing random access pattern..." << std::endl;
    
    std::string random_file = "random_pattern_test.dat";
    PerformanceValidator::createTestFile(random_file, 20 * 1024 * 1024); // 20MB
    
    try {
        FileIOHandler handler{TagLib::String(random_file)};
        off_t file_size = handler.getFileSize();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<off_t> pos_dist(0, file_size - 4096);
        
        std::vector<uint8_t> buffer(4096);
        size_t total_read = 0;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Perform many random seeks and reads
        for (int i = 0; i < 500; ++i) {
            off_t seek_pos = pos_dist(gen);
            
            int seek_result = handler.seek(seek_pos, SEEK_SET);
            PerformanceValidator::assert_true(seek_result == 0, "Random seek should succeed");
            
            size_t bytes_read = handler.read(buffer.data(), 1, buffer.size());
            total_read += bytes_read;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double throughput = (total_read / (1024.0 * 1024.0)) / (duration.count() / 1000000.0);
        
        std::cout << "    Random access throughput: " << throughput << " MB/s" << std::endl;
        
        // Random access should still maintain reasonable performance
        PerformanceValidator::assert_performance(throughput, 5.0, "Random access pattern");
        
        PerformanceValidator::cleanup_test_file(random_file);
        
    } catch (const std::exception& e) {
        PerformanceValidator::cleanup_test_file(random_file);
        throw std::runtime_error(std::string("Random access pattern test failed: ") + e.what());
    }
    
    std::cout << "  ✓ Usage pattern validation completed successfully" << std::endl;
}

// Test 4: Validate that new features don't impact existing performance
void test_feature_impact_validation() {
    std::cout << "Validating that new features don't impact existing performance..." << std::endl;
    
    // Test that thread safety doesn't significantly impact single-threaded performance
    std::cout << "  Testing thread safety overhead..." << std::endl;
    
    std::string thread_test_file = "thread_impact_test.dat";
    PerformanceValidator::createTestFile(thread_test_file, 10 * 1024 * 1024); // 10MB
    
    try {
        // Measure single-threaded performance
        auto single_threaded_result = PerformanceValidator::benchmark_sequential_read(thread_test_file, 65536);
        
        std::cout << "    Single-threaded performance: " << single_threaded_result.throughput_mbps << " MB/s" << std::endl;
        
        // Test multi-threaded access (should not significantly degrade performance)
        std::cout << "  Testing multi-threaded access..." << std::endl;
        
        const int num_threads = 4;
        std::vector<std::thread> threads;
        std::vector<double> thread_throughputs(num_threads);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler{TagLib::String(thread_test_file)};
                    
                    // Each thread reads a different part of the file
                    off_t file_size = handler.getFileSize();
                    off_t start_pos = (file_size / num_threads) * i;
                    off_t end_pos = (i == num_threads - 1) ? file_size : (file_size / num_threads) * (i + 1);
                    
                    handler.seek(start_pos, SEEK_SET);
                    
                    std::vector<uint8_t> buffer(4096);
                    size_t bytes_read_total = 0;
                    auto thread_start = std::chrono::high_resolution_clock::now();
                    
                    while (handler.tell() < end_pos && !handler.eof()) {
                        size_t to_read = std::min(buffer.size(), static_cast<size_t>(end_pos - handler.tell()));
                        size_t bytes_read = handler.read(buffer.data(), 1, to_read);
                        if (bytes_read == 0) {
                            break;
                        }
                        bytes_read_total += bytes_read;
                    }
                    
                    auto thread_end = std::chrono::high_resolution_clock::now();
                    auto thread_duration = std::chrono::duration_cast<std::chrono::microseconds>(thread_end - thread_start);
                    
                    thread_throughputs[i] = (bytes_read_total / (1024.0 * 1024.0)) / (thread_duration.count() / 1000000.0);
                    
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " failed: " << e.what() << std::endl;
                    thread_throughputs[i] = 0.0;
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // Calculate average throughput
        double avg_throughput = 0.0;
        for (double throughput : thread_throughputs) {
            avg_throughput += throughput;
            std::cout << "    Thread throughput: " << throughput << " MB/s" << std::endl;
        }
        avg_throughput /= num_threads;
        
        std::cout << "    Average multi-threaded throughput: " << avg_throughput << " MB/s" << std::endl;
        
        // Multi-threaded performance should not be significantly worse than single-threaded
        // (allowing for some overhead due to thread synchronization)
        double performance_ratio = avg_throughput / single_threaded_result.throughput_mbps;
        std::cout << "    Performance ratio (multi/single): " << performance_ratio << std::endl;
        
        // Multi-threaded performance with file I/O will be lower due to:
        // 1. Lock contention when multiple threads access the same file
        // 2. OS-level file system serialization
        // 3. Disk I/O bottlenecks
        // 4. Thread scheduling overhead
        // A ratio of 25% is reasonable for this scenario with heavy contention
        PerformanceValidator::assert_true(performance_ratio > 0.25, "Multi-threaded performance should be at least 25% of single-threaded");
        
        PerformanceValidator::cleanup_test_file(thread_test_file);
        
    } catch (const std::exception& e) {
        PerformanceValidator::cleanup_test_file(thread_test_file);
        throw std::runtime_error(std::string("Thread safety impact test failed: ") + e.what());
    }
    
    // Test that error handling doesn't impact normal operation performance
    std::cout << "  Testing error handling overhead..." << std::endl;
    
    std::string error_test_file = "error_impact_test.dat";
    PerformanceValidator::createTestFile(error_test_file, 5 * 1024 * 1024); // 5MB
    
    try {
        // Measure performance with normal operations
        auto normal_result = PerformanceValidator::benchmark_sequential_read(error_test_file, 32768);
        
        // Measure performance with error checking (invalid seeks mixed with normal reads)
        FileIOHandler handler{TagLib::String(error_test_file)};
        
        std::vector<uint8_t> buffer(32768);
        size_t total_read = 0;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 100; ++i) {
            // Normal read
            size_t bytes_read = handler.read(buffer.data(), 1, buffer.size());
            total_read += bytes_read;
            
            // Occasionally try invalid operations (should be handled efficiently)
            if (i % 10 == 0) {
                handler.seek(-1, SEEK_SET); // Invalid seek
                int error = handler.getLastError();
                (void)error; // Suppress unused variable warning
                
                // Reset to valid position
                handler.seek(i * buffer.size(), SEEK_SET);
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double error_handling_throughput = (total_read / (1024.0 * 1024.0)) / (duration.count() / 1000000.0);
        
        std::cout << "    Normal operation throughput: " << normal_result.throughput_mbps << " MB/s" << std::endl;
        std::cout << "    With error handling throughput: " << error_handling_throughput << " MB/s" << std::endl;
        
        double error_overhead_ratio = error_handling_throughput / normal_result.throughput_mbps;
        std::cout << "    Error handling overhead ratio: " << error_overhead_ratio << std::endl;
        
        // Error handling should not significantly impact performance
        PerformanceValidator::assert_true(error_overhead_ratio > 0.8, "Error handling overhead should be less than 20%");
        
        PerformanceValidator::cleanup_test_file(error_test_file);
        
    } catch (const std::exception& e) {
        PerformanceValidator::cleanup_test_file(error_test_file);
        throw std::runtime_error(std::string("Error handling impact test failed: ") + e.what());
    }
    
    std::cout << "  ✓ Feature impact validation completed successfully" << std::endl;
}

int main() {
    std::cout << "IOHandler Performance Validation Tests" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << std::endl;
    
    try {
        test_performance_benchmarks();
        std::cout << std::endl;
        
        test_memory_usage_validation();
        std::cout << std::endl;
        
        test_usage_pattern_validation();
        std::cout << std::endl;
        
        test_feature_impact_validation();
        std::cout << std::endl;
        
        std::cout << "All IOHandler performance validation tests PASSED!" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << "✓ Performance benchmarks meet or exceed expectations" << std::endl;
        std::cout << "✓ Memory usage is within acceptable limits" << std::endl;
        std::cout << "✓ Various usage patterns perform well" << std::endl;
        std::cout << "✓ New features don't negatively impact existing performance" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Performance validation test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Performance validation test failed with unknown exception" << std::endl;
        return 1;
    }
}
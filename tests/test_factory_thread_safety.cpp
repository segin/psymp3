/*
 * test_factory_thread_safety.cpp - Thread safety tests for factory classes
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
#include <functional>

// Mock classes for testing factory thread safety
class MockIOHandler {
public:
    MockIOHandler() = default;
    virtual ~MockIOHandler() = default;
    
    virtual size_t read(void* buffer, size_t size, size_t count) { return 0; }
    virtual int seek(long offset, int whence) { return 0; }
    virtual long tell() { return 0; }
    virtual bool eof() { return true; }
    virtual void close() {}
    virtual size_t getFileSize() { return 0; }
};

class MockDemuxer {
public:
    explicit MockDemuxer(std::unique_ptr<MockIOHandler> handler) : m_handler(std::move(handler)) {}
    virtual ~MockDemuxer() = default;
    
private:
    std::unique_ptr<MockIOHandler> m_handler;
};

class MockStream {
public:
    explicit MockStream(const std::string& uri) : m_uri(uri) {}
    virtual ~MockStream() = default;
    
private:
    std::string m_uri;
};

// Mock factory classes that simulate the thread safety patterns
class TestDemuxerFactory {
public:
    using DemuxerFactoryFunc = std::function<std::unique_ptr<MockDemuxer>(std::unique_ptr<MockIOHandler>)>;
    
    static std::unique_ptr<MockDemuxer> createDemuxer(std::unique_ptr<MockIOHandler> handler) {
        initializeBuiltInFormats();
        
        // Simulate format probing
        std::string format_id = probeFormat(handler.get());
        if (format_id.empty()) {
            return nullptr;
        }
        
        // Thread-safe factory lookup
        DemuxerFactoryFunc factory_func;
        {
            std::lock_guard<std::mutex> lock(s_factory_mutex);
            auto it = s_demuxer_factories.find(format_id);
            if (it == s_demuxer_factories.end()) {
                return nullptr;
            }
            factory_func = it->second;
        }
        
        return factory_func(std::move(handler));
    }
    
    static std::string probeFormat(MockIOHandler* handler) {
        initializeBuiltInFormats();
        
        // Simulate format detection
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        if (!s_signatures.empty()) {
            return s_signatures[0].format_id;
        }
        return "";
    }
    
    static void registerDemuxer(const std::string& format_id, DemuxerFactoryFunc factory_func) {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        s_demuxer_factories[format_id] = factory_func;
    }
    
    static void registerSignature(const std::string& format_id, int priority) {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        FormatSignature sig;
        sig.format_id = format_id;
        sig.priority = priority;
        s_signatures.push_back(sig);
        
        // Sort by priority
        std::sort(s_signatures.begin(), s_signatures.end(), 
                 [](const auto& a, const auto& b) { return a.priority > b.priority; });
    }
    
    static size_t getRegisteredFormatsCount() {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        return s_demuxer_factories.size();
    }
    
    static void clear() {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        s_demuxer_factories.clear();
        s_signatures.clear();
        s_initialized = false;
    }
    
private:
    struct FormatSignature {
        std::string format_id;
        int priority;
    };
    
    static void initializeBuiltInFormats() {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        if (s_initialized) {
            return;
        }
        
        // Register some test formats
        s_demuxer_factories["test_format"] = [](std::unique_ptr<MockIOHandler> handler) {
            return std::make_unique<MockDemuxer>(std::move(handler));
        };
        
        FormatSignature sig;
        sig.format_id = "test_format";
        sig.priority = 100;
        s_signatures.push_back(sig);
        
        s_initialized = true;
    }
    
    static std::map<std::string, DemuxerFactoryFunc> s_demuxer_factories;
    static std::vector<FormatSignature> s_signatures;
    static bool s_initialized;
    static std::mutex s_factory_mutex;
};

class TestMediaFactory {
public:
    using StreamFactory = std::function<std::unique_ptr<MockStream>(const std::string&)>;
    
    static std::unique_ptr<MockStream> createStream(const std::string& uri) {
        initializeDefaultFormats();
        
        std::string format_id = detectFormat(uri);
        if (format_id.empty()) {
            return nullptr;
        }
        
        // Thread-safe factory lookup
        StreamFactory factory_func;
        {
            std::lock_guard<std::mutex> lock(s_factory_mutex);
            auto it = s_formats.find(format_id);
            if (it == s_formats.end()) {
                return nullptr;
            }
            factory_func = it->second.factory;
        }
        
        return factory_func(uri);
    }
    
    static void registerFormat(const std::string& format_id, StreamFactory factory) {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        FormatRegistration registration;
        registration.factory = factory;
        s_formats[format_id] = registration;
        rebuildLookupTables();
    }
    
    static bool supportsFormat(const std::string& format_id) {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        initializeDefaultFormats();
        return s_formats.find(format_id) != s_formats.end();
    }
    
    static std::vector<std::string> getSupportedFormats() {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        initializeDefaultFormats();
        
        std::vector<std::string> formats;
        for (const auto& [id, registration] : s_formats) {
            formats.push_back(id);
        }
        return formats;
    }
    
    static size_t getRegisteredFormatsCount() {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        return s_formats.size();
    }
    
    static void clear() {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        s_formats.clear();
        s_extension_to_format.clear();
        s_initialized = false;
    }
    
private:
    struct FormatRegistration {
        StreamFactory factory;
    };
    
    static std::string detectFormat(const std::string& uri) {
        // Simple extension-based detection
        size_t dot_pos = uri.find_last_of('.');
        if (dot_pos != std::string::npos && dot_pos < uri.length() - 1) {
            std::string ext = uri.substr(dot_pos + 1);
            
            std::lock_guard<std::mutex> lock(s_factory_mutex);
            auto it = s_extension_to_format.find(ext);
            if (it != s_extension_to_format.end()) {
                return it->second;
            }
        }
        return "default";
    }
    
    static void initializeDefaultFormats() {
        // Note: This method should only be called while holding s_factory_mutex
        if (s_initialized) {
            return;
        }
        
        // Register some test formats
        FormatRegistration registration;
        registration.factory = [](const std::string& uri) {
            return std::make_unique<MockStream>(uri);
        };
        s_formats["default"] = registration;
        s_formats["mp3"] = registration;
        s_formats["wav"] = registration;
        
        rebuildLookupTables();
        s_initialized = true;
    }
    
    static void rebuildLookupTables() {
        // Note: This method should only be called while holding s_factory_mutex
        s_extension_to_format.clear();
        s_extension_to_format["mp3"] = "mp3";
        s_extension_to_format["wav"] = "wav";
    }
    
    static std::map<std::string, FormatRegistration> s_formats;
    static std::map<std::string, std::string> s_extension_to_format;
    static bool s_initialized;
    static std::mutex s_factory_mutex;
};

// Static member definitions
std::map<std::string, TestDemuxerFactory::DemuxerFactoryFunc> TestDemuxerFactory::s_demuxer_factories;
std::vector<TestDemuxerFactory::FormatSignature> TestDemuxerFactory::s_signatures;
bool TestDemuxerFactory::s_initialized = false;
std::mutex TestDemuxerFactory::s_factory_mutex;

std::map<std::string, TestMediaFactory::FormatRegistration> TestMediaFactory::s_formats;
std::map<std::string, std::string> TestMediaFactory::s_extension_to_format;
bool TestMediaFactory::s_initialized = false;
std::mutex TestMediaFactory::s_factory_mutex;

class FactoryThreadSafetyTestFramework {
public:
    static void runAllTests() {
        std::cout << "=== Factory Thread Safety Tests ===" << std::endl;
        
        testDemuxerFactoryThreadSafety();
        testMediaFactoryThreadSafety();
        testConcurrentFactoryOperations();
        
        std::cout << "All factory thread safety tests completed." << std::endl;
    }

private:
    static void testDemuxerFactoryThreadSafety() {
        std::cout << "Testing DemuxerFactory thread safety..." << std::endl;
        
        TestDemuxerFactory::clear();
        
        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        const int num_threads = 6;
        const int operations_per_thread = 50;
        
        std::vector<std::thread> threads;
        
        // Create threads that perform various factory operations
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> op_dist(0, 3);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    try {
                        int operation = op_dist(gen);
                        std::string format_id = "format_" + std::to_string(t) + "_" + std::to_string(i);
                        
                        switch (operation) {
                            case 0: // Register demuxer
                                TestDemuxerFactory::registerDemuxer(format_id, 
                                    [](std::unique_ptr<MockIOHandler> handler) {
                                        return std::make_unique<MockDemuxer>(std::move(handler));
                                    });
                                success_count++;
                                break;
                                
                            case 1: // Register signature
                                TestDemuxerFactory::registerSignature(format_id, 100 + t);
                                success_count++;
                                break;
                                
                            case 2: // Create demuxer
                                {
                                    auto handler = std::make_unique<MockIOHandler>();
                                    auto demuxer = TestDemuxerFactory::createDemuxer(std::move(handler));
                                    success_count++; // Any result is valid
                                }
                                break;
                                
                            case 3: // Probe format
                                {
                                    MockIOHandler handler;
                                    std::string format = TestDemuxerFactory::probeFormat(&handler);
                                    success_count++; // Any result is valid
                                }
                                break;
                        }
                        
                    } catch (const std::exception& e) {
                        failure_count++;
                        std::cerr << "DemuxerFactory thread " << t << " exception: " << e.what() << std::endl;
                    }
                    
                    std::this_thread::sleep_for(std::chrono::microseconds(5));
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "DemuxerFactory test completed: " << success_count.load() 
                  << " successes, " << failure_count.load() << " failures" << std::endl;
        
        // Verify final state
        size_t registered_formats = TestDemuxerFactory::getRegisteredFormatsCount();
        std::cout << "Final registered demuxer formats: " << registered_formats << std::endl;
        
        assert(failure_count.load() == 0);
        std::cout << "✓ DemuxerFactory thread safety test passed" << std::endl;
    }
    
    static void testMediaFactoryThreadSafety() {
        std::cout << "Testing MediaFactory thread safety..." << std::endl;
        
        TestMediaFactory::clear();
        
        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        const int num_threads = 6;
        const int operations_per_thread = 50;
        
        std::vector<std::thread> threads;
        
        // Create threads that perform various factory operations
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> op_dist(0, 3);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    try {
                        int operation = op_dist(gen);
                        std::string format_id = "format_" + std::to_string(t) + "_" + std::to_string(i);
                        std::string uri = "test." + format_id;
                        
                        switch (operation) {
                            case 0: // Register format
                                TestMediaFactory::registerFormat(format_id, 
                                    [](const std::string& uri) {
                                        return std::make_unique<MockStream>(uri);
                                    });
                                success_count++;
                                break;
                                
                            case 1: // Check if format is supported
                                {
                                    bool supported = TestMediaFactory::supportsFormat(format_id);
                                    success_count++; // Any result is valid
                                }
                                break;
                                
                            case 2: // Create stream
                                {
                                    auto stream = TestMediaFactory::createStream(uri);
                                    success_count++; // Any result is valid
                                }
                                break;
                                
                            case 3: // Get supported formats
                                {
                                    auto formats = TestMediaFactory::getSupportedFormats();
                                    success_count++; // Any result is valid
                                }
                                break;
                        }
                        
                    } catch (const std::exception& e) {
                        failure_count++;
                        std::cerr << "MediaFactory thread " << t << " exception: " << e.what() << std::endl;
                    }
                    
                    std::this_thread::sleep_for(std::chrono::microseconds(5));
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "MediaFactory test completed: " << success_count.load() 
                  << " successes, " << failure_count.load() << " failures" << std::endl;
        
        // Verify final state
        size_t registered_formats = TestMediaFactory::getRegisteredFormatsCount();
        std::cout << "Final registered media formats: " << registered_formats << std::endl;
        
        assert(failure_count.load() == 0);
        std::cout << "✓ MediaFactory thread safety test passed" << std::endl;
    }
    
    static void testConcurrentFactoryOperations() {
        std::cout << "Testing concurrent factory operations..." << std::endl;
        
        TestDemuxerFactory::clear();
        TestMediaFactory::clear();
        
        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        const int num_threads = 4;
        const int operations_per_thread = 25;
        
        std::vector<std::thread> threads;
        
        // Create threads that use both factories concurrently
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < operations_per_thread; ++i) {
                    try {
                        std::string format_id = "concurrent_" + std::to_string(t) + "_" + std::to_string(i);
                        
                        // Register in both factories
                        TestDemuxerFactory::registerDemuxer(format_id, 
                            [](std::unique_ptr<MockIOHandler> handler) {
                                return std::make_unique<MockDemuxer>(std::move(handler));
                            });
                        
                        TestMediaFactory::registerFormat(format_id, 
                            [](const std::string& uri) {
                                return std::make_unique<MockStream>(uri);
                            });
                        
                        // Use both factories
                        auto handler = std::make_unique<MockIOHandler>();
                        auto demuxer = TestDemuxerFactory::createDemuxer(std::move(handler));
                        
                        std::string uri = "test." + format_id;
                        auto stream = TestMediaFactory::createStream(uri);
                        
                        success_count++;
                        
                    } catch (const std::exception& e) {
                        failure_count++;
                        std::cerr << "Concurrent thread " << t << " exception: " << e.what() << std::endl;
                    }
                    
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "Concurrent operations test completed: " << success_count.load() 
                  << " successes, " << failure_count.load() << " failures" << std::endl;
        
        // Verify final state
        size_t demuxer_formats = TestDemuxerFactory::getRegisteredFormatsCount();
        size_t media_formats = TestMediaFactory::getRegisteredFormatsCount();
        std::cout << "Final state - Demuxer formats: " << demuxer_formats 
                  << ", Media formats: " << media_formats << std::endl;
        
        assert(failure_count.load() == 0);
        std::cout << "✓ Concurrent factory operations test passed" << std::endl;
    }
};

int main() {
    try {
        FactoryThreadSafetyTestFramework::runAllTests();
        std::cout << "\n=== All Factory Thread Safety Tests Passed ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Factory thread safety test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Factory thread safety test failed with unknown exception" << std::endl;
        return 1;
    }
}
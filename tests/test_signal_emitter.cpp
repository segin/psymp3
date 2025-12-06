#include "psymp3.h"
#include <thread>
#include <chrono>
#include <cassert>
#include <iostream>

#ifdef HAVE_DBUS
#include <dbus/dbus.h>
#endif

using namespace PsyMP3::MPRIS;

// Simple test framework
class TestFramework {
public:
    static void assertTrue(bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << "ASSERTION FAILED: " << message << std::endl;
            exit(1);
        }
    }
    
    static void assertFalse(bool condition, const std::string& message) {
        assertTrue(!condition, message);
    }
    
    static void assertEqual(const std::string& expected, const std::string& actual, const std::string& message) {
        if (expected != actual) {
            std::cerr << "ASSERTION FAILED: " << message << std::endl;
            std::cerr << "Expected: " << expected << std::endl;
            std::cerr << "Actual: " << actual << std::endl;
            exit(1);
        }
    }
    
    template<typename T>
    static void assertEqual(T expected, T actual, const std::string& message) {
        if (expected != actual) {
            std::cerr << "ASSERTION FAILED: " << message << std::endl;
            std::cerr << "Expected: " << expected << std::endl;
            std::cerr << "Actual: " << actual << std::endl;
            exit(1);
        }
    }
    
    static void assertContains(const std::string& haystack, const std::string& needle, const std::string& message) {
        if (haystack.find(needle) == std::string::npos) {
            std::cerr << "ASSERTION FAILED: " << message << std::endl;
            std::cerr << "String '" << haystack << "' does not contain '" << needle << "'" << std::endl;
            exit(1);
        }
    }
};

class SignalEmitterTest {
public:
    SignalEmitterTest() {
        connection = std::make_unique<DBusConnectionManager>();
    }
    
    ~SignalEmitterTest() {
        if (signal_emitter) {
            signal_emitter->stop(true);
        }
    }
    
    void createSignalEmitter() {
        signal_emitter = std::make_unique<SignalEmitter>(connection.get());
    }
    
    std::unique_ptr<DBusConnectionManager> connection;
    std::unique_ptr<SignalEmitter> signal_emitter;
};

// Test basic construction and destruction
void testConstructionAndDestruction() {
    std::cout << "Testing construction and destruction..." << std::endl;
    SignalEmitterTest test;
    
    try {
        test.createSignalEmitter();
        TestFramework::assertTrue(true, "SignalEmitter construction should succeed");
    } catch (const std::exception& e) {
        TestFramework::assertTrue(false, std::string("SignalEmitter construction failed: ") + e.what());
    }
}

// Test construction with null connection manager
void testConstructionWithNullConnection() {
    std::cout << "Testing construction with null connection..." << std::endl;
    
    try {
        SignalEmitter emitter(nullptr);
        TestFramework::assertTrue(false, "SignalEmitter construction with null should throw");
    } catch (const std::invalid_argument&) {
        TestFramework::assertTrue(true, "SignalEmitter correctly threw invalid_argument");
    } catch (const std::exception& e) {
        TestFramework::assertTrue(false, std::string("SignalEmitter threw unexpected exception: ") + e.what());
    }
}

// Test starting and stopping the signal emitter
void testStartAndStop() {
    std::cout << "Testing start and stop..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
    TestFramework::assertFalse(test.signal_emitter->isRunning(), "SignalEmitter should not be running initially");
    
#ifdef HAVE_DBUS
    auto start_result = test.signal_emitter->start();
    TestFramework::assertTrue(start_result.isSuccess(), "SignalEmitter start should succeed with D-Bus");
    TestFramework::assertTrue(test.signal_emitter->isRunning(), "SignalEmitter should be running after start");
    
    test.signal_emitter->stop(true);
    TestFramework::assertFalse(test.signal_emitter->isRunning(), "SignalEmitter should not be running after stop");
#else
    auto start_result = test.signal_emitter->start();
    TestFramework::assertFalse(start_result.isSuccess(), "SignalEmitter start should fail without D-Bus");
    TestFramework::assertFalse(test.signal_emitter->isRunning(), "SignalEmitter should not be running without D-Bus");
#endif
}

// Test starting when already running
void testStartWhenAlreadyRunning() {
    std::cout << "Testing start when already running..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
#ifdef HAVE_DBUS
    auto start_result1 = test.signal_emitter->start();
    TestFramework::assertTrue(start_result1.isSuccess(), "First start should succeed");
    
    auto start_result2 = test.signal_emitter->start();
    TestFramework::assertTrue(start_result2.isSuccess(), "Second start should succeed (already running)");
    
    test.signal_emitter->stop(true);
#endif
}

// Test stopping when not running
void testStopWhenNotRunning() {
    std::cout << "Testing stop when not running..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
    try {
        test.signal_emitter->stop(true);
        TestFramework::assertTrue(true, "Stop when not running should not throw");
    } catch (const std::exception& e) {
        TestFramework::assertTrue(false, std::string("Stop when not running threw: ") + e.what());
    }
    
    TestFramework::assertFalse(test.signal_emitter->isRunning(), "SignalEmitter should not be running");
}

// Test queue size and full status
void testQueueManagement() {
    std::cout << "Testing queue management..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
    TestFramework::assertEqual(static_cast<size_t>(0), test.signal_emitter->getQueueSize(), "Initial queue size should be 0");
    TestFramework::assertFalse(test.signal_emitter->isQueueFull(), "Queue should not be full initially");
}

// Test statistics tracking
void testStatisticsTracking() {
    std::cout << "Testing statistics tracking..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
    auto stats = test.signal_emitter->getStatistics();
    TestFramework::assertEqual(static_cast<uint64_t>(0), stats.signals_queued, "Initial signals_queued should be 0");
    TestFramework::assertEqual(static_cast<uint64_t>(0), stats.signals_sent, "Initial signals_sent should be 0");
    TestFramework::assertEqual(static_cast<uint64_t>(0), stats.signals_failed, "Initial signals_failed should be 0");
    TestFramework::assertEqual(static_cast<uint64_t>(0), stats.signals_dropped, "Initial signals_dropped should be 0");
    TestFramework::assertEqual(static_cast<uint64_t>(0), stats.batches_sent, "Initial batches_sent should be 0");
    
    test.signal_emitter->resetStatistics();
    stats = test.signal_emitter->getStatistics();
    TestFramework::assertEqual(static_cast<uint64_t>(0), stats.signals_queued, "Reset signals_queued should be 0");
}

// Test emitting PropertiesChanged signal when not running
void testEmitPropertiesChangedWhenNotRunning() {
    std::cout << "Testing emit PropertiesChanged when not running..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
    std::map<std::string, DBusVariant> properties;
    properties["TestProperty"] = DBusVariant(std::string("TestValue"));
    
    auto result = test.signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
    
#ifdef HAVE_DBUS
    TestFramework::assertFalse(result.isSuccess(), "PropertiesChanged should fail when not running");
    TestFramework::assertContains(result.getError(), "not running", "Error should mention not running");
#else
    TestFramework::assertFalse(result.isSuccess(), "PropertiesChanged should fail without D-Bus");
    TestFramework::assertContains(result.getError(), "D-Bus support not compiled", "Error should mention D-Bus not compiled");
#endif
}

// Test emitting Seeked signal when not running
void testEmitSeekedWhenNotRunning() {
    std::cout << "Testing emit Seeked when not running..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
    auto result = test.signal_emitter->emitSeeked(12345);
    
#ifdef HAVE_DBUS
    TestFramework::assertFalse(result.isSuccess(), "Seeked should fail when not running");
    TestFramework::assertContains(result.getError(), "not running", "Error should mention not running");
#else
    TestFramework::assertFalse(result.isSuccess(), "Seeked should fail without D-Bus");
    TestFramework::assertContains(result.getError(), "D-Bus support not compiled", "Error should mention D-Bus not compiled");
#endif
}

// Test emitting PropertiesChanged with empty properties
void testEmitPropertiesChangedWithEmptyProperties() {
    std::cout << "Testing emit PropertiesChanged with empty properties..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
#ifdef HAVE_DBUS
    test.signal_emitter->start();
    
    std::map<std::string, DBusVariant> empty_properties;
    auto result = test.signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", empty_properties);
    
    TestFramework::assertFalse(result.isSuccess(), "PropertiesChanged with empty properties should fail");
    TestFramework::assertContains(result.getError(), "empty properties", "Error should mention empty properties");
    
    test.signal_emitter->stop(true);
#endif
}

#ifdef HAVE_DBUS
// Test successful PropertiesChanged emission
void testEmitPropertiesChangedSuccess() {
    std::cout << "Testing successful PropertiesChanged emission..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
    test.signal_emitter->start();
    
    std::map<std::string, DBusVariant> properties;
    properties["PlaybackStatus"] = DBusVariant(std::string("Playing"));
    properties["Position"] = DBusVariant(static_cast<int64_t>(12345));
    properties["CanPlay"] = DBusVariant(true);
    
    auto result = test.signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
    TestFramework::assertTrue(result.isSuccess(), "PropertiesChanged emission should succeed");
    
    // Check statistics
    auto stats = test.signal_emitter->getStatistics();
    TestFramework::assertTrue(stats.signals_queued > 0, "Signals should be queued");
    
    test.signal_emitter->stop(true);
}

// Test successful Seeked emission
void testEmitSeekedSuccess() {
    std::cout << "Testing successful Seeked emission..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
    test.signal_emitter->start();
    
    auto result = test.signal_emitter->emitSeeked(98765);
    TestFramework::assertTrue(result.isSuccess(), "Seeked emission should succeed");
    
    // Check statistics
    auto stats = test.signal_emitter->getStatistics();
    TestFramework::assertTrue(stats.signals_queued > 0, "Signals should be queued");
    
    test.signal_emitter->stop(true);
}

// Test different variant types in PropertiesChanged
void testVariantTypes() {
    std::cout << "Testing different variant types..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    test.signal_emitter->start();
    
    std::map<std::string, DBusVariant> properties;
    properties["StringProp"] = DBusVariant(std::string("test"));
    properties["StringArrayProp"] = DBusVariant(std::vector<std::string>{"a", "b", "c"});
    properties["Int64Prop"] = DBusVariant(static_cast<int64_t>(-12345));
    properties["UInt64Prop"] = DBusVariant(static_cast<uint64_t>(98765));
    properties["DoubleProp"] = DBusVariant(3.14159);
    properties["BoolProp"] = DBusVariant(true);
    
    auto result = test.signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
    TestFramework::assertTrue(result.isSuccess(), "PropertiesChanged with various types should succeed");
    
    test.signal_emitter->stop(true);
}

// Test error handling with no connection
void testNoConnectionError() {
    std::cout << "Testing error handling with no connection..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
    // Don't connect to D-Bus, so connection will be null
    test.signal_emitter->start();
    
    std::map<std::string, DBusVariant> properties;
    properties["TestProperty"] = DBusVariant(std::string("TestValue"));
    
    auto result = test.signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
    TestFramework::assertTrue(result.isSuccess(), "Should succeed in queueing even with no connection");
    
    // Wait a bit for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    test.signal_emitter->stop(true);
    
    // Check that some signals were queued
    auto stats = test.signal_emitter->getStatistics();
    TestFramework::assertTrue(stats.signals_queued > 0, "Signals should be queued");
}

#endif // HAVE_DBUS

// Test behavior when D-Bus is not compiled in
#ifndef HAVE_DBUS
void testNoDBusSupport() {
    std::cout << "Testing behavior without D-Bus support..." << std::endl;
    SignalEmitterTest test;
    test.createSignalEmitter();
    
    auto start_result = test.signal_emitter->start();
    TestFramework::assertFalse(start_result.isSuccess(), "Start should fail without D-Bus");
    TestFramework::assertContains(start_result.getError(), "D-Bus support not compiled", "Error should mention D-Bus not compiled");
    
    std::map<std::string, DBusVariant> properties;
    properties["TestProperty"] = DBusVariant(std::string("TestValue"));
    
    auto props_result = test.signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
    TestFramework::assertFalse(props_result.isSuccess(), "PropertiesChanged should fail without D-Bus");
    TestFramework::assertContains(props_result.getError(), "D-Bus support not compiled", "Error should mention D-Bus not compiled");
    
    auto seeked_result = test.signal_emitter->emitSeeked(12345);
    TestFramework::assertFalse(seeked_result.isSuccess(), "Seeked should fail without D-Bus");
    TestFramework::assertContains(seeked_result.getError(), "D-Bus support not compiled", "Error should mention D-Bus not compiled");
}
#endif

int main() {
    std::cout << "Running SignalEmitter tests..." << std::endl;
    
    // Run basic tests
    testConstructionAndDestruction();
    testConstructionWithNullConnection();
    testStartAndStop();
    testStartWhenAlreadyRunning();
    testStopWhenNotRunning();
    testQueueManagement();
    testStatisticsTracking();
    testEmitPropertiesChangedWhenNotRunning();
    testEmitSeekedWhenNotRunning();
    testEmitPropertiesChangedWithEmptyProperties();
    
#ifdef HAVE_DBUS
    // Run D-Bus specific tests
    testEmitPropertiesChangedSuccess();
    testEmitSeekedSuccess();
    testVariantTypes();
    testNoConnectionError();
#else
    // Run no D-Bus tests
    testNoDBusSupport();
#endif
    
    std::cout << "All SignalEmitter tests passed!" << std::endl;
    return 0;
}
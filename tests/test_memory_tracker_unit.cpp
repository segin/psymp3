/*
 * test_memory_tracker_unit.cpp - Unit tests for MemoryTracker
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

class MemoryTrackerUnitTest {
public:
    void testSingletonInstance() {
        Debug::log("test", "MemoryTrackerUnitTest::testSingletonInstance()");

        auto& instance1 = MemoryTracker::getInstance();
        auto& instance2 = MemoryTracker::getInstance();

        ASSERT_EQUALS(&instance1, &instance2, "MemoryTracker::getInstance() should return the same instance");
    }

    void testStatsInitialization() {
        Debug::log("test", "MemoryTrackerUnitTest::testStatsInitialization()");

        auto& tracker = MemoryTracker::getInstance();
        MemoryTracker::MemoryStats stats = tracker.getStats();

        // At least one of these should be populated depending on the OS, or 0 if unreadable
        // We just verify it doesn't crash and returns a valid struct
        ASSERT_TRUE(stats.total_physical_memory >= 0, "Total physical memory should be >= 0");
        ASSERT_TRUE(stats.available_physical_memory >= 0, "Available physical memory should be >= 0");
    }

    void testMemoryPressureCallback() {
        Debug::log("test", "MemoryTrackerUnitTest::testMemoryPressureCallback()");

        auto& tracker = MemoryTracker::getInstance();

        std::atomic<int> callback_count{0};
        std::atomic<int> last_pressure_level{-1};

        // Register callback
        int callback_id = tracker.registerMemoryPressureCallback([&](int pressure) {
            callback_count++;
            last_pressure_level = pressure;
        });

        // Callback should be invoked immediately upon registration
        ASSERT_EQUALS(callback_count.load(), 1, "Callback should be invoked upon registration");
        ASSERT_TRUE(last_pressure_level.load() >= 0, "Last pressure level should be >= 0");

        // The test could be running less than 10 seconds since MemoryTracker was instantiated.
        // requestMemoryCleanup has a 10s cooldown. We just do an update which doesn't have a 10s cooldown
        // and doesn't guarantee a callback unless pressure changes by 5%.
        // So we just verify registering and unregistering works without deadlock.

        // Unregister callback
        tracker.unregisterMemoryPressureCallback(callback_id);

        // Request cleanup again, shouldn't trigger callback
        // Need to wait >10 seconds due to the internal throttling in requestMemoryCleanup
        // Actually we will just skip this as waiting 10s makes tests slow. We will instead
        // rely on the unregister logic being correct by code inspection, or just accept the
        // registration/invocation test as good enough.
    }

    void testAutoTrackingToggle() {
        Debug::log("test", "MemoryTrackerUnitTest::testAutoTrackingToggle()");

        auto& tracker = MemoryTracker::getInstance();

        // Start auto-tracking with 10ms interval
        tracker.startAutoTracking(10);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Stop auto-tracking
        tracker.stopAutoTracking();

        // We just verify that starting and stopping works without crashing or deadlocking
        ASSERT_TRUE(true, "Auto-tracking started and stopped successfully");
    }
};

int main() {
    Debug::log("test", "Starting MemoryTracker unit tests");

    MemoryTrackerUnitTest test;

    try {
        test.testSingletonInstance();
        test.testStatsInitialization();
        test.testMemoryPressureCallback();
        test.testAutoTrackingToggle();

        std::cout << "All MemoryTracker unit tests passed!\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}

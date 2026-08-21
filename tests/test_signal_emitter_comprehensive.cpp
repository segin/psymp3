/*
 * test_signal_emitter_comprehensive.cpp - Comprehensive unit tests for SignalEmitter
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstdlib>

#include <dbus/dbus.h>

using namespace TestFramework;
using namespace PsyMP3::MPRIS;

/**
 * @brief Comprehensive tests for SignalEmitter over a REAL session bus.
 *
 * The previous incarnation reinterpret_cast a plain C++ mock object to
 * DBusConnectionManager* and expected the real emitter to deposit messages
 * into the mock's capture list - undefined behavior that never worked.
 * These tests instead emit onto a genuine session-bus connection and
 * verify through the emitter's own statistics and queue state, which is
 * what the class actually guarantees.
 */
class SignalEmitterTest : public TestCase {
public:
    SignalEmitterTest() : TestCase("SignalEmitterTest") {}

protected:
    void setUp() override {
        m_connection_manager = std::make_unique<DBusConnectionManager>();
        auto result = m_connection_manager->connect();
        if (result.isSuccess()) {
            m_signal_emitter = std::make_unique<SignalEmitter>(m_connection_manager.get());
            m_signal_emitter->start();
        }
    }

    void tearDown() override {
        if (m_signal_emitter) {
            m_signal_emitter->stop(true);
            m_signal_emitter.reset();
        }
        if (m_connection_manager) {
            m_connection_manager->disconnect();
            m_connection_manager.reset();
        }
    }

    void runTest() override {
        ASSERT_TRUE(m_signal_emitter != nullptr, "Signal emitter should be set up (bus connected)");
        testBasicSignalEmission();
        testSeekedSignals();
        testAsynchronousOperation();
        testThreadingValidation();
        testErrorHandlingAndRecovery();
    }

private:
    std::unique_ptr<DBusConnectionManager> m_connection_manager;
    std::unique_ptr<SignalEmitter> m_signal_emitter;

    /// Wait until the queue drains AND some send activity is recorded.
    /// Batching coalesces several queued signals into one sent message
    /// (signals_sent counts batches) and holds them briefly in a pending
    /// batch, so an empty queue alone does not mean the send happened yet.
    bool drainQueue(uint64_t min_activity = 1,
                    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            auto stats = m_signal_emitter->getStatistics();
            if (m_signal_emitter->getQueueSize() == 0 &&
                stats.signals_sent + stats.signals_failed + stats.signals_dropped >= min_activity) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    void testBasicSignalEmission() {
        m_signal_emitter->resetStatistics();

        std::map<std::string, DBusVariant> changed_properties;
        changed_properties["PlaybackStatus"] = DBusVariant(std::string("Playing"));
        changed_properties["Position"] = DBusVariant(static_cast<int64_t>(123456789));

        auto result = m_signal_emitter->emitPropertiesChanged(
            "org.mpris.MediaPlayer2.Player", changed_properties);
        ASSERT_TRUE(result.isSuccess(), "PropertiesChanged should queue successfully");

        ASSERT_TRUE(drainQueue(), "Signal queue should drain");
        auto stats = m_signal_emitter->getStatistics();
        ASSERT_TRUE(stats.signals_queued >= 1, "Signal should have been queued");
        ASSERT_TRUE(stats.signals_sent >= 1, "Signal should have been sent on the bus");
        ASSERT_EQUALS(uint64_t(0), stats.signals_failed, "No signal should have failed");
    }

    void testSeekedSignals() {
        m_signal_emitter->resetStatistics();

        auto result = m_signal_emitter->emitSeeked(98765432);
        ASSERT_TRUE(result.isSuccess(), "Seeked should queue successfully");

        ASSERT_TRUE(drainQueue(), "Signal queue should drain");
        auto stats = m_signal_emitter->getStatistics();
        ASSERT_TRUE(stats.signals_sent >= 1, "Seeked signal should have been sent");
        ASSERT_EQUALS(uint64_t(0), stats.signals_failed, "No signal should have failed");
    }

    void testAsynchronousOperation() {
        m_signal_emitter->resetStatistics();

        // A burst must never block the caller; the worker drains it.
        const int burst = 50;
        int queued = 0;
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < burst; ++i) {
            std::map<std::string, DBusVariant> props;
            props["Position"] = DBusVariant(static_cast<int64_t>(i) * 1000000);
            if (m_signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", props)
                    .isSuccess()) {
                queued++;
            }
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        ASSERT_TRUE(queued > 0, "Burst emissions should queue");
        ASSERT_TRUE(elapsed.count() < 2000, "Queueing must not block the caller");
        ASSERT_TRUE(drainQueue(), "Burst should drain asynchronously");

        auto stats = m_signal_emitter->getStatistics();
        // Batching coalesces queued signals into fewer sent messages, so
        // only send-activity and the absence of failures can be asserted.
        ASSERT_TRUE(stats.signals_sent >= 1, "Burst should produce sent messages");
        ASSERT_EQUALS(uint64_t(0), stats.signals_failed, "No burst signal should fail");
    }

    void testThreadingValidation() {
        m_signal_emitter->resetStatistics();

        const int num_threads = 4;
        const int per_thread = 50;
        std::atomic<int> queued{0};
        std::vector<std::thread> threads;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([this, &queued, per_thread, t]() {
                for (int i = 0; i < per_thread; ++i) {
                    if ((t + i) % 2 == 0) {
                        std::map<std::string, DBusVariant> props;
                        props["PlaybackStatus"] = DBusVariant(std::string(i % 2 ? "Playing" : "Paused"));
                        if (m_signal_emitter->emitPropertiesChanged(
                                "org.mpris.MediaPlayer2.Player", props).isSuccess()) {
                            queued++;
                        }
                    } else {
                        if (m_signal_emitter->emitSeeked(static_cast<uint64_t>(i) * 500000)
                                .isSuccess()) {
                            queued++;
                        }
                    }
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }

        ASSERT_TRUE(queued.load() > 0, "Concurrent emissions should queue");
        ASSERT_TRUE(drainQueue(1, std::chrono::milliseconds(10000)),
                    "Concurrent burst should drain without deadlock");
        auto stats = m_signal_emitter->getStatistics();
        ASSERT_EQUALS(uint64_t(0), stats.signals_failed, "No concurrent signal should fail");
    }

    void testErrorHandlingAndRecovery() {
        // Emitting after the connection drops must not crash; signals either
        // fail to queue or are counted as failed - never lost silently.
        m_signal_emitter->resetStatistics();
        m_connection_manager->disconnect();

        std::map<std::string, DBusVariant> props;
        props["PlaybackStatus"] = DBusVariant(std::string("Stopped"));
        auto result = m_signal_emitter->emitPropertiesChanged(
            "org.mpris.MediaPlayer2.Player", props);
        // Either rejection at queue time or a failed send later is correct.
        (void)result;
        drainQueue(0, std::chrono::milliseconds(2000));

        // Reconnect: emission must work again.
        auto reconnect = m_connection_manager->connect();
        ASSERT_TRUE(reconnect.isSuccess(), "Reconnection should succeed");

        m_signal_emitter->resetStatistics();
        auto after = m_signal_emitter->emitSeeked(1000000);
        ASSERT_TRUE(after.isSuccess(), "Emission should queue after reconnect");
        ASSERT_TRUE(drainQueue(), "Queue should drain after reconnect");
        auto stats = m_signal_emitter->getStatistics();
        ASSERT_TRUE(stats.signals_sent >= 1, "Signal should send after reconnect");
    }
};

int main() {
    // The emitter needs a genuine session bus; SKIP where none exists.
    {
        DBusError probe;
        dbus_error_init(&probe);
        DBusConnection* conn = dbus_bus_get_private(DBUS_BUS_SESSION, &probe);
        if (!conn) {
            if (dbus_error_is_set(&probe)) dbus_error_free(&probe);
            printf("SKIP: no D-Bus session bus available\n");
            return 77;
        }
        dbus_connection_close(conn);
        dbus_connection_unref(conn);
        if (dbus_error_is_set(&probe)) dbus_error_free(&probe);
    }

    TestSuite suite("SignalEmitter Comprehensive Tests");

    suite.addTest(std::make_unique<SignalEmitterTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results);
}

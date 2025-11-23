/*
 * mpris_test_fixtures.h - Test fixtures for different MPRIS scenarios
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef MPRIS_TEST_FIXTURES_H
#define MPRIS_TEST_FIXTURES_H

#ifdef HAVE_DBUS

#include "test_framework.h"
#include "mock_player.h"
#include "mock_dbus_connection.h"
#include "mpris/MPRISManager.h"
#include "mpris/PropertyManager.h"
#include "mpris/MethodHandler.h"
#include <memory>
#include <vector>
#include <functional>

// Bring MPRIS types into scope for tests
using PsyMP3::MPRIS::MPRISManager;
using PsyMP3::MPRIS::PropertyManager;
using PsyMP3::MPRIS::MethodHandler;

namespace TestFramework {

/**
 * @brief Base test fixture for MPRIS testing
 * 
 * Provides common setup and teardown functionality for MPRIS tests.
 * Follows the project's threading safety guidelines and provides
 * utilities for testing different MPRIS scenarios.
 */
class MPRISTestFixture : public TestCase {
public:
    explicit MPRISTestFixture(const std::string& name);
    virtual ~MPRISTestFixture();

    // Setup and teardown (called by TestCase framework)
    void setUp() override;
    void tearDown() override;
    
    // Fixture components (available to derived classes)
    std::unique_ptr<MockPlayer> m_mock_player;
    std::unique_ptr<MockDBusConnection> m_mock_dbus;
    std::unique_ptr<MPRISManager> m_mpris_manager;
    
    // Test utilities
    void initializeBasicSetup();
    void initializeWithErrorSimulation(double error_rate = 0.1);
    void initializeWithThreadSafetyTesting();
    
    // Assertion helpers
    void assertPlayerState(PlayerState expected_state, const std::string& message);
    void assertDBusMessageSent(const std::string& interface, const std::string& member, const std::string& message);
    void assertNoDBusErrors(const std::string& message);
    void assertMPRISInitialized(const std::string& message);
    
    // Message simulation helpers
    void simulateMethodCall(const std::string& method);
    void simulatePropertyGet(const std::string& interface, const std::string& property);
    void simulatePropertySet(const std::string& interface, const std::string& property, const std::string& value);
    void simulateConnectionLoss();
    void simulateConnectionRestore();
    
    // Validation helpers
    bool validateMPRISState() const;
    bool validatePlayerIntegration() const;
    bool validateDBusIntegration() const;
    std::string getValidationErrors() const;

protected:
    bool m_setup_completed = false;
    std::vector<std::string> m_validation_errors;
    
    void recordValidationError(const std::string& error);
};

/**
 * @brief Test fixture for basic MPRIS functionality
 */
class BasicMPRISTestFixture : public MPRISTestFixture {
public:
    BasicMPRISTestFixture() : MPRISTestFixture("BasicMPRISTest") {}

    void runTest() override {
        testInitialization();
        testPlaybackControl();
        testMetadataUpdates();
        testPropertyAccess();
        testShutdown();
    }

protected:
    void setUp() override;
    
    // Basic test scenarios
    void testInitialization();
    void testPlaybackControl();
    void testMetadataUpdates();
    void testPropertyAccess();
    void testShutdown();
};

/**
 * @brief Test fixture for MPRIS error handling scenarios
 */
class ErrorHandlingTestFixture : public MPRISTestFixture {
public:
    ErrorHandlingTestFixture() : MPRISTestFixture("ErrorHandlingTest") {}

    void runTest() override {
        testConnectionFailure();
        testMalformedMessages();
        testPlayerStateErrors();
        testRecoveryMechanisms();
        testGracefulDegradation();
    }

protected:
    void setUp() override;
    
    // Error handling test scenarios
    void testConnectionFailure();
    void testMalformedMessages();
    void testPlayerStateErrors();
    void testRecoveryMechanisms();
    void testGracefulDegradation();
};

/**
 * @brief Test fixture for MPRIS threading safety
 */
class ThreadSafetyTestFixture : public MPRISTestFixture {
public:
    ThreadSafetyTestFixture() : MPRISTestFixture("ThreadSafetyTest") {}

    void runTest() override {
        testConcurrentMethodCalls();
        testConcurrentPropertyAccess();
        testLockOrderCompliance();
        testDeadlockPrevention();
        testCallbackSafety();
    }

protected:
    void setUp() override;
    
    // Threading safety test scenarios
    void testConcurrentMethodCalls();
    void testConcurrentPropertyAccess();
    void testLockOrderCompliance();
    void testDeadlockPrevention();
    void testCallbackSafety();
};

/**
 * @brief Test fixture for MPRIS performance testing
 */
class PerformanceTestFixture : public MPRISTestFixture {
public:
    PerformanceTestFixture() : MPRISTestFixture("PerformanceTest") {}

    void runTest() override {
        testHighFrequencyUpdates();
        testLockContention();
        testMemoryUsage();
        testMessageThroughput();
        testScalability();
    }

protected:
    void setUp() override;
    
    // Performance test scenarios
    void testHighFrequencyUpdates();
    void testLockContention();
    void testMemoryUsage();
    void testMessageThroughput();
    void testScalability();
};

/**
 * @brief Test fixture for MPRIS integration scenarios
 */
class IntegrationTestFixture : public MPRISTestFixture {
public:
    IntegrationTestFixture() : MPRISTestFixture("IntegrationTest") {}

    void runTest() override {
        testPlayerIntegration();
        testDBusIntegration();
        testSignalEmission();
        testPropertySynchronization();
        testEndToEndWorkflow();
    }

protected:
    void setUp() override;
    
    // Integration test scenarios
    void testPlayerIntegration();
    void testDBusIntegration();
    void testSignalEmission();
    void testPropertySynchronization();
    void testEndToEndWorkflow();
};

/**
 * @brief Test fixture for MPRIS stress testing
 */
class StressTestFixture : public MPRISTestFixture {
public:
    StressTestFixture() : MPRISTestFixture("StressTest") {}

    void runTest() override {
        testHighConcurrency();
        testLongRunningOperations();
        testResourceExhaustion();
        testConnectionInstability();
        testExtremeCases();
    }

protected:
    void setUp() override;
    
    // Stress test scenarios
    void testHighConcurrency();
    void testLongRunningOperations();
    void testResourceExhaustion();
    void testConnectionInstability();
    void testExtremeCases();
};

/**
 * @brief Factory for creating test fixtures
 */
class MPRISTestFixtureFactory {
public:
    /**
     * @brief Create a basic test fixture
     */
    static std::unique_ptr<BasicMPRISTestFixture> createBasicFixture();
    
    /**
     * @brief Create an error handling test fixture
     */
    static std::unique_ptr<ErrorHandlingTestFixture> createErrorHandlingFixture();
    
    /**
     * @brief Create a threading safety test fixture
     */
    static std::unique_ptr<ThreadSafetyTestFixture> createThreadSafetyFixture();
    
    /**
     * @brief Create a performance test fixture
     */
    static std::unique_ptr<PerformanceTestFixture> createPerformanceFixture();
    
    /**
     * @brief Create an integration test fixture
     */
    static std::unique_ptr<IntegrationTestFixture> createIntegrationFixture();
    
    /**
     * @brief Create a stress test fixture
     */
    static std::unique_ptr<StressTestFixture> createStressFixture();
    
    /**
     * @brief Create all test fixtures
     */
    static std::vector<std::unique_ptr<MPRISTestFixture>> createAllFixtures();
};

/**
 * @brief Test scenario runner for executing predefined test scenarios
 */
class MPRISTestScenarioRunner {
public:
    /**
     * @brief Test scenario configuration
     */
    struct ScenarioConfig {
        std::string name;
        std::function<void(MPRISTestFixture&)> setup_func;
        std::function<bool(MPRISTestFixture&)> test_func;
        std::function<void(MPRISTestFixture&)> cleanup_func;
        size_t iterations = 1;
        std::chrono::milliseconds timeout{5000};
        bool expect_failure = false;
    };
    
    MPRISTestScenarioRunner();
    
    // Scenario registration
    void addScenario(const ScenarioConfig& config);
    void addPredefinedScenarios();
    
    // Scenario execution
    bool runScenario(const std::string& name, MPRISTestFixture& fixture);
    std::vector<TestCaseInfo> runAllScenarios(MPRISTestFixture& fixture);
    
    // Results analysis
    void printScenarioResults(const std::vector<TestCaseInfo>& results);
    size_t getPassedScenarioCount(const std::vector<TestCaseInfo>& results);
    size_t getFailedScenarioCount(const std::vector<TestCaseInfo>& results);

private:
    std::map<std::string, ScenarioConfig> m_scenarios;
    
    // Predefined scenarios
    void addBasicFunctionalityScenarios();
    void addErrorHandlingScenarios();
    void addThreadSafetyScenarios();
    void addPerformanceScenarios();
    void addIntegrationScenarios();
    void addStressTestScenarios();
};

/**
 * @brief Utilities for MPRIS test data generation
 */
class MPRISTestDataGenerator {
public:
    /**
     * @brief Generate test track metadata
     */
    static MockPlayer::TrackInfo generateTestTrack(size_t index = 0);
    
    /**
     * @brief Generate test playlist
     */
    static std::vector<MockPlayer::TrackInfo> generateTestPlaylist(size_t track_count = 10);
    
    /**
     * @brief Generate test D-Bus messages
     */
    static std::vector<std::unique_ptr<MockDBusMessage>> generateTestMessages(size_t count = 10);
    
    /**
     * @brief Generate malformed D-Bus messages for error testing
     */
    static std::vector<std::unique_ptr<MockDBusMessage>> generateMalformedMessages(size_t count = 5);
    
    /**
     * @brief Generate property change scenarios
     */
    static std::vector<std::pair<std::string, std::string>> generatePropertyChanges();
    
    /**
     * @brief Generate stress test data
     */
    static std::vector<std::string> generateStressTestOperations(size_t count = 1000);
};

/**
 * @brief Validation utilities for MPRIS testing
 */
class MPRISTestValidator {
public:
    /**
     * @brief Validate MPRIS manager state
     */
    static bool validateMPRISManager(const MPRISManager& manager, std::string& error_message);
    
    /**
     * @brief Validate Player integration
     */
    static bool validatePlayerIntegration(const MockPlayer& player, const MPRISManager& manager, std::string& error_message);
    
    /**
     * @brief Validate D-Bus integration
     */
    static bool validateDBusIntegration(const MockDBusConnection& connection, std::string& error_message);
    
    /**
     * @brief Validate threading safety compliance
     */
    static bool validateThreadingSafety(const MPRISManager& manager, std::string& error_message);
    
    /**
     * @brief Validate message flow
     */
    static bool validateMessageFlow(const std::vector<MockDBusMessage*>& messages, std::string& error_message);
    
    /**
     * @brief Validate performance metrics
     */
    static bool validatePerformanceMetrics(const std::chrono::milliseconds& execution_time, 
                                         size_t operations_count, 
                                         std::string& error_message);
};

} // namespace TestFramework

#endif // HAVE_DBUS
#endif // MPRIS_TEST_FIXTURES_H
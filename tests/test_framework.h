/*
 * test_framework.h - Common test framework for PsyMP3 test harness
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <exception>
#include <chrono>
#include <functional>

namespace TestFramework {

    // ========================================
    // ASSERTION MACROS
    // ========================================
    
    /**
     * @brief Assert that a condition is true
     * @param condition Boolean condition to test
     * @param message Descriptive message for failure
     */
    #define ASSERT_TRUE(condition, message) \
        do { \
            if (!(condition)) { \
                std::ostringstream oss; \
                oss << "ASSERTION FAILED: " << (message) \
                    << " at " << __FILE__ << ":" << __LINE__ \
                    << " - Expected: true, Got: false"; \
                throw TestFramework::AssertionFailure(oss.str()); \
            } \
        } while(0)
    
    /**
     * @brief Assert that a condition is false
     * @param condition Boolean condition to test
     * @param message Descriptive message for failure
     */
    #define ASSERT_FALSE(condition, message) \
        do { \
            if ((condition)) { \
                std::ostringstream oss; \
                oss << "ASSERTION FAILED: " << (message) \
                    << " at " << __FILE__ << ":" << __LINE__ \
                    << " - Expected: false, Got: true"; \
                throw TestFramework::AssertionFailure(oss.str()); \
            } \
        } while(0)
    
    /**
     * @brief Assert that two values are equal
     * @param expected Expected value
     * @param actual Actual value
     * @param message Descriptive message for failure
     */
    #define ASSERT_EQUALS(expected, actual, message) \
        do { \
            if (!((expected) == (actual))) { \
                std::ostringstream oss; \
                oss << "ASSERTION FAILED: " << (message) \
                    << " at " << __FILE__ << ":" << __LINE__ \
                    << " - Expected: " << (expected) << ", Got: " << (actual); \
                throw TestFramework::AssertionFailure(oss.str()); \
            } \
        } while(0)
    
    /**
     * @brief Assert that two values are not equal
     * @param expected Value that should not match
     * @param actual Actual value
     * @param message Descriptive message for failure
     */
    #define ASSERT_NOT_EQUALS(expected, actual, message) \
        do { \
            if ((expected) == (actual)) { \
                std::ostringstream oss; \
                oss << "ASSERTION FAILED: " << (message) \
                    << " at " << __FILE__ << ":" << __LINE__ \
                    << " - Expected values to be different, but both were: " << (actual); \
                throw TestFramework::AssertionFailure(oss.str()); \
            } \
        } while(0)
    
    /**
     * @brief Assert that a pointer is not null
     * @param ptr Pointer to test
     * @param message Descriptive message for failure
     */
    #define ASSERT_NOT_NULL(ptr, message) \
        do { \
            if ((ptr) == nullptr) { \
                std::ostringstream oss; \
                oss << "ASSERTION FAILED: " << (message) \
                    << " at " << __FILE__ << ":" << __LINE__ \
                    << " - Expected: non-null pointer, Got: null"; \
                throw TestFramework::AssertionFailure(oss.str()); \
            } \
        } while(0)
    
    /**
     * @brief Assert that a pointer is null
     * @param ptr Pointer to test
     * @param message Descriptive message for failure
     */
    #define ASSERT_NULL(ptr, message) \
        do { \
            if ((ptr) != nullptr) { \
                std::ostringstream oss; \
                oss << "ASSERTION FAILED: " << (message) \
                    << " at " << __FILE__ << ":" << __LINE__ \
                    << " - Expected: null pointer, Got: non-null"; \
                throw TestFramework::AssertionFailure(oss.str()); \
            } \
        } while(0)

    // ========================================
    // EXCEPTION CLASSES
    // ========================================
    
    /**
     * @brief Exception thrown when an assertion fails
     */
    class AssertionFailure : public std::exception {
    public:
        explicit AssertionFailure(const std::string& message) : m_message(message) {}
        const char* what() const noexcept override { return m_message.c_str(); }
    private:
        std::string m_message;
    };
    
    /**
     * @brief Exception thrown when a test setup fails
     */
    class TestSetupFailure : public std::exception {
    public:
        explicit TestSetupFailure(const std::string& message) : m_message(message) {}
        const char* what() const noexcept override { return m_message.c_str(); }
    private:
        std::string m_message;
    };

    // ========================================
    // TEST RESULT STRUCTURES
    // ========================================
    
    /**
     * @brief Enumeration of possible test results
     */
    enum class TestResult {
        PASSED,     ///< Test completed successfully
        FAILED,     ///< Test failed with assertion error
        ERROR,      ///< Test failed with unexpected error
        SKIPPED     ///< Test was skipped
    };
    
    /**
     * @brief Detailed information about a test execution
     */
    struct TestInfo {
        std::string name;                           ///< Test function name
        TestResult result;                          ///< Test result status
        std::string failure_message;                ///< Error message if failed
        std::chrono::milliseconds execution_time;   ///< Time taken to execute
        
        TestInfo(const std::string& test_name) 
            : name(test_name), result(TestResult::PASSED), execution_time(0) {}
    };

    // ========================================
    // TEST CASE BASE CLASS
    // ========================================
    
    /**
     * @brief Base class for individual test cases
     * 
     * Provides lifecycle management and standardized test execution.
     * Derived classes should override runTest() to implement test logic.
     */
    class TestCase {
    public:
        /**
         * @brief Constructor
         * @param name Human-readable name for this test case
         */
        explicit TestCase(const std::string& name);
        
        /**
         * @brief Virtual destructor
         */
        virtual ~TestCase() = default;
        
        /**
         * @brief Execute the test case with full lifecycle management
         * @return TestInfo containing execution results
         */
        TestInfo run();
        
        /**
         * @brief Get the test case name
         * @return Test case name
         */
        const std::string& getName() const;
        
        /**
         * @brief Check if the test passed
         * @return true if test completed successfully
         */
        bool hasPassed() const;
        
        /**
         * @brief Get failure messages from test execution
         * @return Vector of failure messages
         */
        const std::vector<std::string>& getFailures() const;
        
    protected:
        /**
         * @brief Override this method to implement test logic
         * 
         * This method should contain the actual test implementation.
         * Use the ASSERT_* macros to validate test conditions.
         */
        virtual void runTest() = 0;
        
        /**
         * @brief Optional setup method called before runTest()
         * 
         * Override to perform test-specific initialization.
         * Throw TestSetupFailure if setup cannot be completed.
         */
        virtual void setUp() {}
        
        /**
         * @brief Optional cleanup method called after runTest()
         * 
         * Override to perform test-specific cleanup.
         * This method is called even if the test fails.
         */
        virtual void tearDown() {}
        
        /**
         * @brief Add a custom failure message
         * @param message Failure message to record
         */
        void addFailure(const std::string& message);
        
    private:
        std::string m_name;                     ///< Test case name
        bool m_passed;                          ///< Whether test passed
        std::vector<std::string> m_failures;   ///< Collected failure messages
    };

    // ========================================
    // TEST SUITE CLASS
    // ========================================
    
    /**
     * @brief Container for multiple test cases with execution management
     * 
     * Manages a collection of test cases and provides batch execution
     * with comprehensive reporting.
     */
    class TestSuite {
    public:
        /**
         * @brief Constructor
         * @param name Name for this test suite
         */
        explicit TestSuite(const std::string& name);
        
        /**
         * @brief Add a test case to the suite
         * @param test Unique pointer to test case (takes ownership)
         */
        void addTest(std::unique_ptr<TestCase> test);
        
        /**
         * @brief Add a test function to the suite
         * @param name Test name
         * @param test_func Function pointer to test function
         */
        void addTest(const std::string& name, std::function<void()> test_func);
        
        /**
         * @brief Execute all tests in the suite
         * @return Vector of TestInfo results for each test
         */
        std::vector<TestInfo> runAll();
        
        /**
         * @brief Execute a specific test by name
         * @param test_name Name of test to execute
         * @return TestInfo result, or empty result if test not found
         */
        TestInfo runTest(const std::string& test_name);
        
        /**
         * @brief Print comprehensive test results to stdout
         * @param results Vector of test results to print
         */
        void printResults(const std::vector<TestInfo>& results);
        
        /**
         * @brief Get count of failed tests from results
         * @param results Vector of test results
         * @return Number of failed tests
         */
        int getFailureCount(const std::vector<TestInfo>& results);
        
        /**
         * @brief Get count of passed tests from results
         * @param results Vector of test results
         * @return Number of passed tests
         */
        int getPassedCount(const std::vector<TestInfo>& results);
        
        /**
         * @brief Get total execution time from results
         * @param results Vector of test results
         * @return Total execution time in milliseconds
         */
        std::chrono::milliseconds getTotalTime(const std::vector<TestInfo>& results);
        
        /**
         * @brief Get the suite name
         * @return Test suite name
         */
        const std::string& getName() const;
        
        /**
         * @brief Get count of tests in suite
         * @return Number of tests
         */
        size_t getTestCount() const;
        
        /**
         * @brief Get list of test names in suite
         * @return Vector of test names
         */
        std::vector<std::string> getTestNames() const;
        
    private:
        std::string m_name;                                 ///< Suite name
        std::vector<std::unique_ptr<TestCase>> m_tests;    ///< Owned test cases
    };

    // ========================================
    // FUNCTION-BASED TEST WRAPPER
    // ========================================
    
    /**
     * @brief Wrapper to adapt function-based tests to TestCase interface
     */
    class FunctionTestCase : public TestCase {
    public:
        /**
         * @brief Constructor
         * @param name Test name
         * @param test_func Function to execute as test
         */
        FunctionTestCase(const std::string& name, std::function<void()> test_func);
        
    protected:
        void runTest() override;
        
    private:
        std::function<void()> m_test_func;
    };

    // ========================================
    // UTILITY FUNCTIONS FOR RECT TESTING
    // ========================================
    
    /**
     * @brief Utility functions specific to Rect testing
     */
    namespace RectTestUtils {
        
        /**
         * @brief Create a test rectangle with known properties
         * @param x X coordinate
         * @param y Y coordinate  
         * @param width Width
         * @param height Height
         * @return Rect instance for testing
         */
        class Rect; // Forward declaration
        // Note: This will be implemented in the .cpp file to avoid circular dependencies
        
        /**
         * @brief Assert that two rectangles are equal
         * @param expected Expected rectangle
         * @param actual Actual rectangle
         * @param message Descriptive message for failure
         */
        void assertRectsEqual(const class Rect& expected, const class Rect& actual, const std::string& message);
        
        /**
         * @brief Assert that a rectangle has expected properties
         * @param rect Rectangle to test
         * @param x Expected X coordinate
         * @param y Expected Y coordinate
         * @param width Expected width
         * @param height Expected height
         * @param message Descriptive message for failure
         */
        void assertRectProperties(const class Rect& rect, int16_t x, int16_t y, uint16_t width, uint16_t height, const std::string& message);
        
        /**
         * @brief Assert that a rectangle is empty
         * @param rect Rectangle to test
         * @param message Descriptive message for failure
         */
        void assertRectEmpty(const class Rect& rect, const std::string& message);
        
        /**
         * @brief Assert that a rectangle is not empty
         * @param rect Rectangle to test
         * @param message Descriptive message for failure
         */
        void assertRectNotEmpty(const class Rect& rect, const std::string& message);
        
        /**
         * @brief Create a standard test rectangle for consistent testing
         * @return Standard test rectangle (10, 20, 100, 50)
         */
        class Rect createStandardTestRect();
        
        /**
         * @brief Create an empty test rectangle
         * @return Empty rectangle (0, 0, 0, 0)
         */
        class Rect createEmptyTestRect();
        
        /**
         * @brief Create a single pixel test rectangle
         * @return Single pixel rectangle (5, 5, 1, 1)
         */
        class Rect createSinglePixelTestRect();
    }

    // ========================================
    // COMMON TEST PATTERNS
    // ========================================
    
    /**
     * @brief Common test patterns and utilities
     */
    namespace TestPatterns {
        
        /**
         * @brief Test a function that should throw a specific exception
         * @param test_func Function to test
         * @param expected_message Expected exception message (optional)
         * @param message Descriptive message for failure
         */
        template<typename ExceptionType>
        void assertThrows(std::function<void()> test_func, const std::string& expected_message = "", const std::string& message = "Expected exception was not thrown") {
            bool exception_thrown = false;
            std::string actual_message;
            
            try {
                test_func();
            } catch (const ExceptionType& e) {
                exception_thrown = true;
                actual_message = e.what();
                
                if (!expected_message.empty() && actual_message.find(expected_message) == std::string::npos) {
                    std::ostringstream oss;
                    oss << "Exception message mismatch - Expected to contain: '" 
                        << expected_message << "', Got: '" << actual_message << "'";
                    throw AssertionFailure(oss.str());
                }
            } catch (const std::exception& e) {
                std::ostringstream oss;
                oss << "Wrong exception type thrown - Expected: " << typeid(ExceptionType).name() 
                    << ", Got: " << typeid(e).name() << " with message: " << e.what();
                throw AssertionFailure(oss.str());
            }
            
            if (!exception_thrown) {
                throw AssertionFailure(message);
            }
        }
        
        /**
         * @brief Test a function that should not throw any exception
         * @param test_func Function to test
         * @param message Descriptive message for failure
         */
        void assertNoThrow(std::function<void()> test_func, const std::string& message = "Unexpected exception was thrown");
        
        /**
         * @brief Test boundary conditions for numeric values
         * @param test_func Function that takes a numeric value
         * @param min_value Minimum value to test
         * @param max_value Maximum value to test
         * @param message Descriptive message for failure
         */
        template<typename T>
        void testBoundaryValues(std::function<void(T)> test_func, T min_value, T max_value, const std::string& message = "Boundary value test failed") {
            try {
                // Test minimum value
                test_func(min_value);
                
                // Test maximum value  
                test_func(max_value);
                
                // Test values just inside boundaries
                if (min_value < max_value - 1) {
                    test_func(min_value + 1);
                    test_func(max_value - 1);
                }
                
                // Test middle value
                if (min_value < max_value) {
                    test_func(min_value + (max_value - min_value) / 2);
                }
            } catch (const std::exception& e) {
                std::ostringstream oss;
                oss << message << " - " << e.what();
                throw AssertionFailure(oss.str());
            }
        }
    }

} // namespace TestFramework

#endif // TEST_FRAMEWORK_H
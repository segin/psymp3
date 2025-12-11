/*
 * test_framework.cpp - Implementation of common test framework for PsyMP3
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "test_framework.h"
#include "../include/core/rect.h"
using PsyMP3::Core::Rect;
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace TestFramework {

    // ========================================
    // TEST CASE IMPLEMENTATION
    // ========================================
    
    TestCase::TestCase(const std::string& name) 
        : m_name(name), m_passed(false) {
    }
    
    TestCaseInfo TestCase::run() {
        TestCaseInfo info(m_name);
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // Clear previous state
            m_passed = false;
            m_failures.clear();
            
            // Execute test lifecycle
            setUp();
            runTest();
            tearDown();
            
            // If we get here, test passed
            m_passed = true;
            info.result = TestResult::PASSED;
            
        } catch (const AssertionFailure& e) {
            m_passed = false;
            info.result = TestResult::FAILED;
            info.failure_message = e.what();
            addFailure(e.what());
            
            // Still call tearDown on assertion failure
            try {
                tearDown();
            } catch (...) {
                // Ignore tearDown exceptions after test failure
            }
            
        } catch (const TestSetupFailure& e) {
            m_passed = false;
            info.result = TestResult::ERROR;
            info.failure_message = std::string("Setup failed: ") + e.what();
            addFailure(info.failure_message);
            
        } catch (const std::exception& e) {
            m_passed = false;
            info.result = TestResult::ERROR;
            info.failure_message = std::string("Unexpected error: ") + e.what();
            addFailure(info.failure_message);
            
            // Still call tearDown on unexpected error
            try {
                tearDown();
            } catch (...) {
                // Ignore tearDown exceptions after test error
            }
            
        } catch (...) {
            m_passed = false;
            info.result = TestResult::ERROR;
            info.failure_message = "Unknown exception occurred";
            addFailure(info.failure_message);
            
            // Still call tearDown on unknown error
            try {
                tearDown();
            } catch (...) {
                // Ignore tearDown exceptions after test error
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        info.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        return info;
    }
    
    const std::string& TestCase::getName() const {
        return m_name;
    }
    
    bool TestCase::hasPassed() const {
        return m_passed;
    }
    
    const std::vector<std::string>& TestCase::getFailures() const {
        return m_failures;
    }
    
    void TestCase::addFailure(const std::string& message) {
        m_failures.push_back(message);
    }

    // ========================================
    // TEST SUITE IMPLEMENTATION
    // ========================================
    
    TestSuite::TestSuite(const std::string& name) : m_name(name) {
    }
    
    void TestSuite::addTest(std::unique_ptr<TestCase> test) {
        if (test) {
            m_tests.push_back(std::move(test));
        }
    }
    
    void TestSuite::addTest(const std::string& name, std::function<void()> test_func) {
        auto test_case = std::make_unique<FunctionTestCase>(name, test_func);
        addTest(std::move(test_case));
    }
    
    std::vector<TestCaseInfo> TestSuite::runAll() {
        std::vector<TestCaseInfo> results;
        results.reserve(m_tests.size());
        
        std::cout << "Running test suite: " << m_name << std::endl;
        std::cout << "===================" << std::string(m_name.length(), '=') << std::endl;
        
        for (auto& test : m_tests) {
            std::cout << "Running " << test->getName() << "... ";
            std::cout.flush();
            
            TestCaseInfo result = test->run();
            results.push_back(result);
            
            // Print immediate result
            switch (result.result) {
                case TestResult::PASSED:
                    std::cout << "PASSED (" << result.execution_time.count() << "ms)" << std::endl;
                    break;
                case TestResult::FAILED:
                    std::cout << "FAILED (" << result.execution_time.count() << "ms)" << std::endl;
                    std::cout << "  Error: " << result.failure_message << std::endl;
                    break;
                case TestResult::ERROR:
                    std::cout << "ERROR (" << result.execution_time.count() << "ms)" << std::endl;
                    std::cout << "  Error: " << result.failure_message << std::endl;
                    break;
                case TestResult::SKIPPED:
                    std::cout << "SKIPPED" << std::endl;
                    break;
            }
        }
        
        return results;
    }
    
    TestCaseInfo TestSuite::runTest(const std::string& test_name) {
        for (auto& test : m_tests) {
            if (test->getName() == test_name) {
                return test->run();
            }
        }
        
        // Test not found
        TestCaseInfo not_found(test_name);
        not_found.result = TestResult::ERROR;
        not_found.failure_message = "Test not found: " + test_name;
        return not_found;
    }
    
    void TestSuite::printResults(const std::vector<TestCaseInfo>& results) {
        std::cout << std::endl;
        std::cout << "Test Results Summary" << std::endl;
        std::cout << "====================" << std::endl;
        
        int passed = getPassedCount(results);
        int failed = getFailureCount(results);
        int errors = 0;
        int skipped = 0;
        
        for (const auto& result : results) {
            if (result.result == TestResult::ERROR) errors++;
            else if (result.result == TestResult::SKIPPED) skipped++;
        }
        
        std::cout << "Total tests: " << results.size() << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << failed << std::endl;
        std::cout << "Errors: " << errors << std::endl;
        std::cout << "Skipped: " << skipped << std::endl;
        std::cout << "Total time: " << getTotalTime(results).count() << "ms" << std::endl;
        
        // Print detailed failure information
        if (failed > 0 || errors > 0) {
            std::cout << std::endl << "Failure Details:" << std::endl;
            std::cout << "================" << std::endl;
            
            for (const auto& result : results) {
                if (result.result == TestResult::FAILED || result.result == TestResult::ERROR) {
                    std::cout << std::endl << "FAILED: " << result.name << std::endl;
                    std::cout << "  " << result.failure_message << std::endl;
                }
            }
        }
        
        std::cout << std::endl;
        if (failed == 0 && errors == 0) {
            std::cout << "All tests passed!" << std::endl;
        } else {
            std::cout << "Some tests failed. See details above." << std::endl;
        }
    }
    
    int TestSuite::getFailureCount(const std::vector<TestCaseInfo>& results) {
        return std::count_if(results.begin(), results.end(), 
            [](const TestCaseInfo& info) { return info.result == TestResult::FAILED; });
    }
    
    int TestSuite::getPassedCount(const std::vector<TestCaseInfo>& results) {
        return std::count_if(results.begin(), results.end(), 
            [](const TestCaseInfo& info) { return info.result == TestResult::PASSED; });
    }
    
    std::chrono::milliseconds TestSuite::getTotalTime(const std::vector<TestCaseInfo>& results) {
        std::chrono::milliseconds total(0);
        for (const auto& result : results) {
            total += result.execution_time;
        }
        return total;
    }
    
    const std::string& TestSuite::getName() const {
        return m_name;
    }
    
    size_t TestSuite::getTestCount() const {
        return m_tests.size();
    }
    
    std::vector<std::string> TestSuite::getTestNames() const {
        std::vector<std::string> names;
        names.reserve(m_tests.size());
        
        for (const auto& test : m_tests) {
            names.push_back(test->getName());
        }
        
        return names;
    }

    // ========================================
    // FUNCTION TEST CASE IMPLEMENTATION
    // ========================================
    
    FunctionTestCase::FunctionTestCase(const std::string& name, std::function<void()> test_func)
        : TestCase(name), m_test_func(test_func) {
    }
    
    void FunctionTestCase::runTest() {
        if (m_test_func) {
            m_test_func();
        } else {
            throw TestSetupFailure("Test function is null");
        }
    }

    // ========================================
    // RECT TEST UTILITIES IMPLEMENTATION
    // ========================================
    
    namespace RectTestUtils {
        
        void assertRectsEqual(const Rect& expected, const Rect& actual, const std::string& message) {
            if (!(expected == actual)) {
                std::ostringstream oss;
                oss << "Rectangle mismatch: " << message 
                    << " - Expected: " << expected.toString() 
                    << ", Got: " << actual.toString();
                throw AssertionFailure(oss.str());
            }
        }
        
        void assertRectProperties(const Rect& rect, int16_t x, int16_t y, uint16_t width, uint16_t height, const std::string& message) {
            bool matches = (rect.x() == x && rect.y() == y && rect.width() == width && rect.height() == height);
            
            if (!matches) {
                std::ostringstream oss;
                oss << "Rectangle properties mismatch: " << message 
                    << " - Expected: (" << x << ", " << y << ", " << width << ", " << height << ")"
                    << ", Got: (" << rect.x() << ", " << rect.y() << ", " << rect.width() << ", " << rect.height() << ")";
                throw AssertionFailure(oss.str());
            }
        }
        
        void assertRectEmpty(const Rect& rect, const std::string& message) {
            if (!rect.isEmpty()) {
                std::ostringstream oss;
                oss << "Rectangle should be empty: " << message 
                    << " - Got: " << rect.toString();
                throw AssertionFailure(oss.str());
            }
        }
        
        void assertRectNotEmpty(const Rect& rect, const std::string& message) {
            if (rect.isEmpty()) {
                std::ostringstream oss;
                oss << "Rectangle should not be empty: " << message 
                    << " - Got: " << rect.toString();
                throw AssertionFailure(oss.str());
            }
        }
        
        Rect createStandardTestRect() {
            return Rect(10, 20, 100, 50);
        }
        
        Rect createEmptyTestRect() {
            return Rect(0, 0, 0, 0);
        }
        
        Rect createSinglePixelTestRect() {
            return Rect(5, 5, 1, 1);
        }
    }

    // ========================================
    // TEST PATTERNS IMPLEMENTATION
    // ========================================
    
    namespace TestPatterns {
        
        void assertNoThrow(std::function<void()> test_func, const std::string& message) {
            try {
                test_func();
            } catch (const std::exception& e) {
                std::ostringstream oss;
                oss << message << " - Unexpected exception: " << e.what();
                throw AssertionFailure(oss.str());
            } catch (...) {
                std::ostringstream oss;
                oss << message << " - Unexpected unknown exception";
                throw AssertionFailure(oss.str());
            }
        }
    }

} // namespace TestFramework
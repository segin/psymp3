/*
 * test_reporter.cpp - Test reporting system implementation for PsyMP3 test harness
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "test_reporter.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <unistd.h>
#include <fstream>

namespace TestFramework {

    // ========================================
    // TEST SUMMARY IMPLEMENTATION
    // ========================================
    
    void TestSummary::calculateFromResults(const std::vector<ExecutionResult>& test_results) {
        results = test_results;
        total_tests = test_results.size();
        
        // Reset counters
        passed_tests = failed_tests = timeout_tests = crashed_tests = 0;
        build_error_tests = system_error_tests = skipped_tests = 0;
        total_duration = std::chrono::milliseconds(0);
        fastest_test = std::chrono::milliseconds::max();
        slowest_test = std::chrono::milliseconds(0);
        
        if (test_results.empty()) {
            return;
        }
        
        // Calculate statistics
        for (const auto& result : test_results) {
            total_duration += result.execution_time;
            
            // Track fastest and slowest tests
            if (result.execution_time < fastest_test) {
                fastest_test = result.execution_time;
                fastest_test_name = result.test_name;
            }
            if (result.execution_time > slowest_test) {
                slowest_test = result.execution_time;
                slowest_test_name = result.test_name;
            }
            
            // Count by status
            switch (result.status) {
                case ExecutionStatus::SUCCESS:
                    passed_tests++;
                    break;
                case ExecutionStatus::FAILURE:
                    failed_tests++;
                    break;
                case ExecutionStatus::TIMEOUT:
                    timeout_tests++;
                    break;
                case ExecutionStatus::CRASH:
                    crashed_tests++;
                    break;
                case ExecutionStatus::BUILD_ERROR:
                    build_error_tests++;
                    break;
                case ExecutionStatus::SYSTEM_ERROR:
                    system_error_tests++;
                    break;
            }
        }
        
        // Calculate average duration
        if (total_tests > 0) {
            average_duration = std::chrono::milliseconds(total_duration.count() / total_tests);
        }
    }
    
    bool TestSummary::allTestsPassed() const {
        return failed_tests == 0 && timeout_tests == 0 && crashed_tests == 0 && 
               build_error_tests == 0 && system_error_tests == 0;
    }
    
    double TestSummary::getSuccessRate() const {
        if (total_tests == 0) return 100.0;
        return (static_cast<double>(passed_tests) / total_tests) * 100.0;
    }

    // ========================================
    // CONSOLE REPORTER IMPLEMENTATION
    // ========================================
    
    ConsoleReporter::ConsoleReporter(std::ostream* output, bool enable_color) {
        m_output = output ? output : &std::cout;
        m_color_output = enable_color && shouldUseColor();
    }
    
    void ConsoleReporter::reportStart(const std::vector<TestInfo>& tests) {
        m_total_tests = tests.size();
        m_completed_tests = 0;
        m_start_time = std::chrono::steady_clock::now();
        
        if (!m_quiet) {
            *m_output << "PsyMP3 Test Harness\n";
            printSeparator('=', 60);
            *m_output << "Running " << m_total_tests << " tests...\n\n";
        }
    }
    
    void ConsoleReporter::reportTestStart(const std::string& test_name) {
        if (m_verbose) {
            *m_output << colorize("Starting: ", "\033[36m") << test_name << std::endl;
        } else if (!m_quiet) {
            *m_output << "Running " << test_name << "... " << std::flush;
        }
    }
    
    void ConsoleReporter::reportTestResult(const ExecutionResult& result) {
        m_completed_tests++;
        
        if (m_verbose) {
            *m_output << colorize("Completed: ", "\033[36m") << result.test_name 
                      << " (" << formatDuration(result.execution_time) << ") - "
                      << colorize(getStatusText(result.status), getStatusColor(result.status))
                      << std::endl;
            
            // Show detailed information for failures
            if (result.status != ExecutionStatus::SUCCESS) {
                if (!result.error_message.empty()) {
                    *m_output << "  " << colorize("Error: ", "\033[31m") << result.error_message << std::endl;
                }
                if (result.exit_code != 0) {
                    *m_output << "  " << colorize("Exit code: ", "\033[31m") << result.exit_code << std::endl;
                }
                if (result.signal_number != 0) {
                    *m_output << "  " << colorize("Signal: ", "\033[31m") << result.signal_number << std::endl;
                }
            }
            
            // Show test output if requested
            if (m_show_test_output) {
                if (!result.stdout_output.empty()) {
                    *m_output << "  " << colorize("stdout:", "\033[33m") << std::endl;
                    std::istringstream iss(result.stdout_output);
                    std::string line;
                    while (std::getline(iss, line)) {
                        *m_output << "    " << line << std::endl;
                    }
                }
                if (!result.stderr_output.empty()) {
                    *m_output << "  " << colorize("stderr:", "\033[33m") << std::endl;
                    std::istringstream iss(result.stderr_output);
                    std::string line;
                    while (std::getline(iss, line)) {
                        *m_output << "    " << line << std::endl;
                    }
                }
            }
            *m_output << std::endl;
        } else if (!m_quiet) {
            *m_output << colorize(getStatusText(result.status), getStatusColor(result.status))
                      << " (" << formatDuration(result.execution_time) << ")" << std::endl;
        }
    }
    
    void ConsoleReporter::reportProgress(int completed, int total) {
        if (!m_quiet && !m_verbose && total > 0) {
            int percentage = (completed * 100) / total;
            *m_output << "\rProgress: " << completed << "/" << total 
                      << " (" << percentage << "%) " << std::flush;
        }
    }
    
    void ConsoleReporter::reportSummary(const TestSummary& summary) {
        if (!m_quiet) {
            *m_output << "\n";
            printSeparator('=', 60);
            *m_output << "TEST SUMMARY\n";
            printSeparator('=', 60);
        }
        
        // Basic statistics
        *m_output << "Tests run: " << summary.total_tests << "\n";
        *m_output << colorize("Passed: " + std::to_string(summary.passed_tests), "\033[32m") << "\n";
        
        if (summary.failed_tests > 0) {
            *m_output << colorize("Failed: " + std::to_string(summary.failed_tests), "\033[31m") << "\n";
        }
        if (summary.timeout_tests > 0) {
            *m_output << colorize("Timeout: " + std::to_string(summary.timeout_tests), "\033[33m") << "\n";
        }
        if (summary.crashed_tests > 0) {
            *m_output << colorize("Crashed: " + std::to_string(summary.crashed_tests), "\033[31m") << "\n";
        }
        if (summary.build_error_tests > 0) {
            *m_output << colorize("Build errors: " + std::to_string(summary.build_error_tests), "\033[31m") << "\n";
        }
        if (summary.system_error_tests > 0) {
            *m_output << colorize("System errors: " + std::to_string(summary.system_error_tests), "\033[31m") << "\n";
        }
        
        // Timing information
        *m_output << "Total time: " << formatDuration(summary.total_duration) << "\n";
        if (summary.total_tests > 0) {
            *m_output << "Average time: " << formatDuration(summary.average_duration) << "\n";
            *m_output << "Success rate: " << std::fixed << std::setprecision(1) 
                      << summary.getSuccessRate() << "%\n";
        }
        
        // Performance information
        if (summary.total_tests > 1) {
            *m_output << "Fastest test: " << summary.fastest_test_name 
                      << " (" << formatDuration(summary.fastest_test) << ")\n";
            *m_output << "Slowest test: " << summary.slowest_test_name 
                      << " (" << formatDuration(summary.slowest_test) << ")\n";
        }
        
        // Show failed tests
        if (!summary.allTestsPassed()) {
            *m_output << "\n" << colorize("FAILED TESTS:", "\033[31m") << "\n";
            for (const auto& result : summary.results) {
                if (result.status != ExecutionStatus::SUCCESS) {
                    *m_output << "  " << result.test_name << " - "
                              << colorize(getStatusText(result.status), getStatusColor(result.status));
                    
                    if (result.status == ExecutionStatus::FAILURE && result.exit_code != 0) {
                        *m_output << " (exit code " << result.exit_code << ")";
                    } else if (result.status == ExecutionStatus::CRASH && result.signal_number != 0) {
                        *m_output << " (signal " << result.signal_number << ")";
                    }
                    *m_output << "\n";
                    
                    if (!result.error_message.empty()) {
                        *m_output << "    Error: " << result.error_message << "\n";
                    }
                }
            }
        }
        
        *m_output << std::endl;
    }
    
    std::string ConsoleReporter::colorize(const std::string& text, const std::string& color_code) const {
        if (!m_color_output) {
            return text;
        }
        return color_code + text + "\033[0m";
    }
    
    std::string ConsoleReporter::getStatusColor(ExecutionStatus status) const {
        switch (status) {
            case ExecutionStatus::SUCCESS: return "\033[32m";  // Green
            case ExecutionStatus::FAILURE: return "\033[31m";  // Red
            case ExecutionStatus::TIMEOUT: return "\033[33m";  // Yellow
            case ExecutionStatus::CRASH: return "\033[31m";    // Red
            case ExecutionStatus::BUILD_ERROR: return "\033[35m"; // Magenta
            case ExecutionStatus::SYSTEM_ERROR: return "\033[31m"; // Red
            default: return "\033[0m";  // Reset
        }
    }
    
    std::string ConsoleReporter::getStatusText(ExecutionStatus status) const {
        switch (status) {
            case ExecutionStatus::SUCCESS: return "PASSED";
            case ExecutionStatus::FAILURE: return "FAILED";
            case ExecutionStatus::TIMEOUT: return "TIMEOUT";
            case ExecutionStatus::CRASH: return "CRASHED";
            case ExecutionStatus::BUILD_ERROR: return "BUILD ERROR";
            case ExecutionStatus::SYSTEM_ERROR: return "SYSTEM ERROR";
            default: return "UNKNOWN";
        }
    }
    
    std::string ConsoleReporter::formatDuration(std::chrono::milliseconds duration) const {
        auto ms = duration.count();
        if (ms < 1000) {
            return std::to_string(ms) + "ms";
        } else if (ms < 60000) {
            return std::to_string(ms / 1000.0) + "s";
        } else {
            int minutes = ms / 60000;
            int seconds = (ms % 60000) / 1000;
            return std::to_string(minutes) + "m" + std::to_string(seconds) + "s";
        }
    }
    
    void ConsoleReporter::printSeparator(char character, int length) const {
        *m_output << std::string(length, character) << "\n";
    }
    
    bool ConsoleReporter::shouldUseColor() const {
        // Check if output is a terminal and supports color
        return isatty(fileno(stdout)) && getenv("TERM") != nullptr;
    }

    // ========================================
    // XML REPORTER IMPLEMENTATION
    // ========================================
    
    XMLReporter::XMLReporter(std::ostream* output) {
        m_output = output ? output : &std::cout;
    }
    
    void XMLReporter::reportStart(const std::vector<TestInfo>& tests) {
        // XML reporters typically collect all results and output at the end
        m_results.clear();
        m_results.reserve(tests.size());
    }
    
    void XMLReporter::reportTestResult(const ExecutionResult& result) {
        m_results.push_back(result);
    }
    
    void XMLReporter::reportSummary(const TestSummary& summary) {
        // Generate JUnit-compatible XML
        *m_output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        *m_output << "<testsuite name=\"" << escapeXml(m_suite_name) << "\"\n";
        *m_output << "           tests=\"" << summary.total_tests << "\"\n";
        *m_output << "           failures=\"" << summary.failed_tests << "\"\n";
        *m_output << "           errors=\"" << (summary.crashed_tests + summary.system_error_tests) << "\"\n";
        *m_output << "           skipped=\"" << (summary.timeout_tests + summary.build_error_tests) << "\"\n";
        *m_output << "           time=\"" << (summary.total_duration.count() / 1000.0) << "\"\n";
        *m_output << "           timestamp=\"" << formatTimestamp(summary.start_time) << "\">\n";
        
        // Output individual test cases
        for (const auto& result : m_results) {
            *m_output << "  <testcase name=\"" << escapeXml(result.test_name) << "\"\n";
            *m_output << "            classname=\"" << escapeXml(m_suite_name) << "\"\n";
            *m_output << "            time=\"" << (result.execution_time.count() / 1000.0) << "\"";
            
            if (result.status == ExecutionStatus::SUCCESS) {
                *m_output << "/>\n";
            } else {
                *m_output << ">\n";
                
                // Add failure/error information
                switch (result.status) {
                    case ExecutionStatus::FAILURE:
                        *m_output << "    <failure message=\"" << escapeXml(result.error_message) << "\">\n";
                        *m_output << escapeXml(result.stderr_output);
                        *m_output << "    </failure>\n";
                        break;
                        
                    case ExecutionStatus::CRASH:
                    case ExecutionStatus::SYSTEM_ERROR:
                        *m_output << "    <error message=\"" << escapeXml(result.error_message) << "\">\n";
                        *m_output << escapeXml(result.stderr_output);
                        *m_output << "    </error>\n";
                        break;
                        
                    case ExecutionStatus::TIMEOUT:
                    case ExecutionStatus::BUILD_ERROR:
                        *m_output << "    <skipped message=\"" << escapeXml(result.error_message) << "\"/>\n";
                        break;
                        
                    default:
                        break;
                }
                
                // Add system output if requested
                if (m_include_system_output) {
                    if (!result.stdout_output.empty()) {
                        *m_output << "    <system-out>\n";
                        *m_output << escapeXml(result.stdout_output);
                        *m_output << "    </system-out>\n";
                    }
                    if (!result.stderr_output.empty()) {
                        *m_output << "    <system-err>\n";
                        *m_output << escapeXml(result.stderr_output);
                        *m_output << "    </system-err>\n";
                    }
                }
                
                *m_output << "  </testcase>\n";
            }
        }
        
        *m_output << "</testsuite>\n";
    }
    
    std::string XMLReporter::escapeXml(const std::string& text) const {
        std::string result;
        result.reserve(text.length() * 1.1); // Reserve some extra space
        
        for (char c : text) {
            switch (c) {
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '&': result += "&amp;"; break;
                case '"': result += "&quot;"; break;
                case '\'': result += "&apos;"; break;
                default: result += c; break;
            }
        }
        
        return result;
    }
    
    std::string XMLReporter::formatTimestamp(const std::chrono::system_clock::time_point& time_point) const {
        auto time_t = std::chrono::system_clock::to_time_t(time_point);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    // ========================================
    // JSON REPORTER IMPLEMENTATION
    // ========================================
    
    JSONReporter::JSONReporter(std::ostream* output) {
        m_output = output ? output : &std::cout;
    }
    
    void JSONReporter::reportStart(const std::vector<TestInfo>& tests) {
        m_results.clear();
        m_results.reserve(tests.size());
        m_test_info = tests;
    }
    
    void JSONReporter::reportTestResult(const ExecutionResult& result) {
        m_results.push_back(result);
    }
    
    void JSONReporter::reportSummary(const TestSummary& summary) {
        *m_output << "{\n";
        *m_output << "  \"summary\": " << summaryToJson(summary) << ",\n";
        *m_output << "  \"results\": [\n";
        
        for (size_t i = 0; i < m_results.size(); ++i) {
            if (i > 0) *m_output << ",\n";
            *m_output << "    " << resultToJson(m_results[i]);
        }
        
        *m_output << "\n  ]";
        
        if (m_include_metadata && !m_test_info.empty()) {
            *m_output << ",\n  \"tests\": [\n";
            for (size_t i = 0; i < m_test_info.size(); ++i) {
                if (i > 0) *m_output << ",\n";
                *m_output << "    " << testInfoToJson(m_test_info[i]);
            }
            *m_output << "\n  ]";
        }
        
        *m_output << "\n}\n";
    }
    
    std::string JSONReporter::escapeJson(const std::string& text) const {
        std::string result;
        result.reserve(text.length() * 1.1);
        
        for (char c : text) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        
        return result;
    }
    
    std::string JSONReporter::statusToJson(ExecutionStatus status) const {
        switch (status) {
            case ExecutionStatus::SUCCESS: return "\"success\"";
            case ExecutionStatus::FAILURE: return "\"failure\"";
            case ExecutionStatus::TIMEOUT: return "\"timeout\"";
            case ExecutionStatus::CRASH: return "\"crash\"";
            case ExecutionStatus::BUILD_ERROR: return "\"build_error\"";
            case ExecutionStatus::SYSTEM_ERROR: return "\"system_error\"";
            default: return "\"unknown\"";
        }
    }
    
    std::string JSONReporter::formatTimestamp(const std::chrono::system_clock::time_point& time_point) const {
        auto time_t = std::chrono::system_clock::to_time_t(time_point);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }
    
    std::string JSONReporter::testInfoToJson(const TestInfo& test) const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "      \"name\": \"" << escapeJson(test.name) << "\",\n";
        oss << "      \"executable_path\": \"" << escapeJson(test.executable_path) << "\",\n";
        oss << "      \"source_path\": \"" << escapeJson(test.source_path) << "\",\n";
        oss << "      \"is_built\": " << (test.is_built ? "true" : "false") << ",\n";
        oss << "      \"metadata\": {\n";
        oss << "        \"description\": \"" << escapeJson(test.metadata.description) << "\",\n";
        oss << "        \"timeout\": " << test.metadata.timeout.count() << ",\n";
        oss << "        \"parallel_safe\": " << (test.metadata.parallel_safe ? "true" : "false") << ",\n";
        oss << "        \"author\": \"" << escapeJson(test.metadata.author) << "\",\n";
        oss << "        \"tags\": [";
        for (size_t i = 0; i < test.metadata.tags.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << escapeJson(test.metadata.tags[i]) << "\"";
        }
        oss << "]\n";
        oss << "      }\n";
        oss << "    }";
        return oss.str();
    }
    
    std::string JSONReporter::resultToJson(const ExecutionResult& result) const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "      \"test_name\": \"" << escapeJson(result.test_name) << "\",\n";
        oss << "      \"status\": " << statusToJson(result.status) << ",\n";
        oss << "      \"exit_code\": " << result.exit_code << ",\n";
        oss << "      \"signal_number\": " << result.signal_number << ",\n";
        oss << "      \"execution_time\": " << result.execution_time.count() << ",\n";
        oss << "      \"timed_out\": " << (result.timed_out ? "true" : "false") << ",\n";
        oss << "      \"error_message\": \"" << escapeJson(result.error_message) << "\",\n";
        oss << "      \"stdout_output\": \"" << escapeJson(result.stdout_output) << "\",\n";
        oss << "      \"stderr_output\": \"" << escapeJson(result.stderr_output) << "\"\n";
        oss << "    }";
        return oss.str();
    }
    
    std::string JSONReporter::summaryToJson(const TestSummary& summary) const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "    \"total_tests\": " << summary.total_tests << ",\n";
        oss << "    \"passed_tests\": " << summary.passed_tests << ",\n";
        oss << "    \"failed_tests\": " << summary.failed_tests << ",\n";
        oss << "    \"timeout_tests\": " << summary.timeout_tests << ",\n";
        oss << "    \"crashed_tests\": " << summary.crashed_tests << ",\n";
        oss << "    \"build_error_tests\": " << summary.build_error_tests << ",\n";
        oss << "    \"system_error_tests\": " << summary.system_error_tests << ",\n";
        oss << "    \"total_duration\": " << summary.total_duration.count() << ",\n";
        oss << "    \"average_duration\": " << summary.average_duration.count() << ",\n";
        oss << "    \"success_rate\": " << summary.getSuccessRate() << ",\n";
        oss << "    \"fastest_test\": \"" << escapeJson(summary.fastest_test_name) << "\",\n";
        oss << "    \"fastest_time\": " << summary.fastest_test.count() << ",\n";
        oss << "    \"slowest_test\": \"" << escapeJson(summary.slowest_test_name) << "\",\n";
        oss << "    \"slowest_time\": " << summary.slowest_test.count() << ",\n";
        oss << "    \"start_time\": \"" << formatTimestamp(summary.start_time) << "\",\n";
        oss << "    \"end_time\": \"" << formatTimestamp(summary.end_time) << "\"\n";
        oss << "  }";
        return oss.str();
    }

    // ========================================
    // PERFORMANCE METRICS IMPLEMENTATION
    // ========================================
    
    void PerformanceMetrics::addTestResult(const ExecutionResult& result) {
        TestPerformance perf(result.test_name, result.execution_time);
        perf.memory_usage = result.resource_usage.peak_memory_kb;
        perf.cpu_usage = result.resource_usage.cpu_time_seconds;
        perf.context_switches = result.resource_usage.context_switches;
        m_performance_data.push_back(perf);
    }
    
    void PerformanceMetrics::addTestPerformance(const std::string& test_name, 
                                               std::chrono::milliseconds duration,
                                               size_t memory_usage,
                                               double cpu_usage,
                                               int context_switches) {
        TestPerformance perf(test_name, duration);
        perf.memory_usage = memory_usage;
        perf.cpu_usage = cpu_usage;
        perf.context_switches = context_switches;
        m_performance_data.push_back(perf);
    }
    
    std::vector<PerformanceMetrics::TestPerformance> PerformanceMetrics::getSlowestTests(int count) const {
        std::vector<TestPerformance> sorted = m_performance_data;
        std::sort(sorted.begin(), sorted.end(), 
                  [](const TestPerformance& a, const TestPerformance& b) {
                      return a.duration > b.duration;
                  });
        
        if (sorted.size() > static_cast<size_t>(count)) {
            sorted.resize(count);
        }
        
        return sorted;
    }
    
    std::vector<PerformanceMetrics::TestPerformance> PerformanceMetrics::getFastestTests(int count) const {
        std::vector<TestPerformance> sorted = m_performance_data;
        std::sort(sorted.begin(), sorted.end(), 
                  [](const TestPerformance& a, const TestPerformance& b) {
                      return a.duration < b.duration;
                  });
        
        if (sorted.size() > static_cast<size_t>(count)) {
            sorted.resize(count);
        }
        
        return sorted;
    }
    
    std::chrono::milliseconds PerformanceMetrics::getAverageExecutionTime() const {
        if (m_performance_data.empty()) {
            return std::chrono::milliseconds(0);
        }
        
        auto total = getTotalExecutionTime();
        return std::chrono::milliseconds(total.count() / m_performance_data.size());
    }
    
    std::chrono::milliseconds PerformanceMetrics::getTotalExecutionTime() const {
        std::chrono::milliseconds total(0);
        for (const auto& perf : m_performance_data) {
            total += perf.duration;
        }
        return total;
    }
    
    void PerformanceMetrics::generateReport(std::ostream& output) const {
        if (m_performance_data.empty()) {
            output << "No performance data available.\n";
            return;
        }
        
        output << "PERFORMANCE REPORT\n";
        output << std::string(60, '=') << "\n";
        
        // Basic statistics
        output << "Total tests: " << m_performance_data.size() << "\n";
        output << "Total execution time: " << getTotalExecutionTime().count() << "ms\n";
        output << "Average execution time: " << getAverageExecutionTime().count() << "ms\n";
        
        // Calculate memory and CPU statistics
        size_t total_memory = 0;
        double total_cpu = 0.0;
        int total_context_switches = 0;
        size_t tests_with_memory = 0;
        size_t tests_with_cpu = 0;
        size_t tests_with_context = 0;
        
        for (const auto& perf : m_performance_data) {
            if (perf.memory_usage > 0) {
                total_memory += perf.memory_usage;
                tests_with_memory++;
            }
            if (perf.cpu_usage > 0.0) {
                total_cpu += perf.cpu_usage;
                tests_with_cpu++;
            }
            if (perf.context_switches > 0) {
                total_context_switches += perf.context_switches;
                tests_with_context++;
            }
        }
        
        if (tests_with_memory > 0) {
            output << "Average memory usage: " << (total_memory / tests_with_memory) << " KB\n";
        }
        if (tests_with_cpu > 0) {
            output << "Average CPU time: " << std::fixed << std::setprecision(3) 
                   << (total_cpu / tests_with_cpu) << "s\n";
        }
        if (tests_with_context > 0) {
            output << "Average context switches: " << (total_context_switches / tests_with_context) << "\n";
        }
        
        output << "\n";
        
        // Slowest tests with detailed metrics
        auto slowest = getSlowestTests(10);
        if (!slowest.empty()) {
            output << "SLOWEST TESTS:\n";
            output << std::left << std::setw(25) << "Test Name" 
                   << std::setw(10) << "Time (ms)"
                   << std::setw(12) << "Memory (KB)"
                   << std::setw(12) << "CPU (s)"
                   << std::setw(10) << "Context SW" << "\n";
            output << std::string(69, '-') << "\n";
            
            for (const auto& test : slowest) {
                output << std::left << std::setw(25) << test.test_name
                       << std::setw(10) << test.duration.count();
                
                if (test.memory_usage > 0) {
                    output << std::setw(12) << test.memory_usage;
                } else {
                    output << std::setw(12) << "N/A";
                }
                
                if (test.cpu_usage > 0.0) {
                    output << std::setw(12) << std::fixed << std::setprecision(3) << test.cpu_usage;
                } else {
                    output << std::setw(12) << "N/A";
                }
                
                if (test.context_switches > 0) {
                    output << std::setw(10) << test.context_switches;
                } else {
                    output << std::setw(10) << "N/A";
                }
                
                output << "\n";
            }
            output << "\n";
        }
        
        // Fastest tests
        auto fastest = getFastestTests(5);
        if (!fastest.empty()) {
            output << "FASTEST TESTS:\n";
            for (const auto& test : fastest) {
                output << "  " << std::left << std::setw(25) << test.test_name 
                       << ": " << test.duration.count() << "ms";
                if (test.memory_usage > 0) {
                    output << " (" << test.memory_usage << " KB)";
                }
                output << "\n";
            }
            output << "\n";
        }
        
        // Performance distribution analysis
        if (m_performance_data.size() > 1) {
            output << "PERFORMANCE DISTRIBUTION:\n";
            
            // Calculate percentiles
            std::vector<std::chrono::milliseconds> durations;
            for (const auto& perf : m_performance_data) {
                durations.push_back(perf.duration);
            }
            std::sort(durations.begin(), durations.end());
            
            size_t p50_idx = durations.size() / 2;
            size_t p90_idx = (durations.size() * 9) / 10;
            size_t p95_idx = (durations.size() * 95) / 100;
            
            output << "  50th percentile (median): " << durations[p50_idx].count() << "ms\n";
            output << "  90th percentile: " << durations[p90_idx].count() << "ms\n";
            output << "  95th percentile: " << durations[p95_idx].count() << "ms\n";
            
            // Identify outliers (tests taking more than 2x the median)
            auto median_time = durations[p50_idx];
            auto outlier_threshold = median_time * 2;
            
            std::vector<TestPerformance> outliers;
            for (const auto& perf : m_performance_data) {
                if (perf.duration > outlier_threshold) {
                    outliers.push_back(perf);
                }
            }
            
            if (!outliers.empty()) {
                output << "\nPERFORMANCE OUTLIERS (>2x median):\n";
                for (const auto& outlier : outliers) {
                    double ratio = static_cast<double>(outlier.duration.count()) / median_time.count();
                    output << "  " << outlier.test_name << ": " << outlier.duration.count() 
                           << "ms (" << std::fixed << std::setprecision(1) << ratio << "x median)\n";
                }
            }
        }
    }
    
    void PerformanceMetrics::saveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return;
        }
        
        // Write header
        file << "test_name,duration_ms,memory_kb,cpu_usage,context_switches\n";
        
        // Write performance data
        for (const auto& perf : m_performance_data) {
            file << perf.test_name << ","
                 << perf.duration.count() << ","
                 << perf.memory_usage << ","
                 << perf.cpu_usage << ","
                 << perf.context_switches << "\n";
        }
    }
    
    bool PerformanceMetrics::loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        m_performance_data.clear();
        
        std::string line;
        // Skip header
        if (!std::getline(file, line)) {
            return false;
        }
        
        // Read performance data
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string test_name, duration_str, memory_str, cpu_str, context_str;
            
            if (std::getline(iss, test_name, ',') &&
                std::getline(iss, duration_str, ',') &&
                std::getline(iss, memory_str, ',') &&
                std::getline(iss, cpu_str, ',') &&
                std::getline(iss, context_str)) {
                
                TestPerformance perf;
                perf.test_name = test_name;
                perf.duration = std::chrono::milliseconds(std::stoll(duration_str));
                perf.memory_usage = std::stoull(memory_str);
                perf.cpu_usage = std::stod(cpu_str);
                perf.context_switches = std::stoi(context_str);
                
                m_performance_data.push_back(perf);
            }
        }
        
        return true;
    }
    
    PerformanceMetrics::PerformanceStats PerformanceMetrics::getStatistics() const {
        PerformanceStats stats;
        
        if (m_performance_data.empty()) {
            return stats;
        }
        
        // Calculate timing statistics
        std::vector<std::chrono::milliseconds> durations;
        for (const auto& perf : m_performance_data) {
            durations.push_back(perf.duration);
        }
        std::sort(durations.begin(), durations.end());
        
        stats.min_time = durations.front();
        stats.max_time = durations.back();
        stats.median_time = durations[durations.size() / 2];
        stats.p90_time = durations[(durations.size() * 9) / 10];
        stats.p95_time = durations[(durations.size() * 95) / 100];
        
        // Calculate resource usage statistics
        for (const auto& perf : m_performance_data) {
            if (perf.memory_usage > 0) {
                stats.total_memory_kb += perf.memory_usage;
                stats.tests_with_memory_data++;
            }
            if (perf.cpu_usage > 0.0) {
                stats.total_cpu_seconds += perf.cpu_usage;
                stats.tests_with_cpu_data++;
            }
            stats.total_context_switches += perf.context_switches;
        }
        
        return stats;
    }
    
    std::vector<PerformanceMetrics::TestPerformance> PerformanceMetrics::getOutliers(double threshold_multiplier) const {
        std::vector<TestPerformance> outliers;
        
        if (m_performance_data.size() < 2) {
            return outliers;
        }
        
        auto stats = getStatistics();
        auto threshold = std::chrono::milliseconds(
            static_cast<long long>(stats.median_time.count() * threshold_multiplier)
        );
        
        for (const auto& perf : m_performance_data) {
            if (perf.duration > threshold) {
                outliers.push_back(perf);
            }
        }
        
        // Sort by duration (slowest first)
        std::sort(outliers.begin(), outliers.end(),
                  [](const TestPerformance& a, const TestPerformance& b) {
                      return a.duration > b.duration;
                  });
        
        return outliers;
    }
    
    std::vector<PerformanceMetrics::TestPerformance> PerformanceMetrics::getHighestMemoryTests(int count) const {
        std::vector<TestPerformance> memory_tests;
        
        // Filter tests with memory data
        for (const auto& perf : m_performance_data) {
            if (perf.memory_usage > 0) {
                memory_tests.push_back(perf);
            }
        }
        
        // Sort by memory usage (highest first)
        std::sort(memory_tests.begin(), memory_tests.end(),
                  [](const TestPerformance& a, const TestPerformance& b) {
                      return a.memory_usage > b.memory_usage;
                  });
        
        if (memory_tests.size() > static_cast<size_t>(count)) {
            memory_tests.resize(count);
        }
        
        return memory_tests;
    }
    
    std::vector<PerformanceMetrics::TestPerformance> PerformanceMetrics::getHighestCpuTests(int count) const {
        std::vector<TestPerformance> cpu_tests;
        
        // Filter tests with CPU data
        for (const auto& perf : m_performance_data) {
            if (perf.cpu_usage > 0.0) {
                cpu_tests.push_back(perf);
            }
        }
        
        // Sort by CPU usage (highest first)
        std::sort(cpu_tests.begin(), cpu_tests.end(),
                  [](const TestPerformance& a, const TestPerformance& b) {
                      return a.cpu_usage > b.cpu_usage;
                  });
        
        if (cpu_tests.size() > static_cast<size_t>(count)) {
            cpu_tests.resize(count);
        }
        
        return cpu_tests;
    }
    
    std::vector<PerformanceMetrics::PerformanceComparison> PerformanceMetrics::compareWithHistorical(const PerformanceMetrics& historical_data) const {
        std::vector<PerformanceComparison> comparisons;
        
        // Create a map of historical data for quick lookup
        std::map<std::string, TestPerformance> historical_map;
        for (const auto& perf : historical_data.m_performance_data) {
            historical_map[perf.test_name] = perf;
        }
        
        // Compare current data with historical
        for (const auto& current_perf : m_performance_data) {
            auto historical_it = historical_map.find(current_perf.test_name);
            if (historical_it != historical_map.end()) {
                PerformanceComparison comparison;
                comparison.test_name = current_perf.test_name;
                comparison.current_time = current_perf.duration;
                comparison.historical_time = historical_it->second.duration;
                
                if (historical_it->second.duration.count() > 0) {
                    double current_ms = static_cast<double>(current_perf.duration.count());
                    double historical_ms = static_cast<double>(historical_it->second.duration.count());
                    comparison.performance_change_percent = 
                        ((current_ms - historical_ms) / historical_ms) * 100.0;
                    
                    // Consider it a regression if performance degraded by more than 10%
                    comparison.is_regression = comparison.performance_change_percent > 10.0;
                } else {
                    comparison.performance_change_percent = 0.0;
                    comparison.is_regression = false;
                }
                
                comparisons.push_back(comparison);
            }
        }
        
        return comparisons;
    }

    // ========================================
    // REPORTER FACTORY IMPLEMENTATION
    // ========================================
    
    std::unique_ptr<TestReporter> ReporterFactory::createReporter(const std::string& type, std::ostream* output) {
        if (type == "console") {
            return std::make_unique<ConsoleReporter>(output);
        } else if (type == "xml") {
            return std::make_unique<XMLReporter>(output);
        } else if (type == "json") {
            return std::make_unique<JSONReporter>(output);
        }
        
        return nullptr;
    }
    
    std::vector<std::string> ReporterFactory::getAvailableTypes() {
        return {"console", "xml", "json"};
    }
    
    bool ReporterFactory::isTypeSupported(const std::string& type) {
        auto types = getAvailableTypes();
        return std::find(types.begin(), types.end(), type) != types.end();
    }

} // namespace TestFramework
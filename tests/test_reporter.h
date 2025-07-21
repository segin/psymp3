/*
 * test_reporter.h - Test reporting system for PsyMP3 test harness
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef TEST_REPORTER_H
#define TEST_REPORTER_H

#include "test_discovery.h"
#include "test_executor.h"
#include <string>
#include <vector>
#include <chrono>
#include <ostream>
#include <memory>
#include <map>

namespace TestFramework {

    // ========================================
    // TEST SUMMARY STRUCTURES
    // ========================================
    
    /**
     * @brief Comprehensive summary of test execution results
     */
    struct TestSummary {
        int total_tests = 0;                        ///< Total number of tests
        int passed_tests = 0;                       ///< Number of passed tests
        int failed_tests = 0;                       ///< Number of failed tests
        int timeout_tests = 0;                      ///< Number of timed out tests
        int crashed_tests = 0;                      ///< Number of crashed tests
        int build_error_tests = 0;                  ///< Number of build error tests
        int system_error_tests = 0;                 ///< Number of system error tests
        int skipped_tests = 0;                      ///< Number of skipped tests
        
        std::chrono::milliseconds total_duration{0}; ///< Total execution time
        std::chrono::milliseconds average_duration{0}; ///< Average test execution time
        std::chrono::milliseconds fastest_test{0};   ///< Fastest test execution time
        std::chrono::milliseconds slowest_test{0};   ///< Slowest test execution time
        
        std::string fastest_test_name;              ///< Name of fastest test
        std::string slowest_test_name;              ///< Name of slowest test
        
        std::vector<ExecutionResult> results;       ///< All test results
        std::chrono::system_clock::time_point start_time; ///< When testing started
        std::chrono::system_clock::time_point end_time;   ///< When testing ended
        
        /**
         * @brief Calculate summary statistics from results
         * @param test_results Vector of execution results
         */
        void calculateFromResults(const std::vector<ExecutionResult>& test_results);
        
        /**
         * @brief Check if all tests passed
         * @return true if no failures, timeouts, crashes, or errors
         */
        bool allTestsPassed() const;
        
        /**
         * @brief Get success rate as percentage
         * @return Success rate (0.0 to 100.0)
         */
        double getSuccessRate() const;
    };

    // ========================================
    // BASE REPORTER INTERFACE
    // ========================================
    
    /**
     * @brief Abstract base class for test reporters
     * 
     * Defines the interface for generating test reports in different formats.
     * Implementations should handle the specific formatting and output details.
     */
    class TestReporter {
    public:
        /**
         * @brief Virtual destructor
         */
        virtual ~TestReporter() = default;
        
        /**
         * @brief Called when test execution begins
         * @param tests Vector of tests that will be executed
         */
        virtual void reportStart(const std::vector<TestInfo>& tests) = 0;
        
        /**
         * @brief Called when an individual test completes
         * @param result Execution result for the completed test
         */
        virtual void reportTestResult(const ExecutionResult& result) = 0;
        
        /**
         * @brief Called when all tests have completed
         * @param summary Comprehensive summary of all test results
         */
        virtual void reportSummary(const TestSummary& summary) = 0;
        
        /**
         * @brief Called to report progress during execution
         * @param completed Number of tests completed
         * @param total Total number of tests
         */
        virtual void reportProgress(int completed, int total) {}
        
        /**
         * @brief Called when a test starts executing
         * @param test_name Name of the test starting
         */
        virtual void reportTestStart(const std::string& test_name) {}
        
        /**
         * @brief Set output stream for the reporter
         * @param output Output stream to write to
         */
        virtual void setOutputStream(std::ostream* output) { m_output = output; }
        
        /**
         * @brief Enable or disable colored output
         * @param enable Whether to use colored output
         */
        virtual void setColorOutput(bool enable) { m_color_output = enable; }
        
        /**
         * @brief Set verbosity level
         * @param verbose Whether to include verbose output
         */
        virtual void setVerbose(bool verbose) { m_verbose = verbose; }
        
    protected:
        std::ostream* m_output = nullptr;           ///< Output stream
        bool m_color_output = false;                ///< Whether to use colored output
        bool m_verbose = false;                     ///< Whether to include verbose output
    };

    // ========================================
    // CONSOLE REPORTER
    // ========================================
    
    /**
     * @brief Console reporter with real-time progress and colored output
     * 
     * Provides human-readable output suitable for interactive terminal use.
     * Supports colored output, progress indicators, and detailed failure reporting.
     */
    class ConsoleReporter : public TestReporter {
    public:
        /**
         * @brief Constructor
         * @param output Output stream (default: std::cout)
         * @param enable_color Whether to use colored output (default: auto-detect)
         */
        explicit ConsoleReporter(std::ostream* output = nullptr, bool enable_color = true);
        
        void reportStart(const std::vector<TestInfo>& tests) override;
        void reportTestResult(const ExecutionResult& result) override;
        void reportSummary(const TestSummary& summary) override;
        void reportProgress(int completed, int total) override;
        void reportTestStart(const std::string& test_name) override;
        
        /**
         * @brief Set quiet mode (minimal output)
         * @param quiet Whether to use quiet mode
         */
        void setQuiet(bool quiet) { m_quiet = quiet; }
        
        /**
         * @brief Set whether to show test output
         * @param show Whether to show stdout/stderr from tests
         */
        void setShowTestOutput(bool show) { m_show_test_output = show; }
        
    private:
        bool m_quiet = false;                       ///< Whether to use quiet mode
        bool m_show_test_output = false;            ///< Whether to show test output
        int m_total_tests = 0;                      ///< Total number of tests
        int m_completed_tests = 0;                  ///< Number of completed tests
        std::chrono::steady_clock::time_point m_start_time; ///< When testing started
        
        /**
         * @brief Apply color formatting to text
         * @param text Text to colorize
         * @param color_code ANSI color code
         * @return Colorized text (or plain text if colors disabled)
         */
        std::string colorize(const std::string& text, const std::string& color_code) const;
        
        /**
         * @brief Get color code for execution status
         * @param status Execution status
         * @return ANSI color code
         */
        std::string getStatusColor(ExecutionStatus status) const;
        
        /**
         * @brief Get status text for execution status
         * @param status Execution status
         * @return Human-readable status text
         */
        std::string getStatusText(ExecutionStatus status) const;
        
        /**
         * @brief Format duration for display
         * @param duration Duration in milliseconds
         * @return Formatted duration string
         */
        std::string formatDuration(std::chrono::milliseconds duration) const;
        
        /**
         * @brief Print a separator line
         * @param character Character to use for separator
         * @param length Length of separator line
         */
        void printSeparator(char character = '=', int length = 60) const;
        
        /**
         * @brief Check if terminal supports color output
         * @return true if colors should be enabled
         */
        bool shouldUseColor() const;
    };

    // ========================================
    // XML REPORTER (JUNIT COMPATIBLE)
    // ========================================
    
    /**
     * @brief XML reporter for JUnit-compatible CI integration
     * 
     * Generates XML output compatible with JUnit test result format,
     * suitable for consumption by CI systems like Jenkins, GitLab CI, etc.
     */
    class XMLReporter : public TestReporter {
    public:
        /**
         * @brief Constructor
         * @param output Output stream (default: std::cout)
         */
        explicit XMLReporter(std::ostream* output = nullptr);
        
        void reportStart(const std::vector<TestInfo>& tests) override;
        void reportTestResult(const ExecutionResult& result) override;
        void reportSummary(const TestSummary& summary) override;
        
        /**
         * @brief Set test suite name for XML output
         * @param suite_name Name of the test suite
         */
        void setTestSuiteName(const std::string& suite_name) { m_suite_name = suite_name; }
        
        /**
         * @brief Set whether to include system output in XML
         * @param include Whether to include stdout/stderr
         */
        void setIncludeSystemOutput(bool include) { m_include_system_output = include; }
        
    private:
        std::string m_suite_name = "PsyMP3Tests";   ///< Test suite name
        bool m_include_system_output = true;        ///< Whether to include system output
        std::vector<ExecutionResult> m_results;     ///< Collected results for final output
        
        /**
         * @brief Escape XML special characters
         * @param text Text to escape
         * @return XML-escaped text
         */
        std::string escapeXml(const std::string& text) const;
        
        /**
         * @brief Convert execution status to JUnit status
         * @param status Execution status
         * @return JUnit-compatible status string
         */
        std::string statusToJUnit(ExecutionStatus status) const;
        
        /**
         * @brief Format timestamp for XML
         * @param time_point Time point to format
         * @return ISO 8601 formatted timestamp
         */
        std::string formatTimestamp(const std::chrono::system_clock::time_point& time_point) const;
    };

    // ========================================
    // JSON REPORTER
    // ========================================
    
    /**
     * @brief JSON reporter for structured data output
     * 
     * Generates JSON output suitable for programmatic consumption,
     * custom tooling, and integration with modern CI/CD pipelines.
     */
    class JSONReporter : public TestReporter {
    public:
        /**
         * @brief Constructor
         * @param output Output stream (default: std::cout)
         */
        explicit JSONReporter(std::ostream* output = nullptr);
        
        void reportStart(const std::vector<TestInfo>& tests) override;
        void reportTestResult(const ExecutionResult& result) override;
        void reportSummary(const TestSummary& summary) override;
        
        /**
         * @brief Set whether to pretty-print JSON output
         * @param pretty Whether to use pretty formatting
         */
        void setPrettyPrint(bool pretty) { m_pretty_print = pretty; }
        
        /**
         * @brief Set whether to include test metadata in output
         * @param include Whether to include metadata
         */
        void setIncludeMetadata(bool include) { m_include_metadata = include; }
        
    private:
        bool m_pretty_print = false;                ///< Whether to pretty-print JSON
        bool m_include_metadata = true;             ///< Whether to include test metadata
        std::vector<ExecutionResult> m_results;     ///< Collected results for final output
        std::vector<TestInfo> m_test_info;          ///< Test information for metadata
        
        /**
         * @brief Escape JSON special characters
         * @param text Text to escape
         * @return JSON-escaped text
         */
        std::string escapeJson(const std::string& text) const;
        
        /**
         * @brief Convert execution status to JSON string
         * @param status Execution status
         * @return JSON-compatible status string
         */
        std::string statusToJson(ExecutionStatus status) const;
        
        /**
         * @brief Format timestamp for JSON
         * @param time_point Time point to format
         * @return ISO 8601 formatted timestamp
         */
        std::string formatTimestamp(const std::chrono::system_clock::time_point& time_point) const;
        
        /**
         * @brief Generate JSON for test metadata
         * @param test Test information
         * @return JSON object string
         */
        std::string testInfoToJson(const TestInfo& test) const;
        
        /**
         * @brief Generate JSON for execution result
         * @param result Execution result
         * @return JSON object string
         */
        std::string resultToJson(const ExecutionResult& result) const;
        
        /**
         * @brief Generate JSON for test summary
         * @param summary Test summary
         * @return JSON object string
         */
        std::string summaryToJson(const TestSummary& summary) const;
    };

    // ========================================
    // PERFORMANCE METRICS COLLECTOR
    // ========================================
    
    /**
     * @brief Collector for performance metrics and timing information
     * 
     * Tracks detailed performance metrics during test execution,
     * including timing, resource usage, and trend analysis.
     */
    class PerformanceMetrics {
    public:
        /**
         * @brief Performance data for a single test
         */
        struct TestPerformance {
            std::string test_name;                  ///< Test name
            std::chrono::milliseconds duration;     ///< Execution time
            size_t memory_usage = 0;                ///< Peak memory usage (if available)
            double cpu_usage = 0.0;                 ///< CPU usage percentage (if available)
            int context_switches = 0;               ///< Number of context switches (if available)
            
            TestPerformance() : duration(0) {}
            TestPerformance(const std::string& name, std::chrono::milliseconds dur)
                : test_name(name), duration(dur) {}
        };
        
        /**
         * @brief Add performance data for a test
         * @param result Execution result containing timing information
         */
        void addTestResult(const ExecutionResult& result);
        
        /**
         * @brief Add performance data with additional metrics
         * @param test_name Name of the test
         * @param duration Execution time
         * @param memory_usage Peak memory usage in KB
         * @param cpu_usage CPU usage percentage
         * @param context_switches Number of context switches
         */
        void addTestPerformance(const std::string& test_name, 
                               std::chrono::milliseconds duration,
                               size_t memory_usage = 0,
                               double cpu_usage = 0.0,
                               int context_switches = 0);
        
        /**
         * @brief Get performance data for all tests
         * @return Vector of performance data
         */
        const std::vector<TestPerformance>& getPerformanceData() const { return m_performance_data; }
        
        /**
         * @brief Get slowest tests
         * @param count Number of slowest tests to return
         * @return Vector of slowest tests
         */
        std::vector<TestPerformance> getSlowestTests(int count = 5) const;
        
        /**
         * @brief Get fastest tests
         * @param count Number of fastest tests to return
         * @return Vector of fastest tests
         */
        std::vector<TestPerformance> getFastestTests(int count = 5) const;
        
        /**
         * @brief Get average execution time
         * @return Average execution time across all tests
         */
        std::chrono::milliseconds getAverageExecutionTime() const;
        
        /**
         * @brief Get total execution time
         * @return Total time for all tests
         */
        std::chrono::milliseconds getTotalExecutionTime() const;
        
        /**
         * @brief Generate performance report
         * @param output Output stream to write report to
         */
        void generateReport(std::ostream& output) const;
        
        /**
         * @brief Save performance data to file for trend tracking
         * @param filename File to save performance data to
         * @param include_timestamp Whether to include timestamp in data
         */
        void saveToFile(const std::string& filename, bool include_timestamp = true) const;
        
        /**
         * @brief Load historical performance data from file
         * @param filename File to load performance data from
         * @return true if loaded successfully
         */
        bool loadFromFile(const std::string& filename);
        
        /**
         * @brief Append current performance data to trend file
         * @param filename Trend file to append to
         * @param run_id Unique identifier for this test run
         */
        void appendToTrendFile(const std::string& filename, const std::string& run_id = "") const;
        
        /**
         * @brief Compare current performance with historical data
         * @param historical_data Previous performance data
         * @return Performance comparison report
         */
        struct PerformanceComparison {
            std::string test_name;
            std::chrono::milliseconds current_time;
            std::chrono::milliseconds historical_time;
            double performance_change_percent;
            bool is_regression;
        };
        
        std::vector<PerformanceComparison> compareWithHistorical(const PerformanceMetrics& historical_data) const;
        
        /**
         * @brief Get performance statistics summary
         * @return Performance statistics
         */
        struct PerformanceStats {
            std::chrono::milliseconds min_time{0};
            std::chrono::milliseconds max_time{0};
            std::chrono::milliseconds median_time{0};
            std::chrono::milliseconds p90_time{0};
            std::chrono::milliseconds p95_time{0};
            size_t total_memory_kb = 0;
            double total_cpu_seconds = 0.0;
            int total_context_switches = 0;
            size_t tests_with_memory_data = 0;
            size_t tests_with_cpu_data = 0;
        };
        
        PerformanceStats getStatistics() const;
        
        /**
         * @brief Identify performance outliers
         * @param threshold_multiplier Multiplier for median time to identify outliers
         * @return Vector of outlier tests
         */
        std::vector<TestPerformance> getOutliers(double threshold_multiplier = 2.0) const;
        
        /**
         * @brief Get tests with highest memory usage
         * @param count Number of tests to return
         * @return Vector of tests with highest memory usage
         */
        std::vector<TestPerformance> getHighestMemoryTests(int count = 5) const;
        
        /**
         * @brief Get tests with highest CPU usage
         * @param count Number of tests to return
         * @return Vector of tests with highest CPU usage
         */
        std::vector<TestPerformance> getHighestCpuTests(int count = 5) const;
        
        /**
         * @brief Generate performance trend analysis
         * @param historical_files Vector of historical performance files
         * @param output Output stream to write trend analysis to
         */
        void generateTrendAnalysis(const std::vector<std::string>& historical_files, std::ostream& output) const;
        
        /**
         * @brief Detect performance regressions compared to baseline
         * @param baseline_data Baseline performance data
         * @param regression_threshold Percentage threshold for regression detection (default: 20%)
         * @return Vector of tests with performance regressions
         */
        std::vector<PerformanceComparison> detectRegressions(const PerformanceMetrics& baseline_data, 
                                                            double regression_threshold = 20.0) const;
        
        /**
         * @brief Get performance improvement recommendations
         * @return Vector of recommendations for slow tests
         */
        struct PerformanceRecommendation {
            std::string test_name;
            std::string issue_type;
            std::string recommendation;
            double severity_score;
        };
        
        std::vector<PerformanceRecommendation> getPerformanceRecommendations() const;
        
        /**
         * @brief Clear all performance data
         */
        void clear() { m_performance_data.clear(); }
        
        /**
         * @brief Get number of tests with performance data
         * @return Number of tests
         */
        size_t size() const { return m_performance_data.size(); }
        
        /**
         * @brief Check if performance data is empty
         * @return true if no data available
         */
        bool empty() const { return m_performance_data.empty(); }
        
    private:
        std::vector<TestPerformance> m_performance_data; ///< Collected performance data
        
        /**
         * @brief Parse performance data from CSV line
         * @param line CSV line to parse
         * @return TestPerformance data, or empty if parsing failed
         */
        TestPerformance parsePerformanceLine(const std::string& line) const;
        
        /**
         * @brief Format performance data as CSV line
         * @param perf Performance data to format
         * @param include_timestamp Whether to include timestamp
         * @return CSV formatted line
         */
        std::string formatPerformanceLine(const TestPerformance& perf, bool include_timestamp = true) const;
    };

    // ========================================
    // REPORTER FACTORY
    // ========================================
    
    /**
     * @brief Factory for creating test reporters
     */
    class ReporterFactory {
    public:
        /**
         * @brief Create a reporter by name
         * @param type Reporter type ("console", "xml", "json")
         * @param output Output stream (optional)
         * @return Unique pointer to reporter, or nullptr if type unknown
         */
        static std::unique_ptr<TestReporter> createReporter(const std::string& type, 
                                                           std::ostream* output = nullptr);
        
        /**
         * @brief Get list of available reporter types
         * @return Vector of supported reporter type names
         */
        static std::vector<std::string> getAvailableTypes();
        
        /**
         * @brief Check if a reporter type is supported
         * @param type Reporter type to check
         * @return true if type is supported
         */
        static bool isTypeSupported(const std::string& type);
    };

} // namespace TestFramework

#endif // TEST_REPORTER_H
/*
 * test_performance.cpp - Performance monitoring and optimization for PsyMP3 test harness
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "test_reporter.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <ctime>
#include <iomanip>
#include <cmath>
#include <map>
#include <unordered_map>
#include <sys/resource.h>

namespace TestFramework {

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
            // Calculate dynamic column width based on longest test name
            size_t max_name_length = 9; // Minimum width for "Test Name" header
            for (const auto& test : slowest) {
                max_name_length = std::max(max_name_length, test.test_name.length());
            }
            max_name_length += 2; // Add padding
            
            output << "SLOWEST TESTS:\n";
            output << std::left << std::setw(max_name_length) << "Test Name" 
                   << std::setw(12) << "Time (ms)"
                   << std::setw(14) << "Memory (KB)"
                   << std::setw(12) << "CPU (s)"
                   << std::setw(12) << "Context SW" << "\n";
            output << std::string(max_name_length + 12 + 14 + 12 + 12, '-') << "\n";
            
            for (const auto& test : slowest) {
                output << std::left << std::setw(max_name_length) << test.test_name
                       << std::setw(12) << test.duration.count();
                
                if (test.memory_usage > 0) {
                    output << std::setw(14) << test.memory_usage;
                } else {
                    output << std::setw(14) << "N/A";
                }
                
                if (test.cpu_usage > 0.0) {
                    output << std::setw(12) << std::fixed << std::setprecision(3) << test.cpu_usage;
                } else {
                    output << std::setw(12) << "N/A";
                }
                
                if (test.context_switches > 0) {
                    output << std::setw(12) << test.context_switches;
                } else {
                    output << std::setw(12) << "N/A";
                }
                
                output << "\n";
            }
        }
    }
    
    void PerformanceMetrics::saveToFile(const std::string& filename, bool include_timestamp) const {
        std::ofstream file(filename);
        if (!file) {
            throw std::runtime_error("Failed to open performance data file for writing: " + filename);
        }
        
        // Write header
        file << "test_name,duration_ms,memory_kb,cpu_seconds,context_switches";
        if (include_timestamp) {
            file << ",timestamp";
        }
        file << "\n";
        
        // Write data
        for (const auto& perf : m_performance_data) {
            file << formatPerformanceLine(perf, include_timestamp) << "\n";
        }
    }
    
    bool PerformanceMetrics::loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) {
            return false;
        }
        
        m_performance_data.clear();
        
        std::string line;
        // Skip header
        std::getline(file, line);
        
        // Read data
        while (std::getline(file, line)) {
            auto perf = parsePerformanceLine(line);
            if (!perf.test_name.empty()) {
                m_performance_data.push_back(perf);
            }
        }
        
        return !m_performance_data.empty();
    }
    
    void PerformanceMetrics::appendToTrendFile(const std::string& filename, const std::string& run_id) const {
        std::ofstream file(filename, std::ios::app);
        if (!file) {
            throw std::runtime_error("Failed to open trend file for appending: " + filename);
        }
        
        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream timestamp;
        timestamp << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        
        // Write data with run ID and timestamp
        for (const auto& perf : m_performance_data) {
            file << run_id << "," << timestamp.str() << "," << perf.test_name << "," 
                 << perf.duration.count() << "," << perf.memory_usage << "," 
                 << perf.cpu_usage << "," << perf.context_switches << "\n";
        }
    }
    
    std::vector<PerformanceMetrics::PerformanceComparison> 
    PerformanceMetrics::compareWithHistorical(const PerformanceMetrics& historical_data) const {
        std::vector<PerformanceComparison> comparisons;
        
        // Create map of historical data for quick lookup
        std::map<std::string, TestPerformance> historical_map;
        for (const auto& perf : historical_data.m_performance_data) {
            historical_map[perf.test_name] = perf;
        }
        
        // Compare current data with historical
        for (const auto& current : m_performance_data) {
            auto it = historical_map.find(current.test_name);
            if (it != historical_map.end()) {
                const auto& historical = it->second;
                
                PerformanceComparison comp;
                comp.test_name = current.test_name;
                comp.current_time = current.duration;
                comp.historical_time = historical.duration;
                
                // Calculate percentage change
                double historical_ms = historical.duration.count();
                double current_ms = current.duration.count();
                
                if (historical_ms > 0) {
                    comp.performance_change_percent = ((current_ms - historical_ms) / historical_ms) * 100.0;
                } else {
                    comp.performance_change_percent = 0.0;
                }
                
                // Determine if this is a regression (slower)
                // Use a threshold to avoid noise (5% slower is considered a regression)
                comp.is_regression = comp.performance_change_percent > 5.0;
                
                comparisons.push_back(comp);
            }
        }
        
        // Sort by regression status and then by percentage change
        std::sort(comparisons.begin(), comparisons.end(), 
                  [](const PerformanceComparison& a, const PerformanceComparison& b) {
                      if (a.is_regression != b.is_regression) {
                          return a.is_regression; // Regressions first
                      }
                      return std::abs(a.performance_change_percent) > std::abs(b.performance_change_percent);
                  });
        
        return comparisons;
    }
    
    PerformanceMetrics::PerformanceStats PerformanceMetrics::getStatistics() const {
        PerformanceStats stats;
        
        if (m_performance_data.empty()) {
            return stats;
        }
        
        // Extract durations for percentile calculations
        std::vector<std::chrono::milliseconds> durations;
        durations.reserve(m_performance_data.size());
        
        for (const auto& perf : m_performance_data) {
            durations.push_back(perf.duration);
            
            // Track min/max
            if (stats.min_time.count() == 0 || perf.duration < stats.min_time) {
                stats.min_time = perf.duration;
            }
            if (perf.duration > stats.max_time) {
                stats.max_time = perf.duration;
            }
            
            // Accumulate resource usage
            if (perf.memory_usage > 0) {
                stats.total_memory_kb += perf.memory_usage;
                stats.tests_with_memory_data++;
            }
            if (perf.cpu_usage > 0.0) {
                stats.total_cpu_seconds += perf.cpu_usage;
                stats.tests_with_cpu_data++;
            }
            if (perf.context_switches > 0) {
                stats.total_context_switches += perf.context_switches;
            }
        }
        
        // Sort durations for percentile calculations
        std::sort(durations.begin(), durations.end());
        
        // Calculate median (50th percentile)
        size_t median_idx = durations.size() / 2;
        stats.median_time = durations[median_idx];
        
        // Calculate 90th percentile
        size_t p90_idx = static_cast<size_t>(durations.size() * 0.9);
        stats.p90_time = durations[p90_idx];
        
        // Calculate 95th percentile
        size_t p95_idx = static_cast<size_t>(durations.size() * 0.95);
        stats.p95_time = durations[p95_idx];
        
        return stats;
    }
    
    std::vector<PerformanceMetrics::TestPerformance> 
    PerformanceMetrics::getOutliers(double threshold_multiplier) const {
        std::vector<TestPerformance> outliers;
        
        if (m_performance_data.empty()) {
            return outliers;
        }
        
        // Get statistics to determine median
        auto stats = getStatistics();
        auto median_ms = stats.median_time.count();
        auto threshold_ms = static_cast<long long>(median_ms * threshold_multiplier);
        
        // Find outliers
        for (const auto& perf : m_performance_data) {
            if (perf.duration.count() > threshold_ms) {
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
    
    std::vector<PerformanceMetrics::TestPerformance> 
    PerformanceMetrics::getHighestMemoryTests(int count) const {
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
    
    std::vector<PerformanceMetrics::TestPerformance> 
    PerformanceMetrics::getHighestCpuTests(int count) const {
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
    
    void PerformanceMetrics::generateTrendAnalysis(
        const std::vector<std::string>& historical_files, std::ostream& output) const {
        
        if (historical_files.empty()) {
            output << "No historical data files provided for trend analysis.\n";
            return;
        }
        
        // Load historical data from each file
        std::vector<std::pair<std::string, PerformanceMetrics>> historical_data;
        for (const auto& file : historical_files) {
            PerformanceMetrics metrics;
            if (metrics.loadFromFile(file)) {
                historical_data.emplace_back(file, std::move(metrics));
            }
        }
        
        if (historical_data.empty()) {
            output << "No valid historical data found in provided files.\n";
            return;
        }
        
        // Create map of test names to trend data
        std::map<std::string, std::vector<std::pair<std::string, std::chrono::milliseconds>>> trends;
        
        // Collect trend data for each test
        for (const auto& [file, metrics] : historical_data) {
            for (const auto& perf : metrics.m_performance_data) {
                trends[perf.test_name].emplace_back(file, perf.duration);
            }
        }
        
        // Add current data
        for (const auto& perf : m_performance_data) {
            trends[perf.test_name].emplace_back("current", perf.duration);
        }
        
        // Generate trend report
        output << "PERFORMANCE TREND ANALYSIS\n";
        output << "=========================\n\n";
        
        for (const auto& [test_name, trend_data] : trends) {
            if (trend_data.size() < 2) {
                continue; // Need at least two data points for a trend
            }
            
            output << "Test: " << test_name << "\n";
            output << "-------------------------\n";
            
            // Calculate trend statistics
            std::chrono::milliseconds min_time = trend_data[0].second;
            std::chrono::milliseconds max_time = trend_data[0].second;
            std::chrono::milliseconds total_time(0);
            
            for (const auto& [file, duration] : trend_data) {
                output << "  " << file << ": " << duration.count() << "ms\n";
                
                if (duration < min_time) min_time = duration;
                if (duration > max_time) max_time = duration;
                total_time += duration;
            }
            
            auto avg_time = std::chrono::milliseconds(total_time.count() / trend_data.size());
            
            output << "\n  Min: " << min_time.count() << "ms\n";
            output << "  Max: " << max_time.count() << "ms\n";
            output << "  Avg: " << avg_time.count() << "ms\n";
            output << "  Range: " << (max_time - min_time).count() << "ms\n";
            
            // Calculate trend direction
            if (trend_data.size() >= 3) {
                bool increasing = true;
                bool decreasing = true;
                
                for (size_t i = 1; i < trend_data.size(); ++i) {
                    if (trend_data[i].second <= trend_data[i-1].second) {
                        increasing = false;
                    }
                    if (trend_data[i].second >= trend_data[i-1].second) {
                        decreasing = false;
                    }
                }
                
                if (increasing) {
                    output << "  Trend: Consistently increasing (getting slower)\n";
                } else if (decreasing) {
                    output << "  Trend: Consistently decreasing (getting faster)\n";
                } else {
                    output << "  Trend: Fluctuating\n";
                }
            }
            
            output << "\n";
        }
    }
    
    std::vector<PerformanceMetrics::PerformanceRecommendation> 
    PerformanceMetrics::getPerformanceRecommendations() const {
        std::vector<PerformanceRecommendation> recommendations;
        
        if (m_performance_data.empty()) {
            return recommendations;
        }
        
        // Get statistics
        auto stats = getStatistics();
        
        // Get outliers
        auto outliers = getOutliers(2.0);
        for (const auto& outlier : outliers) {
            PerformanceRecommendation rec;
            rec.test_name = outlier.test_name;
            rec.issue_type = "Execution Time";
            
            double ratio = static_cast<double>(outlier.duration.count()) / stats.median_time.count();
            rec.severity_score = ratio;
            
            std::ostringstream oss;
            oss << "Test is " << std::fixed << std::setprecision(1) << ratio 
                << "x slower than median. Consider optimizing or splitting into smaller tests.";
            rec.recommendation = oss.str();
            
            recommendations.push_back(rec);
        }
        
        // Check for high memory usage
        auto high_memory_tests = getHighestMemoryTests(5);
        for (const auto& test : high_memory_tests) {
            // Only flag tests with significantly high memory usage (>100MB)
            if (test.memory_usage > 100000) {
                PerformanceRecommendation rec;
                rec.test_name = test.test_name;
                rec.issue_type = "Memory Usage";
                rec.severity_score = test.memory_usage / 1000.0; // Convert to MB for score
                
                std::ostringstream oss;
                oss << "Test uses " << (test.memory_usage / 1024) 
                    << "MB of memory. Consider reducing memory footprint or checking for leaks.";
                rec.recommendation = oss.str();
                
                recommendations.push_back(rec);
            }
        }
        
        // Check for high CPU usage
        auto high_cpu_tests = getHighestCpuTests(5);
        for (const auto& test : high_cpu_tests) {
            // Only flag tests with high CPU time (>5s)
            if (test.cpu_usage > 5.0) {
                PerformanceRecommendation rec;
                rec.test_name = test.test_name;
                rec.issue_type = "CPU Usage";
                rec.severity_score = test.cpu_usage;
                
                std::ostringstream oss;
                oss << "Test uses " << std::fixed << std::setprecision(1) << test.cpu_usage 
                    << "s of CPU time. Consider optimizing CPU-intensive operations.";
                rec.recommendation = oss.str();
                
                recommendations.push_back(rec);
            }
        }
        
        // Sort by severity score (highest first)
        std::sort(recommendations.begin(), recommendations.end(), 
                  [](const PerformanceRecommendation& a, const PerformanceRecommendation& b) {
                      return a.severity_score > b.severity_score;
                  });
        
        return recommendations;
    }
    
    PerformanceMetrics::TestPerformance 
    PerformanceMetrics::parsePerformanceLine(const std::string& line) const {
        TestPerformance perf;
        
        std::istringstream iss(line);
        std::string token;
        
        // Parse CSV format: test_name,duration_ms,memory_kb,cpu_seconds,context_switches[,timestamp]
        if (std::getline(iss, token, ',')) {
            perf.test_name = token;
            
            if (std::getline(iss, token, ',')) {
                try {
                    perf.duration = std::chrono::milliseconds(std::stoll(token));
                    
                    if (std::getline(iss, token, ',')) {
                        perf.memory_usage = std::stoull(token);
                        
                        if (std::getline(iss, token, ',')) {
                            perf.cpu_usage = std::stod(token);
                            
                            if (std::getline(iss, token, ',')) {
                                perf.context_switches = std::stoi(token);
                            }
                        }
                    }
                } catch (const std::exception&) {
                    // Invalid format, return empty performance data
                    return TestPerformance();
                }
            }
        }
        
        return perf;
    }
    
    std::string PerformanceMetrics::formatPerformanceLine(
        const TestPerformance& perf, bool include_timestamp) const {
        
        std::ostringstream oss;
        oss << perf.test_name << "," 
            << perf.duration.count() << "," 
            << perf.memory_usage << "," 
            << std::fixed << std::setprecision(6) << perf.cpu_usage << "," 
            << perf.context_switches;
        
        if (include_timestamp) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            oss << "," << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        }
        
        return oss.str();
    }
    
    std::vector<PerformanceMetrics::PerformanceComparison> 
    PerformanceMetrics::detectRegressions(
        const PerformanceMetrics& baseline_data, double regression_threshold) const {
        
        std::vector<PerformanceComparison> regressions;
        auto comparisons = compareWithHistorical(baseline_data);
        
        for (const auto& comp : comparisons) {
            if (comp.performance_change_percent > regression_threshold) {
                regressions.push_back(comp);
            }
        }
        
        return regressions;
    }

} // namespace TestFramework
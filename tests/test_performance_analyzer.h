/*
 * test_performance_analyzer.h - Performance analysis utilities for PsyMP3 test harness
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef TEST_PERFORMANCE_ANALYZER_H
#define TEST_PERFORMANCE_ANALYZER_H

#include "test_reporter.h"
#include <string>
#include <vector>
#include <chrono>
#include <ostream>

namespace TestFramework {

    /**
     * @brief Performance analysis utilities for test results
     * 
     * This class provides additional analysis tools for performance data
     * collected by the PerformanceMetrics class.
     */
    class PerformanceAnalyzer {
    public:
        /**
         * @brief Constructor
         * @param metrics Performance metrics to analyze
         */
        explicit PerformanceAnalyzer(const PerformanceMetrics& metrics) : m_metrics(metrics) {}
        
        /**
         * @brief Generate trend analysis from historical data files
         * @param historical_files Vector of historical performance data files
         * @param output Output stream to write trend analysis to
         */
        void generateTrendAnalysis(const std::vector<std::string>& historical_files, std::ostream& output) const;
        
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
         * @brief Detect performance regressions compared to baseline
         * @param baseline_data Baseline performance data
         * @param regression_threshold Percentage threshold for regression detection (default: 20%)
         * @return Vector of tests with performance regressions
         */
        std::vector<PerformanceMetrics::PerformanceComparison> detectRegressions(
            const PerformanceMetrics& baseline_data, double regression_threshold = 20.0) const;
        
    private:
        const PerformanceMetrics& m_metrics;
    };

} // namespace TestFramework

#endif // TEST_PERFORMANCE_ANALYZER_H
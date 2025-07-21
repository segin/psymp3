/*
 * test_harness.cpp - Main test harness application for PsyMP3
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "test_discovery.h"
#include "test_executor.h"
#include "test_framework.h"
#include "test_reporter.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <getopt.h>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <fstream>

using namespace TestFramework;

// ========================================
// COMMAND LINE ARGUMENT STRUCTURE
// ========================================

struct CommandLineArgs {
    bool verbose = false;
    bool quiet = false;
    bool list_tests = false;
    bool parallel = false;
    bool stop_on_failure = false;
    std::string filter_pattern = "";
    std::string output_format = "console";
    std::string test_directory = ".";
    int max_parallel = 4;
    int timeout_seconds = 30;
    bool track_performance = false;
    std::string performance_file = "test_performance.csv";
    bool show_performance_report = false;
    bool show_detailed_performance = false;
    bool show_memory_report = false;
    bool show_outliers = false;
    double outlier_threshold = 2.0;
    
    void printUsage(const char* program_name) {
        std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
        std::cout << "PsyMP3 Test Harness - Unified test execution and reporting\n\n";
        std::cout << "OPTIONS:\n";
        std::cout << "  -v, --verbose           Display detailed output from each test\n";
        std::cout << "  -q, --quiet             Suppress progress output, show only summary\n";
        std::cout << "  -l, --list              List all available tests without running them\n";
        std::cout << "  -p, --parallel          Run independent tests concurrently\n";
        std::cout << "  -s, --stop-on-failure   Stop execution on first test failure\n";
        std::cout << "  -f, --filter PATTERN    Run only tests matching the pattern (glob-style)\n";
        std::cout << "  -o, --output FORMAT     Output format: console, xml, json (default: console)\n";
        std::cout << "  -d, --directory DIR     Test directory to scan (default: .)\n";
        std::cout << "  -j, --jobs N            Maximum parallel processes (default: 4)\n";
        std::cout << "  -t, --timeout SECONDS   Test timeout in seconds (default: 30)\n";
        std::cout << "  --track-performance     Enable performance tracking and trend analysis\n";
        std::cout << "  --performance-file FILE Performance data file (default: test_performance.csv)\n";
        std::cout << "  --show-performance      Show detailed performance report\n";
        std::cout << "  --show-detailed-perf    Show detailed performance metrics with memory/CPU\n";
        std::cout << "  --show-memory-report    Show memory usage analysis\n";
        std::cout << "  --show-outliers         Show performance outliers\n";
        std::cout << "  --outlier-threshold N   Outlier threshold multiplier (default: 2.0)\n";
        std::cout << "  -h, --help              Show this help message\n\n";
        std::cout << "EXAMPLES:\n";
        std::cout << "  " << program_name << "                    # Run all tests\n";
        std::cout << "  " << program_name << " -v                 # Run with verbose output\n";
        std::cout << "  " << program_name << " -f \"*rect*\"        # Run only rectangle tests\n";
        std::cout << "  " << program_name << " -p -j 8            # Run with 8 parallel processes\n";
        std::cout << "  " << program_name << " -l                 # List available tests\n";
        std::cout << "  " << program_name << " -o xml > results.xml # Generate XML report\n";
    }
};

// ========================================
// COMMAND LINE PARSING
// ========================================

CommandLineArgs parseCommandLine(int argc, char* argv[]) {
    CommandLineArgs args;
    
    static struct option long_options[] = {
        {"verbose",         no_argument,       0, 'v'},
        {"quiet",           no_argument,       0, 'q'},
        {"list",            no_argument,       0, 'l'},
        {"parallel",        no_argument,       0, 'p'},
        {"stop-on-failure", no_argument,       0, 's'},
        {"filter",          required_argument, 0, 'f'},
        {"output",          required_argument, 0, 'o'},
        {"directory",       required_argument, 0, 'd'},
        {"jobs",            required_argument, 0, 'j'},
        {"timeout",         required_argument, 0, 't'},
        {"track-performance", no_argument,     0, 1001},
        {"performance-file", required_argument, 0, 1002},
        {"show-performance", no_argument,      0, 1003},
        {"show-detailed-perf", no_argument,    0, 1004},
        {"show-memory-report", no_argument,    0, 1005},
        {"show-outliers", no_argument,         0, 1006},
        {"outlier-threshold", required_argument, 0, 1007},
        {"help",            no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "vqlpsf:o:d:j:t:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'v':
                args.verbose = true;
                break;
            case 'q':
                args.quiet = true;
                break;
            case 'l':
                args.list_tests = true;
                break;
            case 'p':
                args.parallel = true;
                break;
            case 's':
                args.stop_on_failure = true;
                break;
            case 'f':
                args.filter_pattern = optarg;
                break;
            case 'o':
                args.output_format = optarg;
                break;
            case 'd':
                args.test_directory = optarg;
                break;
            case 'j':
                args.max_parallel = std::atoi(optarg);
                if (args.max_parallel <= 0) {
                    std::cerr << "Error: Invalid number of parallel jobs: " << optarg << std::endl;
                    exit(1);
                }
                break;
            case 't':
                args.timeout_seconds = std::atoi(optarg);
                if (args.timeout_seconds <= 0) {
                    std::cerr << "Error: Invalid timeout value: " << optarg << std::endl;
                    exit(1);
                }
                break;
            case 1001:
                args.track_performance = true;
                break;
            case 1002:
                args.performance_file = optarg;
                break;
            case 1003:
                args.show_performance_report = true;
                break;
            case 1004:
                args.show_detailed_performance = true;
                break;
            case 1005:
                args.show_memory_report = true;
                break;
            case 1006:
                args.show_outliers = true;
                break;
            case 1007:
                args.outlier_threshold = std::stod(optarg);
                if (args.outlier_threshold <= 0.0) {
                    std::cerr << "Error: Invalid outlier threshold: " << optarg << std::endl;
                    exit(1);
                }
                break;
            case 'h':
                args.printUsage(argv[0]);
                exit(0);
            case '?':
                // getopt_long already printed an error message
                exit(1);
            default:
                std::cerr << "Error: Unknown option" << std::endl;
                exit(1);
        }
    }
    
    // Validate conflicting options
    if (args.verbose && args.quiet) {
        std::cerr << "Error: Cannot specify both --verbose and --quiet" << std::endl;
        exit(1);
    }
    
    if (args.output_format != "console" && args.output_format != "xml" && args.output_format != "json") {
        std::cerr << "Error: Invalid output format: " << args.output_format << std::endl;
        std::cerr << "Valid formats: console, xml, json" << std::endl;
        exit(1);
    }
    
    return args;
}

// ========================================
// TEST LISTING FUNCTIONALITY
// ========================================

void listTests(const std::vector<TestInfo>& tests, const CommandLineArgs& args) {
    if (!args.quiet) {
        std::cout << "Available tests in " << args.test_directory << ":\n\n";
    }
    
    if (tests.empty()) {
        std::cout << "No tests found.\n";
        return;
    }
    
    // Sort tests by name for consistent output
    std::vector<TestInfo> sorted_tests = tests;
    std::sort(sorted_tests.begin(), sorted_tests.end(), 
              [](const TestInfo& a, const TestInfo& b) {
                  return a.name < b.name;
              });
    
    for (const auto& test : sorted_tests) {
        if (args.verbose) {
            std::cout << "Test: " << test.name << "\n";
            std::cout << "  Executable: " << test.executable_path << "\n";
            std::cout << "  Source: " << test.source_path << "\n";
            std::cout << "  Built: " << (test.is_built ? "Yes" : "No") << "\n";
            std::cout << "  Timeout: " << test.metadata.timeout.count() << "ms\n";
            std::cout << "  Parallel Safe: " << (test.metadata.parallel_safe ? "Yes" : "No") << "\n";
            
            if (!test.metadata.description.empty()) {
                std::cout << "  Description: " << test.metadata.description << "\n";
            }
            
            if (!test.metadata.tags.empty()) {
                std::cout << "  Tags: ";
                for (size_t i = 0; i < test.metadata.tags.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << test.metadata.tags[i];
                }
                std::cout << "\n";
            }
            
            if (!test.metadata.dependencies.empty()) {
                std::cout << "  Dependencies: ";
                for (size_t i = 0; i < test.metadata.dependencies.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << test.metadata.dependencies[i];
                }
                std::cout << "\n";
            }
            
            std::cout << "\n";
        } else {
            std::cout << test.name;
            if (!test.is_built) {
                std::cout << " (not built)";
            }
            if (!test.metadata.description.empty()) {
                std::cout << " - " << test.metadata.description;
            }
            std::cout << "\n";
        }
    }
    
    if (!args.quiet) {
        int built_count = 0;
        for (const auto& test : tests) {
            if (test.is_built) built_count++;
        }
        
        std::cout << "\nSummary: " << tests.size() << " tests found, " 
                  << built_count << " built, " << (tests.size() - built_count) << " need building\n";
    }
}



// ========================================
// MAIN EXECUTION LOGIC
// ========================================

int main(int argc, char* argv[]) {
    try {
        // Parse command line arguments
        CommandLineArgs args = parseCommandLine(argc, argv);
        
        // Initialize test discovery
        TestDiscovery discovery(args.test_directory);
        discovery.setDefaultTimeout(std::chrono::milliseconds(args.timeout_seconds * 1000));
        
        // Discover tests
        std::vector<TestInfo> tests;
        if (args.filter_pattern.empty()) {
            tests = discovery.discoverTests();
        } else {
            tests = discovery.discoverTests(args.filter_pattern);
        }
        
        // Handle list mode
        if (args.list_tests) {
            listTests(tests, args);
            return 0;
        }
        
        // Check if any tests were found
        if (tests.empty()) {
            if (!args.quiet) {
                std::cout << "No tests found";
                if (!args.filter_pattern.empty()) {
                    std::cout << " matching pattern: " << args.filter_pattern;
                }
                std::cout << std::endl;
            }
            return 0;
        }
        
        // Filter out tests that aren't built
        std::vector<TestInfo> runnable_tests;
        std::vector<TestInfo> unbuilt_tests;
        
        for (const auto& test : tests) {
            if (test.is_built) {
                runnable_tests.push_back(test);
            } else {
                unbuilt_tests.push_back(test);
            }
        }
        
        // Report unbuilt tests
        if (!unbuilt_tests.empty() && !args.quiet) {
            std::cout << "Warning: " << unbuilt_tests.size() << " tests are not built:\n";
            for (const auto& test : unbuilt_tests) {
                std::cout << "  " << test.name << " (missing: " << test.executable_path << ")\n";
            }
            std::cout << "\nRun 'make check' to build all tests.\n\n";
        }
        
        if (runnable_tests.empty()) {
            std::cerr << "Error: No runnable tests found. Run 'make check' to build tests." << std::endl;
            return 1;
        }
        
        // Initialize test executor
        TestExecutor executor;
        executor.setGlobalTimeout(std::chrono::milliseconds(args.timeout_seconds * 1000));
        executor.enableParallelExecution(args.parallel);
        executor.setMaxParallelProcesses(args.max_parallel);
        executor.enableOutputCapture(true);
        
        // Create reporter
        auto reporter = ReporterFactory::createReporter(args.output_format);
        if (!reporter) {
            std::cerr << "Error: Unknown output format: " << args.output_format << std::endl;
            return 1;
        }
        
        // Configure reporter
        if (args.output_format == "console") {
            auto console_reporter = static_cast<ConsoleReporter*>(reporter.get());
            console_reporter->setQuiet(args.quiet);
            console_reporter->setVerbose(args.verbose);
            console_reporter->setShowTestOutput(args.verbose);
        }
        
        // Start reporting
        reporter->reportStart(runnable_tests);
        
        // Execute tests
        std::vector<ExecutionResult> results;
        
        if (args.parallel) {
            // Separate parallel-safe and sequential tests
            std::vector<TestInfo> parallel_tests = discovery.getParallelSafeTests(runnable_tests);
            std::vector<TestInfo> sequential_tests = discovery.getSequentialTests(runnable_tests);
            
            // Run parallel tests first
            if (!parallel_tests.empty()) {
                auto parallel_results = executor.executeTestsParallel(parallel_tests, args.max_parallel);
                for (const auto& result : parallel_results) {
                    results.push_back(result);
                    reporter->reportTestResult(result);
                }
            }
            
            // Run sequential tests
            if (!sequential_tests.empty()) {
                auto sequential_results = executor.executeTests(sequential_tests);
                for (const auto& result : sequential_results) {
                    results.push_back(result);
                    reporter->reportTestResult(result);
                }
            }
        } else {
            // Run all tests sequentially
            for (const auto& test : runnable_tests) {
                reporter->reportTestStart(test.name);
                
                ExecutionResult result = executor.executeTest(test);
                results.push_back(result);
                
                reporter->reportTestResult(result);
                reporter->reportProgress(results.size(), runnable_tests.size());
                
                // Stop on failure if requested
                if (args.stop_on_failure && result.status != ExecutionStatus::SUCCESS) {
                    if (!args.quiet) {
                        std::cout << "\nStopping execution due to test failure.\n";
                    }
                    break;
                }
            }
        }
        
        // Generate summary
        TestSummary summary;
        summary.start_time = std::chrono::system_clock::now() - std::chrono::milliseconds(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - std::chrono::steady_clock::time_point{}
            ).count()
        );
        summary.end_time = std::chrono::system_clock::now();
        summary.calculateFromResults(results);
        
        reporter->reportSummary(summary);
        
        // Performance tracking and reporting
        if (args.track_performance || args.show_performance_report || args.show_detailed_performance ||
            args.show_memory_report || args.show_outliers) {
            
            PerformanceMetrics current_metrics;
            for (const auto& result : results) {
                current_metrics.addTestResult(result);
            }
            
            // Basic performance report
            if (args.show_performance_report) {
                if (!args.quiet) {
                    std::cout << "\n";
                    std::cout << "============================================================\n";
                    std::cout << "PERFORMANCE REPORT\n";
                    std::cout << "============================================================\n";
                }
                current_metrics.generateReport(std::cout);
            }
            
            // Detailed performance metrics
            if (args.show_detailed_performance) {
                if (!args.quiet) {
                    std::cout << "\n";
                    std::cout << "============================================================\n";
                    std::cout << "DETAILED PERFORMANCE METRICS\n";
                    std::cout << "============================================================\n";
                }
                
                auto stats = current_metrics.getStatistics();
                std::cout << "TIMING STATISTICS:\n";
                std::cout << "  Minimum time: " << stats.min_time.count() << "ms\n";
                std::cout << "  Maximum time: " << stats.max_time.count() << "ms\n";
                std::cout << "  Median time: " << stats.median_time.count() << "ms\n";
                std::cout << "  90th percentile: " << stats.p90_time.count() << "ms\n";
                std::cout << "  95th percentile: " << stats.p95_time.count() << "ms\n\n";
                
                if (stats.tests_with_memory_data > 0) {
                    std::cout << "MEMORY STATISTICS:\n";
                    std::cout << "  Tests with memory data: " << stats.tests_with_memory_data << "\n";
                    std::cout << "  Average memory usage: " << (stats.total_memory_kb / stats.tests_with_memory_data) << " KB\n";
                    
                    auto highest_memory = current_metrics.getHighestMemoryTests(5);
                    if (!highest_memory.empty()) {
                        std::cout << "  Highest memory usage:\n";
                        for (const auto& test : highest_memory) {
                            std::cout << "    " << test.test_name << ": " << test.memory_usage << " KB\n";
                        }
                    }
                    std::cout << "\n";
                }
                
                if (stats.tests_with_cpu_data > 0) {
                    std::cout << "CPU STATISTICS:\n";
                    std::cout << "  Tests with CPU data: " << stats.tests_with_cpu_data << "\n";
                    std::cout << "  Average CPU time: " << std::fixed << std::setprecision(3) 
                             << (stats.total_cpu_seconds / stats.tests_with_cpu_data) << "s\n";
                    
                    auto highest_cpu = current_metrics.getHighestCpuTests(5);
                    if (!highest_cpu.empty()) {
                        std::cout << "  Highest CPU usage:\n";
                        for (const auto& test : highest_cpu) {
                            std::cout << "    " << test.test_name << ": " << std::fixed << std::setprecision(3) 
                                     << test.cpu_usage << "s\n";
                        }
                    }
                    std::cout << "\n";
                }
            }
            
            // Memory usage report
            if (args.show_memory_report) {
                if (!args.quiet) {
                    std::cout << "\n";
                    std::cout << "============================================================\n";
                    std::cout << "MEMORY USAGE REPORT\n";
                    std::cout << "============================================================\n";
                }
                
                auto highest_memory = current_metrics.getHighestMemoryTests(10);
                if (!highest_memory.empty()) {
                    std::cout << std::left << std::setw(30) << "Test Name" 
                             << std::setw(15) << "Memory (KB)" 
                             << std::setw(15) << "Time (ms)" << "\n";
                    std::cout << std::string(60, '-') << "\n";
                    
                    for (const auto& test : highest_memory) {
                        std::cout << std::left << std::setw(30) << test.test_name
                                 << std::setw(15) << test.memory_usage
                                 << std::setw(15) << test.duration.count() << "\n";
                    }
                } else {
                    std::cout << "No memory usage data available.\n";
                }
            }
            
            // Performance outliers
            if (args.show_outliers) {
                if (!args.quiet) {
                    std::cout << "\n";
                    std::cout << "============================================================\n";
                    std::cout << "PERFORMANCE OUTLIERS (>" << args.outlier_threshold << "x median)\n";
                    std::cout << "============================================================\n";
                }
                
                auto outliers = current_metrics.getOutliers(args.outlier_threshold);
                if (!outliers.empty()) {
                    auto stats = current_metrics.getStatistics();
                    
                    std::cout << "Median execution time: " << stats.median_time.count() << "ms\n";
                    std::cout << "Outlier threshold: " << (stats.median_time.count() * args.outlier_threshold) << "ms\n\n";
                    
                    std::cout << std::left << std::setw(30) << "Test Name" 
                             << std::setw(15) << "Time (ms)"
                             << std::setw(15) << "Ratio" << "\n";
                    std::cout << std::string(60, '-') << "\n";
                    
                    for (const auto& outlier : outliers) {
                        double ratio = static_cast<double>(outlier.duration.count()) / stats.median_time.count();
                        std::cout << std::left << std::setw(30) << outlier.test_name
                                 << std::setw(15) << outlier.duration.count()
                                 << std::setw(15) << std::fixed << std::setprecision(1) << ratio << "x\n";
                    }
                } else {
                    std::cout << "No performance outliers detected.\n";
                }
            }
            
            // Performance trend tracking
            if (args.track_performance) {
                // Load historical data for comparison
                PerformanceMetrics historical_metrics;
                bool has_historical = historical_metrics.loadFromFile(args.performance_file);
                
                if (has_historical) {
                    auto comparisons = current_metrics.compareWithHistorical(historical_metrics);
                    
                    if (!comparisons.empty() && !args.quiet) {
                        std::cout << "\n";
                        std::cout << "============================================================\n";
                        std::cout << "PERFORMANCE TRENDS\n";
                        std::cout << "============================================================\n";
                        
                        // Separate regressions and improvements
                        std::vector<PerformanceMetrics::PerformanceComparison> regressions;
                        std::vector<PerformanceMetrics::PerformanceComparison> improvements;
                        std::vector<PerformanceMetrics::PerformanceComparison> stable;
                        
                        for (const auto& comp : comparisons) {
                            if (comp.is_regression) {
                                regressions.push_back(comp);
                            } else if (comp.performance_change_percent < -5.0) {
                                improvements.push_back(comp);
                            } else {
                                stable.push_back(comp);
                            }
                        }
                        
                        if (!regressions.empty()) {
                            std::cout << "PERFORMANCE REGRESSIONS:\n";
                            for (const auto& comp : regressions) {
                                std::cout << "  " << comp.test_name << ": +" 
                                         << std::fixed << std::setprecision(1) 
                                         << comp.performance_change_percent << "% slower ("
                                         << comp.current_time.count() << "ms vs " 
                                         << comp.historical_time.count() << "ms)\n";
                            }
                            std::cout << "\n";
                        }
                        
                        if (!improvements.empty()) {
                            std::cout << "PERFORMANCE IMPROVEMENTS:\n";
                            for (const auto& comp : improvements) {
                                std::cout << "  " << comp.test_name << ": " 
                                         << std::fixed << std::setprecision(1) 
                                         << comp.performance_change_percent << "% faster ("
                                         << comp.current_time.count() << "ms vs " 
                                         << comp.historical_time.count() << "ms)\n";
                            }
                            std::cout << "\n";
                        }
                        
                        std::cout << "Summary: " << regressions.size() << " regressions, " 
                                 << improvements.size() << " improvements, " 
                                 << stable.size() << " stable\n";
                    }
                }
                
                // Save current performance data
                current_metrics.saveToFile(args.performance_file);
                if (!args.quiet) {
                    std::cout << "\nPerformance data saved to: " << args.performance_file << "\n";
                }
            }
        }
        
        // Return appropriate exit code
        return summary.allTestsPassed() ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}
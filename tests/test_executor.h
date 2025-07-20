/*
 * test_executor.h - Test execution engine for PsyMP3 test harness
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef TEST_EXECUTOR_H
#define TEST_EXECUTOR_H

#include "test_discovery.h"
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

namespace TestFramework {

    // ========================================
    // EXECUTION RESULT STRUCTURES
    // ========================================
    
    /**
     * @brief Enumeration of possible execution results
     */
    enum class ExecutionStatus {
        SUCCESS,        ///< Test completed successfully (exit code 0)
        FAILURE,        ///< Test failed (non-zero exit code)
        TIMEOUT,        ///< Test exceeded time limit
        CRASH,          ///< Test crashed (signal received)
        BUILD_ERROR,    ///< Test executable not found or not executable
        SYSTEM_ERROR    ///< System error during execution
    };
    
    /**
     * @brief Detailed result of test execution
     */
    struct ExecutionResult {
        std::string test_name;                      ///< Name of executed test
        ExecutionStatus status;                     ///< Execution status
        int exit_code;                              ///< Process exit code
        int signal_number;                          ///< Signal that terminated process (if any)
        std::chrono::milliseconds execution_time;   ///< Actual execution time
        std::string stdout_output;                  ///< Captured stdout
        std::string stderr_output;                  ///< Captured stderr
        std::string error_message;                  ///< Human-readable error description
        bool timed_out;                             ///< Whether execution timed out
        
        ExecutionResult() : status(ExecutionStatus::SYSTEM_ERROR), exit_code(-1), 
                           signal_number(0), execution_time(0), timed_out(false) {}
        
        ExecutionResult(const std::string& name) : test_name(name), status(ExecutionStatus::SYSTEM_ERROR), 
                                                  exit_code(-1), signal_number(0), execution_time(0), timed_out(false) {}
    };

    // ========================================
    // PROCESS MANAGEMENT
    // ========================================
    
    /**
     * @brief Information about a running test process
     */
    struct ProcessInfo {
        pid_t pid;                                  ///< Process ID
        std::string test_name;                      ///< Test name
        std::chrono::steady_clock::time_point start_time; ///< When process started
        std::chrono::milliseconds timeout;          ///< Maximum allowed execution time
        int stdout_fd;                              ///< File descriptor for stdout pipe
        int stderr_fd;                              ///< File descriptor for stderr pipe
        bool is_running;                            ///< Whether process is still running
        
        ProcessInfo() : pid(-1), timeout(30000), stdout_fd(-1), stderr_fd(-1), is_running(false) {}
    };

    // ========================================
    // TEST EXECUTION ENGINE
    // ========================================
    
    /**
     * @brief Engine for executing test processes with timeout and output capture
     * 
     * Manages test process lifecycle, implements timeout mechanisms, captures
     * output streams, and handles process termination gracefully.
     */
    class TestExecutor {
    public:
        /**
         * @brief Constructor
         */
        TestExecutor();
        
        /**
         * @brief Destructor - ensures all processes are terminated
         */
        ~TestExecutor();
        
        /**
         * @brief Execute a single test
         * @param test TestInfo containing test details
         * @return ExecutionResult with detailed execution information
         */
        ExecutionResult executeTest(const TestInfo& test);
        
        /**
         * @brief Execute multiple tests sequentially
         * @param tests Vector of tests to execute
         * @return Vector of execution results
         */
        std::vector<ExecutionResult> executeTests(const std::vector<TestInfo>& tests);
        
        /**
         * @brief Execute multiple tests in parallel
         * @param tests Vector of tests to execute
         * @param max_parallel Maximum number of concurrent processes
         * @return Vector of execution results
         */
        std::vector<ExecutionResult> executeTestsParallel(const std::vector<TestInfo>& tests, 
                                                         int max_parallel = 4);
        
        /**
         * @brief Set global timeout for all tests
         * @param timeout Maximum execution time in milliseconds
         */
        void setGlobalTimeout(std::chrono::milliseconds timeout);
        
        /**
         * @brief Enable or disable parallel execution
         * @param enable Whether to allow parallel execution
         */
        void enableParallelExecution(bool enable);
        
        /**
         * @brief Set maximum number of parallel processes
         * @param max_parallel Maximum concurrent processes
         */
        void setMaxParallelProcesses(int max_parallel);
        
        /**
         * @brief Set working directory for test execution
         * @param working_dir Directory to execute tests in
         */
        void setWorkingDirectory(const std::string& working_dir);
        
        /**
         * @brief Set environment variables for test execution
         * @param env_vars Map of environment variable name to value
         */
        void setEnvironmentVariables(const std::map<std::string, std::string>& env_vars);
        
        /**
         * @brief Add environment variable for test execution
         * @param name Variable name
         * @param value Variable value
         */
        void addEnvironmentVariable(const std::string& name, const std::string& value);
        
        /**
         * @brief Set callback for real-time output processing
         * @param callback Function called with test name and output line
         */
        void setOutputCallback(std::function<void(const std::string&, const std::string&)> callback);
        
        /**
         * @brief Enable or disable output capture
         * @param capture Whether to capture stdout/stderr
         */
        void enableOutputCapture(bool capture);
        
        /**
         * @brief Terminate all running processes
         */
        void terminateAll();
        
        /**
         * @brief Check if any tests are currently running
         * @return true if processes are active
         */
        bool hasRunningTests() const;
        
        /**
         * @brief Get count of currently running tests
         * @return Number of active processes
         */
        int getRunningTestCount() const;
        
        /**
         * @brief Get names of currently running tests
         * @return Vector of test names
         */
        std::vector<std::string> getRunningTestNames() const;
        
    private:
        std::chrono::milliseconds m_global_timeout;     ///< Default timeout for all tests
        bool m_parallel_enabled;                        ///< Whether parallel execution is enabled
        int m_max_parallel;                             ///< Maximum concurrent processes
        std::string m_working_directory;                ///< Working directory for execution
        std::map<std::string, std::string> m_env_vars;  ///< Environment variables
        bool m_capture_output;                          ///< Whether to capture output
        std::function<void(const std::string&, const std::string&)> m_output_callback; ///< Output callback
        
        mutable std::mutex m_process_mutex;             ///< Mutex for process list
        std::vector<std::unique_ptr<ProcessInfo>> m_running_processes; ///< Active processes
        std::atomic<bool> m_shutdown_requested;         ///< Whether shutdown is requested
        
        /**
         * @brief Execute a single test process
         * @param test TestInfo for test to execute
         * @return ExecutionResult with execution details
         */
        ExecutionResult executeSingleTest(const TestInfo& test);
        
        /**
         * @brief Spawn a test process
         * @param executable_path Path to test executable
         * @param test_name Name of test for tracking
         * @param timeout Maximum execution time
         * @return ProcessInfo for spawned process, or nullptr on failure
         */
        std::unique_ptr<ProcessInfo> spawnProcess(const std::string& executable_path, 
                                                 const std::string& test_name,
                                                 std::chrono::milliseconds timeout);
        
        /**
         * @brief Wait for process completion with timeout
         * @param process ProcessInfo for process to wait for
         * @return ExecutionResult with completion details
         */
        ExecutionResult waitForProcess(ProcessInfo& process);
        
        /**
         * @brief Terminate a process gracefully
         * @param process ProcessInfo for process to terminate
         * @param force Whether to use SIGKILL immediately
         * @return true if process was terminated successfully
         */
        bool terminateProcess(ProcessInfo& process, bool force = false);
        
        /**
         * @brief Capture output from process pipes
         * @param process ProcessInfo containing pipe file descriptors
         * @param stdout_output String to store stdout content
         * @param stderr_output String to store stderr content
         */
        void captureOutput(ProcessInfo& process, std::string& stdout_output, std::string& stderr_output);
        
        /**
         * @brief Read data from file descriptor non-blocking
         * @param fd File descriptor to read from
         * @param buffer String to append data to
         * @return Number of bytes read
         */
        ssize_t readNonBlocking(int fd, std::string& buffer);
        
        /**
         * @brief Check if process is still running
         * @param pid Process ID to check
         * @return true if process exists and is running
         */
        bool isProcessRunning(pid_t pid) const;
        
        /**
         * @brief Get exit status of completed process
         * @param pid Process ID
         * @param exit_code Output parameter for exit code
         * @param signal_number Output parameter for signal number
         * @return true if status was retrieved successfully
         */
        bool getProcessExitStatus(pid_t pid, int& exit_code, int& signal_number);
        
        /**
         * @brief Clean up completed processes from tracking list
         */
        void cleanupCompletedProcesses();
        
        /**
         * @brief Set up pipes for process output capture
         * @param stdout_pipe Array to store stdout pipe file descriptors
         * @param stderr_pipe Array to store stderr pipe file descriptors
         * @return true if pipes were created successfully
         */
        bool setupOutputPipes(int stdout_pipe[2], int stderr_pipe[2]);
        
        /**
         * @brief Close pipe file descriptors
         * @param pipe_fds Array of pipe file descriptors to close
         */
        void closePipes(int pipe_fds[2]);
        
        /**
         * @brief Convert signal number to human-readable string
         * @param signal_number Signal number
         * @return Signal name and description
         */
        std::string signalToString(int signal_number) const;
        
        /**
         * @brief Build environment array for execve
         * @return Vector of environment strings
         */
        std::vector<std::string> buildEnvironment() const;
        
        /**
         * @brief Worker function for parallel execution
         * @param tests Vector of tests to execute
         * @param start_index Starting index in tests vector
         * @param count Number of tests to execute
         * @param results Vector to store results
         * @param results_mutex Mutex for results vector
         */
        void parallelWorker(const std::vector<TestInfo>& tests, 
                           size_t start_index, 
                           size_t count,
                           std::vector<ExecutionResult>& results,
                           std::mutex& results_mutex);
    };

    // ========================================
    // TIMEOUT MANAGER
    // ========================================
    
    /**
     * @brief Manager for handling test timeouts
     * 
     * Runs in a separate thread to monitor test execution times
     * and terminate processes that exceed their timeout limits.
     */
    class TimeoutManager {
    public:
        /**
         * @brief Constructor
         */
        TimeoutManager();
        
        /**
         * @brief Destructor
         */
        ~TimeoutManager();
        
        /**
         * @brief Start the timeout monitoring thread
         */
        void start();
        
        /**
         * @brief Stop the timeout monitoring thread
         */
        void stop();
        
        /**
         * @brief Register a process for timeout monitoring
         * @param process ProcessInfo to monitor
         */
        void registerProcess(std::shared_ptr<ProcessInfo> process);
        
        /**
         * @brief Unregister a process from timeout monitoring
         * @param pid Process ID to unregister
         */
        void unregisterProcess(pid_t pid);
        
        /**
         * @brief Set callback for timeout events
         * @param callback Function called when process times out
         */
        void setTimeoutCallback(std::function<void(pid_t, const std::string&)> callback);
        
    private:
        std::atomic<bool> m_running;                    ///< Whether manager is running
        std::thread m_monitor_thread;                   ///< Monitoring thread
        std::mutex m_processes_mutex;                   ///< Mutex for process list
        std::condition_variable m_condition;            ///< Condition variable for thread coordination
        std::vector<std::shared_ptr<ProcessInfo>> m_monitored_processes; ///< Processes being monitored
        std::function<void(pid_t, const std::string&)> m_timeout_callback; ///< Timeout callback
        
        /**
         * @brief Main monitoring loop
         */
        void monitorLoop();
        
        /**
         * @brief Check for timed out processes
         */
        void checkTimeouts();
    };

    // ========================================
    // OUTPUT CAPTURE UTILITIES
    // ========================================
    
    /**
     * @brief Utilities for capturing and processing test output
     */
    namespace OutputCapture {
        
        /**
         * @brief Parse test output for structured information
         * @param output Raw output from test
         * @return Map of parsed information (assertions, errors, etc.)
         */
        std::map<std::string, std::string> parseTestOutput(const std::string& output);
        
        /**
         * @brief Extract assertion failures from output
         * @param output Test output to parse
         * @return Vector of assertion failure messages
         */
        std::vector<std::string> extractAssertionFailures(const std::string& output);
        
        /**
         * @brief Extract performance metrics from output
         * @param output Test output to parse
         * @return Map of metric names to values
         */
        std::map<std::string, double> extractPerformanceMetrics(const std::string& output);
        
        /**
         * @brief Filter output for relevant information
         * @param output Raw output
         * @param include_debug Whether to include debug messages
         * @return Filtered output
         */
        std::string filterOutput(const std::string& output, bool include_debug = false);
        
        /**
         * @brief Colorize output for console display
         * @param output Output to colorize
         * @param status Test execution status
         * @return Colorized output with ANSI codes
         */
        std::string colorizeOutput(const std::string& output, ExecutionStatus status);
    }

} // namespace TestFramework

#endif // TEST_EXECUTOR_H
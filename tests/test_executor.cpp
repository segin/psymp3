/*
 * test_executor.cpp - Implementation of test execution engine
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "test_executor.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace TestFramework {

    // ========================================
    // TEST EXECUTOR IMPLEMENTATION
    // ========================================
    
    TestExecutor::TestExecutor() 
        : m_global_timeout(30000), m_parallel_enabled(true), m_max_parallel(4),
          m_working_directory("."), m_capture_output(true), m_shutdown_requested(false) {
    }
    
    TestExecutor::~TestExecutor() {
        terminateAll();
    }
    
    ExecutionResult TestExecutor::executeTest(const TestInfo& test) {
        return executeSingleTest(test);
    }
    
    std::vector<ExecutionResult> TestExecutor::executeTests(const std::vector<TestInfo>& tests) {
        std::vector<ExecutionResult> results;
        results.reserve(tests.size());
        
        for (const TestInfo& test : tests) {
            if (m_shutdown_requested) {
                ExecutionResult result(test.name);
                result.status = ExecutionStatus::SYSTEM_ERROR;
                result.error_message = "Execution cancelled";
                results.push_back(result);
                continue;
            }
            
            ExecutionResult result = executeSingleTest(test);
            results.push_back(result);
        }
        
        return results;
    }
    
    std::vector<ExecutionResult> TestExecutor::executeTestsParallel(const std::vector<TestInfo>& tests, int max_parallel) {
        if (!m_parallel_enabled || max_parallel <= 1) {
            return executeTests(tests);
        }
        
        std::vector<ExecutionResult> results(tests.size());
        std::mutex results_mutex;
        std::vector<std::thread> workers;
        
        int actual_parallel = std::min(max_parallel, static_cast<int>(tests.size()));
        int tests_per_worker = tests.size() / actual_parallel;
        int remaining_tests = tests.size() % actual_parallel;
        
        size_t start_index = 0;
        for (int i = 0; i < actual_parallel; ++i) {
            int count = tests_per_worker + (i < remaining_tests ? 1 : 0);
            
            workers.emplace_back(&TestExecutor::parallelWorker, this,
                               std::cref(tests), start_index, count,
                               std::ref(results), std::ref(results_mutex));
            
            start_index += count;
        }
        
        // Wait for all workers to complete
        for (std::thread& worker : workers) {
            worker.join();
        }
        
        return results;
    }
    
    void TestExecutor::setGlobalTimeout(std::chrono::milliseconds timeout) {
        m_global_timeout = timeout;
    }
    
    void TestExecutor::enableParallelExecution(bool enable) {
        m_parallel_enabled = enable;
    }
    
    void TestExecutor::setMaxParallelProcesses(int max_parallel) {
        m_max_parallel = std::max(1, max_parallel);
    }
    
    void TestExecutor::setWorkingDirectory(const std::string& working_dir) {
        m_working_directory = working_dir;
    }
    
    void TestExecutor::setEnvironmentVariables(const std::map<std::string, std::string>& env_vars) {
        m_env_vars = env_vars;
    }
    
    void TestExecutor::addEnvironmentVariable(const std::string& name, const std::string& value) {
        m_env_vars[name] = value;
    }
    
    void TestExecutor::setOutputCallback(std::function<void(const std::string&, const std::string&)> callback) {
        m_output_callback = callback;
    }
    
    void TestExecutor::enableOutputCapture(bool capture) {
        m_capture_output = capture;
    }
    
    void TestExecutor::terminateAll() {
        m_shutdown_requested = true;
        
        std::lock_guard<std::mutex> lock(m_process_mutex);
        for (auto& process : m_running_processes) {
            if (process && process->is_running) {
                terminateProcess(*process, true);
            }
        }
        m_running_processes.clear();
    }
    
    bool TestExecutor::hasRunningTests() const {
        std::lock_guard<std::mutex> lock(m_process_mutex);
        return !m_running_processes.empty();
    }
    
    int TestExecutor::getRunningTestCount() const {
        std::lock_guard<std::mutex> lock(m_process_mutex);
        return m_running_processes.size();
    }
    
    std::vector<std::string> TestExecutor::getRunningTestNames() const {
        std::lock_guard<std::mutex> lock(m_process_mutex);
        std::vector<std::string> names;
        names.reserve(m_running_processes.size());
        
        for (const auto& process : m_running_processes) {
            if (process) {
                names.push_back(process->test_name);
            }
        }
        
        return names;
    }
    
    // ========================================
    // PRIVATE IMPLEMENTATION METHODS
    // ========================================
    
    ExecutionResult TestExecutor::executeSingleTest(const TestInfo& test) {
        ExecutionResult result(test.name);
        
        // Check if executable exists
        struct stat file_stat;
        if (stat(test.executable_path.c_str(), &file_stat) != 0) {
            result.status = ExecutionStatus::BUILD_ERROR;
            result.error_message = "Test executable not found: " + test.executable_path;
            return result;
        }
        
        // Check if executable is actually executable
        if (!(file_stat.st_mode & S_IXUSR)) {
            result.status = ExecutionStatus::BUILD_ERROR;
            result.error_message = "Test file is not executable: " + test.executable_path;
            return result;
        }
        
        // Determine timeout
        std::chrono::milliseconds timeout = test.metadata.timeout;
        if (timeout.count() <= 0) {
            timeout = m_global_timeout;
        }
        
        // Spawn the process
        auto process = spawnProcess(test.executable_path, test.name, timeout);
        if (!process) {
            result.status = ExecutionStatus::SYSTEM_ERROR;
            result.error_message = "Failed to spawn test process";
            return result;
        }
        
        // Wait for completion
        result = waitForProcess(*process);
        
        // Clean up
        cleanupCompletedProcesses();
        
        return result;
    }
    
    std::unique_ptr<ProcessInfo> TestExecutor::spawnProcess(const std::string& executable_path,
                                                           const std::string& test_name,
                                                           std::chrono::milliseconds timeout) {
        auto process = std::make_unique<ProcessInfo>();
        process->test_name = test_name;
        process->timeout = timeout;
        process->start_time = std::chrono::steady_clock::now();
        
        int stdout_pipe[2] = {-1, -1};
        int stderr_pipe[2] = {-1, -1};
        
        // Set up output pipes if capture is enabled
        if (m_capture_output) {
            if (!setupOutputPipes(stdout_pipe, stderr_pipe)) {
                return nullptr;
            }
        }
        
        // Fork the process
        pid_t pid = fork();
        if (pid == -1) {
            // Fork failed
            if (m_capture_output) {
                closePipes(stdout_pipe);
                closePipes(stderr_pipe);
            }
            return nullptr;
        }
        
        if (pid == 0) {
            // Child process
            
            // Change working directory if specified
            if (!m_working_directory.empty() && m_working_directory != ".") {
                if (chdir(m_working_directory.c_str()) != 0) {
                    std::cerr << "Failed to change directory to: " << m_working_directory << std::endl;
                    _exit(127);
                }
            }
            
            // Set up output redirection
            if (m_capture_output) {
                // Close read ends of pipes
                close(stdout_pipe[0]);
                close(stderr_pipe[0]);
                
                // Redirect stdout and stderr to pipes
                if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1 ||
                    dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
                    _exit(127);
                }
                
                // Close write ends (now duplicated)
                close(stdout_pipe[1]);
                close(stderr_pipe[1]);
            }
            
            // Build environment
            std::vector<std::string> env_strings = buildEnvironment();
            std::vector<char*> env_ptrs;
            env_ptrs.reserve(env_strings.size() + 1);
            
            for (const std::string& env_str : env_strings) {
                env_ptrs.push_back(const_cast<char*>(env_str.c_str()));
            }
            env_ptrs.push_back(nullptr);
            
            // Execute the test
            char* const argv[] = {const_cast<char*>(executable_path.c_str()), nullptr};
            execve(executable_path.c_str(), argv, env_ptrs.data());
            
            // If we get here, exec failed
            std::cerr << "Failed to execute: " << executable_path << std::endl;
            _exit(127);
        }
        
        // Parent process
        process->pid = pid;
        process->is_running = true;
        
        if (m_capture_output) {
            // Close write ends of pipes
            close(stdout_pipe[1]);
            close(stderr_pipe[1]);
            
            // Store read ends
            process->stdout_fd = stdout_pipe[0];
            process->stderr_fd = stderr_pipe[0];
            
            // Make pipes non-blocking
            fcntl(process->stdout_fd, F_SETFL, O_NONBLOCK);
            fcntl(process->stderr_fd, F_SETFL, O_NONBLOCK);
        }
        
        // Add to running processes list
        {
            std::lock_guard<std::mutex> lock(m_process_mutex);
            m_running_processes.push_back(std::move(process));
        }
        
        return std::unique_ptr<ProcessInfo>(m_running_processes.back().get());
    }
    
    ExecutionResult TestExecutor::waitForProcess(ProcessInfo& process) {
        ExecutionResult result(process.test_name);
        result.status = ExecutionStatus::SUCCESS;
        
        auto start_time = std::chrono::steady_clock::now();
        std::string stdout_buffer, stderr_buffer;
        
        while (process.is_running && !m_shutdown_requested) {
            // Check for timeout
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed >= process.timeout) {
                result.timed_out = true;
                result.status = ExecutionStatus::TIMEOUT;
                result.error_message = "Test exceeded timeout of " + 
                                     std::to_string(process.timeout.count()) + "ms";
                
                terminateProcess(process, false);
                break;
            }
            
            // Capture output
            if (m_capture_output) {
                captureOutput(process, stdout_buffer, stderr_buffer);
            }
            
            // Check if process is still running
            int status;
            pid_t wait_result = waitpid(process.pid, &status, WNOHANG);
            
            if (wait_result == process.pid) {
                // Process has completed
                process.is_running = false;
                
                if (WIFEXITED(status)) {
                    result.exit_code = WEXITSTATUS(status);
                    if (result.exit_code != 0) {
                        result.status = ExecutionStatus::FAILURE;
                        result.error_message = "Test failed with exit code " + std::to_string(result.exit_code);
                    }
                } else if (WIFSIGNALED(status)) {
                    result.signal_number = WTERMSIG(status);
                    result.status = ExecutionStatus::CRASH;
                    result.error_message = "Test crashed with signal " + signalToString(result.signal_number);
                }
                
                break;
            } else if (wait_result == -1 && errno != ECHILD) {
                // Error occurred
                result.status = ExecutionStatus::SYSTEM_ERROR;
                result.error_message = "Error waiting for process: " + std::string(strerror(errno));
                break;
            }
            
            // Sleep briefly to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Final output capture
        if (m_capture_output) {
            captureOutput(process, stdout_buffer, stderr_buffer);
            result.stdout_output = stdout_buffer;
            result.stderr_output = stderr_buffer;
        }
        
        // Calculate execution time
        auto end_time = std::chrono::steady_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Close pipe file descriptors
        if (process.stdout_fd != -1) {
            close(process.stdout_fd);
            process.stdout_fd = -1;
        }
        if (process.stderr_fd != -1) {
            close(process.stderr_fd);
            process.stderr_fd = -1;
        }
        
        return result;
    }
    
    bool TestExecutor::terminateProcess(ProcessInfo& process, bool force) {
        if (!process.is_running) {
            return true;
        }
        
        // First try SIGTERM for graceful shutdown
        if (!force) {
            if (kill(process.pid, SIGTERM) == 0) {
                // Wait a bit for graceful shutdown
                for (int i = 0; i < 50; ++i) { // Wait up to 500ms
                    if (!isProcessRunning(process.pid)) {
                        process.is_running = false;
                        return true;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }
        
        // If graceful shutdown failed, use SIGKILL
        if (kill(process.pid, SIGKILL) == 0) {
            // Wait for process to die
            for (int i = 0; i < 100; ++i) { // Wait up to 1 second
                if (!isProcessRunning(process.pid)) {
                    process.is_running = false;
                    return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        return false;
    }
    
    void TestExecutor::captureOutput(ProcessInfo& process, std::string& stdout_output, std::string& stderr_output) {
        if (process.stdout_fd != -1) {
            readNonBlocking(process.stdout_fd, stdout_output);
        }
        if (process.stderr_fd != -1) {
            readNonBlocking(process.stderr_fd, stderr_output);
        }
        
        // Call output callback if set
        if (m_output_callback && (!stdout_output.empty() || !stderr_output.empty())) {
            std::string combined_output = stdout_output;
            if (!stderr_output.empty()) {
                if (!combined_output.empty()) {
                    combined_output += "\n";
                }
                combined_output += stderr_output;
            }
            m_output_callback(process.test_name, combined_output);
        }
    }
    
    ssize_t TestExecutor::readNonBlocking(int fd, std::string& buffer) {
        char temp_buffer[4096];
        ssize_t total_read = 0;
        
        while (true) {
            ssize_t bytes_read = read(fd, temp_buffer, sizeof(temp_buffer) - 1);
            
            if (bytes_read > 0) {
                temp_buffer[bytes_read] = '\0';
                buffer.append(temp_buffer, bytes_read);
                total_read += bytes_read;
            } else if (bytes_read == 0) {
                // EOF
                break;
            } else {
                // Error or would block
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    // Real error
                    break;
                }
                // Would block, no more data available
                break;
            }
        }
        
        return total_read;
    }
    
    bool TestExecutor::isProcessRunning(pid_t pid) const {
        return kill(pid, 0) == 0;
    }
    
    bool TestExecutor::getProcessExitStatus(pid_t pid, int& exit_code, int& signal_number) {
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);
        
        if (result == pid) {
            if (WIFEXITED(status)) {
                exit_code = WEXITSTATUS(status);
                signal_number = 0;
                return true;
            } else if (WIFSIGNALED(status)) {
                exit_code = -1;
                signal_number = WTERMSIG(status);
                return true;
            }
        }
        
        return false;
    }
    
    void TestExecutor::cleanupCompletedProcesses() {
        std::lock_guard<std::mutex> lock(m_process_mutex);
        
        m_running_processes.erase(
            std::remove_if(m_running_processes.begin(), m_running_processes.end(),
                          [](const std::unique_ptr<ProcessInfo>& process) {
                              return !process || !process->is_running;
                          }),
            m_running_processes.end());
    }
    
    bool TestExecutor::setupOutputPipes(int stdout_pipe[2], int stderr_pipe[2]) {
        if (pipe(stdout_pipe) != 0) {
            return false;
        }
        
        if (pipe(stderr_pipe) != 0) {
            closePipes(stdout_pipe);
            return false;
        }
        
        return true;
    }
    
    void TestExecutor::closePipes(int pipe_fds[2]) {
        if (pipe_fds[0] != -1) {
            close(pipe_fds[0]);
            pipe_fds[0] = -1;
        }
        if (pipe_fds[1] != -1) {
            close(pipe_fds[1]);
            pipe_fds[1] = -1;
        }
    }
    
    std::string TestExecutor::signalToString(int signal_number) const {
        switch (signal_number) {
            case SIGTERM: return "SIGTERM (Terminated)";
            case SIGKILL: return "SIGKILL (Killed)";
            case SIGSEGV: return "SIGSEGV (Segmentation fault)";
            case SIGABRT: return "SIGABRT (Aborted)";
            case SIGFPE:  return "SIGFPE (Floating point exception)";
            case SIGILL:  return "SIGILL (Illegal instruction)";
            case SIGBUS:  return "SIGBUS (Bus error)";
            case SIGPIPE: return "SIGPIPE (Broken pipe)";
            default:
                return "Signal " + std::to_string(signal_number);
        }
    }
    
    std::vector<std::string> TestExecutor::buildEnvironment() const {
        std::vector<std::string> env_strings;
        
        // Copy current environment
        for (char** env = ::environ; *env != nullptr; ++env) {
            std::string env_str(*env);
            
            // Check if we have a custom value for this variable
            size_t equals_pos = env_str.find('=');
            if (equals_pos != std::string::npos) {
                std::string var_name = env_str.substr(0, equals_pos);
                auto custom_it = m_env_vars.find(var_name);
                if (custom_it != m_env_vars.end()) {
                    // Use custom value
                    env_strings.push_back(var_name + "=" + custom_it->second);
                    continue;
                }
            }
            
            // Use original value
            env_strings.push_back(env_str);
        }
        
        // Add any new environment variables
        for (const auto& env_pair : m_env_vars) {
            bool found = false;
            for (const std::string& existing : env_strings) {
                if (existing.find(env_pair.first + "=") == 0) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                env_strings.push_back(env_pair.first + "=" + env_pair.second);
            }
        }
        
        return env_strings;
    }
    
    void TestExecutor::parallelWorker(const std::vector<TestInfo>& tests,
                                     size_t start_index,
                                     size_t count,
                                     std::vector<ExecutionResult>& results,
                                     std::mutex& results_mutex) {
        for (size_t i = 0; i < count && !m_shutdown_requested; ++i) {
            size_t test_index = start_index + i;
            if (test_index >= tests.size()) {
                break;
            }
            
            ExecutionResult result = executeSingleTest(tests[test_index]);
            
            {
                std::lock_guard<std::mutex> lock(results_mutex);
                results[test_index] = result;
            }
        }
    }

    // ========================================
    // OUTPUT CAPTURE UTILITIES IMPLEMENTATION
    // ========================================
    
    namespace OutputCapture {
        
        std::map<std::string, std::string> parseTestOutput(const std::string& output) {
            std::map<std::string, std::string> parsed;
            
            // Extract basic information
            if (output.find("PASSED") != std::string::npos) {
                parsed["status"] = "passed";
            } else if (output.find("FAILED") != std::string::npos) {
                parsed["status"] = "failed";
            }
            
            // Extract assertion count
            std::regex assertion_regex(R"((\d+)\s+assertion[s]?\s+(?:passed|failed))");
            std::smatch match;
            if (std::regex_search(output, match, assertion_regex)) {
                parsed["assertions"] = match[1].str();
            }
            
            return parsed;
        }
        
        std::vector<std::string> extractAssertionFailures(const std::string& output) {
            std::vector<std::string> failures;
            
            std::regex failure_regex(R"(ASSERTION FAILED:([^\n]+))");
            std::sregex_iterator iter(output.begin(), output.end(), failure_regex);
            std::sregex_iterator end;
            
            for (; iter != end; ++iter) {
                failures.push_back(iter->str(1));
            }
            
            return failures;
        }
        
        std::map<std::string, double> extractPerformanceMetrics(const std::string& output) {
            std::map<std::string, double> metrics;
            
            // Look for timing information
            std::regex time_regex(R"((\w+):\s*(\d+(?:\.\d+)?)\s*ms)");
            std::sregex_iterator iter(output.begin(), output.end(), time_regex);
            std::sregex_iterator end;
            
            for (; iter != end; ++iter) {
                std::string metric_name = iter->str(1);
                double value = std::stod(iter->str(2));
                metrics[metric_name] = value;
            }
            
            return metrics;
        }
        
        std::string filterOutput(const std::string& output, bool include_debug) {
            std::istringstream iss(output);
            std::ostringstream filtered;
            std::string line;
            
            while (std::getline(iss, line)) {
                // Skip debug lines unless requested
                if (!include_debug && line.find("DEBUG:") != std::string::npos) {
                    continue;
                }
                
                // Include important lines
                if (line.find("PASSED") != std::string::npos ||
                    line.find("FAILED") != std::string::npos ||
                    line.find("ERROR") != std::string::npos ||
                    line.find("ASSERTION") != std::string::npos) {
                    filtered << line << "\n";
                }
            }
            
            return filtered.str();
        }
        
        std::string colorizeOutput(const std::string& output, ExecutionStatus status) {
            std::string colorized = output;
            
            // ANSI color codes
            const std::string RED = "\033[31m";
            const std::string GREEN = "\033[32m";
            const std::string YELLOW = "\033[33m";
            const std::string RESET = "\033[0m";
            
            switch (status) {
                case ExecutionStatus::SUCCESS:
                    // Highlight PASSED in green
                    {
                        std::regex passed_regex(R"(PASSED)");
                        colorized = std::regex_replace(colorized, passed_regex, GREEN + "PASSED" + RESET);
                    }
                    break;
                    
                case ExecutionStatus::FAILURE:
                    // Highlight FAILED in red
                    {
                        std::regex failed_regex(R"(FAILED)");
                        colorized = std::regex_replace(colorized, failed_regex, RED + "FAILED" + RESET);
                    }
                    break;
                    
                case ExecutionStatus::TIMEOUT:
                    // Highlight timeout messages in yellow
                    {
                        std::regex timeout_regex(R"(timeout|TIMEOUT)");
                        colorized = std::regex_replace(colorized, timeout_regex, YELLOW + "$&" + RESET);
                    }
                    break;
                    
                default:
                    break;
            }
            
            return colorized;
        }
    }

} // namespace TestFramework
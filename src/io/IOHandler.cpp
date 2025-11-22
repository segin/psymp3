/*
 * IOHandler.cpp - Base I/O handler interface implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"

// Static member initialization
std::mutex IOHandler::s_memory_mutex;
size_t IOHandler::s_total_memory_usage = 0;
size_t IOHandler::s_max_total_memory = 64 * 1024 * 1024;  // 64MB default
size_t IOHandler::s_max_per_handler_memory = 16 * 1024 * 1024;  // 16MB default
size_t IOHandler::s_active_handlers = 0;
std::chrono::steady_clock::time_point IOHandler::s_last_memory_warning = std::chrono::steady_clock::now();

// IOHandler base class implementation

IOHandler::IOHandler() {
    // Initialize memory tracking for new handler
    std::lock_guard<std::mutex> lock(s_memory_mutex);
    s_active_handlers++;
    m_memory_usage = 0;
    
    // Initialize memory pool manager if this is the first handler
    if (s_active_handlers == 1) {
        MemoryPoolManager::getInstance().initializePools();
    }
    
    Debug::log("memory", "IOHandler::IOHandler() - Created new handler, active handlers: ", s_active_handlers);
}

IOHandler::~IOHandler() {
    // Update global memory tracking
    std::lock_guard<std::mutex> lock(s_memory_mutex);
    s_total_memory_usage -= m_memory_usage;
    s_active_handlers--;
    
    Debug::log("memory", "IOHandler::~IOHandler() - Released ", m_memory_usage, 
              " bytes, total usage: ", s_total_memory_usage, ", active handlers: ", s_active_handlers);
}

size_t IOHandler::read(void* buffer, size_t size, size_t count) {
    // Thread-safe read operation using shared lock (allows concurrent reads)
    std::shared_lock<std::shared_mutex> lock(m_operation_mutex);
    
    return read_unlocked(buffer, size, count);
}

size_t IOHandler::read_unlocked(void* buffer, size_t size, size_t count) {
    // Default implementation returns 0 (no data read) for non-functional state
    // This follows fread-like semantics where 0 indicates EOF or error
    
    // Reset error state
    updateErrorState(0);
    
    if (m_closed.load()) {
        updateErrorState(EBADF);  // Bad file descriptor
        return 0;
    }
    
    if (!buffer) {
        updateErrorState(EINVAL); // Invalid argument
        return 0;
    }
    
    if (size == 0 || count == 0) {
        // Not an error, just nothing to do
        return 0;
    }
    
    if (m_eof.load()) {
        // Already at EOF, not an error
        return 0;
    }
    
    // Mark as EOF since we can't actually read anything in the base implementation
    updateEofState(true);
    return 0;
}

int IOHandler::seek(off_t offset, int whence) {
    // Thread-safe seek operation using exclusive lock
    std::unique_lock<std::shared_mutex> lock(m_operation_mutex);
    
    return seek_unlocked(offset, whence);
}

int IOHandler::seek_unlocked(off_t offset, int whence) {
    // Default implementation returns -1 (failure) for non-functional state
    // Support SEEK_SET, SEEK_CUR, SEEK_END positioning modes
    
    // Reset error state
    updateErrorState(0);
    
    if (m_closed.load()) {
        updateErrorState(EBADF);  // Bad file descriptor
        return -1;
    }
    
    // Calculate new position based on whence
    off_t current_pos = m_position.load();
    off_t new_position = current_pos;
    switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position += offset;
            break;
        case SEEK_END:
            // Can't determine end position in base class
            updateErrorState(EINVAL);  // Invalid argument
            return -1;
        default:
            // Invalid whence parameter
            updateErrorState(EINVAL);  // Invalid argument
            return -1;
    }
    
    // Validate and update position atomically
    if (!updatePosition(new_position)) {
        updateErrorState(EINVAL);  // Invalid argument
        return -1;
    }
    
    // Clear EOF if we've moved away from the end
    updateEofState(false);
    
    return 0;
}

off_t IOHandler::tell() {
    // Thread-safe tell operation using shared lock
    std::shared_lock<std::shared_mutex> lock(m_operation_mutex);
    
    return tell_unlocked();
}

off_t IOHandler::tell_unlocked() {
    // Return current byte offset with off_t for large file support
    // Return -1 on failure (e.g., if closed)
    
    // Reset error state
    updateErrorState(0);
    
    if (m_closed.load()) {
        updateErrorState(EBADF);  // Bad file descriptor
        return -1;
    }
    
    return m_position.load();
}

int IOHandler::close() {
    // Thread-safe close operation using exclusive lock
    std::unique_lock<std::shared_mutex> lock(m_operation_mutex);
    
    return close_unlocked();
}

int IOHandler::close_unlocked() {
    // Resource cleanup with standard return codes
    // 0 on success, non-zero on failure
    
    // Reset error state
    updateErrorState(0);
    
    if (m_closed.load()) {
        // Already closed, not an error
        return 0;
    }
    
    updateClosedState(true);
    updateEofState(true);
    return 0;
}

bool IOHandler::eof() {
    // Return boolean end-of-stream condition
    // True if at end of stream or closed, false otherwise
    // Thread-safe using atomic loads
    return m_closed.load() || m_eof.load();
}

off_t IOHandler::getFileSize() {
    // Return total size in bytes or -1 if unknown
    // Base implementation doesn't know the size, so return -1
    return -1;
}

int IOHandler::getLastError() const {
    // Return the last error code (0 = no error)
    // Thread-safe using atomic load
    return m_error.load();
}

// Cross-platform utility methods

std::string IOHandler::normalizePath(const std::string& path) {
    if (path.empty()) {
        return path;
    }
    
    std::string normalized = path;
    
#ifdef _WIN32
    // On Windows, convert forward slashes to backslashes
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    
    // Handle UNC paths (\\server\share) - don't normalize the leading slashes
    if (normalized.length() >= 2 && normalized[0] == '\\' && normalized[1] == '\\') {
        // UNC path - leave as is
        return normalized;
    }
    
    // Handle drive letters (C:\path) - ensure proper format
    if (normalized.length() >= 2 && normalized[1] == ':') {
        // Drive letter path - ensure uppercase drive letter
        normalized[0] = std::toupper(normalized[0]);
    }
#else
    // On Unix/Linux, convert backslashes to forward slashes
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    
    // Remove duplicate slashes (but preserve leading slash for absolute paths)
    std::string result;
    bool last_was_slash = false;
    bool is_first_char = true;
    
    for (char c : normalized) {
        if (c == '/') {
            if (!last_was_slash || is_first_char) {
                result += c;
                last_was_slash = true;
            }
        } else {
            result += c;
            last_was_slash = false;
        }
        is_first_char = false;
    }
    
    normalized = result;
#endif
    
    return normalized;
}

char IOHandler::getPathSeparator() {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

std::string IOHandler::getErrorMessage(int error_code, const std::string& context) {
    std::string message;
    
    if (!context.empty()) {
        message = context + ": ";
    }
    
    // Get standard error message
    const char* error_str = strerror(error_code);
    if (error_str) {
        message += error_str;
    } else {
        message += "Unknown error " + std::to_string(error_code);
    }
    
#ifdef _WIN32
    // On Windows, also try to get Windows-specific error message
    if (error_code != 0) {
        DWORD win_error = GetLastError();
        if (win_error != 0) {
            LPSTR messageBuffer = nullptr;
            size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                       NULL, win_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
            if (size > 0 && messageBuffer) {
                std::string win_msg(messageBuffer, size);
                // Remove trailing newlines
                while (!win_msg.empty() && (win_msg.back() == '\n' || win_msg.back() == '\r')) {
                    win_msg.pop_back();
                }
                if (!win_msg.empty()) {
                    message += " (Windows: " + win_msg + ")";
                }
                LocalFree(messageBuffer);
            }
        }
    }
#endif
    
    return message;
}

bool IOHandler::isRecoverableError(int error_code) {
    switch (error_code) {
        // Temporary I/O errors that might be recoverable
        case EIO:           // I/O error
        case EAGAIN:        // Resource temporarily unavailable
        case EINTR:         // Interrupted system call
        case ENOMEM:        // Out of memory (might be temporary)
        case ENOSPC:        // No space left on device (might be temporary)
            return true;
            
        // Network-related errors that might be recoverable
#ifdef ETIMEDOUT
        case ETIMEDOUT:     // Connection timed out
            return true;
#endif
#ifdef ECONNRESET
        case ECONNRESET:    // Connection reset by peer
            return true;
#endif
#ifdef ECONNABORTED
        case ECONNABORTED:  // Connection aborted
            return true;
#endif
#ifdef ENETDOWN
        case ENETDOWN:      // Network is down
            return true;
#endif
#ifdef ENETUNREACH
        case ENETUNREACH:   // Network is unreachable
            return true;
#endif
#ifdef EHOSTDOWN
        case EHOSTDOWN:     // Host is down
            return true;
#endif
#ifdef EHOSTUNREACH
        case EHOSTUNREACH:  // Host is unreachable
            return true;
#endif
            
        // Permanent errors that are not recoverable
        case ENOENT:        // No such file or directory
        case EACCES:        // Permission denied
        case EISDIR:        // Is a directory
        case ENOTDIR:       // Not a directory
        case EBADF:         // Bad file descriptor
        case EINVAL:        // Invalid argument
        case EROFS:         // Read-only file system
        case ELOOP:         // Too many symbolic links
        case ENAMETOOLONG:  // File name too long
        case EOVERFLOW:     // Value too large for defined data type
        default:
            return false;
    }
}

filesize_t IOHandler::getMaxFileSize() {
    // Return maximum file size supported on current platform
#ifdef _WIN32
    // Windows with MinGW: Use _off64_t maximum for large file support
    return 0x7FFFFFFFFFFFFFFFLL;  // Maximum signed 64-bit value
#else
    // Unix/Linux: Use native off_t maximum
    if (sizeof(off_t) == 8) {
        return 0x7FFFFFFFFFFFFFFFLL;  // Maximum signed 64-bit value
    } else {
        return 0x7FFFFFFF;  // Maximum signed 32-bit value
    }
#endif
}

std::map<std::string, size_t> IOHandler::getMemoryStats() {
    std::lock_guard<std::mutex> lock(s_memory_mutex);
    
    return getMemoryStats_unlocked();
}

std::map<std::string, size_t> IOHandler::getMemoryStats_unlocked() {
    std::map<std::string, size_t> stats;
    stats["total_memory_usage"] = s_total_memory_usage;
    stats["max_total_memory"] = s_max_total_memory;
    stats["max_per_handler_memory"] = s_max_per_handler_memory;
    stats["active_handlers"] = s_active_handlers;
    
    if (s_max_total_memory > 0) {
        stats["memory_usage_percent"] = (s_total_memory_usage * 100) / s_max_total_memory;
    } else {
        stats["memory_usage_percent"] = 0;
    }
    
    // Get memory pool manager stats
    auto pool_manager_stats = MemoryPoolManager::getInstance().getMemoryStats();
    for (const auto& stat : pool_manager_stats) {
        stats["memory_pool_" + stat.first] = stat.second;
    }
    
    // Get legacy buffer pool stats
    auto pool_stats = IOBufferPool::getInstance().getStats();
    for (const auto& stat : pool_stats) {
        stats["legacy_pool_" + stat.first] = stat.second;
    }
    
    // Get memory tracker stats
    auto tracker_stats = MemoryTracker::getInstance().getStats();
    stats["system_total_memory"] = tracker_stats.total_physical_memory;
    stats["system_available_memory"] = tracker_stats.available_physical_memory;
    stats["process_memory_usage"] = tracker_stats.process_memory_usage;
    stats["process_peak_memory"] = tracker_stats.peak_memory_usage;
    
    return stats;
}

void IOHandler::setMemoryLimits(size_t max_total_memory, size_t max_per_handler) {
    std::lock_guard<std::mutex> lock(s_memory_mutex);
    s_max_total_memory = max_total_memory;
    s_max_per_handler_memory = max_per_handler;
    
    Debug::log("memory", "IOHandler::setMemoryLimits() - Set limits: total=", max_total_memory, 
              ", per_handler=", max_per_handler);
    
    // Update memory pool manager limits
    MemoryPoolManager::getInstance().setMemoryLimits(max_total_memory, max_total_memory / 2);
    
    // Update legacy buffer pool limits for backward compatibility
    IOBufferPool::getInstance().setMaxPoolSize(max_total_memory / 4); // 25% of total for pool
}

void IOHandler::updateMemoryUsage(size_t new_usage) {
    std::lock_guard<std::mutex> lock(s_memory_mutex);
    
    updateMemoryUsage_unlocked(new_usage);
}

void IOHandler::updateMemoryUsage_unlocked(size_t new_usage) {
    size_t old_usage = m_memory_usage.exchange(new_usage);
    
    s_total_memory_usage = s_total_memory_usage - old_usage + new_usage;
    
    Debug::log("memory", "IOHandler::updateMemoryUsage_unlocked() - Updated usage from ", old_usage, 
              " to ", new_usage, ", total: ", s_total_memory_usage);
}

bool IOHandler::updatePosition(off_t new_position) {
    // Check for overflow before updating
    static const off_t OFF_T_MAX_VAL = (sizeof(off_t) == 8) ? 0x7FFFFFFFFFFFFFFFLL : 0x7FFFFFFFL;
    
    if (new_position < 0 || new_position > OFF_T_MAX_VAL) {
        Debug::log("io", "IOHandler::updatePosition() - Position overflow/underflow prevented: ", new_position);
        return false;
    }
    
    m_position.store(new_position);
    return true;
}

void IOHandler::updateErrorState(int error_code, const std::string& error_message) {
    m_error.store(error_code);
    
    if (!error_message.empty()) {
        Debug::log("error", "IOHandler::updateErrorState() - Error ", error_code, ": ", error_message);
    }
}

void IOHandler::updateEofState(bool eof_state) {
    m_eof.store(eof_state);
    Debug::log("io", "IOHandler::updateEofState() - EOF state updated to: ", eof_state);
}

void IOHandler::updateClosedState(bool closed_state) {
    m_closed.store(closed_state);
    Debug::log("io", "IOHandler::updateClosedState() - Closed state updated to: ", closed_state);
}

bool IOHandler::checkMemoryLimits(size_t additional_bytes) const {
    std::lock_guard<std::mutex> lock(s_memory_mutex);
    
    return checkMemoryLimits_unlocked(additional_bytes);
}

bool IOHandler::checkMemoryLimits_unlocked(size_t additional_bytes) const {
    // Use MemoryPoolManager to check if allocation is safe
    if (!MemoryPoolManager::getInstance().isSafeToAllocate(additional_bytes, "iohandler")) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - s_last_memory_warning).count();
        
        // Log warning at most once per 5 seconds to avoid log spam
        if (elapsed >= 5) {
            Debug::log("memory", "IOHandler::checkMemoryLimits_unlocked() - Memory allocation of ", 
                      additional_bytes, " bytes not safe according to MemoryPoolManager");
            s_last_memory_warning = now;
            
            // Try to free some memory from buffer pools
            MemoryPoolManager::getInstance().optimizeMemoryUsage();
            IOBufferPool::getInstance().clear();
        }
        return false;
    }
    
    // Legacy checks for backward compatibility
    
    // Check per-handler limit
    if (m_memory_usage + additional_bytes > s_max_per_handler_memory) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - s_last_memory_warning).count();
        
        // Log warning at most once per 5 seconds to avoid log spam
        if (elapsed >= 5) {
            Debug::log("memory", "IOHandler::checkMemoryLimits_unlocked() - Per-handler limit exceeded: ", 
                      m_memory_usage + additional_bytes, " > ", s_max_per_handler_memory);
            s_last_memory_warning = now;
        }
        return false;
    }
    
    // Check total memory limit
    if (s_total_memory_usage + additional_bytes > s_max_total_memory) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - s_last_memory_warning).count();
        
        // Log warning at most once per 5 seconds to avoid log spam
        if (elapsed >= 5) {
            Debug::log("memory", "IOHandler::checkMemoryLimits_unlocked() - Total memory limit exceeded: ", 
                      s_total_memory_usage + additional_bytes, " > ", s_max_total_memory);
            s_last_memory_warning = now;
        }
        return false;
    }
    
    // Check if we're approaching the limit (over 80% usage)
    if (s_total_memory_usage + additional_bytes > s_max_total_memory * 0.8) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - s_last_memory_warning).count();
        
        // Log warning at most once per 30 seconds to avoid log spam
        if (elapsed >= 30) {
            Debug::log("memory", "IOHandler::checkMemoryLimits_unlocked() - Approaching memory limit: ", 
                      s_total_memory_usage + additional_bytes, " / ", s_max_total_memory, 
                      " (", (s_total_memory_usage + additional_bytes) * 100 / s_max_total_memory, "%)");
            s_last_memory_warning = now;
        }
        
        // Still allow allocation but trigger memory optimization
        MemoryPoolManager::getInstance().optimizeMemoryUsage();
    }
    
    return true;
}

void IOHandler::performMemoryOptimization() {
    std::lock_guard<std::mutex> lock(s_memory_mutex);
    
    performMemoryOptimization_unlocked();
}

void IOHandler::performMemoryOptimization_unlocked() {
    Debug::log("memory", "IOHandler::performMemoryOptimization_unlocked() - Starting global memory optimization");
    
    // Get current memory statistics
    auto memory_stats = getMemoryStats_unlocked();
    size_t total_usage = memory_stats["total_memory_usage"];
    size_t max_memory = memory_stats["max_total_memory"];
    
    if (max_memory == 0) {
        return; // No limits set
    }
    
    float usage_percent = static_cast<float>(total_usage) / static_cast<float>(max_memory) * 100.0f;
    
    Debug::log("memory", "IOHandler::performMemoryOptimization_unlocked() - Memory usage: ", usage_percent, 
              "% (", total_usage, " / ", max_memory, " bytes)");
    
    // Use the MemoryPoolManager for centralized memory optimization
    MemoryPoolManager::getInstance().optimizeMemoryUsage();
    
    // Also optimize the legacy buffer pool for backward compatibility
    if (usage_percent > 90.0f) {
        // Critical memory pressure - aggressive optimization
        IOBufferPool::getInstance().clear();
        IOBufferPool::getInstance().setMaxPoolSize(max_memory / 8); // 12.5% of total
        IOBufferPool::getInstance().setMaxBuffersPerSize(2); // Minimal buffering
    } else if (usage_percent > 75.0f) {
        // High memory pressure - moderate optimization
        IOBufferPool::getInstance().optimizeAllocationPatterns();
        IOBufferPool::getInstance().compactMemory();
        IOBufferPool::getInstance().setMaxPoolSize(max_memory / 6); // ~16.7% of total
        IOBufferPool::getInstance().setMaxBuffersPerSize(4); // Reduced buffering
    } else if (usage_percent > 50.0f) {
        // Moderate memory pressure - light optimization
        IOBufferPool::getInstance().defragmentPools();
        IOBufferPool::getInstance().optimizeAllocationPatterns();
    } else {
        // Low memory pressure - maintenance optimization
        IOBufferPool::getInstance().defragmentPools();
    }
    
    // Log final statistics
    auto final_stats = getMemoryStats_unlocked();
    size_t final_usage = final_stats["total_memory_usage"];
    float final_percent = static_cast<float>(final_usage) / static_cast<float>(max_memory) * 100.0f;
    
    Debug::log("memory", "IOHandler::performMemoryOptimization_unlocked() - Optimization complete: ", 
              usage_percent, "% -> ", final_percent, "% (saved ", total_usage - final_usage, " bytes)");
}

bool IOHandler::handleMemoryAllocationFailure(size_t requested_size, const std::string& context) {
    Debug::log("memory", "IOHandler::handleMemoryAllocationFailure() - Failed to allocate ", 
              requested_size, " bytes in context: ", context);
    
    // Set appropriate error code
    m_error = ENOMEM;
    
    // Try to free memory through optimization
    {
        std::lock_guard<std::mutex> lock(s_memory_mutex);
        performMemoryOptimization_unlocked();
    }
    
    // Try to reduce buffer sizes if this is a buffer allocation
    if (context.find("buffer") != std::string::npos) {
        // Reduce buffer sizes by 50%
        size_t reduced_size = requested_size / 2;
        if (reduced_size >= 1024) { // Minimum 1KB buffer
            Debug::log("memory", "IOHandler::handleMemoryAllocationFailure() - Attempting reduced allocation: ", 
                      reduced_size, " bytes");
            
            // Check if reduced allocation would be within limits
            if (checkMemoryLimits(reduced_size)) {
                Debug::log("memory", "IOHandler::handleMemoryAllocationFailure() - Reduced allocation may succeed");
                return true;
            }
        }
    }
    
    // Try emergency memory cleanup
    {
        std::lock_guard<std::mutex> lock(s_memory_mutex);
        
        // Clear all buffer pools
        IOBufferPool::getInstance().clear();
        
        // Request memory cleanup through tracker
        MemoryTracker::getInstance().requestMemoryCleanup(100); // High urgency
        
        Debug::log("memory", "IOHandler::handleMemoryAllocationFailure() - Emergency cleanup completed");
    }
    
    // Check if we now have enough memory
    if (checkMemoryLimits(requested_size)) {
        Debug::log("memory", "IOHandler::handleMemoryAllocationFailure() - Recovery successful after emergency cleanup");
        m_error = 0; // Clear error
        return true;
    }
    
    // Try one more time with a much smaller allocation
    if (context.find("buffer") != std::string::npos) {
        size_t minimal_size = std::max(static_cast<size_t>(1024), requested_size / 8); // 1KB or 1/8 original
        if (checkMemoryLimits(minimal_size)) {
            Debug::log("memory", "IOHandler::handleMemoryAllocationFailure() - Minimal allocation may succeed: ", 
                      minimal_size, " bytes");
            return true;
        }
    }
    
    Debug::log("memory", "IOHandler::handleMemoryAllocationFailure() - Recovery failed, memory allocation not possible");
    return false;
}

bool IOHandler::handleResourceExhaustion(const std::string& resource_type, const std::string& context) {
    Debug::log("resource", "IOHandler::handleResourceExhaustion() - Resource exhausted: ", 
              resource_type, " in context: ", context);
    
    // Set appropriate error code based on resource type
    if (resource_type == "memory") {
        m_error = ENOMEM;
        return handleMemoryAllocationFailure(0, context);
    } else if (resource_type == "file_descriptors") {
        m_error = EMFILE; // Too many open files
        
        // Try to close unused file descriptors
        Debug::log("resource", "IOHandler::handleResourceExhaustion() - Attempting to free file descriptors");
        
        // Force cleanup of any cached file handles
        {
            std::lock_guard<std::mutex> lock(s_memory_mutex);
            performMemoryOptimization_unlocked();
        }
        
        // Wait a bit for system to clean up
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        Debug::log("resource", "IOHandler::handleResourceExhaustion() - File descriptor cleanup attempted");
        return true; // Assume cleanup helped
        
    } else if (resource_type == "disk_space") {
        m_error = ENOSPC; // No space left on device
        
        Debug::log("resource", "IOHandler::handleResourceExhaustion() - Disk space exhausted, cannot recover");
        return false; // Cannot recover from disk space issues
        
    } else if (resource_type == "network_connections") {
        m_error = ECONNABORTED; // Connection aborted
        
        Debug::log("resource", "IOHandler::handleResourceExhaustion() - Network connection limit reached");
        
        // Wait for connections to be released
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        return true; // Assume connections were released
        
    } else {
        m_error = ENOSYS; // Function not implemented (unknown resource type)
        Debug::log("resource", "IOHandler::handleResourceExhaustion() - Unknown resource type: ", resource_type);
        return false;
    }
}

void IOHandler::safeErrorPropagation(int error_code, const std::string& error_message, 
                                    std::function<void()> cleanup_func) {
    Debug::log("error", "IOHandler::safeErrorPropagation() - Propagating error ", error_code, ": ", error_message);
    
    // Set error state
    m_error = error_code;
    
    // Perform cleanup if provided
    if (cleanup_func) {
        try {
            Debug::log("error", "IOHandler::safeErrorPropagation() - Executing cleanup function");
            cleanup_func();
            Debug::log("error", "IOHandler::safeErrorPropagation() - Cleanup completed successfully");
        } catch (const std::exception& e) {
            Debug::log("error", "IOHandler::safeErrorPropagation() - Cleanup function threw exception: ", e.what());
            // Continue with error propagation even if cleanup fails
        } catch (...) {
            Debug::log("error", "IOHandler::safeErrorPropagation() - Cleanup function threw unknown exception");
            // Continue with error propagation even if cleanup fails
        }
    }
    
    // Ensure we're in a safe state for error propagation
    try {
        // Update memory tracking to reflect any cleanup
        updateMemoryUsage(0);
        
        // Mark as closed if this is a fatal error
        if (error_code == ENOMEM || error_code == ENOSPC || error_code == EMFILE) {
            Debug::log("error", "IOHandler::safeErrorPropagation() - Fatal error, marking as closed");
            m_closed = true;
            m_eof = true;
        }
        
    } catch (const std::exception& e) {
        Debug::log("error", "IOHandler::safeErrorPropagation() - Exception during safe state setup: ", e.what());
        // Force closed state to prevent further operations
        m_closed = true;
        m_eof = true;
    } catch (...) {
        Debug::log("error", "IOHandler::safeErrorPropagation() - Unknown exception during safe state setup");
        // Force closed state to prevent further operations
        m_closed = true;
        m_eof = true;
    }
    
    Debug::log("error", "IOHandler::safeErrorPropagation() - Error propagation completed safely");
}
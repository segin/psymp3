/*
 * debug.cpp - Debug output system implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#else
#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_set>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "debug.h"
#endif

// Initialize static members
std::mutex Debug::m_mutex;
bool Debug::m_log_to_file = false;
std::atomic<bool> Debug::m_any_channel_enabled{false};

// Construct-on-first-use accessors, INTENTIONALLY LEAKED: some static
// destructors log during process teardown (CurlLifecycleManager::forceCleanup
// runs from a static object's destructor and logs on the "http" channel).
// A function-local static would be destroyed before those destructors run —
// with channels enabled that was a use-after-destruction segfault on exit —
// so the set and the log sink live for the whole process instead.
std::unordered_set<std::string>& Debug::enabledChannels() {
    static auto* channels = new std::unordered_set<std::string>();
    return *channels;
}

std::ofstream& Debug::logFile() {
    static auto* logfile = new std::ofstream();
    return *logfile;
}

/**
 * @brief Initialises the debug subsystem.
 *
 * Enables a set of named debug channels. If `logfile` is non-empty, all
 * subsequent debug output is appended to that file instead of stdout.
 * Calling this function again replaces the previous configuration entirely.
 *
 * @param logfile  Path to a log file, or an empty string to write to stdout.
 * @param channels List of channel names to enable (e.g., `{"audio", "ogg"}`).
 *                 Specifying `"all"` enables every channel.
 */
void Debug::init(const std::string& logfile, const std::vector<std::string>& channels) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clear previous state
    enabledChannels().clear();
    if (logFile().is_open()) {
        logFile().close();
    }
    m_log_to_file = false;
    
    // Set up new state
    if (!logfile.empty()) {
        logFile().open(logfile, std::ios::out | std::ios::app);
        if (logFile().is_open()) {
            m_log_to_file = true;
        }
    }
    enabledChannels().insert(channels.begin(), channels.end());
    m_any_channel_enabled.store(!enabledChannels().empty(), std::memory_order_relaxed);
}

/**
 * @brief Shuts down the debug subsystem.
 *
 * Clears all enabled channels and closes the log file if one is open.
 * After this call, all `Debug::log` calls are silent.
 */
void Debug::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (logFile().is_open()) {
        logFile().close();
    }
    enabledChannels().clear();
    m_log_to_file = false;
    m_any_channel_enabled.store(false, std::memory_order_relaxed);
}

/**
 * @brief Returns whether the given debug channel is currently enabled.
 *
 * The lookup rules are:
 * - The special channel `"all"` enables everything.
 * - An exact match to a channel name enables that channel.
 * - A sub-channel (e.g., `"flac:frame"`) is enabled if its parent
 *   (`"flac"`) is enabled.
 * - A parent channel is **not** implicitly enabled if only specific
 *   sub-channels are enabled.
 *
 * @param channel The channel identifier to test (e.g., `"audio"`, `"flac:frame"`).
 * @return `true` if the channel is enabled.
 */
bool Debug::isChannelEnabled(const std::string& channel) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // A global "all" channel can enable all logging.
    if (enabledChannels().count("all") > 0) {
        return true;
    }
    
    // Check for exact match
    if (enabledChannels().count(channel) > 0) {
        return true;
    }
    
    // Check if this is a sub-channel (contains ':')
    size_t colon_pos = channel.find(':');
    if (colon_pos != std::string::npos) {
        // This is a sub-channel like "flac:frame"
        std::string parent_channel = channel.substr(0, colon_pos);
        
        // Check if parent channel is enabled (e.g., "flac" enables "flac:frame")
        if (enabledChannels().count(parent_channel) > 0) {
            return true;
        }
    } else {
        // This is a parent channel like "flac"
        // Check if any specific sub-channels are enabled
        // If so, don't show parent channel messages unless parent is explicitly enabled
        bool has_specific_subchannels = false;
        for (const auto& enabled : enabledChannels()) {
            if (enabled.find(channel + ":") == 0) {
                has_specific_subchannels = true;
                break;
            }
        }
        
        // If specific sub-channels are enabled but not the parent, don't show parent messages
        if (has_specific_subchannels) {
            return false;
        }
    }
    
    return false;
}

/**
 * @brief Writes a formatted debug message to the configured output.
 *
 * Prepends a timestamp (`HH:MM:SS.microseconds`), the channel name, and
 * optionally the source function name and line number. This is the low-level
 * implementation used by the `log()` template methods and `DEBUG_LOG` macros.
 * Thread-safe; acquires `m_mutex` internally.
 *
 * @param channel  Name of the debug channel.
 * @param function Name of the calling function (may be empty).
 * @param line     Source line number (0 means not available).
 * @param message  The formatted message string to write.
 */
void Debug::write(const std::string& channel, const std::string& function, int line, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
    auto timer = std::chrono::system_clock::to_time_t(now);
    // localtime() returns a pointer to a shared static tm and is not thread-safe;
    // this runs from the main, decoder, loader, and audio threads.
    std::tm bt{};
#ifdef _WIN32
    localtime_s(&bt, &timer);
#else
    localtime_r(&timer, &bt);
#endif

    std::stringstream ss;
    ss << std::put_time(&bt, "%H:%M:%S") << '.' << std::dec << std::setfill('0') << std::setw(6) << us.count()
       << " [" << channel << "]";
    
    // Add function and line if provided
    if (!function.empty() && line > 0) {
        ss << " [" << function << ":" << line << "]";
    }
    
    ss << ": " << message;

    if (m_log_to_file && logFile().is_open()) {
        logFile() << ss.str() << '\n';
    } else {
        std::cout << ss.str() << '\n';
    }
}
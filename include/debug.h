/*
 * debug.h - Debug output system header
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_set>
#include <sstream>
#include <atomic>

class Debug {
public:
    static void init(const std::string& logfile, const std::vector<std::string>& channels);
    static void shutdown();

    // Check if a debug channel is enabled (O(1) lookup)
    static bool isChannelEnabled(const std::string& channel);

    // Lock-free fast path: true only while at least one channel is enabled.
    // Lets hot paths (e.g. the real-time audio callback) skip the mutex
    // entirely when logging is off, which is the common case.
    static std::atomic<bool> m_any_channel_enabled;

    // Basic logging without location info
    template<typename... Args>
    static inline void log(const std::string& channel, Args&&... args) {
        if (!m_any_channel_enabled.load(std::memory_order_relaxed)) {
            return;
        }
        if (isChannelEnabled(channel)) {
            std::stringstream ss;
            if constexpr (sizeof...(args) > 0) {
                (ss << ... << args);
            }
            write(channel, "", 0, ss.str());
        }
    }

    // Logging with function and line number
    template<typename... Args>
    static inline void log(const std::string& channel, const char* function, int line, Args&&... args) {
        if (!m_any_channel_enabled.load(std::memory_order_relaxed)) {
            return;
        }
        if (isChannelEnabled(channel)) {
            std::stringstream ss;
            if constexpr (sizeof...(args) > 0) {
                (ss << ... << args);
            }
            write(channel, function, line, ss.str());
        }
    }

private:
    static void write(const std::string& channel, const std::string& function, int line, const std::string& message);

    static std::ofstream m_logfile;
    static std::mutex m_mutex;
    // Construct-on-first-use: Debug::log() (via isChannelEnabled) can run from
    // other translation units' static initializers (e.g. truetype/HTTP
    // lifecycle managers) before a plain static member's constructor would have
    // run — unspecified cross-TU init order, which crashed on the unordered_set.
    static std::unordered_set<std::string>& enabledChannels();
    static bool m_log_to_file;
};

// Convenience macro for logging with location info
#define DEBUG_LOG(channel, ...) Debug::log(channel, __FUNCTION__, __LINE__, __VA_ARGS__)

// Lazy evaluation macro - checks if channel is enabled before evaluating arguments
// This prevents string formatting overhead when logging is disabled (Requirements 3.1, 3.3)
#define DEBUG_LOG_LAZY(channel, ...) \
    do { \
        if (Debug::isChannelEnabled(channel)) { \
            Debug::log(channel, __VA_ARGS__); \
        } \
    } while(0)

#endif // DEBUG_H
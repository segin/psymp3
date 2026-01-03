/*
 * debug.cpp - Debug output system implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// Initialize static members
std::ofstream Debug::m_logfile;
std::mutex Debug::m_mutex;
std::unordered_set<std::string> Debug::m_enabled_channels;
bool Debug::m_log_to_file = false;

void Debug::init(const std::string& logfile, const std::vector<std::string>& channels) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clear previous state
    m_enabled_channels.clear();
    if (m_logfile.is_open()) {
        m_logfile.close();
    }
    m_log_to_file = false;
    
    // Set up new state
    if (!logfile.empty()) {
        m_logfile.open(logfile, std::ios::out | std::ios::app);
        if (m_logfile.is_open()) {
            m_log_to_file = true;
        }
    }
    m_enabled_channels.insert(channels.begin(), channels.end());
}

void Debug::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logfile.is_open()) {
        m_logfile.close();
    }
    m_enabled_channels.clear();
    m_log_to_file = false;
}

bool Debug::isChannelEnabled(const std::string& channel) {
    // A global "all" channel can enable all logging.
    if (m_enabled_channels.count("all") > 0) {
        return true;
    }
    
    // Check for exact match
    if (m_enabled_channels.count(channel) > 0) {
        return true;
    }
    
    // Check if this is a sub-channel (contains ':')
    size_t colon_pos = channel.find(':');
    if (colon_pos != std::string::npos) {
        // This is a sub-channel like "flac:frame"
        std::string parent_channel = channel.substr(0, colon_pos);
        
        // Check if parent channel is enabled (e.g., "flac" enables "flac:frame")
        if (m_enabled_channels.count(parent_channel) > 0) {
            return true;
        }
    } else {
        // This is a parent channel like "flac"
        // Check if any specific sub-channels are enabled
        // If so, don't show parent channel messages unless parent is explicitly enabled
        bool has_specific_subchannels = false;
        for (const auto& enabled : m_enabled_channels) {
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

void Debug::write(const std::string& channel, const std::string& function, int line, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);

    std::stringstream ss;
    ss << std::put_time(&bt, "%H:%M:%S") << '.' << std::dec << std::setfill('0') << std::setw(6) << us.count()
       << " [" << channel << "]";
    
    // Add function and line if provided
    if (!function.empty() && line > 0) {
        ss << " [" << function << ":" << line << "]";
    }
    
    ss << ": " << message;

    if (m_log_to_file && m_logfile.is_open()) {
        m_logfile << ss.str() << std::endl;
    } else {
        std::cout << ss.str() << std::endl;
    }
}
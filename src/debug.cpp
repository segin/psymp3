/*
 * debug.cpp - Debug output system implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "debug.h"
#include <chrono>
#include <iomanip>
#include <iostream>

// Initialize static members
std::ofstream Debug::m_logfile;
std::mutex Debug::m_mutex;
std::unordered_set<std::string> Debug::m_enabled_channels;
bool Debug::m_log_to_file = false;

void Debug::init(const std::string& logfile, const std::vector<std::string>& channels) {
    std::lock_guard<std::mutex> lock(m_mutex);
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
}

bool Debug::isChannelEnabled(const std::string& channel) {
    // A global "all" channel can enable all logging.
    return m_enabled_channels.count("all") > 0 || m_enabled_channels.count(channel) > 0;
}

void Debug::write(const std::string& channel, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);

    std::stringstream ss;
    ss << std::put_time(&bt, "%H:%M:%S") << '.' << std::dec << std::setfill('0') << std::setw(6) << us.count()
       << " [" << channel << "]: " << message;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_log_to_file && m_logfile.is_open()) {
        m_logfile << ss.str() << std::endl;
    } else {
        std::cout << ss.str() << std::endl;
    }
}
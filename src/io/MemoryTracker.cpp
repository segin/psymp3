/*
 * MemoryTracker.cpp - Memory usage tracking and pressure monitoring
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace IO {

MemoryTracker& MemoryTracker::getInstance() {
    static MemoryTracker instance;
    return instance;
}

MemoryTracker::MemoryTracker() 
    : m_memory_pressure_level(0)
    , m_next_callback_id(1)
    , m_auto_tracking_enabled(false)
    , m_auto_tracking_interval_ms(5000)
    , m_cleanup_requested(false)
    , m_cleanup_urgency(0)
    , m_last_cleanup_request(std::chrono::steady_clock::now()) {
    // Initialize memory stats
    update();
}

MemoryTracker::~MemoryTracker() {
    // Stop auto-tracking if active
    stopAutoTracking();
}

void MemoryTracker::update() {
    MemoryStats new_stats{};
    int new_pressure_level = 0;
    
    // Set last update time
    new_stats.last_update = std::chrono::steady_clock::now();
    
    // Platform-specific memory usage detection
    #ifdef _WIN32
    // Windows implementation
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        new_stats.total_physical_memory = status.ullTotalPhys;
        new_stats.available_physical_memory = status.ullAvailPhys;
        new_pressure_level = static_cast<int>(100 - (status.ullAvailPhys * 100 / status.ullTotalPhys));
        new_stats.virtual_memory_usage = status.ullTotalVirtual - status.ullAvailVirtual;
    }
    
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        new_stats.process_memory_usage = pmc.WorkingSetSize;
        new_stats.peak_memory_usage = pmc.PeakWorkingSetSize;
    }
    #elif defined(__APPLE__)
    // macOS implementation
    int mib[2];
    size_t length;
    struct vm_statistics64 vm_stats;
    
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    length = sizeof(new_stats.total_physical_memory);
    if (sysctl(mib, 2, &new_stats.total_physical_memory, &length, NULL, 0) == 0) {
        mach_port_t host_port = mach_host_self();
        mach_msg_type_number_t host_size = sizeof(vm_statistics64) / sizeof(integer_t);
        host_statistics64(host_port, HOST_VM_INFO64, (host_info64_t)&vm_stats, &host_size);
        
        new_stats.available_physical_memory = vm_stats.free_count * PAGE_SIZE;
        new_pressure_level = static_cast<int>(100 - (new_stats.available_physical_memory * 100 / new_stats.total_physical_memory));
    }
    
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count) == KERN_SUCCESS) {
        new_stats.process_memory_usage = t_info.resident_size;
        new_stats.virtual_memory_usage = t_info.virtual_size;
    }
    
    // Get peak memory usage (not directly available on macOS)
    new_stats.peak_memory_usage = new_stats.process_memory_usage;
    #else
    // Linux implementation
    FILE* file = fopen("/proc/meminfo", "r");
    if (file) {
        char line[128];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                new_stats.total_physical_memory = static_cast<size_t>(std::stoll(line + 9) * 1024);
            } else if (strncmp(line, "MemAvailable:", 13) == 0) {
                new_stats.available_physical_memory = static_cast<size_t>(std::stoll(line + 13) * 1024);
            }
        }
        fclose(file);
        
        if (new_stats.total_physical_memory > 0) {
            new_pressure_level = static_cast<int>(100 - (new_stats.available_physical_memory * 100 / new_stats.total_physical_memory));
        }
    }
    
    // Get process memory usage
    file = fopen("/proc/self/statm", "r");
    if (file) {
        unsigned long size, resident;
        if (fscanf(file, "%lu %lu", &size, &resident) == 2) {
            new_stats.process_memory_usage = resident * sysconf(_SC_PAGESIZE);
            new_stats.virtual_memory_usage = size * sysconf(_SC_PAGESIZE);
        }
        fclose(file);
    }
    
    // Get peak memory usage
    file = fopen("/proc/self/status", "r");
    if (file) {
        char line[128];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "VmHWM:", 6) == 0) {
                new_stats.peak_memory_usage = static_cast<size_t>(std::stoll(line + 6) * 1024);
                break;
            }
        }
        fclose(file);
    }
    #endif
    
    // Update stats and pressure level
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Store previous stats for trend calculation
        if (m_memory_history.size() >= MEMORY_HISTORY_SIZE) {
            m_memory_history.pop_front();
        }
        
        // Add current memory usage to history
        m_memory_history.push_back({new_stats.last_update, new_stats.process_memory_usage});
        
        // Calculate memory usage trend
        calculateMemoryTrend();
        
        // Update stats
        m_stats = new_stats;
    }
    
    // Only notify if pressure level changed significantly (by at least 5%)
    int old_level = m_memory_pressure_level.load();
    if (std::abs(new_pressure_level - old_level) >= 5) {
        m_memory_pressure_level = new_pressure_level;
        notifyCallbacks();
    }
}

int MemoryTracker::getMemoryPressureLevel() const {
    return m_memory_pressure_level;
}

int MemoryTracker::registerMemoryPressureCallback(std::function<void(int)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int id = m_next_callback_id++;
    m_callbacks.push_back({id, callback});
    
    // Immediately notify with current pressure level
    callback(m_memory_pressure_level);
    
    return id;
}

void MemoryTracker::unregisterMemoryPressureCallback(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(),
                          [id](const CallbackInfo& info) { return info.id == id; });
    
    if (it != m_callbacks.end()) {
        m_callbacks.erase(it);
    }
}

MemoryTracker::MemoryStats MemoryTracker::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void MemoryTracker::startAutoTracking(unsigned int interval_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_auto_tracking_enabled) {
        return; // Already running
    }
    
    m_auto_tracking_interval_ms = interval_ms;
    m_auto_tracking_enabled = true;
    
    // Start tracking thread
    m_auto_tracking_thread = std::thread(&MemoryTracker::autoTrackingThread, this);
}

void MemoryTracker::stopAutoTracking() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_auto_tracking_enabled) {
            return; // Not running
        }
        
        m_auto_tracking_enabled = false;
    }
    
    // Wait for thread to finish
    if (m_auto_tracking_thread.joinable()) {
        m_auto_tracking_thread.join();
    }
}

void MemoryTracker::requestMemoryCleanup(int urgency_level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only allow cleanup requests at a reasonable interval
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - m_last_cleanup_request).count() < 10) {
        return; // Too soon for another cleanup
    }
    
    m_last_cleanup_request = now;
    m_cleanup_requested = true;
    m_cleanup_urgency = urgency_level;
    
    // Notify all callbacks with the urgency level
    for (const auto& info : m_callbacks) {
        try {
            info.callback(urgency_level);
        } catch (...) {
            // Ignore exceptions from callbacks
        }
    }
    
    // Reset cleanup flag after notification
    m_cleanup_requested = false;
}

void MemoryTracker::notifyCallbacks() {
    std::vector<CallbackInfo> callbacks;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callbacks = m_callbacks;
    }
    
    int level = m_memory_pressure_level.load();
    for (const auto& info : callbacks) {
        try {
            info.callback(level);
        } catch (...) {
            // Ignore exceptions from callbacks
        }
    }
}

void MemoryTracker::autoTrackingThread() {
    Debug::log("memory", "MemoryTracker: Auto-tracking thread started");
    
    while (m_auto_tracking_enabled) {
        // Update memory stats
        update();
        
        // Check if we need to request cleanup
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // Request cleanup if memory pressure is high
            if (m_memory_pressure_level > 80 && m_stats.memory_usage_trend > 0.1f) {
                requestMemoryCleanup(m_memory_pressure_level);
            }
        }
        
        // Sleep for the specified interval
        std::this_thread::sleep_for(std::chrono::milliseconds(m_auto_tracking_interval_ms));
    }
    
    Debug::log("memory", "MemoryTracker: Auto-tracking thread stopped");
}

void MemoryTracker::calculateMemoryTrend() {
    if (m_memory_history.size() < 2) {
        m_stats.memory_usage_trend = 0.0f;
        return;
    }
    
    // Calculate trend over the available history
    auto oldest = m_memory_history.front();
    auto newest = m_memory_history.back();
    
    auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
        newest.first - oldest.first).count();
    
    if (time_diff <= 0) {
        m_stats.memory_usage_trend = 0.0f;
        return;
    }
    
    // Calculate memory change rate in MB per second
    float memory_diff_mb = static_cast<float>(newest.second - oldest.second) / (1024 * 1024);
    m_stats.memory_usage_trend = memory_diff_mb / time_diff;
}

} // namespace IO
} // namespace PsyMP3
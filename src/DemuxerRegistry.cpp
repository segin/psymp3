/*
 * DemuxerRegistry.cpp - Registry for demuxer factories implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// Static member definitions
std::map<std::string, DemuxerFactoryFunc> DemuxerRegistry::s_demuxer_factories;

// UnsupportedFormatException implementation
UnsupportedFormatException::UnsupportedFormatException(const std::string& format_id, const std::string& reason)
    : std::runtime_error("Unsupported format '" + format_id + "': " + reason)
    , m_format_id(format_id)
    , m_reason(reason) {
}

// FormatDetectionException implementation
FormatDetectionException::FormatDetectionException(const std::string& file_path, const std::string& reason)
    : std::runtime_error("Format detection failed for '" + file_path + "': " + reason)
    , m_file_path(file_path)
    , m_reason(reason) {
}

// DemuxerRegistry implementation
void DemuxerRegistry::registerDemuxer(const std::string& format_id, DemuxerFactoryFunc factory) {
    if (!isValidFormatId(format_id)) {
        Debug::log("demuxer", "DemuxerRegistry::registerDemuxer invalid format ID: ", format_id);
        return;
    }
    
    if (!isValidFactory(factory)) {
        Debug::log("demuxer", "DemuxerRegistry::registerDemuxer invalid factory function for format: ", format_id);
        return;
    }
    
    Debug::log("demuxer", "DemuxerRegistry::registerDemuxer registering demuxer: ", format_id);
    s_demuxer_factories[format_id] = factory;
}

std::unique_ptr<Demuxer> DemuxerRegistry::createDemuxer(const std::string& format_id, std::unique_ptr<IOHandler> handler) {
    Debug::log("demuxer", "DemuxerRegistry::createDemuxer attempting to create demuxer for format: ", format_id);
    
    if (!handler) {
        Debug::log("demuxer", "DemuxerRegistry::createDemuxer null IOHandler provided for format: ", format_id);
        throw UnsupportedFormatException(format_id, "Invalid IOHandler provided");
    }
    
    auto it = s_demuxer_factories.find(format_id);
    if (it == s_demuxer_factories.end()) {
        Debug::log("demuxer", "DemuxerRegistry::createDemuxer format not found in registry: ", format_id);
        throw UnsupportedFormatException(format_id, "Format not registered or support disabled at compile time");
    }
    
    try {
        auto demuxer = it->second(std::move(handler));
        if (!demuxer) {
            Debug::log("demuxer", "DemuxerRegistry::createDemuxer factory returned null for format: ", format_id);
            throw UnsupportedFormatException(format_id, "Factory function returned null demuxer instance");
        }
        
        Debug::log("demuxer", "DemuxerRegistry::createDemuxer successfully created demuxer for format: ", format_id);
        return demuxer;
        
    } catch (const UnsupportedFormatException&) {
        // Re-throw format exceptions as-is
        throw;
    } catch (const std::exception& e) {
        Debug::log("demuxer", "DemuxerRegistry::createDemuxer exception creating demuxer for format ", format_id, ": ", e.what());
        throw UnsupportedFormatException(format_id, "Failed to create demuxer instance: " + std::string(e.what()));
    }
}

bool DemuxerRegistry::isFormatSupported(const std::string& format_id) {
    return s_demuxer_factories.find(format_id) != s_demuxer_factories.end();
}

std::vector<std::string> DemuxerRegistry::getSupportedFormats() {
    std::vector<std::string> formats;
    formats.reserve(s_demuxer_factories.size());
    
    for (const auto& [format_id, factory] : s_demuxer_factories) {
        formats.push_back(format_id);
    }
    
    return formats;
}

bool DemuxerRegistry::unregisterDemuxer(const std::string& format_id) {
    auto it = s_demuxer_factories.find(format_id);
    if (it != s_demuxer_factories.end()) {
        Debug::log("demuxer", "DemuxerRegistry::unregisterDemuxer removing demuxer: ", format_id);
        s_demuxer_factories.erase(it);
        return true;
    }
    return false;
}

void DemuxerRegistry::clearRegistry() {
    Debug::log("demuxer", "DemuxerRegistry::clearRegistry clearing all registered demuxers");
    s_demuxer_factories.clear();
}

size_t DemuxerRegistry::getRegisteredDemuxerCount() {
    return s_demuxer_factories.size();
}

bool DemuxerRegistry::isInitialized() {
    return !s_demuxer_factories.empty();
}

bool DemuxerRegistry::isValidFormatId(const std::string& format_id) {
    if (format_id.empty()) {
        return false;
    }
    
    // Check for reasonable length (1-32 characters)
    if (format_id.length() > 32) {
        return false;
    }
    
    // Check for valid characters (alphanumeric, underscore, hyphen)
    for (char c : format_id) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    
    return true;
}

bool DemuxerRegistry::isValidFactory(const DemuxerFactoryFunc& factory) {
    // Check if the function object is valid (not empty)
    return static_cast<bool>(factory);
}
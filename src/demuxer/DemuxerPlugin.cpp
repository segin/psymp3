/*
 * DemuxerPlugin.cpp - Plugin architecture for demuxer extensibility
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Demuxer {

#ifdef __linux__
#include <dlfcn.h>
#include <dirent.h>
#elif defined(_WIN32)
#include <windows.h>
#include <io.h>
#else
#include <dlfcn.h>
#include <dirent.h>
#endif

// Static member initialization
DemuxerPluginManager& DemuxerPluginManager::getInstance() {
    static DemuxerPluginManager instance;
    return instance;
}

DemuxerPluginManager::DemuxerPluginManager() {
    // Initialize default search paths
    m_search_paths = {
        "./plugins",
        "/usr/local/lib/psymp3/plugins",
        "/usr/lib/psymp3/plugins"
    };
    
    // Initialize statistics
    m_stats = {};
    
    Debug::log("plugin", "DemuxerPluginManager: Initialized with default search paths");
}

DemuxerPluginManager::~DemuxerPluginManager() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Unload all plugins
    for (auto& [name, plugin] : m_loaded_plugins) {
        if (plugin && plugin->cleanup_func) {
            try {
                plugin->cleanup_func();
            } catch (...) {
                Debug::log("plugin", "DemuxerPluginManager: Exception during plugin cleanup: ", name);
            }
        }
        
        if (plugin && plugin->library_handle) {
            unloadLibrary(plugin->library_handle);
        }
    }
    
    m_loaded_plugins.clear();
    Debug::log("plugin", "DemuxerPluginManager: Cleaned up all plugins");
}

bool DemuxerPluginManager::loadPlugin(const std::string& plugin_path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("plugin", "DemuxerPluginManager::loadPlugin: Loading plugin: ", plugin_path);
    
    // Validate plugin file
    if (!isValidPluginFile(plugin_path)) {
        Debug::log("plugin", "DemuxerPluginManager::loadPlugin: Invalid plugin file: ", plugin_path);
        m_stats.failed_loads++;
        return false;
    }
    
    // Load library
    void* library_handle = loadLibrary(plugin_path);
    if (!library_handle) {
        Debug::log("plugin", "DemuxerPluginManager::loadPlugin: Failed to load library: ", plugin_path);
        m_stats.failed_loads++;
        return false;
    }
    
    // Get plugin initialization function
    auto init_func = reinterpret_cast<DemuxerPluginInitFunc>(
        getSymbol(library_handle, "demuxer_plugin_init"));
    if (!init_func) {
        Debug::log("plugin", "DemuxerPluginManager::loadPlugin: Missing init function: ", plugin_path);
        unloadLibrary(library_handle);
        m_stats.failed_loads++;
        return false;
    }
    
    // Get plugin cleanup function (optional)
    auto cleanup_func = reinterpret_cast<DemuxerPluginCleanupFunc>(
        getSymbol(library_handle, "demuxer_plugin_cleanup"));
    
    // Initialize plugin
    const DemuxerPluginInfo* plugin_info = nullptr;
    const DemuxerPluginFormat* formats = nullptr;
    DemuxerPluginFactoryFunc factory_func = nullptr;
    DemuxerPluginDetectorFunc detector_func = nullptr;
    
    int result = init_func(&plugin_info, &formats, &factory_func, &detector_func);
    if (result != 1 || !plugin_info || !formats || !factory_func) {
        Debug::log("plugin", "DemuxerPluginManager::loadPlugin: Plugin initialization failed: ", plugin_path);
        unloadLibrary(library_handle);
        m_stats.failed_loads++;
        return false;
    }
    
    // Validate ABI compatibility
    if (!validatePluginABI(plugin_info)) {
        Debug::log("plugin", "DemuxerPluginManager::loadPlugin: ABI incompatible: ", plugin_path);
        unloadLibrary(library_handle);
        m_stats.failed_loads++;
        return false;
    }
    
    // Check if plugin already loaded
    std::string plugin_name = plugin_info->plugin_name;
    if (m_loaded_plugins.find(plugin_name) != m_loaded_plugins.end()) {
        Debug::log("plugin", "DemuxerPluginManager::loadPlugin: Plugin already loaded: ", plugin_name);
        unloadLibrary(library_handle);
        return true; // Not an error, just already loaded
    }
    
    // Create plugin handle
    auto plugin_handle = std::make_unique<PluginHandle>();
    plugin_handle->plugin_name = plugin_name;
    plugin_handle->library_path = plugin_path;
    plugin_handle->library_handle = library_handle;
    plugin_handle->plugin_info = *plugin_info;
    plugin_handle->factory_func = factory_func;
    plugin_handle->detector_func = detector_func;
    plugin_handle->cleanup_func = cleanup_func;
    
    // Copy format information
    for (uint32_t i = 0; i < plugin_info->format_count; ++i) {
        plugin_handle->formats.push_back(formats[i]);
    }
    
    // Register plugin formats
    if (!registerPluginFormats(plugin_handle.get())) {
        Debug::log("plugin", "DemuxerPluginManager::loadPlugin: Failed to register formats: ", plugin_name);
        unloadLibrary(library_handle);
        m_stats.failed_loads++;
        return false;
    }
    
    // Store plugin handle
    m_loaded_plugins[plugin_name] = std::move(plugin_handle);
    m_stats.total_plugins_loaded++;
    
    Debug::log("plugin", "DemuxerPluginManager::loadPlugin: Successfully loaded plugin: ", plugin_name);
    return true;
}

bool DemuxerPluginManager::unloadPlugin(const std::string& plugin_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_loaded_plugins.find(plugin_name);
    if (it == m_loaded_plugins.end()) {
        Debug::log("plugin", "DemuxerPluginManager::unloadPlugin: Plugin not found: ", plugin_name);
        return false;
    }
    
    auto& plugin = it->second;
    
    // Unregister formats
    unregisterPluginFormats(plugin.get());
    
    // Call cleanup function
    if (plugin->cleanup_func) {
        try {
            plugin->cleanup_func();
        } catch (...) {
            Debug::log("plugin", "DemuxerPluginManager::unloadPlugin: Exception during cleanup: ", plugin_name);
        }
    }
    
    // Unload library
    if (plugin->library_handle) {
        unloadLibrary(plugin->library_handle);
    }
    
    // Remove from loaded plugins
    m_loaded_plugins.erase(it);
    m_stats.total_plugins_loaded--;
    
    Debug::log("plugin", "DemuxerPluginManager::unloadPlugin: Successfully unloaded plugin: ", plugin_name);
    return true;
}

bool DemuxerPluginManager::registerCustomDemuxer(const std::string& format_id,
                                                DemuxerFactory::DemuxerFactoryFunc factory_func,
                                                const MediaFormat& format_info) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("plugin", "DemuxerPluginManager::registerCustomDemuxer: Registering format: ", format_id);
    
    // Validate parameters
    if (format_id.empty() || !factory_func) {
        Debug::log("plugin", "DemuxerPluginManager::registerCustomDemuxer: Invalid parameters");
        return false;
    }
    
    // Check if format already exists
    if (m_custom_formats.find(format_id) != m_custom_formats.end()) {
        Debug::log("plugin", "DemuxerPluginManager::registerCustomDemuxer: Format already registered: ", format_id);
        return false;
    }
    
    // Register with MediaFactory
    try {
        MediaFactory::registerFormat(format_info, [factory_func](const std::string& uri, const ContentInfo& info) -> std::unique_ptr<Stream> {
            // Create IOHandler for the URI
            std::unique_ptr<IOHandler> handler;
            if (MediaFactory::isHttpUri(uri)) {
                handler = std::make_unique<HTTPIOHandler>(uri);
            } else {
                handler = std::make_unique<FileIOHandler>(uri);
            }
            
            if (!handler) {
                return nullptr;
            }
            
            // Create demuxer using factory
            auto demuxer = factory_func(std::move(handler));
            if (!demuxer) {
                return nullptr;
            }
            
            // Wrap in DemuxedStream
            return std::make_unique<DemuxedStream>(TagLib::String(uri.c_str()));
        });
        
        // Register with DemuxerFactory
        DemuxerFactory::registerDemuxer(format_id, factory_func);
        
        // Store in custom registrations
        m_custom_formats[format_id] = format_info;
        m_custom_factories[format_id] = factory_func;
        m_stats.total_formats_registered++;
        
        Debug::log("plugin", "DemuxerPluginManager::registerCustomDemuxer: Successfully registered: ", format_id);
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("plugin", "DemuxerPluginManager::registerCustomDemuxer: Exception: ", e.what());
        return false;
    }
}

bool DemuxerPluginManager::registerCustomDetector(const std::string& format_id,
                                                 ContentDetector detector_func) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("plugin", "DemuxerPluginManager::registerCustomDetector: Registering detector: ", format_id);
    
    // Validate parameters
    if (format_id.empty() || !detector_func) {
        Debug::log("plugin", "DemuxerPluginManager::registerCustomDetector: Invalid parameters");
        return false;
    }
    
    try {
        // Register with MediaFactory
        MediaFactory::registerContentDetector(format_id, detector_func);
        
        // Store in custom registrations
        m_custom_detectors[format_id] = detector_func;
        m_stats.custom_detectors_registered++;
        
        Debug::log("plugin", "DemuxerPluginManager::registerCustomDetector: Successfully registered: ", format_id);
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("plugin", "DemuxerPluginManager::registerCustomDetector: Exception: ", e.what());
        return false;
    }
}

bool DemuxerPluginManager::unregisterCustomFormat(const std::string& format_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("plugin", "DemuxerPluginManager::unregisterCustomFormat: Unregistering: ", format_id);
    
    bool found = false;
    
    // Remove from custom formats
    if (m_custom_formats.erase(format_id) > 0) {
        found = true;
        m_stats.total_formats_registered--;
    }
    
    // Remove from custom factories
    m_custom_factories.erase(format_id);
    
    // Remove from custom detectors
    if (m_custom_detectors.erase(format_id) > 0) {
        found = true;
        m_stats.custom_detectors_registered--;
    }
    
    // Unregister from MediaFactory
    try {
        MediaFactory::unregisterFormat(format_id);
    } catch (const std::exception& e) {
        Debug::log("plugin", "DemuxerPluginManager::unregisterCustomFormat: Exception: ", e.what());
    }
    
    if (found) {
        Debug::log("plugin", "DemuxerPluginManager::unregisterCustomFormat: Successfully unregistered: ", format_id);
    } else {
        Debug::log("plugin", "DemuxerPluginManager::unregisterCustomFormat: Format not found: ", format_id);
    }
    
    return found;
}

std::vector<DemuxerPluginManager::LoadedPluginInfo> DemuxerPluginManager::getLoadedPlugins() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<LoadedPluginInfo> plugins;
    for (const auto& [name, plugin] : m_loaded_plugins) {
        LoadedPluginInfo info;
        info.plugin_name = plugin->plugin_info.plugin_name;
        info.plugin_version = plugin->plugin_info.plugin_version;
        info.author = plugin->plugin_info.author;
        info.description = plugin->plugin_info.description;
        info.library_path = plugin->library_path;
        info.library_handle = plugin->library_handle;
        
        // Collect supported formats
        for (const auto& format : plugin->formats) {
            info.supported_formats.push_back(format.format_id);
        }
        
        plugins.push_back(info);
    }
    
    return plugins;
}

std::vector<MediaFormat> DemuxerPluginManager::getCustomFormats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<MediaFormat> formats;
    for (const auto& [id, format] : m_custom_formats) {
        formats.push_back(format);
    }
    
    return formats;
}

bool DemuxerPluginManager::isPluginFormat(const std::string& format_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check custom formats
    if (m_custom_formats.find(format_id) != m_custom_formats.end()) {
        return true;
    }
    
    // Check plugin formats
    for (const auto& [name, plugin] : m_loaded_plugins) {
        for (const auto& format : plugin->formats) {
            if (format.format_id == format_id) {
                return true;
            }
        }
    }
    
    return false;
}

int DemuxerPluginManager::scanPluginDirectory(const std::string& plugin_dir) {
    Debug::log("plugin", "DemuxerPluginManager::scanPluginDirectory: Scanning: ", plugin_dir);
    
    int loaded_count = 0;
    
#ifdef __linux__
    DIR* dir = opendir(plugin_dir.c_str());
    if (!dir) {
        Debug::log("plugin", "DemuxerPluginManager::scanPluginDirectory: Cannot open directory: ", plugin_dir);
        return 0;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        if (filename.length() > 3 && filename.substr(filename.length() - 3) == ".so") {
            std::string full_path = plugin_dir + "/" + filename;
            if (loadPlugin(full_path)) {
                loaded_count++;
            }
        }
    }
    
    closedir(dir);
#elif defined(_WIN32)
    std::string search_pattern = plugin_dir + "\\*.dll";
    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFile(search_pattern.c_str(), &find_data);
    
    if (find_handle != INVALID_HANDLE_VALUE) {
        do {
            std::string full_path = plugin_dir + "\\" + find_data.cFileName;
            if (loadPlugin(full_path)) {
                loaded_count++;
            }
        } while (FindNextFile(find_handle, &find_data));
        
        FindClose(find_handle);
    }
#endif
    
    Debug::log("plugin", "DemuxerPluginManager::scanPluginDirectory: Loaded ", loaded_count, " plugins from: ", plugin_dir);
    return loaded_count;
}

void DemuxerPluginManager::setPluginSearchPaths(const std::vector<std::string>& paths) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_search_paths = paths;
    Debug::log("plugin", "DemuxerPluginManager::setPluginSearchPaths: Set ", paths.size(), " search paths");
}

std::vector<std::string> DemuxerPluginManager::getPluginSearchPaths() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_search_paths;
}

int DemuxerPluginManager::autoLoadPlugins() {
    Debug::log("plugin", "DemuxerPluginManager::autoLoadPlugins: Starting auto-load");
    
    int total_loaded = 0;
    auto search_paths = getPluginSearchPaths();
    
    for (const auto& path : search_paths) {
        total_loaded += scanPluginDirectory(path);
    }
    
    Debug::log("plugin", "DemuxerPluginManager::autoLoadPlugins: Total plugins loaded: ", total_loaded);
    return total_loaded;
}

bool DemuxerPluginManager::validatePluginABI(const DemuxerPluginInfo* plugin_info) const {
    if (!plugin_info) {
        return false;
    }
    
    // Check API version compatibility
    if (plugin_info->api_version != DEMUXER_PLUGIN_API_VERSION) {
        Debug::log("plugin", "DemuxerPluginManager::validatePluginABI: API version mismatch: expected ", 
                   DEMUXER_PLUGIN_API_VERSION, ", got ", plugin_info->api_version);
        return false;
    }
    
    // Validate required fields
    if (!plugin_info->plugin_name || !plugin_info->plugin_version) {
        Debug::log("plugin", "DemuxerPluginManager::validatePluginABI: Missing required fields");
        return false;
    }
    
    return true;
}

DemuxerPluginManager::PluginStats DemuxerPluginManager::getPluginStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void* DemuxerPluginManager::loadLibrary(const std::string& path) {
#ifdef __linux__
    return dlopen(path.c_str(), RTLD_LAZY);
#elif defined(_WIN32)
    return LoadLibrary(path.c_str());
#else
    return dlopen(path.c_str(), RTLD_LAZY);
#endif
}

void DemuxerPluginManager::unloadLibrary(void* handle) {
    if (!handle) return;
    
#ifdef __linux__
    dlclose(handle);
#elif defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* DemuxerPluginManager::getSymbol(void* handle, const std::string& symbol_name) {
    if (!handle) return nullptr;
    
#ifdef __linux__
    return dlsym(handle, symbol_name.c_str());
#elif defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), symbol_name.c_str()));
#else
    return dlsym(handle, symbol_name.c_str());
#endif
}

bool DemuxerPluginManager::registerPluginFormats(PluginHandle* plugin) {
    if (!plugin) return false;
    
    try {
        for (const auto& plugin_format : plugin->formats) {
            // Convert plugin format to MediaFormat
            MediaFormat format = convertPluginFormat(plugin_format);
            
            // Wrap plugin factory
            auto factory = wrapPluginFactory(plugin->factory_func);
            
            // Register with MediaFactory
            if (!registerCustomDemuxer(plugin_format.format_id, factory, format)) {
                Debug::log("plugin", "DemuxerPluginManager::registerPluginFormats: Failed to register format: ", 
                           plugin_format.format_id);
                return false;
            }
            
            // Register detector if available
            if (plugin->detector_func) {
                auto detector = wrapPluginDetector(plugin->detector_func);
                registerCustomDetector(plugin_format.format_id, detector);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("plugin", "DemuxerPluginManager::registerPluginFormats: Exception: ", e.what());
        return false;
    }
}

void DemuxerPluginManager::unregisterPluginFormats(PluginHandle* plugin) {
    if (!plugin) return;
    
    for (const auto& plugin_format : plugin->formats) {
        unregisterCustomFormat(plugin_format.format_id);
    }
}

std::string DemuxerPluginManager::getPluginFileName(const std::string& plugin_name) const {
#ifdef __linux__
    return "lib" + plugin_name + ".so";
#elif defined(_WIN32)
    return plugin_name + ".dll";
#else
    return "lib" + plugin_name + ".so";
#endif
}

bool DemuxerPluginManager::isValidPluginFile(const std::string& file_path) const {
    // Check file extension
#ifdef __linux__
    return file_path.length() > 3 && file_path.substr(file_path.length() - 3) == ".so";
#elif defined(_WIN32)
    return file_path.length() > 4 && file_path.substr(file_path.length() - 4) == ".dll";
#else
    return file_path.length() > 3 && file_path.substr(file_path.length() - 3) == ".so";
#endif
}

MediaFormat DemuxerPluginManager::convertPluginFormat(const DemuxerPluginFormat& plugin_format) const {
    MediaFormat format;
    format.format_id = plugin_format.format_id;
    format.display_name = plugin_format.format_name;
    format.description = plugin_format.description;
    format.priority = plugin_format.priority;
    
    // Convert extensions
    for (uint32_t i = 0; i < plugin_format.extension_count; ++i) {
        format.extensions.push_back(plugin_format.extensions[i]);
    }
    
    // Convert magic signature
    if (plugin_format.magic_signature && plugin_format.signature_size > 0) {
        std::string signature(reinterpret_cast<const char*>(plugin_format.magic_signature), 
                             plugin_format.signature_size);
        format.magic_signatures.push_back(signature);
    }
    
    // Convert capabilities
    format.supports_streaming = (plugin_format.capabilities & DEMUXER_CAP_STREAMING) != 0;
    format.supports_seeking = (plugin_format.capabilities & DEMUXER_CAP_SEEKING) != 0;
    
    return format;
}

DemuxerFactory::DemuxerFactoryFunc DemuxerPluginManager::wrapPluginFactory(DemuxerPluginFactoryFunc plugin_factory) const {
    return [plugin_factory](std::unique_ptr<IOHandler> handler) -> std::unique_ptr<Demuxer> {
        if (!plugin_factory || !handler) {
            return nullptr;
        }
        
        try {
            // Call plugin factory (note: this is a simplified wrapper)
            // In a real implementation, you'd need proper ABI conversion
            void* plugin_demuxer = plugin_factory(handler.get());
            if (!plugin_demuxer) {
                return nullptr;
            }
            
            // For now, return nullptr as we'd need a plugin demuxer wrapper
            // This would require implementing a PluginDemuxerWrapper class
            Debug::log("plugin", "DemuxerPluginManager::wrapPluginFactory: Plugin factory called successfully");
            return nullptr;
            
        } catch (...) {
            Debug::log("plugin", "DemuxerPluginManager::wrapPluginFactory: Exception in plugin factory");
            return nullptr;
        }
    };
}

ContentDetector DemuxerPluginManager::wrapPluginDetector(DemuxerPluginDetectorFunc plugin_detector) const {
    return [plugin_detector](std::unique_ptr<IOHandler>& handler) -> std::optional<ContentInfo> {
        if (!plugin_detector || !handler) {
            return std::nullopt;
        }
        
        try {
            // Call plugin detector (note: this is a simplified wrapper)
            // In a real implementation, you'd need proper ABI conversion
            ContentInfo info;
            int result = plugin_detector(handler.get(), &info);
            
            if (result == 1) {
                return info;
            }
            
            return std::nullopt;
            
        } catch (...) {
            Debug::log("plugin", "DemuxerPluginManager::wrapPluginDetector: Exception in plugin detector");
            return std::nullopt;
        }
    };
}

// PluginLoader implementation
PluginLoader::PluginLoader(const std::string& plugin_path) 
    : m_plugin_path(plugin_path), m_loaded(false) {
    
    m_loaded = DemuxerPluginManager::getInstance().loadPlugin(plugin_path);
    if (!m_loaded) {
        m_error_message = "Failed to load plugin: " + plugin_path;
    } else {
        // Extract plugin name from path
        size_t pos = plugin_path.find_last_of("/\\");
        if (pos != std::string::npos) {
            m_plugin_name = plugin_path.substr(pos + 1);
        } else {
            m_plugin_name = plugin_path;
        }
    }
}

PluginLoader::~PluginLoader() {
    if (m_loaded && !m_plugin_name.empty()) {
        DemuxerPluginManager::getInstance().unloadPlugin(m_plugin_name);
    }
}

} // namespace Demuxer
} // namespace PsyMP3

/*
 * CodecRegistry.cpp - Registry for audio codec factories implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

// Static member definitions
std::map<std::string, CodecFactoryFunc> CodecRegistry::s_codec_factories;

// UnsupportedCodecException implementation
UnsupportedCodecException::UnsupportedCodecException(const std::string& codec_name, const std::string& reason)
    : std::runtime_error("Unsupported codec '" + codec_name + "': " + reason)
    , m_codec_name(codec_name)
    , m_reason(reason) {
}

// CodecRegistry implementation
void CodecRegistry::registerCodec(const std::string& codec_name, CodecFactoryFunc factory) {
    if (!isValidCodecName(codec_name)) {
        Debug::log("codec", "CodecRegistry::registerCodec invalid codec name: ", codec_name);
        return;
    }
    
    if (!isValidFactory(factory)) {
        Debug::log("codec", "CodecRegistry::registerCodec invalid factory function for codec: ", codec_name);
        return;
    }
    
    Debug::log("codec", "CodecRegistry::registerCodec registering codec: ", codec_name);
    s_codec_factories[codec_name] = factory;
}

std::unique_ptr<AudioCodec> CodecRegistry::createCodec(const StreamInfo& stream_info) {
    const std::string& codec_name = stream_info.codec_name;
    
    Debug::log("codec", "CodecRegistry::createCodec attempting to create codec: ", codec_name);
    
    auto it = s_codec_factories.find(codec_name);
    if (it == s_codec_factories.end()) {
        Debug::log("codec", "CodecRegistry::createCodec codec not found in registry: ", codec_name);
        throw UnsupportedCodecException(codec_name, "Codec not registered or support disabled at compile time");
    }
    
    try {
        auto codec = it->second(stream_info);
        if (!codec) {
            Debug::log("codec", "CodecRegistry::createCodec factory returned null for codec: ", codec_name);
            throw UnsupportedCodecException(codec_name, "Factory function returned null codec instance");
        }
        
        if (!codec->canDecode(stream_info)) {
            Debug::log("codec", "CodecRegistry::createCodec codec cannot decode stream: ", codec_name);
            throw UnsupportedCodecException(codec_name, "Codec cannot decode the provided stream format");
        }
        
        Debug::log("codec", "CodecRegistry::createCodec successfully created codec: ", codec_name);
        return codec;
        
    } catch (const UnsupportedCodecException&) {
        // Re-throw codec exceptions as-is
        throw;
    } catch (const std::exception& e) {
        Debug::log("codec", "CodecRegistry::createCodec exception creating codec ", codec_name, ": ", e.what());
        throw UnsupportedCodecException(codec_name, "Failed to create codec instance: " + std::string(e.what()));
    }
}

bool CodecRegistry::isCodecSupported(const std::string& codec_name) {
    return s_codec_factories.find(codec_name) != s_codec_factories.end();
}

std::vector<std::string> CodecRegistry::getSupportedCodecs() {
    std::vector<std::string> codecs;
    codecs.reserve(s_codec_factories.size());
    
    for (const auto& [codec_name, factory] : s_codec_factories) {
        codecs.push_back(codec_name);
    }
    
    return codecs;
}

bool CodecRegistry::unregisterCodec(const std::string& codec_name) {
    auto it = s_codec_factories.find(codec_name);
    if (it != s_codec_factories.end()) {
        Debug::log("codec", "CodecRegistry::unregisterCodec removing codec: ", codec_name);
        s_codec_factories.erase(it);
        return true;
    }
    return false;
}

void CodecRegistry::clearRegistry() {
    Debug::log("codec", "CodecRegistry::clearRegistry clearing all registered codecs");
    s_codec_factories.clear();
}

size_t CodecRegistry::getRegisteredCodecCount() {
    return s_codec_factories.size();
}

bool CodecRegistry::isInitialized() {
    return !s_codec_factories.empty();
}

bool CodecRegistry::isValidCodecName(const std::string& codec_name) {
    if (codec_name.empty()) {
        return false;
    }
    
    // Check for reasonable length (1-32 characters)
    if (codec_name.length() > 32) {
        return false;
    }
    
    // Check for valid characters (alphanumeric, underscore, hyphen)
    for (char c : codec_name) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    
    return true;
}

bool CodecRegistry::isValidFactory(const CodecFactoryFunc& factory) {
    // Check if the function object is valid (not empty)
    return static_cast<bool>(factory);
}
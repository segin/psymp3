/*
 * CodecRegistry.h - Registry for audio codec factories
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

#ifndef CODECREGISTRY_H
#define CODECREGISTRY_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Exception thrown when a codec is not supported
 */
class UnsupportedCodecException : public std::runtime_error {
public:
    explicit UnsupportedCodecException(const std::string& codec_name, const std::string& reason);
    
    /**
     * @brief Get the codec name that was not supported
     */
    const std::string& getCodecName() const { return m_codec_name; }
    
    /**
     * @brief Get the reason why the codec is not supported
     */
    const std::string& getReason() const { return m_reason; }

private:
    std::string m_codec_name;
    std::string m_reason;
};

/**
 * @brief Factory function type for creating codec instances
 */
using CodecFactoryFunc = std::function<std::unique_ptr<AudioCodec>(const StreamInfo&)>;

/**
 * @brief Registry for audio codec factories
 * 
 * This registry provides a centralized system for managing audio codec factories.
 * Codecs register themselves at application startup through registration functions,
 * eliminating the need for conditional compilation checks throughout the codebase.
 * 
 * The registry supports:
 * - Dynamic codec registration and lookup
 * - Graceful handling of missing codecs
 * - Introspection of available codecs
 * - Factory-based codec creation
 */
class CodecRegistry {
public:
    /**
     * @brief Register a codec factory function
     * @param codec_name Name of the codec (e.g., "pcm", "mp3", "vorbis")
     * @param factory Factory function that creates codec instances
     */
    static void registerCodec(const std::string& codec_name, CodecFactoryFunc factory);
    
    /**
     * @brief Create a codec instance for the given stream
     * @param stream_info Information about the audio stream
     * @return Unique pointer to appropriate codec
     * @throws UnsupportedCodecException if codec is not supported
     */
    static std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
    
    /**
     * @brief Check if a codec is supported
     * @param codec_name Name of the codec to check
     * @return true if codec is registered and supported
     */
    static bool isCodecSupported(const std::string& codec_name);
    
    /**
     * @brief Get list of all supported codec names
     * @return Vector of codec names that are currently registered
     */
    static std::vector<std::string> getSupportedCodecs();
    
    /**
     * @brief Unregister a codec (for testing or dynamic unloading)
     * @param codec_name Name of the codec to unregister
     * @return true if codec was found and unregistered
     */
    static bool unregisterCodec(const std::string& codec_name);
    
    /**
     * @brief Clear all registered codecs (for testing)
     */
    static void clearRegistry();
    
    /**
     * @brief Get the number of registered codecs
     * @return Number of codecs currently in the registry
     */
    static size_t getRegisteredCodecCount();
    
    /**
     * @brief Check if the registry has been initialized
     * @return true if at least one codec is registered
     */
    static bool isInitialized();

private:
    static std::map<std::string, CodecFactoryFunc> s_codec_factories;
    
    /**
     * @brief Validate codec name for registration
     * @param codec_name Name to validate
     * @return true if name is valid
     */
    static bool isValidCodecName(const std::string& codec_name);
    
    /**
     * @brief Validate factory function
     * @param factory Factory function to validate
     * @return true if factory is valid
     */
    static bool isValidFactory(const CodecFactoryFunc& factory);
};

#endif // CODECREGISTRY_H
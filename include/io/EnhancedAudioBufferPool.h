/*
 * EnhancedAudioBufferPool.h - Enhanced audio buffer pool for memory optimization
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

#ifndef ENHANCEDAUDIOBUFFERPOOL_H
#define ENHANCEDAUDIOBUFFERPOOL_H

#include "io/EnhancedTemplateBufferPool.h"

namespace PsyMP3 {
namespace IO {

/**
 * @brief Enhanced audio buffer pool for memory optimization
 * 
 * This class provides an enhanced buffer pool specifically for audio samples
 * with memory pressure awareness, usage statistics tracking, and adaptive buffer management.
 */
class EnhancedAudioBufferPool : public EnhancedTemplateBufferPool<int16_t> {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the global enhanced audio buffer pool instance
     */
    static EnhancedAudioBufferPool& getInstance();
    
    /**
     * @brief Get a sample buffer with specified minimum and preferred sizes
     * @param min_samples Minimum required number of samples
     * @param preferred_samples Preferred number of samples (0 = use min_samples)
     * @return Buffer vector with at least min_samples capacity
     */
    std::vector<int16_t> getSampleBuffer(size_t min_samples, size_t preferred_samples = 0) {
        return getBuffer(min_samples, preferred_samples);
    }
    
    /**
     * @brief Return a sample buffer to the pool for reuse
     * @param buffer Buffer to return (moved)
     */
    void returnSampleBuffer(std::vector<int16_t>&& buffer) {
        returnBuffer(std::move(buffer));
    }

private:
    EnhancedAudioBufferPool();
    ~EnhancedAudioBufferPool() = default;
    
    // Disable copy constructor and assignment
    EnhancedAudioBufferPool(const EnhancedAudioBufferPool&) = delete;
    EnhancedAudioBufferPool& operator=(const EnhancedAudioBufferPool&) = delete;

    /**
     * @brief Calculate optimal buffer size
     */
    size_t calculateRoundedSize(size_t target_size) const override;
};

} // namespace IO
} // namespace PsyMP3

// Alias for backward compatibility
using AudioBufferPool = PsyMP3::IO::EnhancedAudioBufferPool;

#endif // ENHANCEDAUDIOBUFFERPOOL_H

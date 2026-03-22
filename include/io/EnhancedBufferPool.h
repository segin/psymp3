/*
 * EnhancedBufferPool.h - Enhanced buffer pool for memory optimization
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

#ifndef ENHANCEDBUFFERPOOL_H
#define ENHANCEDBUFFERPOOL_H

#include "io/EnhancedTemplateBufferPool.h"

namespace PsyMP3 {
namespace IO {

/**
 * @brief Enhanced buffer pool for memory optimization
 * 
 * This class provides an enhanced buffer pool with memory pressure awareness,
 * usage statistics tracking, and adaptive buffer management.
 */
class EnhancedBufferPool : public EnhancedTemplateBufferPool<uint8_t> {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the global enhanced buffer pool instance
     */
    static EnhancedBufferPool& getInstance();

private:
    EnhancedBufferPool();
    ~EnhancedBufferPool() = default;
    
    // Disable copy constructor and assignment
    EnhancedBufferPool(const EnhancedBufferPool&) = delete;
    EnhancedBufferPool& operator=(const EnhancedBufferPool&) = delete;
    
    /**
     * @brief Calculate optimal buffer size
     */
    size_t calculateRoundedSize(size_t target_size) const override;
};

} // namespace IO
} // namespace PsyMP3

#endif // ENHANCEDBUFFERPOOL_H

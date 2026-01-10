/*
 * MemoryIOHandler.h - Memory-based IOHandler implementation
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

#ifndef MEMORYIOHANDLER_H
#define MEMORYIOHANDLER_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace IO {

/**
 * @brief Memory-based IOHandler implementation
 *
 * Allows reading from a memory buffer as if it were a file.
 * Supports referencing external buffers (fixed) or dynamic internal buffering.
 */
class MemoryIOHandler : public IOHandler {
public:
    /**
     * @brief Construct from existing data (copy or reference)
     * @param data Pointer to data
     * @param size Size of data
     * @param copy If true, copies data to internal buffer. If false, references external data (must remain valid).
     */
    MemoryIOHandler(const void* data, size_t size, bool copy = true);

    /**
     * @brief Construct empty handler for dynamic writing
     */
    MemoryIOHandler();

    ~MemoryIOHandler() override;

    // IOHandler interface
    size_t read(void* buffer, size_t size, size_t count) override;
    int seek(off_t offset, int whence) override;
    off_t tell() override;
    int close() override;
    bool eof() override;
    off_t getFileSize() override;

    // Extended interface for dynamic feeding

    /**
     * @brief Append data to the internal buffer
     * @param data Data to append
     * @param size Size of data
     * @return Number of bytes written
     */
    size_t write(const void* data, size_t size);

    /**
     * @brief Remove consumed data from the beginning of the buffer
     * @param count Number of bytes to discard
     * Adjusts current position accordingly (moves it back by count).
     */
    void discard(size_t count);

    /**
     * @brief Discard all data that has been read (up to current position)
     */
    void discardRead();

    /**
     * @brief clear all data and reset position
     */
    void clear();

private:
    std::vector<uint8_t> m_buffer;
    const uint8_t* m_external_data = nullptr;
    size_t m_external_size = 0;
    bool m_own_buffer = true;
    size_t m_pos = 0;

    // Tracking for virtual file offset
    size_t m_discarded_bytes = 0; // Bytes discarded from front
};

} // namespace IO
} // namespace PsyMP3

#endif // MEMORYIOHANDLER_H

/*
 * RAIIFileHandle.h - RAII wrapper for FILE* handles
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

#ifndef RAIIFILEHANDLE_H
#define RAIIFILEHANDLE_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace IO {

/**
 * @brief RAII wrapper for FILE* handles with automatic cleanup
 * 
 * This class provides automatic resource management for FILE* handles,
 * ensuring they are properly closed even in exception scenarios.
 * It follows RAII principles for exception-safe file handling.
 */
class RAIIFileHandle {
public:
    /**
     * @brief Default constructor - creates empty handle
     */
    RAIIFileHandle() noexcept;
    
    /**
     * @brief Constructor that takes ownership of a FILE* handle
     * @param file FILE* handle to manage (can be nullptr)
     * @param take_ownership Whether to take ownership and close on destruction
     */
    explicit RAIIFileHandle(FILE* file, bool take_ownership = true) noexcept;
    
    /**
     * @brief Move constructor
     * @param other RAIIFileHandle to move from
     */
    RAIIFileHandle(RAIIFileHandle&& other) noexcept;
    
    /**
     * @brief Move assignment operator
     * @param other RAIIFileHandle to move from
     * @return Reference to this object
     */
    RAIIFileHandle& operator=(RAIIFileHandle&& other) noexcept;
    
    /**
     * @brief Destructor - automatically closes file if owned
     */
    ~RAIIFileHandle() noexcept;
    
    // Delete copy constructor and copy assignment to prevent accidental copying
    RAIIFileHandle(const RAIIFileHandle&) = delete;
    RAIIFileHandle& operator=(const RAIIFileHandle&) = delete;
    
    /**
     * @brief Open a file with RAII management
     * @param filename Path to the file to open
     * @param mode File open mode (e.g., "rb", "wb", etc.)
     * @return true if file was opened successfully, false otherwise
     */
    bool open(const char* filename, const char* mode) noexcept;
    
    /**
     * @brief Open a file with wide character support (Windows)
     * @param filename Wide character path to the file
     * @param mode Wide character file open mode
     * @return true if file was opened successfully, false otherwise
     */
#ifdef _WIN32
    bool open(const wchar_t* filename, const wchar_t* mode) noexcept;
#endif
    
    /**
     * @brief Close the file handle if owned
     * @return 0 on success, EOF on error (same as fclose)
     */
    int close() noexcept;
    
    /**
     * @brief Release ownership of the file handle
     * @return The FILE* handle (caller takes ownership)
     */
    FILE* release() noexcept;
    
    /**
     * @brief Reset with a new file handle
     * @param file New FILE* handle to manage
     * @param take_ownership Whether to take ownership
     */
    void reset(FILE* file = nullptr, bool take_ownership = true) noexcept;
    
    /**
     * @brief Get the raw FILE* handle
     * @return FILE* handle (may be nullptr)
     */
    FILE* get() const noexcept;
    
    /**
     * @brief Check if handle is valid
     * @return true if handle is not nullptr, false otherwise
     */
    bool is_valid() const noexcept;
    
    /**
     * @brief Check if this object owns the handle
     * @return true if handle will be closed on destruction
     */
    bool owns_handle() const noexcept;
    
    /**
     * @brief Boolean conversion operator
     * @return true if handle is valid, false otherwise
     */
    explicit operator bool() const noexcept;
    
    /**
     * @brief Get FILE* for use in C functions
     * @return FILE* handle (may be nullptr)
     */
    operator FILE*() const noexcept;
    
    /**
     * @brief Swap contents with another RAIIFileHandle
     * @param other RAIIFileHandle to swap with
     */
    void swap(RAIIFileHandle& other) noexcept;

private:
    FILE* m_file;           // The managed FILE* handle
    bool m_owns_handle;     // Whether we own the handle and should close it
};

/**
 * @brief Swap function for RAIIFileHandle (ADL)
 * @param lhs First RAIIFileHandle
 * @param rhs Second RAIIFileHandle
 */
void swap(RAIIFileHandle& lhs, RAIIFileHandle& rhs) noexcept;

/**
 * @brief Factory function to create RAIIFileHandle from filename
 * @param filename Path to file to open
 * @param mode File open mode
 * @return RAIIFileHandle managing the opened file
 */
RAIIFileHandle make_file_handle(const char* filename, const char* mode);

#ifdef _WIN32
/**
 * @brief Factory function to create RAIIFileHandle from wide filename (Windows)
 * @param filename Wide character path to file
 * @param mode Wide character file open mode
 * @return RAIIFileHandle managing the opened file
 */
RAIIFileHandle make_file_handle(const wchar_t* filename, const wchar_t* mode);
#endif

} // namespace IO
} // namespace PsyMP3

#endif // RAIIFILEHANDLE_H

/*
 * RAIIFileHandle.cpp - Implementation of RAII wrapper for FILE* handles
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

#include "psymp3.h"

// RAIIFileHandle implementation

RAIIFileHandle::RAIIFileHandle() noexcept : m_file(nullptr), m_owns_handle(false) {
    Debug::log("raii", "RAIIFileHandle::RAIIFileHandle() - Default constructor");
}

RAIIFileHandle::RAIIFileHandle(FILE* file, bool take_ownership) noexcept 
    : m_file(file), m_owns_handle(take_ownership) {
    Debug::log("raii", "RAIIFileHandle::RAIIFileHandle() - Constructor with file handle: ", 
              (file ? "valid" : "null"), ", ownership: ", (take_ownership ? "yes" : "no"));
}

RAIIFileHandle::RAIIFileHandle(RAIIFileHandle&& other) noexcept 
    : m_file(other.m_file), m_owns_handle(other.m_owns_handle) {
    other.m_file = nullptr;
    other.m_owns_handle = false;
    Debug::log("raii", "RAIIFileHandle::RAIIFileHandle() - Move constructor");
}

RAIIFileHandle& RAIIFileHandle::operator=(RAIIFileHandle&& other) noexcept {
    if (this != &other) {
        Debug::log("raii", "RAIIFileHandle::operator=() - Move assignment");
        close(); // Close current handle if owned
        m_file = other.m_file;
        m_owns_handle = other.m_owns_handle;
        other.m_file = nullptr;
        other.m_owns_handle = false;
    }
    return *this;
}

RAIIFileHandle::~RAIIFileHandle() noexcept {
    Debug::log("raii", "RAIIFileHandle::~RAIIFileHandle() - Destructor, owns_handle: ", 
              (m_owns_handle ? "yes" : "no"));
    close();
}

bool RAIIFileHandle::open(const char* filename, const char* mode) noexcept {
    Debug::log("raii", "RAIIFileHandle::open() - Opening file: ", (filename ? filename : "null"), 
              ", mode: ", (mode ? mode : "null"));
    
    close(); // Close any existing handle
    
    if (!filename || !mode) {
        Debug::log("raii", "RAIIFileHandle::open() - Invalid parameters");
        return false;
    }
    
    try {
        m_file = fopen(filename, mode);
        m_owns_handle = (m_file != nullptr);
        
        if (m_file) {
            Debug::log("raii", "RAIIFileHandle::open() - File opened successfully");
        } else {
            Debug::log("raii", "RAIIFileHandle::open() - Failed to open file: ", strerror(errno));
        }
        
        return m_file != nullptr;
    } catch (const std::exception& e) {
        Debug::log("raii", "RAIIFileHandle::open() - Exception during file open: ", e.what());
        m_file = nullptr;
        m_owns_handle = false;
        return false;
    } catch (...) {
        Debug::log("raii", "RAIIFileHandle::open() - Unknown exception during file open");
        m_file = nullptr;
        m_owns_handle = false;
        return false;
    }
}

#ifdef _WIN32
bool RAIIFileHandle::open(const wchar_t* filename, const wchar_t* mode) noexcept {
    Debug::log("raii", "RAIIFileHandle::open() - Opening wide character file");
    
    close(); // Close any existing handle
    
    if (!filename || !mode) {
        Debug::log("raii", "RAIIFileHandle::open() - Invalid wide character parameters");
        return false;
    }
    
    try {
        m_file = _wfopen(filename, mode);
        m_owns_handle = (m_file != nullptr);
        
        if (m_file) {
            Debug::log("raii", "RAIIFileHandle::open() - Wide character file opened successfully");
        } else {
            Debug::log("raii", "RAIIFileHandle::open() - Failed to open wide character file: ", strerror(errno));
        }
        
        return m_file != nullptr;
    } catch (const std::exception& e) {
        Debug::log("raii", "RAIIFileHandle::open() - Exception during wide character file open: ", e.what());
        m_file = nullptr;
        m_owns_handle = false;
        return false;
    } catch (...) {
        Debug::log("raii", "RAIIFileHandle::open() - Unknown exception during wide character file open");
        m_file = nullptr;
        m_owns_handle = false;
        return false;
    }
}
#endif

int RAIIFileHandle::close() noexcept {
    int result = 0;
    
    if (m_file && m_owns_handle) {
        Debug::log("raii", "RAIIFileHandle::close() - Closing owned file handle");
        try {
            result = fclose(m_file);
            if (result == 0) {
                Debug::log("raii", "RAIIFileHandle::close() - File closed successfully");
            } else {
                Debug::log("raii", "RAIIFileHandle::close() - Error closing file: ", strerror(errno));
            }
        } catch (const std::exception& e) {
            Debug::log("raii", "RAIIFileHandle::close() - Exception during file close: ", e.what());
            result = EOF; // Indicate error
        } catch (...) {
            Debug::log("raii", "RAIIFileHandle::close() - Unknown exception during file close");
            result = EOF; // Indicate error
        }
    } else if (m_file) {
        Debug::log("raii", "RAIIFileHandle::close() - File handle not owned, not closing");
    }
    
    m_file = nullptr;
    m_owns_handle = false;
    return result;
}

FILE* RAIIFileHandle::release() noexcept {
    Debug::log("raii", "RAIIFileHandle::release() - Releasing ownership of file handle");
    FILE* file = m_file;
    m_file = nullptr;
    m_owns_handle = false;
    return file;
}

void RAIIFileHandle::reset(FILE* file, bool take_ownership) noexcept {
    Debug::log("raii", "RAIIFileHandle::reset() - Resetting with new file handle: ", 
              (file ? "valid" : "null"), ", ownership: ", (take_ownership ? "yes" : "no"));
    close(); // Close current handle if owned
    m_file = file;
    m_owns_handle = take_ownership;
}

FILE* RAIIFileHandle::get() const noexcept {
    return m_file;
}

bool RAIIFileHandle::is_valid() const noexcept {
    return m_file != nullptr;
}

bool RAIIFileHandle::owns_handle() const noexcept {
    return m_owns_handle;
}

RAIIFileHandle::operator bool() const noexcept {
    return is_valid();
}

RAIIFileHandle::operator FILE*() const noexcept {
    return m_file;
}

void RAIIFileHandle::swap(RAIIFileHandle& other) noexcept {
    Debug::log("raii", "RAIIFileHandle::swap() - Swapping file handles");
    std::swap(m_file, other.m_file);
    std::swap(m_owns_handle, other.m_owns_handle);
}

// Free functions

void swap(RAIIFileHandle& lhs, RAIIFileHandle& rhs) noexcept {
    lhs.swap(rhs);
}

RAIIFileHandle make_file_handle(const char* filename, const char* mode) {
    Debug::log("raii", "make_file_handle() - Creating file handle for: ", 
              (filename ? filename : "null"));
    RAIIFileHandle handle;
    handle.open(filename, mode);
    return handle;
}

#ifdef _WIN32
RAIIFileHandle make_file_handle(const wchar_t* filename, const wchar_t* mode) {
    Debug::log("raii", "make_file_handle() - Creating wide character file handle");
    RAIIFileHandle handle;
    handle.open(filename, mode);
    return handle;
}
#endif
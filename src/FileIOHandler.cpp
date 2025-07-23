/*
 * FileIOHandler.cpp - Implementation for the file I/O handler.
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

/**
 * @brief Constructs a FileIOHandler for a given local file path.
 *
 * This opens the specified file in binary read mode. It handles both
 * standard and wide-character paths for cross-platform compatibility.
 * @param path The file path to open.
 * @throws InvalidMediaException if the file cannot be opened.
 */
FileIOHandler::FileIOHandler(const TagLib::String& path) : m_file_path(path) {
#ifdef _WIN32
    m_file_handle = _wfopen(path.toCWString(), L"rb");
#else
    // Use the raw C string without UTF-8 conversion to handle non-UTF-8 filenames
    // This preserves the original filesystem encoding
    m_file_handle = fopen(path.toCString(false), "rb");
#endif
    if (!m_file_handle) {
        throw InvalidMediaException("FileIOHandler: Could not open file: " + path.to8Bit(false));
    }
    
    // Debug: check file size immediately after opening
    struct stat st;
    int fd = fileno(m_file_handle);
    if (fstat(fd, &st) == 0) {
        Debug::log("io", "FileIOHandler::FileIOHandler() - opened file, fstat size=", st.st_size, " (hex=0x", std::hex, st.st_size, std::dec, ")");
    }
}

/**
 * @brief Destroys the FileIOHandler object.
 *
 * This ensures the underlying file handle is closed by calling the close() method.
 */
FileIOHandler::~FileIOHandler() {
    if (m_file_handle) {
        fclose(m_file_handle);
        m_file_handle = nullptr;
    }
}

/**
 * @brief Reads data from the file.
 *
 * This is a direct wrapper around the standard C `fread` function.
 * @param buffer A pointer to the buffer to store the read data.
 * @param size The size of each element to be read.
 * @param count The number of elements to read.
 * @return The number of elements successfully read.
 */
size_t FileIOHandler::read(void* buffer, size_t size, size_t count) {
    if (!m_file_handle) return 0;
    return fread(buffer, size, count, m_file_handle);
}

/**
 * @brief Seeks to a new position in the file.
 *
 * This uses 64-bit file operations (fseeko) to support large files.
 * @param offset The offset from the position specified by `whence`.
 * @param whence The reference position (e.g., SEEK_SET, SEEK_CUR, SEEK_END).
 * @return 0 on success, or a non-zero value on failure.
 */
int FileIOHandler::seek(long offset, int whence) {
    if (!m_file_handle) return -1;
    return fseeko(m_file_handle, offset, whence);
}

/**
 * @brief Gets the current position in the file.
 *
 * This uses 64-bit file operations (ftello) to support large files.
 * @return The current position in bytes, or -1 on error.
 */
long FileIOHandler::tell() const {
    if (!m_file_handle) return -1;
    long pos = ftell(m_file_handle);
    if (pos > 800000) {
        Debug::log("io", "FileIOHandler::tell() - ftello returned pos=", pos, " (hex=0x", std::hex, pos, std::dec, ")");
        
        // Also check with fstat for comparison
        struct stat st;
        int fd = fileno(m_file_handle);
        if (fstat(fd, &st) == 0) {
            Debug::log("io", "FileIOHandler::tell() - fstat says file size is=", st.st_size, " (hex=0x", std::hex, st.st_size, std::dec, ")");
        }
    }
    return pos;
}



/**
 * @brief Checks for the end-of-file condition on the file.
 *
 * This is a direct wrapper around the standard C `feof` function.
 * @return `true` if the end of the file has been reached, `false` otherwise.
 */
bool FileIOHandler::eof() const {
    if (!m_file_handle) return true;
    return feof(m_file_handle) != 0;
}

long FileIOHandler::getSize() const {
    if (!m_file_handle) return -1;
    
    // Use fstat on the file descriptor to get the size
#ifdef _WIN32
    struct _stat64 file_stat;
    int fd = _fileno(m_file_handle);
    if (_fstat64(fd, &file_stat) != 0) {
        return -1;
    }
#else
    struct stat file_stat;
    int fd = fileno(m_file_handle);
    if (fstat(fd, &file_stat) != 0) {
        return -1;
    }
#endif
    
    Debug::log("io", "FileIOHandler::getFileSize() - fstat returned size=", file_stat.st_size, " (hex=0x", std::hex, file_stat.st_size, std::dec, ")");
    
    return file_stat.st_size;
}std
::unique_ptr<IOHandler> FileIOHandler::duplicate() const {
    return std::make_unique<FileIOHandler>(m_file_path);
}
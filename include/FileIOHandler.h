/*
 * FileIOHandler.h - Concrete IOHandler for local file access.
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

#ifndef FILEIOHANDLER_H
#define FILEIOHANDLER_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Concrete IOHandler implementation for local file access
 * 
 * This class provides access to local files with cross-platform support
 * for Unicode filenames and large files (>2GB).
 */
class FileIOHandler : public IOHandler {
public:
    /**
     * @brief Constructs a FileIOHandler for a given local file path
     * @param path The file path to open with Unicode support
     * @throws InvalidMediaException if the file cannot be opened
     */
    explicit FileIOHandler(const TagLib::String& path);
    
    /**
     * @brief Destroys the FileIOHandler and closes the file
     */
    ~FileIOHandler() override;
    
    /**
     * @brief Read data from the file with fread-like semantics
     * @param buffer Buffer to read data into
     * @param size Size of each element to read
     * @param count Number of elements to read
     * @return Number of elements successfully read
     */
    size_t read(void* buffer, size_t size, size_t count) override;
    
    /**
     * @brief Seek to a position in the file
     * @param offset Offset to seek to (off_t for large file support)
     * @param whence SEEK_SET, SEEK_CUR, or SEEK_END positioning mode
     * @return 0 on success, -1 on failure
     */
    int seek(off_t offset, int whence) override;
    
    /**
     * @brief Get current byte offset position in the file
     * @return Current position as off_t for large file support, -1 on failure
     */
    off_t tell() override;
    
    /**
     * @brief Close the file and cleanup resources
     * @return 0 on success, standard error codes on failure
     */
    int close() override;
    
    /**
     * @brief Check if at end-of-file condition
     * @return true if at end of file, false otherwise
     */
    bool eof() override;
    
    /**
     * @brief Get total size of the file in bytes
     * @return Size in bytes, or -1 if unknown
     */
    off_t getFileSize() override;

private:
    FILE* m_file_handle = nullptr;  // File handle for I/O operations
    TagLib::String m_file_path;     // Original file path for error reporting
    
    /**
     * @brief Validate that the file handle is in a usable state
     * @return true if handle is valid and file is open, false otherwise
     */
    bool validateFileHandle() const;
    
    /**
     * @brief Attempt to recover from certain error conditions
     * @return true if recovery was successful, false otherwise
     */
    bool attemptErrorRecovery();
};

#endif // FILEIOHANDLER_H
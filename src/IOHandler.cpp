/*
 * IOHandler.cpp - Implementation for the I/O handler base class.
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

// Default implementations for the IOHandler base class.
// These can be overridden by concrete subclasses.

/**
 * @brief Destroys the IOHandler object.
 *
 * As a base class, this destructor is virtual to ensure that derived class
 * destructors are called correctly.
 */
IOHandler::~IOHandler() = default;

/**
 * @brief Reads data from the I/O source.
 *
 * The default implementation does nothing and indicates that zero bytes were read.
 * Derived classes should override this to provide actual read functionality.
 * @param buffer A pointer to the buffer to store the read data.
 * @param size The size of each element to be read.
 * @param count The number of elements to read.
 * @return The number of elements successfully read.
 */
size_t IOHandler::read(void* buffer, size_t size, size_t count)
{
    // Default behavior: read nothing.
    return 0;
}

/**
 * @brief Seeks to a new position in the I/O source.
 *
 * The default implementation indicates that the stream is not seekable.
 * Derived classes should override this for seekable sources.
 * @param offset The offset from the position specified by `whence`.
 * @param whence The reference position (e.g., SEEK_SET, SEEK_CUR, SEEK_END).
 * @return 0 on success, or a non-zero value on failure.
 */
int IOHandler::seek(long offset, int whence)
{
    // Default behavior: indicate an error (not seekable).
    return -1;
}

/**
 * @brief Gets the current position in the I/O source.
 *
 * The default implementation indicates an error.
 * Derived classes should override this for seekable sources.
 * @return The current position in bytes, or -1 on error.
 */
long IOHandler::tell()
{
    // Default behavior: indicate an error.
    return -1;
}

/**
 * @brief Closes the I/O source.
 *
 * The default implementation does nothing. Derived classes should override this
 * to release any underlying resources (e.g., file handles, network sockets).
 * @return 0 on success, or EOF on failure.
 */
int IOHandler::close()
{
    // Default behavior: do nothing, return success.
    return 0;
}

/**
 * @brief Checks for the end-of-file condition on the I/O source.
 *
 * The default implementation always returns true, indicating the end has been reached.
 * Derived classes should override this to provide an accurate EOF check.
 * @return `true` if the end of the source has been reached, `false` otherwise.
 */
bool IOHandler::eof()
{
    // Default behavior: always at end of file.
    return true;
}
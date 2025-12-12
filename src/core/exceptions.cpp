/*
 * exceptions.cpp - Exception classes code
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

namespace PsyMP3 {
namespace Core {

/**
 * @brief Constructs an InvalidMediaException.
 *
 * This exception is thrown when a file cannot be opened or identified by any
 * of the available media decoders.
 * @param why A string describing the reason for the failure.
 */
InvalidMediaException::InvalidMediaException(TagLib::String why)
    : std::exception(), m_why(why) {
  // ctor
}

/**
 * @brief Returns the exception's explanatory string.
 * @return A C-style string detailing why the media is considered invalid.
 */
const char *InvalidMediaException::what() const noexcept {
  return m_why.toCString(true); // Return UTF-8 C-string representation
}

/**
 * @brief Constructs a BadFormatException.
 *
 * This exception is thrown when a file is identified as a particular media
 * type, but the data within the file is corrupt or malformed, preventing
 * successful decoding.
 * @param why A string describing the nature of the format error.
 */
BadFormatException::BadFormatException(TagLib::String why)
    : std::exception(), m_why(why) {
  // ctor
}

/**
 * @brief Returns the exception's explanatory string.
 * @return A C-style string detailing the format corruption.
 */
const char *BadFormatException::what() const noexcept {
  return m_why.toCString(true); // Return UTF-8 C-string representation
}

/**
 * @brief Constructs a WrongFormatException.
 *
 * This is a special-case exception used internally by decoders. It signals that
 * the file is not of the format the current decoder handles, allowing the media
 * loader to try the next available decoder.
 * @param why A string describing the format mismatch.
 */
WrongFormatException::WrongFormatException(TagLib::String why)
    : std::exception(), m_why(why) {
  // ctor
}

/**
 * @brief Returns the exception's explanatory string.
 * @return A C-style string detailing the format mismatch.
 */
const char *WrongFormatException::what() const noexcept {
  return m_why.toCString(true); // Return UTF-8 C-string representation
}

/**
 * @brief Constructs an IOException.
 *
 * This exception is used for general file I/O operations that don't fit the
 * media-specific exception categories, such as lyrics file loading,
 * configuration file access, or other auxiliary file operations.
 * @param why A string describing the I/O error.
 */
IOException::IOException(const std::string &why)
    : std::exception(), m_why(why) {
  // ctor
}

/**
 * @brief Returns the exception's explanatory string.
 * @return A C-style string detailing the I/O error.
 */
const char *IOException::what() const noexcept { return m_why.c_str(); }

} // namespace Core
} // namespace PsyMP3

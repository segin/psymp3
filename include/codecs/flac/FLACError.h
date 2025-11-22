/*
 * FLACError.h - Error types and exception handling for Native FLAC decoder
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

#ifndef FLACERROR_H
#define FLACERROR_H

// No direct includes - all includes should be in psymp3.h

#ifdef HAVE_NATIVE_FLAC

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

/**
 * @brief Error codes for FLAC decoder operations
 * 
 * These error codes represent different failure modes that can occur
 * during FLAC decoding. They are used both for error reporting and
 * for determining appropriate recovery strategies.
 * 
 * Requirements: 11
 */
enum class FLACError {
    /**
     * @brief No error occurred
     */
    NONE = 0,
    
    /**
     * @brief Frame sync pattern not found
     * 
     * Recovery: Search for next valid sync pattern
     * Requirements: 11.1
     */
    INVALID_SYNC,
    
    /**
     * @brief Frame header is invalid or corrupted
     * 
     * Recovery: Skip to next frame
     * Requirements: 11.2
     */
    INVALID_HEADER,
    
    /**
     * @brief Subframe decoding failed
     * 
     * Recovery: Output silence for affected channel
     * Requirements: 11.3
     */
    INVALID_SUBFRAME,
    
    /**
     * @brief Residual decoding failed
     * 
     * Recovery: Output silence for affected channel
     * Requirements: 11.3
     */
    INVALID_RESIDUAL,
    
    /**
     * @brief CRC checksum validation failed
     * 
     * Recovery: Log error and attempt to use data (RFC allows)
     * Requirements: 11.4
     */
    CRC_MISMATCH,
    
    /**
     * @brief Bitstream underflow - not enough input data
     * 
     * Recovery: Request more input data
     * Requirements: 11.5
     */
    BUFFER_UNDERFLOW,
    
    /**
     * @brief Memory allocation failed
     * 
     * Recovery: Return error code and clean up
     * Requirements: 11.6
     */
    MEMORY_ALLOCATION,
    
    /**
     * @brief Unsupported feature or forbidden pattern
     * 
     * Recovery: Reject frame and continue
     * Requirements: 11.7
     */
    UNSUPPORTED_FEATURE,
    
    /**
     * @brief Corrupted or invalid data detected
     * 
     * Recovery: Reject frame and continue
     * Requirements: 11.7
     */
    CORRUPTED_DATA,
    
    /**
     * @brief Unrecoverable error occurred
     * 
     * Recovery: Reset to clean state
     * Requirements: 11.8
     */
    UNRECOVERABLE_ERROR
};

/**
 * @brief Exception class for FLAC decoder errors
 * 
 * This exception class wraps FLAC error codes with descriptive messages.
 * It is used for error reporting and allows callers to distinguish between
 * different error types programmatically.
 * 
 * USAGE:
 * ======
 * throw FLACException(FLACError::INVALID_SYNC, "Frame sync pattern not found");
 * 
 * try {
 *     // FLAC decoding operations
 * } catch (const FLACException& e) {
 *     if (e.getError() == FLACError::BUFFER_UNDERFLOW) {
 *         // Request more input data
 *     }
 * }
 * 
 * Requirements: 11
 */
class FLACException : public std::runtime_error {
public:
    /**
     * @brief Construct FLAC exception with error code and message
     * 
     * @param error Error code indicating type of failure
     * @param message Descriptive error message
     */
    FLACException(FLACError error, const std::string& message)
        : std::runtime_error(message), m_error(error) {}
    
    /**
     * @brief Get the error code
     * 
     * @return FLACError code indicating type of failure
     */
    FLACError getError() const { return m_error; }
    
    /**
     * @brief Get human-readable error type name
     * 
     * @return String representation of error type
     */
    const char* getErrorName() const {
        switch (m_error) {
            case FLACError::NONE:
                return "NONE";
            case FLACError::INVALID_SYNC:
                return "INVALID_SYNC";
            case FLACError::INVALID_HEADER:
                return "INVALID_HEADER";
            case FLACError::INVALID_SUBFRAME:
                return "INVALID_SUBFRAME";
            case FLACError::INVALID_RESIDUAL:
                return "INVALID_RESIDUAL";
            case FLACError::CRC_MISMATCH:
                return "CRC_MISMATCH";
            case FLACError::BUFFER_UNDERFLOW:
                return "BUFFER_UNDERFLOW";
            case FLACError::MEMORY_ALLOCATION:
                return "MEMORY_ALLOCATION";
            case FLACError::UNSUPPORTED_FEATURE:
                return "UNSUPPORTED_FEATURE";
            case FLACError::CORRUPTED_DATA:
                return "CORRUPTED_DATA";
            case FLACError::UNRECOVERABLE_ERROR:
                return "UNRECOVERABLE_ERROR";
            default:
                return "UNKNOWN";
        }
    }
    
private:
    FLACError m_error;
};

/**
 * @brief Get human-readable error message for error code
 * 
 * @param error Error code
 * @return Descriptive error message
 */
inline const char* getErrorMessage(FLACError error) {
    switch (error) {
        case FLACError::NONE:
            return "No error";
        case FLACError::INVALID_SYNC:
            return "Frame sync pattern not found";
        case FLACError::INVALID_HEADER:
            return "Invalid or corrupted frame header";
        case FLACError::INVALID_SUBFRAME:
            return "Subframe decoding failed";
        case FLACError::INVALID_RESIDUAL:
            return "Residual decoding failed";
        case FLACError::CRC_MISMATCH:
            return "CRC checksum validation failed";
        case FLACError::BUFFER_UNDERFLOW:
            return "Not enough input data available";
        case FLACError::MEMORY_ALLOCATION:
            return "Memory allocation failed";
        case FLACError::UNSUPPORTED_FEATURE:
            return "Unsupported feature or forbidden pattern";
        case FLACError::CORRUPTED_DATA:
            return "Corrupted or invalid data detected";
        case FLACError::UNRECOVERABLE_ERROR:
            return "Unrecoverable error occurred";
        default:
            return "Unknown error";
    }
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // HAVE_NATIVE_FLAC

#endif // FLACERROR_H

/*
 * ISODemuxerErrorRecovery.h - Error handling and recovery for ISO demuxer
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

#ifndef ISO_DEMUXER_ERROR_RECOVERY_H
#define ISO_DEMUXER_ERROR_RECOVERY_H

namespace PsyMP3 {
namespace Demuxer {
namespace ISO {

// No direct includes - all includes should be in psymp3.h

// Bring IO types into this namespace
using PsyMP3::IO::IOHandler;

// Forward declarations
struct SampleTableInfo;
struct AudioTrackInfo;
struct BoxHeader;

/**
 * @brief Error recovery class for handling corrupted ISO containers
 * 
 * This class provides methods for recovering from various error conditions:
 * - Corrupted box structures
 * - Invalid box sizes
 * - Inconsistent sample tables
 * - Missing codec configuration
 * - I/O errors
 */
class ISODemuxerErrorRecovery {
public:
    ISODemuxerErrorRecovery(std::shared_ptr<IOHandler> io);
    ~ISODemuxerErrorRecovery() = default;
    
    /**
     * @brief Attempt to recover from corrupted box structures
     * @param header Potentially corrupted box header
     * @param containerSize Size of the container box
     * @param fileSize Total file size
     * @return Corrected box header or empty header if unrecoverable
     */
    BoxHeader RecoverCorruptedBox(const BoxHeader& header, uint64_t containerSize, uint64_t fileSize);
    
    /**
     * @brief Validate and repair sample tables for consistency
     * @param tables Sample tables to validate and repair
     * @return true if tables are valid or were successfully repaired
     */
    bool RepairSampleTables(SampleTableInfo& tables);
    
    /**
     * @brief Infer codec configuration from sample data
     * @param track Audio track information
     * @param sampleData Sample data to analyze
     * @return true if configuration was successfully inferred
     */
    bool InferCodecConfig(AudioTrackInfo& track, const std::vector<uint8_t>& sampleData);
    
    /**
     * @brief Handle I/O errors with retry mechanism
     * @param operation Function to retry
     * @param maxRetries Maximum number of retry attempts
     * @return true if operation eventually succeeded
     */
    bool RetryIOOperation(std::function<bool()> operation, int maxRetries = 3);
    
    /**
     * @brief Log error information for debugging
     * @param errorType Type of error
     * @param message Error message
     * @param boxType Box type if applicable
     */
    void LogError(const std::string& errorType, const std::string& message, uint32_t boxType = 0);
    
    /**
     * @brief Get error statistics
     * @return Map of error types to counts
     */
    std::map<std::string, int> GetErrorStats() const;
    
    /**
     * @brief Reset error statistics
     */
    void ResetErrorStats();
    
private:
    std::shared_ptr<IOHandler> io;
    std::map<std::string, int> errorStats;
    
    // Constants for recovery
    static constexpr int MAX_REPAIR_ATTEMPTS = 3;
    static constexpr double BACKOFF_MULTIPLIER = 2.0;
    static constexpr uint32_t MIN_VALID_BOX_SIZE = 8;
    static constexpr uint32_t MAX_REASONABLE_BOX_SIZE = 1024 * 1024 * 100; // 100 MB
    
    // Sample table repair methods
    bool RepairTimeToSampleTable(SampleTableInfo& tables);
    bool RepairSampleToChunkTable(SampleTableInfo& tables);
    bool RepairSampleSizeTable(SampleTableInfo& tables);
    bool RepairChunkOffsetTable(SampleTableInfo& tables);
    bool ValidateTableConsistency(const SampleTableInfo& tables);
    
    // Codec configuration inference methods
    bool InferAACConfig(AudioTrackInfo& track, const std::vector<uint8_t>& sampleData);
    bool InferALACConfig(AudioTrackInfo& track, const std::vector<uint8_t>& sampleData);
    bool InferPCMConfig(AudioTrackInfo& track, const std::vector<uint8_t>& sampleData);
    
    // Helper methods
    uint32_t EstimateReasonableBoxSize(uint32_t boxType, uint64_t containerSize);
    bool IsKnownBoxType(uint32_t boxType);
    std::string BoxTypeToString(uint32_t boxType);
};


} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
#endif // ISO_DEMUXER_ERROR_RECOVERY_H
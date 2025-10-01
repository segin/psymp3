/*
 * FLACRFCComplianceValidator.h - RFC 9639 compliance validation and debugging tools
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

#ifndef FLAC_RFC_COMPLIANCE_VALIDATOR_H
#define FLAC_RFC_COMPLIANCE_VALIDATOR_H

#ifdef HAVE_FLAC

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <FLAC/stream_decoder.h>

/**
 * @brief RFC 9639 compliance violation severity levels
 */
enum class RFCViolationSeverity {
    INFO,       ///< Informational - not a violation but worth noting
    WARNING,    ///< Warning - minor deviation that may affect compatibility
    ERROR,      ///< Error - clear RFC violation that should be fixed
    CRITICAL    ///< Critical - severe violation that breaks FLAC compliance
};

/**
 * @brief RFC 9639 compliance violation report
 */
struct RFCViolationReport {
    RFCViolationSeverity severity;
    std::string rfc_section;        ///< RFC 9639 section reference (e.g., "9.1.2")
    std::string violation_type;     ///< Type of violation (e.g., "Invalid block size")
    std::string description;        ///< Detailed description of the violation
    std::string expected_value;     ///< What the RFC expects
    std::string actual_value;       ///< What was actually found
    size_t byte_offset;            ///< Byte offset in stream where violation occurred
    size_t frame_number;           ///< Frame number (0-based) where violation occurred
    std::chrono::high_resolution_clock::time_point timestamp; ///< When violation was detected
    
    /**
     * @brief Get severity as string for logging
     */
    const char* getSeverityString() const {
        switch (severity) {
            case RFCViolationSeverity::INFO: return "INFO";
            case RFCViolationSeverity::WARNING: return "WARNING";
            case RFCViolationSeverity::ERROR: return "ERROR";
            case RFCViolationSeverity::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief Format violation report as string for logging
     */
    std::string toString() const {
        return std::string("[") + getSeverityString() + "] RFC 9639 Section " + rfc_section + 
               ": " + violation_type + " - " + description + 
               " (Expected: " + expected_value + ", Actual: " + actual_value + 
               ", Frame: " + std::to_string(frame_number) + 
               ", Offset: " + std::to_string(byte_offset) + ")";
    }
};

/**
 * @brief Frame-level RFC 9639 compliance analysis results
 */
struct FrameComplianceAnalysis {
    size_t frame_number;
    bool is_compliant;
    std::vector<RFCViolationReport> violations;
    
    // Frame header analysis
    bool sync_pattern_valid;
    bool reserved_bits_valid;
    bool blocking_strategy_valid;
    bool block_size_valid;
    bool sample_rate_valid;
    bool channel_assignment_valid;
    bool sample_size_valid;
    bool frame_number_valid;
    bool crc8_valid;
    
    // Subframe analysis
    bool subframe_types_valid;
    bool wasted_bits_valid;
    bool predictor_coefficients_valid;
    bool residual_coding_valid;
    bool channel_reconstruction_valid;
    
    // Frame footer analysis
    bool crc16_valid;
    
    /**
     * @brief Get compliance summary string
     */
    std::string getComplianceSummary() const {
        if (is_compliant) {
            return "COMPLIANT";
        }
        
        size_t error_count = 0;
        size_t warning_count = 0;
        size_t critical_count = 0;
        
        for (const auto& violation : violations) {
            switch (violation.severity) {
                case RFCViolationSeverity::CRITICAL: critical_count++; break;
                case RFCViolationSeverity::ERROR: error_count++; break;
                case RFCViolationSeverity::WARNING: warning_count++; break;
                default: break;
            }
        }
        
        return "NON-COMPLIANT (" + std::to_string(critical_count) + " critical, " +
               std::to_string(error_count) + " errors, " + 
               std::to_string(warning_count) + " warnings)";
    }
};

/**
 * @brief Bit-level analysis tools for debugging complex RFC compliance issues
 */
class BitLevelAnalyzer {
public:
    /**
     * @brief Analyze frame header bit-by-bit for RFC 9639 compliance
     */
    static FrameComplianceAnalysis analyzeFrameHeader(const uint8_t* data, size_t size, 
                                                      size_t frame_number, size_t byte_offset);
    
    /**
     * @brief Validate sync pattern per RFC 9639 Section 9.1
     */
    static bool validateSyncPattern(const uint8_t* data, size_t size, 
                                   std::vector<RFCViolationReport>& violations,
                                   size_t frame_number, size_t byte_offset);
    
    /**
     * @brief Validate frame header structure per RFC 9639 Section 9.1
     */
    static bool validateFrameHeader(const uint8_t* data, size_t size,
                                   std::vector<RFCViolationReport>& violations,
                                   size_t frame_number, size_t byte_offset);
    
    /**
     * @brief Validate subframe structure per RFC 9639 Section 9.2
     */
    static bool validateSubframes(const uint8_t* data, size_t size,
                                 const FLAC__Frame* frame,
                                 std::vector<RFCViolationReport>& violations,
                                 size_t frame_number, size_t byte_offset);
    
    /**
     * @brief Validate CRC checksums per RFC 9639 Section 9.3
     */
    static bool validateCRCs(const uint8_t* data, size_t size,
                            std::vector<RFCViolationReport>& violations,
                            size_t frame_number, size_t byte_offset);
    
    /**
     * @brief Extract and validate block size encoding per RFC 9639 Table 1
     */
    static uint32_t validateBlockSizeEncoding(uint8_t block_size_bits,
                                             const uint8_t* header_data,
                                             std::vector<RFCViolationReport>& violations,
                                             size_t frame_number, size_t byte_offset);
    
    /**
     * @brief Extract and validate sample rate encoding per RFC 9639 Table 2
     */
    static uint32_t validateSampleRateEncoding(uint8_t sample_rate_bits,
                                              const uint8_t* header_data,
                                              std::vector<RFCViolationReport>& violations,
                                              size_t frame_number, size_t byte_offset);
    
    /**
     * @brief Validate channel assignment per RFC 9639 Table 3
     */
    static bool validateChannelAssignment(uint8_t channel_assignment,
                                         uint8_t channels,
                                         std::vector<RFCViolationReport>& violations,
                                         size_t frame_number, size_t byte_offset);
    
    /**
     * @brief Validate sample size encoding per RFC 9639 Table 4
     */
    static uint8_t validateSampleSizeEncoding(uint8_t sample_size_bits,
                                             std::vector<RFCViolationReport>& violations,
                                             size_t frame_number, size_t byte_offset);
    
    /**
     * @brief Calculate CRC-8 for frame header per RFC 9639
     */
    static uint8_t calculateCRC8(const uint8_t* data, size_t length);
    
    /**
     * @brief Calculate CRC-16 for frame per RFC 9639
     */
    static uint16_t calculateCRC16(const uint8_t* data, size_t length);
    
    /**
     * @brief Dump frame header in human-readable format with bit-level details
     */
    static std::string dumpFrameHeader(const uint8_t* data, size_t size);
    
    /**
     * @brief Dump subframe structure with RFC section references
     */
    static std::string dumpSubframes(const uint8_t* data, size_t size, const FLAC__Frame* frame);
    
private:
    // CRC lookup tables for performance
    static const uint8_t crc8_table[256];
    static const uint16_t crc16_table[256];
    
    // RFC 9639 validation tables
    static const uint32_t block_size_table[16];
    static const uint32_t sample_rate_table[16];
    static const uint8_t sample_size_table[8];
};

/**
 * @brief Comprehensive RFC 9639 compliance validator and debugging tool
 * 
 * This class provides comprehensive validation of FLAC streams against RFC 9639
 * specification requirements. It performs frame-by-frame analysis and generates
 * detailed compliance reports with specific RFC section references.
 */
class FLACRFCComplianceValidator {
public:
    FLACRFCComplianceValidator();
    ~FLACRFCComplianceValidator();
    
    /**
     * @brief Enable/disable real-time compliance checking
     * 
     * When enabled, performs RFC compliance validation during decoding.
     * This adds overhead but provides immediate feedback on violations.
     * 
     * @param enabled True to enable real-time checking
     * @param performance_impact_threshold_us Maximum acceptable performance impact in microseconds
     */
    void setRealTimeValidation(bool enabled, uint64_t performance_impact_threshold_us = 100);
    
    /**
     * @brief Validate a complete FLAC frame against RFC 9639
     * 
     * Performs comprehensive validation of a FLAC frame including:
     * - Frame header structure and encoding
     * - Subframe types and parameters
     * - Channel assignment and reconstruction
     * - CRC validation
     * - Bit-level compliance checking
     * 
     * @param frame_data Raw frame data
     * @param frame_size Size of frame data in bytes
     * @param frame_number Frame number for reporting (0-based)
     * @param stream_offset Byte offset in stream
     * @return Detailed compliance analysis results
     */
    FrameComplianceAnalysis validateFrame(const uint8_t* frame_data, size_t frame_size,
                                         size_t frame_number, size_t stream_offset);
    
    /**
     * @brief Validate decoded samples against RFC bit-perfect requirements
     * 
     * Validates that decoded samples meet RFC 9639 lossless requirements:
     * - Proper sign extension for bit depths < 32
     * - No overflow or underflow in conversion
     * - Bit-perfect reconstruction for same bit depth
     * - Proper dithering for bit depth reduction
     * 
     * @param samples Decoded sample data
     * @param sample_count Number of samples
     * @param channels Number of channels
     * @param source_bit_depth Original FLAC bit depth
     * @param target_bit_depth Target output bit depth
     * @return Compliance analysis for sample format
     */
    FrameComplianceAnalysis validateSamples(const int16_t* samples, size_t sample_count,
                                           uint8_t channels, uint8_t source_bit_depth,
                                           uint8_t target_bit_depth);
    
    /**
     * @brief Generate comprehensive compliance report
     * 
     * Creates a detailed report of all RFC 9639 compliance issues found
     * during validation, organized by severity and RFC section.
     * 
     * @return Formatted compliance report string
     */
    std::string generateComplianceReport() const;
    
    /**
     * @brief Get violation statistics
     */
    struct ViolationStats {
        size_t total_frames_analyzed;
        size_t compliant_frames;
        size_t non_compliant_frames;
        size_t total_violations;
        size_t critical_violations;
        size_t error_violations;
        size_t warning_violations;
        size_t info_violations;
        double compliance_percentage;
        
        std::string toString() const {
            return "Compliance: " + std::to_string(compliance_percentage) + "% " +
                   "(" + std::to_string(compliant_frames) + "/" + std::to_string(total_frames_analyzed) + " frames), " +
                   "Violations: " + std::to_string(total_violations) + " " +
                   "(" + std::to_string(critical_violations) + " critical, " +
                   std::to_string(error_violations) + " errors, " +
                   std::to_string(warning_violations) + " warnings)";
        }
    };
    
    ViolationStats getViolationStats() const;
    
    /**
     * @brief Clear all violation history
     */
    void clearViolationHistory();
    
    /**
     * @brief Set maximum number of violations to store (for memory management)
     */
    void setMaxViolationHistory(size_t max_violations);
    
    /**
     * @brief Enable/disable specific validation categories
     */
    void setValidationCategories(bool frame_header = true,
                                bool subframes = true,
                                bool channel_reconstruction = true,
                                bool crc_validation = true,
                                bool sample_format = true,
                                bool performance_monitoring = true);
    
    /**
     * @brief Create test suite with RFC-compliant reference files
     * 
     * Generates test cases that validate specific RFC 9639 compliance
     * scenarios including edge cases and error conditions.
     * 
     * @param output_directory Directory to create test files
     * @return True if test suite created successfully
     */
    bool createRFCComplianceTestSuite(const std::string& output_directory);
    
    /**
     * @brief Add a violation to the history (public for GlobalRFCValidator access)
     */
    void addViolation(const RFCViolationReport& violation);
    
private:
    // Validation state
    bool m_real_time_validation_enabled;
    uint64_t m_performance_threshold_us;
    size_t m_max_violation_history;
    
    // Validation categories
    bool m_validate_frame_header;
    bool m_validate_subframes;
    bool m_validate_channel_reconstruction;
    bool m_validate_crc;
    bool m_validate_sample_format;
    bool m_monitor_performance;
    
    // Violation tracking
    std::vector<RFCViolationReport> m_violation_history;
    mutable std::mutex m_violation_mutex;
    
    // Statistics
    mutable std::mutex m_stats_mutex;
    size_t m_total_frames_analyzed;
    size_t m_compliant_frames;
    std::chrono::high_resolution_clock::time_point m_validation_start_time;
    uint64_t m_total_validation_time_us;
    
    // Performance monitoring
    struct PerformanceMetrics {
        uint64_t frame_validation_time_us;
        uint64_t header_validation_time_us;
        uint64_t subframe_validation_time_us;
        uint64_t crc_validation_time_us;
        uint64_t sample_validation_time_us;
    };
    
    std::vector<PerformanceMetrics> m_performance_history;
    
    // Internal validation methods
    bool checkPerformanceImpact(uint64_t validation_time_us) const;
    
    // RFC 9639 specific validation helpers
    bool validateReservedBits(const uint8_t* data, size_t size,
                             std::vector<RFCViolationReport>& violations,
                             size_t frame_number, size_t byte_offset);
    
    bool validateBlockingStrategy(uint8_t blocking_strategy_bit,
                                 std::vector<RFCViolationReport>& violations,
                                 size_t frame_number, size_t byte_offset);
    
    bool validateFrameNumberEncoding(const uint8_t* data, size_t size,
                                   bool variable_block_size,
                                   std::vector<RFCViolationReport>& violations,
                                   size_t frame_number, size_t byte_offset);
    
    // Test case generation helpers
    bool generateSyncPatternTests(const std::string& output_dir);
    bool generateFrameHeaderTests(const std::string& output_dir);
    bool generateSubframeTests(const std::string& output_dir);
    bool generateCRCTests(const std::string& output_dir);
    bool generateSampleFormatTests(const std::string& output_dir);
};

/**
 * @brief Global RFC compliance validator instance
 * 
 * Provides a singleton instance for use throughout the FLAC codec
 * to maintain consistent validation state and reporting.
 */
class GlobalRFCValidator {
public:
    static FLACRFCComplianceValidator& getInstance();
    
    /**
     * @brief Quick compliance check for critical path validation
     * 
     * Performs minimal RFC compliance checking suitable for real-time
     * decoding with minimal performance impact.
     * 
     * @param frame_data Frame data to validate
     * @param frame_size Size of frame data
     * @param frame_number Frame number for reporting
     * @return True if frame passes basic RFC compliance checks
     */
    static bool quickComplianceCheck(const uint8_t* frame_data, size_t frame_size, size_t frame_number);
    
    /**
     * @brief Log RFC compliance violation with automatic severity assessment
     */
    static void logViolation(const std::string& rfc_section,
                           const std::string& violation_type,
                           const std::string& description,
                           const std::string& expected,
                           const std::string& actual,
                           size_t frame_number,
                           size_t byte_offset);
    
private:
    static std::unique_ptr<FLACRFCComplianceValidator> s_instance;
    static std::mutex s_instance_mutex;
};

#endif // HAVE_FLAC

#endif // FLAC_RFC_COMPLIANCE_VALIDATOR_H
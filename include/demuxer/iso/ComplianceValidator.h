/*
 * ComplianceValidator.h - ISO compliance validation component
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef COMPLIANCEVALIDATOR_H
#define COMPLIANCEVALIDATOR_H

namespace PsyMP3 {
namespace Demuxer {
namespace ISO {

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Result structure for box size validation
 */
struct BoxSizeValidationResult {
    bool isValid = false;
    bool is64BitSize = false;
    uint64_t actualSize = 0;
    std::string errorMessage;
};

/**
 * @brief Result structure for timestamp validation
 */
struct TimestampValidationResult {
    bool isValid = false;
    bool hasValidTimescale = false;
    uint64_t normalizedTimestamp = 0;
    std::string errorMessage;
};

/**
 * @brief Result structure for codec validation
 */
struct CodecValidationResult {
    bool isValid = false;
    std::string codecType;
    std::string errorMessage;
};

/**
 * @brief Comprehensive compliance validation result
 */
struct ComplianceValidationResult {
    bool isCompliant = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::map<std::string, std::string> validationDetails;
};

/**
 * @brief ISO compliance validation component
 */
class ComplianceValidator {
public:
    explicit ComplianceValidator(std::shared_ptr<IOHandler> io);
    ~ComplianceValidator();
    
    // Box structure validation
    BoxSizeValidationResult ValidateBoxStructure(uint32_t boxType, uint64_t size, 
                                                uint64_t offset, uint64_t containerSize);
    bool Validate32BitBoxSize(uint32_t size, uint64_t offset, uint64_t containerSize);
    bool Validate64BitBoxSize(uint64_t size, uint64_t offset, uint64_t containerSize);
    
    // Timestamp validation
    TimestampValidationResult ValidateTimestamp(uint64_t timestamp, uint32_t timescale);
    TimestampValidationResult ValidateTimestampConfiguration(uint64_t timestamp, uint32_t timescale, uint64_t duration);
    bool ValidateTimescale(uint32_t timescale);
    
    // Codec validation
    CodecValidationResult ValidateCodecConfiguration(const AudioTrackInfo& track);
    bool ValidateAACConfiguration(const AudioTrackInfo& track);
    bool ValidateALACConfiguration(const AudioTrackInfo& track);
    bool ValidateTelephonyCodecConfiguration(const AudioTrackInfo& track);
    bool ValidatePCMConfiguration(const AudioTrackInfo& track);
    
    // Sample table validation
    bool ValidateSampleTableConsistency(const SampleTableInfo& sampleTable);
    bool ValidateSampleDistribution(const SampleTableInfo& sampleTable);
    
    // Container format compliance
    bool ValidateContainerFormat(const std::string& brand);
    
    // Comprehensive validation
    ComplianceValidationResult ValidateFile();
    
    // Additional methods used by ISODemuxer
    ComplianceValidationResult GetComplianceReport() const;
    bool ValidateBoxNesting(uint32_t parentType, uint32_t childType);
    std::string BoxTypeToString(uint32_t boxType);
    bool ValidateContainerCompliance(const std::vector<uint8_t>& data, const std::string& brand);
    bool ValidateCodecDataIntegrity(const std::string& codecType, const std::vector<uint8_t>& codecData, const AudioTrackInfo& track);
    bool ValidateTrackCompliance(const AudioTrackInfo& track);
    
private:
    std::shared_ptr<IOHandler> io;
    uint64_t fileSize;
    
    // Helper methods
    bool IsValidBoxType(uint32_t boxType);
    bool IsContainerBox(uint32_t boxType);
    uint64_t GetMinimumBoxSize(uint32_t boxType, bool is64Bit);
    bool ValidateBoxAlignment(uint64_t offset);
    
    // Codec-specific validation helpers
    bool ValidateAACProfile(const std::vector<uint8_t>& config);
    bool ValidateALACMagicCookie(const std::vector<uint8_t>& config);
    bool ValidateSampleRate(uint32_t sampleRate, const std::string& codecType);
    bool ValidateChannelCount(uint16_t channels, const std::string& codecType);
    bool ValidateBitsPerSample(uint16_t bits, const std::string& codecType);
};


} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
#endif // COMPLIANCEVALIDATOR_H
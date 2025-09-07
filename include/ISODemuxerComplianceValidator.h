/*
 * ISODemuxerComplianceValidator.h - ISO/IEC 14496-12 standards compliance validation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef ISODEMUXERCOMPLIANCEVALIDATOR_H
#define ISODEMUXERCOMPLIANCEVALIDATOR_H

// All necessary headers are included via psymp3.h

/**
 * @brief Box size validation result
 */
struct BoxSizeValidationResult {
    bool isValid;
    bool is64BitSize;
    uint64_t actualSize;
    std::string errorMessage;
};

/**
 * @brief Timestamp validation result
 */
struct TimestampValidationResult {
    bool isValid;
    bool hasValidTimescale;
    uint64_t normalizedTimestamp;
    std::string errorMessage;
};

/**
 * @brief Compliance validation result
 */
struct ComplianceValidationResult {
    bool isCompliant;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    std::string complianceLevel; // "strict", "relaxed", "non-compliant"
};

/**
 * @brief ISO/IEC 14496-12 standards compliance validator
 * 
 * This class validates ISO Base Media File Format structures against
 * the ISO/IEC 14496-12 specification to ensure standards compliance.
 * 
 * Key validation areas:
 * - Box structure and size validation (32-bit and 64-bit)
 * - Timestamp and timescale validation
 * - Sample table consistency
 * - Codec-specific data integrity
 * - Container format compliance
 */
class ISODemuxerComplianceValidator {
public:
    /**
     * @brief Constructor
     * @param ioHandler Shared pointer to IOHandler for file access
     */
    explicit ISODemuxerComplianceValidator(std::shared_ptr<IOHandler> ioHandler);
    
    /**
     * @brief Destructor
     */
    ~ISODemuxerComplianceValidator();
    
    /**
     * @brief Validate box structure according to ISO/IEC 14496-12
     * @param boxType Box type FOURCC
     * @param boxSize Box size (can be 32-bit or 64-bit)
     * @param boxOffset Offset of box in file
     * @param containerSize Size of containing box
     * @return BoxSizeValidationResult with validation details
     */
    BoxSizeValidationResult ValidateBoxStructure(uint32_t boxType, 
                                                uint64_t boxSize, 
                                                uint64_t boxOffset, 
                                                uint64_t containerSize);
    
    /**
     * @brief Validate 32-bit box size compliance
     * @param boxSize Box size value
     * @param boxOffset Offset of box in file
     * @param containerSize Size of containing box
     * @return true if 32-bit size is valid
     */
    bool Validate32BitBoxSize(uint32_t boxSize, uint64_t boxOffset, uint64_t containerSize);
    
    /**
     * @brief Validate 64-bit box size compliance
     * @param boxSize Box size value (should be > 4GB or special case)
     * @param boxOffset Offset of box in file
     * @param containerSize Size of containing box
     * @return true if 64-bit size is valid and necessary
     */
    bool Validate64BitBoxSize(uint64_t boxSize, uint64_t boxOffset, uint64_t containerSize);
    
    /**
     * @brief Validate timestamp and timescale configuration
     * @param timestamp Timestamp value in track timescale units
     * @param timescale Track timescale (samples per second)
     * @param duration Track duration in timescale units
     * @return TimestampValidationResult with validation details
     */
    TimestampValidationResult ValidateTimestampConfiguration(uint64_t timestamp, 
                                                            uint32_t timescale, 
                                                            uint64_t duration);
    
    /**
     * @brief Validate sample table consistency according to ISO specification
     * @param sampleTableInfo Sample table information to validate
     * @return true if sample tables are consistent and compliant
     */
    bool ValidateSampleTableConsistency(const SampleTableInfo& sampleTableInfo);
    
    /**
     * @brief Validate codec-specific data integrity
     * @param codecType Codec type identifier
     * @param codecConfig Codec configuration data
     * @param track Audio track information
     * @return true if codec data is valid and compliant
     */
    bool ValidateCodecDataIntegrity(const std::string& codecType, 
                                   const std::vector<uint8_t>& codecConfig,
                                   const AudioTrackInfo& track);
    
    /**
     * @brief Validate container format compliance
     * @param fileTypeBox File type box data
     * @param containerBrand Container brand identifier
     * @return ComplianceValidationResult with detailed compliance assessment
     */
    ComplianceValidationResult ValidateContainerCompliance(const std::vector<uint8_t>& fileTypeBox,
                                                          const std::string& containerBrand);
    
    /**
     * @brief Validate track structure compliance
     * @param track Audio track information to validate
     * @return ComplianceValidationResult with track-specific compliance details
     */
    ComplianceValidationResult ValidateTrackCompliance(const AudioTrackInfo& track);
    
    /**
     * @brief Validate edit list compliance (timeline modifications)
     * @param editList Edit list entries
     * @param trackDuration Track duration in timescale units
     * @param timescale Track timescale
     * @return true if edit list is valid and compliant
     */
    bool ValidateEditListCompliance(const std::vector<uint8_t>& editList,
                                   uint64_t trackDuration,
                                   uint32_t timescale);
    
    /**
     * @brief Validate fragment structure compliance (for fragmented MP4)
     * @param fragmentData Fragment box data
     * @param fragmentType Fragment type (moof, traf, trun, etc.)
     * @return true if fragment structure is compliant
     */
    bool ValidateFragmentCompliance(const std::vector<uint8_t>& fragmentData,
                                   uint32_t fragmentType);
    
    /**
     * @brief Get comprehensive compliance report
     * @return ComplianceValidationResult with overall file compliance status
     */
    ComplianceValidationResult GetComplianceReport() const;
    
    /**
     * @brief Set compliance strictness level
     * @param level Strictness level: "strict", "relaxed", "permissive"
     */
    void SetComplianceLevel(const std::string& level);
    
    /**
     * @brief Check if specific box type is required by ISO specification
     * @param boxType Box type FOURCC
     * @param containerType Container box type
     * @return true if box is required in the given container
     */
    bool IsRequiredBox(uint32_t boxType, uint32_t containerType);
    
    /**
     * @brief Validate box nesting compliance
     * @param childBoxType Child box type
     * @param parentBoxType Parent box type
     * @return true if nesting is allowed by specification
     */
    bool ValidateBoxNesting(uint32_t childBoxType, uint32_t parentBoxType);
    
    /**
     * @brief Convert box type to string for logging
     * @param boxType Box type FOURCC
     * @return String representation of box type
     */
    std::string BoxTypeToString(uint32_t boxType);
    
private:
    std::shared_ptr<IOHandler> m_ioHandler;
    std::string m_complianceLevel;
    
    // Compliance tracking
    std::vector<std::string> m_warnings;
    std::vector<std::string> m_errors;
    std::map<uint32_t, uint32_t> m_boxCounts; // Track box type occurrences
    
    // Validation state
    bool m_hasFileTypeBox;
    bool m_hasMovieBox;
    bool m_hasMediaDataBox;
    std::set<uint32_t> m_encounteredBoxTypes;
    
    /**
     * @brief Add warning to compliance report
     * @param message Warning message
     */
    void AddWarning(const std::string& message);
    
    /**
     * @brief Add error to compliance report
     * @param message Error message
     */
    void AddError(const std::string& message);
    
    /**
     * @brief Validate box size against ISO specification limits
     * @param boxSize Box size to validate
     * @param boxType Box type for context
     * @return true if size is within specification limits
     */
    bool ValidateBoxSizeLimits(uint64_t boxSize, uint32_t boxType);
    
    /**
     * @brief Validate timescale value according to ISO specification
     * @param timescale Timescale value to validate
     * @return true if timescale is valid
     */
    bool ValidateTimescaleValue(uint32_t timescale);
    
    /**
     * @brief Validate sample description compliance
     * @param sampleDescription Sample description data
     * @param codecType Expected codec type
     * @return true if sample description is compliant
     */
    bool ValidateSampleDescriptionCompliance(const std::vector<uint8_t>& sampleDescription,
                                           const std::string& codecType);
    
    /**
     * @brief Check for required boxes in container
     * @param containerType Container box type
     * @param childBoxTypes Set of child box types found
     * @return true if all required boxes are present
     */
    bool CheckRequiredBoxes(uint32_t containerType, const std::set<uint32_t>& childBoxTypes);
    
    /**
     * @brief Validate telephony codec compliance (mulaw/alaw)
     * @param track Audio track with telephony codec
     * @return true if telephony codec configuration is compliant
     */
    bool ValidateTelephonyCodecCompliance(const AudioTrackInfo& track);
    
    /**
     * @brief Get maximum allowed box size for given type
     * @param boxType Box type FOURCC
     * @return Maximum allowed size in bytes
     */
    uint64_t GetMaxAllowedBoxSize(uint32_t boxType);
    
    /**
     * @brief Check if box type supports 64-bit sizes
     * @param boxType Box type FOURCC
     * @return true if 64-bit sizes are supported for this box type
     */
    bool Supports64BitSize(uint32_t boxType);
};

#endif // ISODEMUXERCOMPLIANCEVALIDATOR_H
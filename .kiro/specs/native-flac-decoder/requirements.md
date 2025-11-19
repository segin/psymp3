# Requirements Document

## Introduction

This specification defines the requirements for implementing a native FLAC audio decoder for PsyMP3 to eliminate the dependency on libFLAC. The native decoder will implement the complete FLAC decoding pipeline from bitstream parsing to PCM sample reconstruction, following RFC 9639 specifications. This implementation will replace the current libFLAC-based FLACCodec while maintaining full compatibility with the existing AudioCodec architecture and all container formats (native FLAC, Ogg FLAC).

The native implementation provides several strategic advantages:
- **Dependency elimination**: Removes external libFLAC dependency, simplifying builds and distribution
- **Full control**: Complete control over decoding pipeline for optimization and debugging
- **Integration**: Tighter integration with PsyMP3's architecture and threading model
- **Learning resource**: Reference implementation based on libFLAC source patterns in tests/data
- **RFC compliance**: Direct implementation of RFC 9639 specification

## Glossary

- **Native_FLAC_Decoder**: The new pure C++ FLAC decoder implementation without libFLAC dependency
- **Bitstream_Reader**: Component that reads variable-length bit fields from FLAC bitstream
- **Frame_Parser**: Component that parses FLAC frame headers and structure
- **Subframe_Decoder**: Component that decodes individual channel subframes
- **Predictor_Engine**: Component that applies FIXED and LPC prediction algorithms
- **Residual_Decoder**: Component that decodes Rice/Golomb entropy-coded residuals
- **Sample_Reconstructor**: Component that combines prediction and residuals to produce PCM samples
- **CRC_Validator**: Component that validates frame and stream CRC checksums
- **Bit_Depth_Converter**: Component that converts between different PCM bit depths
- **Channel_Decorrelator**: Component that handles stereo decorrelation (left-side, right-side, mid-side)
- **RFC_9639**: The official FLAC format specification document
- **libFLAC_Reference**: The reference libFLAC source code in tests/data used as implementation guide

## Requirements

### Requirement 1: Bitstream Reading Infrastructure

**User Story:** As a FLAC decoder, I want efficient bit-level reading capabilities, so that I can parse variable-length FLAC bitstream fields accurately.

#### Acceptance Criteria

1. WHEN reading N bits from bitstream, THE Native_FLAC_Decoder SHALL extract exactly N bits and advance the bit position
2. WHEN reading unary-coded values, THE Native_FLAC_Decoder SHALL decode unary encoding per RFC 9639
3. WHEN reading UTF-8 coded values, THE Native_FLAC_Decoder SHALL decode UTF-8 integers per RFC 9639
4. WHEN reading Rice-coded values, THE Native_FLAC_Decoder SHALL decode Rice/Golomb codes per RFC 9639
5. WHEN bit buffer is exhausted, THE Native_FLAC_Decoder SHALL refill from input stream efficiently
6. WHEN handling byte alignment, THE Native_FLAC_Decoder SHALL align to byte boundaries when required
7. WHEN reading signed values, THE Native_FLAC_Decoder SHALL apply proper sign extension
8. WHEN bit position exceeds available data, THE Native_FLAC_Decoder SHALL report underflow error

### Requirement 2: Frame Header Parsing

**User Story:** As a FLAC decoder, I want to parse frame headers correctly, so that I can extract frame parameters and validate frame structure.

#### Acceptance Criteria

1. WHEN encountering frame sync code, THE Native_FLAC_Decoder SHALL validate 0xFFF8-0xFFFF sync pattern
2. WHEN parsing blocking strategy, THE Native_FLAC_Decoder SHALL determine fixed or variable block size mode
3. WHEN parsing block size, THE Native_FLAC_Decoder SHALL decode block size from 4-bit field with special cases
4. WHEN parsing sample rate, THE Native_FLAC_Decoder SHALL decode sample rate from 4-bit field with special cases
5. WHEN parsing channel assignment, THE Native_FLAC_Decoder SHALL identify independent, left-side, right-side, or mid-side modes
6. WHEN parsing sample size, THE Native_FLAC_Decoder SHALL decode bits per sample from 3-bit field
7. WHEN parsing frame/sample number, THE Native_FLAC_Decoder SHALL decode UTF-8 coded position value
8. WHEN frame header CRC fails, THE Native_FLAC_Decoder SHALL reject frame and report error

### Requirement 3: Subframe Decoding

**User Story:** As a FLAC decoder, I want to decode all subframe types, so that I can reconstruct audio samples for each channel.

#### Acceptance Criteria

1. WHEN encountering CONSTANT subframe, THE Native_FLAC_Decoder SHALL extract single constant value for all samples
2. WHEN encountering VERBATIM subframe, THE Native_FLAC_Decoder SHALL extract uncompressed sample values
3. WHEN encountering FIXED subframe, THE Native_FLAC_Decoder SHALL apply fixed predictor of order 0-4
4. WHEN encountering LPC subframe, THE Native_FLAC_Decoder SHALL apply linear prediction with decoded coefficients
5. WHEN parsing subframe header, THE Native_FLAC_Decoder SHALL extract subframe type and wasted bits
6. WHEN wasted bits are present, THE Native_FLAC_Decoder SHALL left-shift reconstructed samples appropriately
7. WHEN subframe type is reserved, THE Native_FLAC_Decoder SHALL reject frame and report error
8. WHEN subframe decoding fails, THE Native_FLAC_Decoder SHALL output silence for affected channel

### Requirement 4: FIXED Prediction Implementation

**User Story:** As a FLAC decoder, I want to implement FIXED prediction orders 0-4, so that I can reconstruct samples from fixed predictors and residuals.

#### Acceptance Criteria

1. WHEN applying order-0 prediction, THE Native_FLAC_Decoder SHALL use residual values directly as samples
2. WHEN applying order-1 prediction, THE Native_FLAC_Decoder SHALL compute s[i] = residual[i] + s[i-1]
3. WHEN applying order-2 prediction, THE Native_FLAC_Decoder SHALL compute s[i] = residual[i] + 2*s[i-1] - s[i-2]
4. WHEN applying order-3 prediction, THE Native_FLAC_Decoder SHALL compute s[i] = residual[i] + 3*s[i-1] - 3*s[i-2] + s[i-3]
5. WHEN applying order-4 prediction, THE Native_FLAC_Decoder SHALL compute s[i] = residual[i] + 4*s[i-1] - 6*s[i-2] + 4*s[i-3] - s[i-4]
6. WHEN initializing prediction, THE Native_FLAC_Decoder SHALL use warm-up samples from bitstream
7. WHEN prediction order exceeds block size, THE Native_FLAC_Decoder SHALL reject frame as invalid
8. WHEN overflow occurs during prediction, THE Native_FLAC_Decoder SHALL handle with appropriate bit width

### Requirement 5: LPC Prediction Implementation

**User Story:** As a FLAC decoder, I want to implement LPC prediction, so that I can decode FLAC streams using linear predictive coding.

#### Acceptance Criteria

1. WHEN parsing LPC coefficients, THE Native_FLAC_Decoder SHALL extract quantized coefficient values
2. WHEN parsing precision, THE Native_FLAC_Decoder SHALL decode coefficient precision from 4-bit field
3. WHEN parsing shift, THE Native_FLAC_Decoder SHALL extract quantization level shift value
4. WHEN applying LPC prediction, THE Native_FLAC_Decoder SHALL compute s[i] = residual[i] + sum(coeff[j] * s[i-j-1]) >> shift
5. WHEN LPC order is 1-32, THE Native_FLAC_Decoder SHALL support all valid LPC orders
6. WHEN initializing LPC, THE Native_FLAC_Decoder SHALL use warm-up samples from bitstream
7. WHEN coefficient precision is invalid, THE Native_FLAC_Decoder SHALL reject frame and report error
8. WHEN LPC computation overflows, THE Native_FLAC_Decoder SHALL use appropriate 64-bit arithmetic

### Requirement 6: Rice/Golomb Residual Decoding

**User Story:** As a FLAC decoder, I want to decode entropy-coded residuals, so that I can reconstruct prediction errors from compressed bitstream.

#### Acceptance Criteria

1. WHEN parsing residual header, THE Native_FLAC_Decoder SHALL extract coding method and partition order
2. WHEN decoding Rice parameter, THE Native_FLAC_Decoder SHALL extract parameter for each partition
3. WHEN decoding Rice-coded value, THE Native_FLAC_Decoder SHALL decode unary quotient and binary remainder
4. WHEN Rice parameter is 15 (escape code), THE Native_FLAC_Decoder SHALL read unencoded binary values
5. WHEN partition order is 0-15, THE Native_FLAC_Decoder SHALL support all valid partition orders
6. WHEN decoding signed residuals, THE Native_FLAC_Decoder SHALL convert unsigned to signed per RFC 9639
7. WHEN residual count mismatches block size, THE Native_FLAC_Decoder SHALL reject frame as invalid
8. WHEN Rice decoding fails, THE Native_FLAC_Decoder SHALL report error and skip frame

### Requirement 7: Channel Decorrelation

**User Story:** As a FLAC decoder, I want to handle stereo decorrelation modes, so that I can reconstruct independent left/right channels from correlated data.

#### Acceptance Criteria

1. WHEN channel assignment is independent, THE Native_FLAC_Decoder SHALL use decoded subframes directly
2. WHEN channel assignment is left-side, THE Native_FLAC_Decoder SHALL compute right = left - side
3. WHEN channel assignment is right-side, THE Native_FLAC_Decoder SHALL compute left = right + side
4. WHEN channel assignment is mid-side, THE Native_FLAC_Decoder SHALL compute left = mid + (side>>1), right = mid - (side>>1)
5. WHEN handling mid-side with odd side values, THE Native_FLAC_Decoder SHALL apply proper rounding
6. WHEN decorrelating channels, THE Native_FLAC_Decoder SHALL maintain sample accuracy
7. WHEN channel count is 1, THE Native_FLAC_Decoder SHALL skip decorrelation
8. WHEN channel assignment is invalid, THE Native_FLAC_Decoder SHALL reject frame and report error

### Requirement 8: CRC Validation

**User Story:** As a FLAC decoder, I want to validate CRC checksums, so that I can detect corrupted frames and ensure data integrity.

#### Acceptance Criteria

1. WHEN parsing frame header, THE Native_FLAC_Decoder SHALL validate 8-bit CRC-8 checksum
2. WHEN parsing frame footer, THE Native_FLAC_Decoder SHALL validate 16-bit CRC-16 checksum
3. WHEN computing CRC-8, THE Native_FLAC_Decoder SHALL use polynomial 0x07 per RFC 9639
4. WHEN computing CRC-16, THE Native_FLAC_Decoder SHALL use polynomial 0x8005 per RFC 9639
5. WHEN frame header CRC fails, THE Native_FLAC_Decoder SHALL reject frame immediately
6. WHEN frame footer CRC fails, THE Native_FLAC_Decoder SHALL log warning but may use decoded data
7. WHEN CRC validation is disabled, THE Native_FLAC_Decoder SHALL skip CRC checks for performance
8. WHEN CRC tables are needed, THE Native_FLAC_Decoder SHALL use precomputed lookup tables

### Requirement 9: Bit Depth Conversion

**User Story:** As an audio output system, I want consistent 16-bit PCM output, so that I can handle audio uniformly regardless of source bit depth.

#### Acceptance Criteria

1. WHEN source is 16-bit, THE Native_FLAC_Decoder SHALL output samples directly without conversion
2. WHEN source is 8-bit, THE Native_FLAC_Decoder SHALL left-shift by 8 bits to scale to 16-bit range
3. WHEN source is 24-bit, THE Native_FLAC_Decoder SHALL right-shift by 8 bits with rounding
4. WHEN source is 32-bit, THE Native_FLAC_Decoder SHALL right-shift by 16 bits with rounding
5. WHEN source is 4-12 bit, THE Native_FLAC_Decoder SHALL left-shift to 16-bit range with proper scaling
6. WHEN source is 20-bit, THE Native_FLAC_Decoder SHALL right-shift by 4 bits with rounding
7. WHEN downscaling, THE Native_FLAC_Decoder SHALL add 0.5 LSB before shifting for proper rounding
8. WHEN converting, THE Native_FLAC_Decoder SHALL prevent clipping and maintain audio quality

### Requirement 10: Sample Reconstruction and Output

**User Story:** As an audio playback system, I want properly formatted PCM output, so that I can send audio to the output device.

#### Acceptance Criteria

1. WHEN frame decoding completes, THE Native_FLAC_Decoder SHALL produce interleaved PCM samples
2. WHEN outputting stereo, THE Native_FLAC_Decoder SHALL interleave left and right channels
3. WHEN outputting multi-channel, THE Native_FLAC_Decoder SHALL interleave all channels in order
4. WHEN block size varies, THE Native_FLAC_Decoder SHALL output correct number of samples per frame
5. WHEN samples are reconstructed, THE Native_FLAC_Decoder SHALL ensure values are within valid 16-bit range
6. WHEN outputting to AudioFrame, THE Native_FLAC_Decoder SHALL populate frame with correct sample count and format
7. WHEN multiple frames are decoded, THE Native_FLAC_Decoder SHALL maintain continuous sample stream
8. WHEN end of stream is reached, THE Native_FLAC_Decoder SHALL flush any remaining samples

### Requirement 11: Error Handling and Recovery

**User Story:** As a media player, I want robust error handling, so that corrupted frames don't crash the application.

#### Acceptance Criteria

1. WHEN sync pattern is not found, THE Native_FLAC_Decoder SHALL search for next valid sync
2. WHEN frame header is invalid, THE Native_FLAC_Decoder SHALL skip to next frame
3. WHEN subframe decoding fails, THE Native_FLAC_Decoder SHALL output silence for affected channel
4. WHEN CRC validation fails, THE Native_FLAC_Decoder SHALL log error and attempt to use data
5. WHEN bitstream underflow occurs, THE Native_FLAC_Decoder SHALL request more input data
6. WHEN memory allocation fails, THE Native_FLAC_Decoder SHALL return error code and clean up
7. WHEN invalid parameters are detected, THE Native_FLAC_Decoder SHALL reject frame and continue
8. WHEN unrecoverable error occurs, THE Native_FLAC_Decoder SHALL reset to clean state

### Requirement 12: Performance Optimization

**User Story:** As a user of high-resolution audio, I want efficient decoding, so that CPU usage remains reasonable for 24-bit/96kHz+ files.

#### Acceptance Criteria

1. WHEN decoding 44.1kHz/16-bit FLAC, THE Native_FLAC_Decoder SHALL maintain <2% CPU usage on target hardware
2. WHEN decoding 96kHz/24-bit FLAC, THE Native_FLAC_Decoder SHALL maintain real-time performance
3. WHEN reading bits, THE Native_FLAC_Decoder SHALL use efficient bit manipulation and buffering
4. WHEN decoding Rice codes, THE Native_FLAC_Decoder SHALL use optimized unary decoding
5. WHEN applying prediction, THE Native_FLAC_Decoder SHALL use efficient arithmetic operations
6. WHEN converting bit depths, THE Native_FLAC_Decoder SHALL use fast shift operations
7. WHEN allocating memory, THE Native_FLAC_Decoder SHALL minimize allocations during decoding
8. WHEN processing frames, THE Native_FLAC_Decoder SHALL optimize for cache efficiency

### Requirement 13: Threading Safety

**User Story:** As a multi-threaded media player, I want thread-safe decoder instances, so that multiple decoders can operate concurrently.

#### Acceptance Criteria

1. WHEN multiple decoder instances exist, THE Native_FLAC_Decoder SHALL maintain independent state per instance
2. WHEN implementing public methods, THE Native_FLAC_Decoder SHALL follow public/private lock pattern
3. WHEN acquiring locks, THE Native_FLAC_Decoder SHALL use RAII lock guards for exception safety
4. WHEN calling internal methods, THE Native_FLAC_Decoder SHALL use _unlocked versions
5. WHEN accessing shared resources, THE Native_FLAC_Decoder SHALL use appropriate synchronization
6. WHEN static data is used, THE Native_FLAC_Decoder SHALL ensure thread-safe initialization
7. WHEN decoder is destroyed, THE Native_FLAC_Decoder SHALL ensure no operations are in progress
8. WHEN logging occurs, THE Native_FLAC_Decoder SHALL use thread-safe logging mechanisms

### Requirement 14: AudioCodec Integration

**User Story:** As a PsyMP3 component, I want seamless integration with AudioCodec architecture, so that the native decoder works like other codecs.

#### Acceptance Criteria

1. WHEN implementing AudioCodec interface, THE Native_FLAC_Decoder SHALL provide all required virtual methods
2. WHEN initializing from StreamInfo, THE Native_FLAC_Decoder SHALL configure sample rate, channels, and bit depth
3. WHEN decoding MediaChunk, THE Native_FLAC_Decoder SHALL convert chunk data to AudioFrame output
4. WHEN flushing, THE Native_FLAC_Decoder SHALL output any remaining decoded samples
5. WHEN resetting for seek, THE Native_FLAC_Decoder SHALL clear decoder state and buffers
6. WHEN reporting capabilities, THE Native_FLAC_Decoder SHALL indicate support for "flac" codec
7. WHEN handling errors, THE Native_FLAC_Decoder SHALL use PsyMP3 error reporting mechanisms
8. WHEN logging debug info, THE Native_FLAC_Decoder SHALL use Debug::log with "flac_codec" channel

### Requirement 15: Container Agnostic Operation

**User Story:** As a demuxer component, I want the decoder to work with any container, so that native FLAC, Ogg FLAC, and other containers are supported.

#### Acceptance Criteria

1. WHEN receiving frames from FLACDemuxer, THE Native_FLAC_Decoder SHALL decode native FLAC frames
2. WHEN receiving frames from OggDemuxer, THE Native_FLAC_Decoder SHALL decode Ogg FLAC frames
3. WHEN processing frames, THE Native_FLAC_Decoder SHALL not depend on container-specific information
4. WHEN initializing, THE Native_FLAC_Decoder SHALL only require StreamInfo parameters
5. WHEN working with MediaChunk, THE Native_FLAC_Decoder SHALL handle chunks from any demuxer
6. WHEN seeking occurs, THE Native_FLAC_Decoder SHALL reset without container-specific operations
7. WHEN handling metadata, THE Native_FLAC_Decoder SHALL not process container-specific metadata
8. WHEN registering with MediaFactory, THE Native_FLAC_Decoder SHALL indicate container-agnostic support

### Requirement 16: RFC 9639 Compliance

**User Story:** As a FLAC format implementer, I want strict RFC 9639 compliance, so that all valid FLAC streams decode correctly.

#### Acceptance Criteria

1. WHEN parsing frame structure, THE Native_FLAC_Decoder SHALL follow RFC 9639 Section 9 exactly
2. WHEN decoding subframes, THE Native_FLAC_Decoder SHALL implement all subframe types per RFC 9639 Section 9.2
3. WHEN applying prediction, THE Native_FLAC_Decoder SHALL use RFC 9639 Section 4.3 algorithms
4. WHEN decoding residuals, THE Native_FLAC_Decoder SHALL follow RFC 9639 Section 4.4 entropy coding
5. WHEN validating CRC, THE Native_FLAC_Decoder SHALL use RFC 9639 specified polynomials
6. WHEN handling channel assignments, THE Native_FLAC_Decoder SHALL follow RFC 9639 Section 9.1 rules
7. WHEN processing bit depths, THE Native_FLAC_Decoder SHALL support RFC 9639 specified range (4-32 bits)
8. WHEN encountering reserved values, THE Native_FLAC_Decoder SHALL handle per RFC 9639 error handling

### Requirement 17: Conditional Compilation and Build-Time Selection

**User Story:** As a build system maintainer, I want to select between native and libFLAC decoders at configure time, so that builds can use either implementation.

#### Acceptance Criteria

1. WHEN ./configure --enable-native-flac is used, THE build system SHALL define HAVE_NATIVE_FLAC and compile native decoder
2. WHEN ./configure --disable-native-flac is used, THE build system SHALL use libFLAC wrapper if available
3. WHEN HAVE_NATIVE_FLAC is defined, THE Native_FLAC_Decoder SHALL be compiled and provide FLACCodec class
4. WHEN HAVE_FLAC is defined without HAVE_NATIVE_FLAC, THE libFLAC wrapper SHALL provide FLACCodec class
5. WHEN neither is defined, THE build SHALL succeed without FLAC support
6. WHEN both implementations exist, THE configure script SHALL allow user to choose via --enable-native-flac flag
7. WHEN native decoder is selected, THE build SHALL not link against libFLAC libraries
8. WHEN libFLAC wrapper is selected, THE build SHALL link against libFLAC libraries
9. WHEN FLACCodec is instantiated, THE application SHALL receive selected implementation transparently
10. WHEN testing, THE appropriate decoder tests SHALL be conditionally compiled based on selection

### Requirement 18: Compatibility and Migration

**User Story:** As a PsyMP3 user, I want the native decoder to work identically to libFLAC, so that my FLAC files continue to play without issues.

#### Acceptance Criteria

1. WHEN decoding FLAC files, THE Native_FLAC_Decoder SHALL produce identical output to libFLAC decoder
2. WHEN handling edge cases, THE Native_FLAC_Decoder SHALL match libFLAC behavior
3. WHEN performance is measured, THE Native_FLAC_Decoder SHALL provide comparable performance to libFLAC
4. WHEN memory usage is measured, THE Native_FLAC_Decoder SHALL use reasonable memory amounts
5. WHEN seeking occurs, THE Native_FLAC_Decoder SHALL provide equivalent seeking support
6. WHEN errors are encountered, THE Native_FLAC_Decoder SHALL provide equivalent error handling
7. WHEN integrated with existing code, THE Native_FLAC_Decoder SHALL work as drop-in replacement
8. WHEN testing with real files, THE Native_FLAC_Decoder SHALL decode all test FLAC files correctly

### Requirement 19: Streamable Subset Support

**User Story:** As a streaming audio application, I want support for the FLAC streamable subset, so that I can decode streams without seeking capability.

#### Acceptance Criteria

1. WHEN decoding streamable subset, THE Native_FLAC_Decoder SHALL handle frames without streaminfo metadata references
2. WHEN sample rate is in frame header, THE Native_FLAC_Decoder SHALL extract sample rate from frame (not streaminfo)
3. WHEN bit depth is in frame header, THE Native_FLAC_Decoder SHALL extract bit depth from frame (not streaminfo)
4. WHEN block size is ≤16384, THE Native_FLAC_Decoder SHALL decode blocks up to maximum streamable size
5. WHEN sample rate is ≤48kHz with block size ≤4608, THE Native_FLAC_Decoder SHALL handle streamable audio constraints
6. WHEN LPC order is ≤12 for ≤48kHz audio, THE Native_FLAC_Decoder SHALL decode streamable LPC subframes
7. WHEN Rice partition order is ≤8, THE Native_FLAC_Decoder SHALL decode streamable residual partitions
8. WHEN starting mid-stream, THE Native_FLAC_Decoder SHALL synchronize and decode without prior metadata

### Requirement 20: Wasted Bits Optimization

**User Story:** As a FLAC decoder, I want to handle wasted bits efficiently, so that I can decode files with padded samples correctly.

#### Acceptance Criteria

1. WHEN wasted bits flag is set, THE Native_FLAC_Decoder SHALL decode unary-coded wasted bits count
2. WHEN decoding samples with wasted bits, THE Native_FLAC_Decoder SHALL decode at reduced bit depth
3. WHEN reconstructing samples, THE Native_FLAC_Decoder SHALL left-shift by wasted bits count to restore padding
4. WHEN wasted bits vary per subframe, THE Native_FLAC_Decoder SHALL handle different wasted bits per channel
5. WHEN wasted bits vary per frame, THE Native_FLAC_Decoder SHALL adapt to changing wasted bits across frames
6. WHEN applying stereo decorrelation, THE Native_FLAC_Decoder SHALL apply wasted bits padding before channel restoration
7. WHEN wasted bits would result in zero bit depth, THE Native_FLAC_Decoder SHALL reject frame as invalid
8. WHEN wasted bits are used, THE Native_FLAC_Decoder SHALL maintain lossless reconstruction

### Requirement 21: UTF-8 Coded Number Handling

**User Story:** As a FLAC decoder, I want to decode UTF-8 coded frame/sample numbers, so that I can track position in stream.

#### Acceptance Criteria

1. WHEN decoding 1-byte UTF-8 number, THE Native_FLAC_Decoder SHALL decode 7-bit values (0x00-0x7F)
2. WHEN decoding 2-byte UTF-8 number, THE Native_FLAC_Decoder SHALL decode 11-bit values (0x80-0x7FF)
3. WHEN decoding 3-byte UTF-8 number, THE Native_FLAC_Decoder SHALL decode 16-bit values (0x800-0xFFFF)
4. WHEN decoding 4-byte UTF-8 number, THE Native_FLAC_Decoder SHALL decode 21-bit values (0x10000-0x1FFFFF)
5. WHEN decoding 5-byte UTF-8 number, THE Native_FLAC_Decoder SHALL decode 26-bit values (0x200000-0x3FFFFFF)
6. WHEN decoding 6-byte UTF-8 number, THE Native_FLAC_Decoder SHALL decode 31-bit values (0x4000000-0x7FFFFFFF)
7. WHEN decoding 7-byte UTF-8 number, THE Native_FLAC_Decoder SHALL decode 36-bit values (0x800000000-0xFFFFFFFFF)
8. WHEN UTF-8 number is invalid, THE Native_FLAC_Decoder SHALL reject frame and report error

### Requirement 22: Uncommon Parameter Handling

**User Story:** As a FLAC decoder, I want to handle uncommon block sizes and sample rates, so that I can decode files with non-standard parameters.

#### Acceptance Criteria

1. WHEN block size bits are 0b0110, THE Native_FLAC_Decoder SHALL read 8-bit block size minus 1 from bitstream
2. WHEN block size bits are 0b0111, THE Native_FLAC_Decoder SHALL read 16-bit block size minus 1 from bitstream
3. WHEN uncommon block size is 65535 (65536 samples), THE Native_FLAC_Decoder SHALL reject as forbidden value
4. WHEN uncommon block size is 0-14 in non-final frame, THE Native_FLAC_Decoder SHALL reject as invalid
5. WHEN sample rate bits are 0b1100, THE Native_FLAC_Decoder SHALL read 8-bit sample rate in kHz
6. WHEN sample rate bits are 0b1101, THE Native_FLAC_Decoder SHALL read 16-bit sample rate in Hz
7. WHEN sample rate bits are 0b1110, THE Native_FLAC_Decoder SHALL read 16-bit sample rate in Hz/10
8. WHEN sample rate bits are 0b1111, THE Native_FLAC_Decoder SHALL reject as forbidden value

### Requirement 23: Forbidden Pattern Detection

**User Story:** As a FLAC decoder, I want to detect forbidden bit patterns, so that I can reject invalid streams per RFC 9639.

#### Acceptance Criteria

1. WHEN metadata block type is 127, THE Native_FLAC_Decoder SHALL reject as forbidden pattern
2. WHEN streaminfo minimum block size is <16, THE Native_FLAC_Decoder SHALL reject as forbidden
3. WHEN streaminfo maximum block size is <16, THE Native_FLAC_Decoder SHALL reject as forbidden
4. WHEN sample rate bits are 0b1111, THE Native_FLAC_Decoder SHALL reject as forbidden pattern
5. WHEN uncommon block size is 65536, THE Native_FLAC_Decoder SHALL reject as forbidden pattern
6. WHEN predictor coefficient precision bits are 0b1111, THE Native_FLAC_Decoder SHALL reject as forbidden
7. WHEN predictor right shift is negative, THE Native_FLAC_Decoder SHALL reject as forbidden
8. WHEN reserved subframe type is encountered, THE Native_FLAC_Decoder SHALL reject frame

### Requirement 24: Numerical Precision and Overflow Handling

**User Story:** As a FLAC decoder, I want proper numerical precision, so that I can decode without overflow or precision loss.

#### Acceptance Criteria

1. WHEN applying LPC prediction, THE Native_FLAC_Decoder SHALL use 64-bit arithmetic to prevent overflow
2. WHEN computing mid-side decorrelation, THE Native_FLAC_Decoder SHALL handle side channel requiring extra bit
3. WHEN decoding residuals, THE Native_FLAC_Decoder SHALL ensure residuals fit in 32-bit signed integers
4. WHEN sample values exceed bit depth range, THE Native_FLAC_Decoder SHALL detect and report as invalid
5. WHEN performing arithmetic shifts, THE Native_FLAC_Decoder SHALL use arithmetic (not logical) shifts
6. WHEN handling signed samples, THE Native_FLAC_Decoder SHALL use two's complement representation
7. WHEN converting unsigned to signed, THE Native_FLAC_Decoder SHALL center samples around zero
8. WHEN overflow is detected, THE Native_FLAC_Decoder SHALL handle gracefully without undefined behavior

### Requirement 25: MD5 Checksum Validation

**User Story:** As a FLAC decoder, I want to validate MD5 checksums, so that I can verify decoded audio integrity.

#### Acceptance Criteria

1. WHEN streaminfo contains MD5 checksum, THE Native_FLAC_Decoder SHALL compute MD5 of decoded samples
2. WHEN computing MD5, THE Native_FLAC_Decoder SHALL interleave all channels on per-sample basis
3. WHEN computing MD5, THE Native_FLAC_Decoder SHALL use signed little-endian sample representation
4. WHEN bit depth is not byte-aligned, THE Native_FLAC_Decoder SHALL sign-extend samples to next byte boundary
5. WHEN stream decoding completes, THE Native_FLAC_Decoder SHALL compare computed MD5 with streaminfo MD5
6. WHEN MD5 checksum mismatches, THE Native_FLAC_Decoder SHALL report integrity error
7. WHEN streaminfo MD5 is zero, THE Native_FLAC_Decoder SHALL skip MD5 validation
8. WHEN MD5 validation is disabled, THE Native_FLAC_Decoder SHALL skip checksum computation for performance

### Requirement 26: Multi-Channel Audio Support

**User Story:** As a FLAC decoder, I want to support multi-channel audio, so that I can decode surround sound and other multi-channel formats.

#### Acceptance Criteria

1. WHEN channels bits are 0b0000, THE Native_FLAC_Decoder SHALL decode 1 channel (mono)
2. WHEN channels bits are 0b0001, THE Native_FLAC_Decoder SHALL decode 2 channels (stereo)
3. WHEN channels bits are 0b0010, THE Native_FLAC_Decoder SHALL decode 3 channels (left, right, center)
4. WHEN channels bits are 0b0011, THE Native_FLAC_Decoder SHALL decode 4 channels (quad)
5. WHEN channels bits are 0b0100, THE Native_FLAC_Decoder SHALL decode 5 channels (5.0 surround)
6. WHEN channels bits are 0b0101, THE Native_FLAC_Decoder SHALL decode 6 channels (5.1 surround)
7. WHEN channels bits are 0b0110, THE Native_FLAC_Decoder SHALL decode 7 channels (6.1 surround)
8. WHEN channels bits are 0b0111, THE Native_FLAC_Decoder SHALL decode 8 channels (7.1 surround)

### Requirement 27: Sample Rate Range Support

**User Story:** As a FLAC decoder, I want to support wide sample rate range, so that I can decode audio from 1 Hz to 1048575 Hz.

#### Acceptance Criteria

1. WHEN sample rate is 88.2 kHz (0b0001), THE Native_FLAC_Decoder SHALL decode at 88200 Hz
2. WHEN sample rate is 176.4 kHz (0b0010), THE Native_FLAC_Decoder SHALL decode at 176400 Hz
3. WHEN sample rate is 192 kHz (0b0011), THE Native_FLAC_Decoder SHALL decode at 192000 Hz
4. WHEN sample rate is 8 kHz (0b0100), THE Native_FLAC_Decoder SHALL decode at 8000 Hz
5. WHEN sample rate is 16 kHz (0b0101), THE Native_FLAC_Decoder SHALL decode at 16000 Hz
6. WHEN sample rate is 22.05 kHz (0b0110), THE Native_FLAC_Decoder SHALL decode at 22050 Hz
7. WHEN sample rate is 24 kHz (0b0111), THE Native_FLAC_Decoder SHALL decode at 24000 Hz
8. WHEN sample rate is 32/44.1/48/96 kHz, THE Native_FLAC_Decoder SHALL decode at standard rates

### Requirement 28: Predictor Coefficient Precision

**User Story:** As a FLAC decoder, I want to handle variable coefficient precision, so that I can decode LPC subframes with different precision levels.

#### Acceptance Criteria

1. WHEN coefficient precision bits are 0b0000, THE Native_FLAC_Decoder SHALL use 1-bit coefficients
2. WHEN coefficient precision bits are 0b0001-0b1110, THE Native_FLAC_Decoder SHALL use 2-15 bit coefficients
3. WHEN coefficient precision bits are 0b1111, THE Native_FLAC_Decoder SHALL reject as forbidden
4. WHEN reading coefficients, THE Native_FLAC_Decoder SHALL decode as signed two's complement
5. WHEN applying coefficients, THE Native_FLAC_Decoder SHALL multiply with past samples
6. WHEN summing products, THE Native_FLAC_Decoder SHALL use sufficient precision to prevent overflow
7. WHEN applying right shift, THE Native_FLAC_Decoder SHALL perform arithmetic shift on sum
8. WHEN right shift is negative, THE Native_FLAC_Decoder SHALL reject as forbidden per RFC 9639


### Requirement 29: Rice Partition Coding

**User Story:** As a FLAC decoder, I want to decode partitioned Rice-coded residuals, so that I can reconstruct prediction errors efficiently.

#### Acceptance Criteria

1. WHEN coding method is 0b00, THE Native_FLAC_Decoder SHALL use 4-bit Rice parameters
2. WHEN coding method is 0b01, THE Native_FLAC_Decoder SHALL use 5-bit Rice parameters
3. WHEN coding method is 0b10 or 0b11, THE Native_FLAC_Decoder SHALL reject as reserved
4. WHEN parsing partition order, THE Native_FLAC_Decoder SHALL read 4-bit unsigned partition order
5. WHEN partition order is N, THE Native_FLAC_Decoder SHALL decode 2^N partitions
6. WHEN calculating partition sample count, THE Native_FLAC_Decoder SHALL use (block_size >> partition_order) - predictor_order for first partition
7. WHEN calculating partition sample count, THE Native_FLAC_Decoder SHALL use (block_size >> partition_order) for remaining partitions
8. WHEN partition order is invalid, THE Native_FLAC_Decoder SHALL reject if block size not evenly divisible by partition count

### Requirement 30: Rice Escape Code Handling

**User Story:** As a FLAC decoder, I want to handle Rice escape codes, so that I can decode unencoded residual partitions.

#### Acceptance Criteria

1. WHEN 4-bit Rice parameter is 0b1111, THE Native_FLAC_Decoder SHALL treat as escape code
2. WHEN 5-bit Rice parameter is 0b11111, THE Native_FLAC_Decoder SHALL treat as escape code
3. WHEN escape code is encountered, THE Native_FLAC_Decoder SHALL read 5-bit sample bit width
4. WHEN sample bit width is 0, THE Native_FLAC_Decoder SHALL treat all partition samples as zero
5. WHEN sample bit width is >0, THE Native_FLAC_Decoder SHALL read samples as signed two's complement
6. WHEN decoding escaped partition, THE Native_FLAC_Decoder SHALL read fixed-length samples (not Rice-coded)
7. WHEN escaped partition completes, THE Native_FLAC_Decoder SHALL continue with next partition
8. WHEN escape code is used, THE Native_FLAC_Decoder SHALL handle without Rice decoding for that partition

### Requirement 31: Zigzag Encoding/Decoding

**User Story:** As a FLAC decoder, I want to decode zigzag-encoded residuals, so that I can convert unsigned Rice codes to signed residuals.

#### Acceptance Criteria

1. WHEN folded residual is even, THE Native_FLAC_Decoder SHALL compute residual = folded >> 1
2. WHEN folded residual is odd, THE Native_FLAC_Decoder SHALL compute residual = -((folded >> 1) + 1)
3. WHEN encoding positive residual, THE Native_FLAC_Decoder SHALL verify folded = residual * 2
4. WHEN encoding negative residual, THE Native_FLAC_Decoder SHALL verify folded = (-residual * 2) - 1
5. WHEN decoding Rice code, THE Native_FLAC_Decoder SHALL first decode to folded residual
6. WHEN unfolding residual, THE Native_FLAC_Decoder SHALL apply zigzag decoding formula
7. WHEN residual is zero, THE Native_FLAC_Decoder SHALL decode from folded value 0
8. WHEN handling signed residuals, THE Native_FLAC_Decoder SHALL use two's complement representation

### Requirement 32: Rice Code Decoding Algorithm

**User Story:** As a FLAC decoder, I want to decode Rice codes efficiently, so that I can extract residual samples from bitstream.

#### Acceptance Criteria

1. WHEN decoding Rice code, THE Native_FLAC_Decoder SHALL count zero bits until encountering one bit
2. WHEN one bit is encountered, THE Native_FLAC_Decoder SHALL read Rice parameter bits as remainder
3. WHEN computing folded value, THE Native_FLAC_Decoder SHALL compute (zero_count << rice_param) | remainder
4. WHEN Rice parameter is 0, THE Native_FLAC_Decoder SHALL decode unary-only codes
5. WHEN Rice parameter is large, THE Native_FLAC_Decoder SHALL handle long binary remainders
6. WHEN decoding partition, THE Native_FLAC_Decoder SHALL decode all partition samples consecutively
7. WHEN Rice code is invalid, THE Native_FLAC_Decoder SHALL detect bitstream errors
8. WHEN partition completes, THE Native_FLAC_Decoder SHALL verify correct sample count decoded

### Requirement 33: Frame Sync and Alignment

**User Story:** As a FLAC decoder, I want to synchronize to frame boundaries, so that I can decode streams starting at any position.

#### Acceptance Criteria

1. WHEN searching for sync, THE Native_FLAC_Decoder SHALL look for 0xFFF8-0xFFFF sync pattern
2. WHEN sync pattern is found, THE Native_FLAC_Decoder SHALL validate frame header CRC
3. WHEN frame header CRC fails, THE Native_FLAC_Decoder SHALL continue searching for next sync
4. WHEN frame header is valid, THE Native_FLAC_Decoder SHALL begin decoding frame
5. WHEN frame footer is reached, THE Native_FLAC_Decoder SHALL align to byte boundary with zero padding
6. WHEN zero padding is present, THE Native_FLAC_Decoder SHALL skip padding bits
7. WHEN frame CRC is validated, THE Native_FLAC_Decoder SHALL verify 16-bit CRC-16
8. WHEN mid-stream sync is needed, THE Native_FLAC_Decoder SHALL synchronize without prior metadata

### Requirement 34: Blocking Strategy Handling

**User Story:** As a FLAC decoder, I want to handle blocking strategy correctly, so that I can decode both fixed and variable block size streams.

#### Acceptance Criteria

1. WHEN blocking strategy bit is 0, THE Native_FLAC_Decoder SHALL treat stream as fixed block size
2. WHEN blocking strategy bit is 1, THE Native_FLAC_Decoder SHALL treat stream as variable block size
3. WHEN fixed block size stream, THE Native_FLAC_Decoder SHALL decode frame number from coded number
4. WHEN variable block size stream, THE Native_FLAC_Decoder SHALL decode sample number from coded number
5. WHEN frame number is decoded, THE Native_FLAC_Decoder SHALL verify sequential frame numbering
6. WHEN sample number is decoded, THE Native_FLAC_Decoder SHALL verify sequential sample numbering
7. WHEN block size changes in fixed stream, THE Native_FLAC_Decoder SHALL detect as error
8. WHEN block size varies in variable stream, THE Native_FLAC_Decoder SHALL handle correctly

### Requirement 35: Warm-up Sample Handling

**User Story:** As a FLAC decoder, I want to decode warm-up samples correctly, so that I can initialize predictors.

#### Acceptance Criteria

1. WHEN predictor order is N, THE Native_FLAC_Decoder SHALL decode N warm-up samples
2. WHEN decoding warm-up samples, THE Native_FLAC_Decoder SHALL read unencoded samples at subframe bit depth
3. WHEN warm-up samples are decoded, THE Native_FLAC_Decoder SHALL store for predictor initialization
4. WHEN applying predictor, THE Native_FLAC_Decoder SHALL use warm-up samples as initial history
5. WHEN warm-up sample count equals block size, THE Native_FLAC_Decoder SHALL handle edge case
6. WHEN warm-up samples exceed block size, THE Native_FLAC_Decoder SHALL reject as invalid
7. WHEN predictor order is 0, THE Native_FLAC_Decoder SHALL decode zero warm-up samples
8. WHEN warm-up samples are read, THE Native_FLAC_Decoder SHALL apply wasted bits padding after decoding

### Requirement 36: Subframe Bit Depth Calculation

**User Story:** As a FLAC decoder, I want to calculate subframe bit depth correctly, so that I can decode samples at proper precision.

#### Acceptance Criteria

1. WHEN calculating subframe bit depth, THE Native_FLAC_Decoder SHALL start with frame bit depth
2. WHEN wasted bits are used, THE Native_FLAC_Decoder SHALL subtract wasted bits from bit depth
3. WHEN subframe is side channel, THE Native_FLAC_Decoder SHALL add 1 bit to bit depth
4. WHEN subframe bit depth is ≤0, THE Native_FLAC_Decoder SHALL reject as invalid
5. WHEN decoding samples, THE Native_FLAC_Decoder SHALL use calculated subframe bit depth
6. WHEN reading unencoded samples, THE Native_FLAC_Decoder SHALL read at subframe bit depth
7. WHEN applying wasted bits, THE Native_FLAC_Decoder SHALL left-shift after decoding
8. WHEN bit depth calculation is complete, THE Native_FLAC_Decoder SHALL validate result is positive

### Requirement 37: Residual Sample Value Limits

**User Story:** As a FLAC decoder, I want to enforce residual value limits, so that I can use 32-bit integer arithmetic safely.

#### Acceptance Criteria

1. WHEN decoding residuals, THE Native_FLAC_Decoder SHALL verify values fit in 32-bit signed integer
2. WHEN residual absolute value is ≥2^31, THE Native_FLAC_Decoder SHALL reject as invalid
3. WHEN residual is most negative 32-bit value, THE Native_FLAC_Decoder SHALL reject as forbidden
4. WHEN all residuals are valid, THE Native_FLAC_Decoder SHALL process using 32-bit arithmetic
5. WHEN encoder produces invalid residuals, THE Native_FLAC_Decoder SHALL detect and report error
6. WHEN verbatim subframe is needed, THE Native_FLAC_Decoder SHALL handle samples exceeding residual limits
7. WHEN residual limits are enforced, THE Native_FLAC_Decoder SHALL simplify decoder implementation
8. WHEN processing residuals, THE Native_FLAC_Decoder SHALL avoid undefined behavior with most negative value

### Requirement 38: Integer-Only Arithmetic

**User Story:** As a FLAC decoder, I want to use integer-only arithmetic, so that I can maintain lossless decoding without floating-point errors.

#### Acceptance Criteria

1. WHEN performing all arithmetic, THE Native_FLAC_Decoder SHALL use integer data types exclusively
2. WHEN computing predictions, THE Native_FLAC_Decoder SHALL use integer multiplication and addition
3. WHEN applying shifts, THE Native_FLAC_Decoder SHALL use arithmetic (not logical) shifts
4. WHEN handling rounding, THE Native_FLAC_Decoder SHALL use integer rounding methods
5. WHEN avoiding overflow, THE Native_FLAC_Decoder SHALL use sufficiently large integer types
6. WHEN decoding is complete, THE Native_FLAC_Decoder SHALL produce bit-perfect output
7. WHEN floating-point is avoided, THE Native_FLAC_Decoder SHALL ensure lossless reconstruction
8. WHEN integer arithmetic is used, THE Native_FLAC_Decoder SHALL maintain deterministic behavior

### Requirement 39: Big-Endian Encoding

**User Story:** As a FLAC decoder, I want to handle big-endian encoding correctly, so that I can decode bitstream fields properly.

#### Acceptance Criteria

1. WHEN reading multi-bit fields, THE Native_FLAC_Decoder SHALL decode as big-endian
2. WHEN reading frame header fields, THE Native_FLAC_Decoder SHALL use big-endian byte order
3. WHEN reading metadata blocks, THE Native_FLAC_Decoder SHALL decode lengths as big-endian
4. WHEN reading predictor coefficients, THE Native_FLAC_Decoder SHALL decode as big-endian signed integers
5. WHEN reading sample values, THE Native_FLAC_Decoder SHALL decode as big-endian signed integers
6. WHEN Vorbis comment fields are encountered, THE Native_FLAC_Decoder SHALL use little-endian for field lengths
7. WHEN handling byte order, THE Native_FLAC_Decoder SHALL convert to host byte order as needed
8. WHEN encoding is specified, THE Native_FLAC_Decoder SHALL follow RFC 9639 byte order rules

### Requirement 40: Unary Coding

**User Story:** As a FLAC decoder, I want to decode unary-coded values correctly, so that I can extract variable-length fields.

#### Acceptance Criteria

1. WHEN decoding unary value, THE Native_FLAC_Decoder SHALL count zero bits until one bit
2. WHEN one bit is encountered, THE Native_FLAC_Decoder SHALL terminate unary decoding
3. WHEN unary value is 5, THE Native_FLAC_Decoder SHALL decode as 0b000001
4. WHEN unary value is 0, THE Native_FLAC_Decoder SHALL decode as 0b1
5. WHEN decoding wasted bits, THE Native_FLAC_Decoder SHALL use unary encoding
6. WHEN decoding Rice quotient, THE Native_FLAC_Decoder SHALL use unary encoding
7. WHEN unary code is excessively long, THE Native_FLAC_Decoder SHALL detect potential DoS attack
8. WHEN unary decoding completes, THE Native_FLAC_Decoder SHALL return decoded value

### Requirement 41: Metadata Block Parsing

**User Story:** As a FLAC decoder, I want to parse metadata blocks correctly, so that I can extract stream information and tags.

#### Acceptance Criteria

1. WHEN parsing metadata header, THE Native_FLAC_Decoder SHALL read 1-bit last-block flag
2. WHEN parsing metadata header, THE Native_FLAC_Decoder SHALL read 7-bit block type
3. WHEN parsing metadata header, THE Native_FLAC_Decoder SHALL read 24-bit block length
4. WHEN block type is 0, THE Native_FLAC_Decoder SHALL parse STREAMINFO block
5. WHEN block type is 1-6, THE Native_FLAC_Decoder SHALL parse corresponding metadata type
6. WHEN block type is 7-126, THE Native_FLAC_Decoder SHALL skip reserved block
7. WHEN block type is 127, THE Native_FLAC_Decoder SHALL reject as forbidden
8. WHEN last-block flag is set, THE Native_FLAC_Decoder SHALL expect audio frames next

### Requirement 42: STREAMINFO Metadata Parsing

**User Story:** As a FLAC decoder, I want to parse STREAMINFO metadata, so that I can configure decoder for stream parameters.

#### Acceptance Criteria

1. WHEN parsing STREAMINFO, THE Native_FLAC_Decoder SHALL read 16-bit minimum block size
2. WHEN parsing STREAMINFO, THE Native_FLAC_Decoder SHALL read 16-bit maximum block size
3. WHEN parsing STREAMINFO, THE Native_FLAC_Decoder SHALL read 24-bit minimum frame size
4. WHEN parsing STREAMINFO, THE Native_FLAC_Decoder SHALL read 24-bit maximum frame size
5. WHEN parsing STREAMINFO, THE Native_FLAC_Decoder SHALL read 20-bit sample rate
6. WHEN parsing STREAMINFO, THE Native_FLAC_Decoder SHALL read 3-bit channel count minus 1
7. WHEN parsing STREAMINFO, THE Native_FLAC_Decoder SHALL read 5-bit bits per sample minus 1
8. WHEN parsing STREAMINFO, THE Native_FLAC_Decoder SHALL read 36-bit total samples and 128-bit MD5

### Requirement 43: Seek Table Support

**User Story:** As a FLAC decoder, I want to use seek tables for efficient seeking, so that I can jump to specific positions quickly.

#### Acceptance Criteria

1. WHEN seek table is present, THE Native_FLAC_Decoder SHALL parse seek points
2. WHEN parsing seek point, THE Native_FLAC_Decoder SHALL read 64-bit sample number
3. WHEN parsing seek point, THE Native_FLAC_Decoder SHALL read 64-bit byte offset
4. WHEN parsing seek point, THE Native_FLAC_Decoder SHALL read 16-bit frame sample count
5. WHEN sample number is 0xFFFFFFFFFFFFFFFF, THE Native_FLAC_Decoder SHALL treat as placeholder
6. WHEN seeking to position, THE Native_FLAC_Decoder SHALL use seek table for fast navigation
7. WHEN seek table is absent, THE Native_FLAC_Decoder SHALL support seeking via frame scanning
8. WHEN seek points are used, THE Native_FLAC_Decoder SHALL verify ascending sample number order

### Requirement 44: Vorbis Comment Parsing

**User Story:** As a FLAC decoder, I want to parse Vorbis comments, so that I can extract metadata tags.

#### Acceptance Criteria

1. WHEN parsing Vorbis comment, THE Native_FLAC_Decoder SHALL read 32-bit vendor string length (little-endian)
2. WHEN parsing Vorbis comment, THE Native_FLAC_Decoder SHALL read UTF-8 vendor string
3. WHEN parsing Vorbis comment, THE Native_FLAC_Decoder SHALL read 32-bit field count (little-endian)
4. WHEN parsing fields, THE Native_FLAC_Decoder SHALL read 32-bit field length for each field (little-endian)
5. WHEN parsing field, THE Native_FLAC_Decoder SHALL read UTF-8 field name and content separated by =
6. WHEN field name is parsed, THE Native_FLAC_Decoder SHALL accept printable ASCII except =
7. WHEN comparing field names, THE Native_FLAC_Decoder SHALL use case-insensitive comparison
8. WHEN Vorbis comment is complete, THE Native_FLAC_Decoder SHALL provide metadata to application

### Requirement 45: Padding and Application Blocks

**User Story:** As a FLAC decoder, I want to handle padding and application blocks, so that I can skip non-essential metadata.

#### Acceptance Criteria

1. WHEN padding block is encountered, THE Native_FLAC_Decoder SHALL skip block length bytes
2. WHEN application block is encountered, THE Native_FLAC_Decoder SHALL read 32-bit application ID
3. WHEN application block is encountered, THE Native_FLAC_Decoder SHALL skip or parse application data
4. WHEN padding is used, THE Native_FLAC_Decoder SHALL verify all padding bits are zero
5. WHEN application ID is recognized, THE Native_FLAC_Decoder SHALL optionally parse application data
6. WHEN application ID is unknown, THE Native_FLAC_Decoder SHALL skip application data
7. WHEN multiple padding blocks exist, THE Native_FLAC_Decoder SHALL handle all padding blocks
8. WHEN metadata blocks are skipped, THE Native_FLAC_Decoder SHALL advance to next block correctly

### Requirement 46: Picture Metadata Support

**User Story:** As a FLAC decoder, I want to parse picture metadata, so that I can extract embedded album art.

#### Acceptance Criteria

1. WHEN picture block is encountered, THE Native_FLAC_Decoder SHALL read 32-bit picture type
2. WHEN parsing picture, THE Native_FLAC_Decoder SHALL read 32-bit MIME type length
3. WHEN parsing picture, THE Native_FLAC_Decoder SHALL read UTF-8 MIME type string
4. WHEN parsing picture, THE Native_FLAC_Decoder SHALL read 32-bit description length
5. WHEN parsing picture, THE Native_FLAC_Decoder SHALL read UTF-8 description string
6. WHEN parsing picture, THE Native_FLAC_Decoder SHALL read width, height, depth, and color count
7. WHEN parsing picture, THE Native_FLAC_Decoder SHALL read 32-bit picture data length
8. WHEN parsing picture, THE Native_FLAC_Decoder SHALL read picture data bytes

### Requirement 47: Cuesheet Metadata Support

**User Story:** As a FLAC decoder, I want to parse cuesheet metadata, so that I can extract track and index information.

#### Acceptance Criteria

1. WHEN cuesheet block is encountered, THE Native_FLAC_Decoder SHALL read 128-byte media catalog number
2. WHEN parsing cuesheet, THE Native_FLAC_Decoder SHALL read 64-bit lead-in sample count
3. WHEN parsing cuesheet, THE Native_FLAC_Decoder SHALL read 1-bit CD flag
4. WHEN parsing cuesheet, THE Native_FLAC_Decoder SHALL read track count
5. WHEN parsing tracks, THE Native_FLAC_Decoder SHALL read track offset, number, and ISRC
6. WHEN parsing tracks, THE Native_FLAC_Decoder SHALL read track type and pre-emphasis flags
7. WHEN parsing track indices, THE Native_FLAC_Decoder SHALL read index offset and number
8. WHEN cuesheet is complete, THE Native_FLAC_Decoder SHALL provide track information to application

### Requirement 48: Security and DoS Protection

**User Story:** As a secure FLAC decoder, I want to protect against malicious files, so that I can prevent DoS attacks and buffer overflows.

#### Acceptance Criteria

1. WHEN allocating memory, THE Native_FLAC_Decoder SHALL validate sizes before allocation
2. WHEN parsing metadata, THE Native_FLAC_Decoder SHALL enforce reasonable limits on field counts
3. WHEN decoding frames, THE Native_FLAC_Decoder SHALL prevent excessive resource utilization
4. WHEN seeking, THE Native_FLAC_Decoder SHALL validate coded numbers to prevent infinite loops
5. WHEN parsing lengths, THE Native_FLAC_Decoder SHALL verify lengths don't exceed block size
6. WHEN following URIs, THE Native_FLAC_Decoder SHALL NOT follow without explicit user approval
7. WHEN handling extreme cases, THE Native_FLAC_Decoder SHALL impose limits beyond RFC minimums
8. WHEN malformed data is detected, THE Native_FLAC_Decoder SHALL fail safely without crashes

### Requirement 49: Ogg Container Support

**User Story:** As a FLAC decoder, I want to decode Ogg FLAC streams, so that I can support FLAC in Ogg containers.

#### Acceptance Criteria

1. WHEN Ogg FLAC is detected, THE Native_FLAC_Decoder SHALL parse Ogg FLAC header packet
2. WHEN parsing Ogg header, THE Native_FLAC_Decoder SHALL verify 0x7F 0x46 0x4C 0x41 0x43 signature
3. WHEN parsing Ogg header, THE Native_FLAC_Decoder SHALL read version number (0x01 0x00)
4. WHEN parsing Ogg header, THE Native_FLAC_Decoder SHALL read header packet count
5. WHEN parsing Ogg metadata, THE Native_FLAC_Decoder SHALL parse metadata blocks from header packets
6. WHEN parsing Ogg audio, THE Native_FLAC_Decoder SHALL decode FLAC frames from audio packets
7. WHEN handling Ogg pages, THE Native_FLAC_Decoder SHALL use granule position for sample numbering
8. WHEN Ogg stream changes properties, THE Native_FLAC_Decoder SHALL handle chained Ogg streams

### Requirement 50: Interoperability and Compatibility

**User Story:** As a FLAC decoder, I want maximum interoperability, so that I can decode files from various encoders.

#### Acceptance Criteria

1. WHEN decoding common files, THE Native_FLAC_Decoder SHALL prioritize streamable subset compatibility
2. WHEN variable block size is used, THE Native_FLAC_Decoder SHALL handle correctly despite rarity
3. WHEN 5-bit Rice parameters are used, THE Native_FLAC_Decoder SHALL decode correctly
4. WHEN Rice escape codes are used, THE Native_FLAC_Decoder SHALL handle unencoded partitions
5. WHEN uncommon block sizes are used, THE Native_FLAC_Decoder SHALL decode correctly
6. WHEN uncommon bit depths are used, THE Native_FLAC_Decoder SHALL handle 4-32 bit range
7. WHEN multi-channel audio is used, THE Native_FLAC_Decoder SHALL support up to 8 channels
8. WHEN properties change mid-stream, THE Native_FLAC_Decoder SHALL handle via stream chaining


### Requirement 51: LPC Coefficient Quantization

**User Story:** As a FLAC decoder, I want to handle LPC coefficient quantization correctly, so that I can apply linear prediction accurately.

#### Acceptance Criteria

1. WHEN reading coefficient precision, THE Native_FLAC_Decoder SHALL decode 4-bit precision field as (precision - 1)
2. WHEN coefficient precision is 0b1111, THE Native_FLAC_Decoder SHALL reject as forbidden value
3. WHEN coefficient precision is 0b0000-0b1110, THE Native_FLAC_Decoder SHALL use 1-15 bit precision
4. WHEN reading coefficients, THE Native_FLAC_Decoder SHALL decode each as signed two's complement
5. WHEN applying coefficients, THE Native_FLAC_Decoder SHALL multiply each with corresponding past sample
6. WHEN summing products, THE Native_FLAC_Decoder SHALL accumulate all coefficient*sample products
7. WHEN applying right shift, THE Native_FLAC_Decoder SHALL shift accumulated sum right by shift amount
8. WHEN right shift is negative, THE Native_FLAC_Decoder SHALL reject as forbidden per RFC 9639

### Requirement 52: LPC Coefficient Ordering

**User Story:** As a FLAC decoder, I want to apply LPC coefficients in correct order, so that prediction is accurate.

#### Acceptance Criteria

1. WHEN reading coefficients from bitstream, THE Native_FLAC_Decoder SHALL read in reverse chronological order
2. WHEN first coefficient is read, THE Native_FLAC_Decoder SHALL apply to most recent past sample
3. WHEN second coefficient is read, THE Native_FLAC_Decoder SHALL apply to second most recent past sample
4. WHEN applying prediction, THE Native_FLAC_Decoder SHALL multiply coeff[0] with sample[n-1]
5. WHEN applying prediction, THE Native_FLAC_Decoder SHALL multiply coeff[1] with sample[n-2]
6. WHEN applying prediction, THE Native_FLAC_Decoder SHALL continue pattern for all coefficients
7. WHEN LPC order is N, THE Native_FLAC_Decoder SHALL use N past samples in reverse chronological order
8. WHEN coefficient ordering is incorrect, THE Native_FLAC_Decoder SHALL produce incorrect predictions

### Requirement 53: Partition Sample Count Calculation

**User Story:** As a FLAC decoder, I want to calculate partition sample counts correctly, so that I can decode residual partitions.

#### Acceptance Criteria

1. WHEN calculating first partition size, THE Native_FLAC_Decoder SHALL compute (block_size >> partition_order) - predictor_order
2. WHEN calculating remaining partition sizes, THE Native_FLAC_Decoder SHALL compute (block_size >> partition_order)
3. WHEN partition order is 0, THE Native_FLAC_Decoder SHALL have single partition with (block_size - predictor_order) samples
4. WHEN partition order is N, THE Native_FLAC_Decoder SHALL have 2^N partitions
5. WHEN block size is not evenly divisible, THE Native_FLAC_Decoder SHALL reject as invalid
6. WHEN (block_size >> partition_order) ≤ predictor_order, THE Native_FLAC_Decoder SHALL reject as invalid
7. WHEN partition sample counts are calculated, THE Native_FLAC_Decoder SHALL verify total equals (block_size - predictor_order)
8. WHEN odd block size is used, THE Native_FLAC_Decoder SHALL only allow partition order 0

### Requirement 54: Sequential Decoding Requirement

**User Story:** As a FLAC decoder, I want to decode samples sequentially, so that predictions can use previously decoded samples.

#### Acceptance Criteria

1. WHEN decoding subframe with predictor, THE Native_FLAC_Decoder SHALL decode samples in sequential order
2. WHEN computing prediction for sample N, THE Native_FLAC_Decoder SHALL use fully decoded samples 0 through N-1
3. WHEN parallel decoding is attempted, THE Native_FLAC_Decoder SHALL recognize it is not possible for predicted subframes
4. WHEN warm-up samples are decoded, THE Native_FLAC_Decoder SHALL decode before residual samples
5. WHEN residual is decoded, THE Native_FLAC_Decoder SHALL add to prediction sequentially
6. WHEN subframe decoding completes, THE Native_FLAC_Decoder SHALL have decoded all samples in order
7. WHEN constant or verbatim subframes are used, THE Native_FLAC_Decoder MAY decode in parallel
8. WHEN sequential decoding is required, THE Native_FLAC_Decoder SHALL not skip ahead in sample stream

### Requirement 55: Frame and Subframe Byte Alignment

**User Story:** As a FLAC decoder, I want to handle byte alignment correctly, so that I can parse frames and padding properly.

#### Acceptance Criteria

1. WHEN frame starts, THE Native_FLAC_Decoder SHALL expect frame sync on byte boundary
2. WHEN subframes complete, THE Native_FLAC_Decoder SHALL check if byte-aligned
3. WHEN subframes are not byte-aligned, THE Native_FLAC_Decoder SHALL read zero padding bits
4. WHEN padding bits are present, THE Native_FLAC_Decoder SHALL verify all padding bits are zero
5. WHEN byte alignment is reached, THE Native_FLAC_Decoder SHALL read 16-bit frame CRC
6. WHEN frame CRC is read, THE Native_FLAC_Decoder SHALL be at byte boundary
7. WHEN next frame starts, THE Native_FLAC_Decoder SHALL expect sync code at byte boundary
8. WHEN padding bits are non-zero, THE Native_FLAC_Decoder SHALL treat as potential corruption

### Requirement 56: CRC Polynomial Implementation

**User Story:** As a FLAC decoder, I want to implement CRC polynomials correctly, so that I can validate frame integrity.

#### Acceptance Criteria

1. WHEN computing CRC-8, THE Native_FLAC_Decoder SHALL use polynomial x^8 + x^2 + x^1 + x^0 (0x07)
2. WHEN computing CRC-16, THE Native_FLAC_Decoder SHALL use polynomial x^16 + x^15 + x^2 + x^0 (0x8005)
3. WHEN initializing CRC-8, THE Native_FLAC_Decoder SHALL initialize to 0
4. WHEN initializing CRC-16, THE Native_FLAC_Decoder SHALL initialize to 0
5. WHEN computing frame header CRC, THE Native_FLAC_Decoder SHALL include sync code through coded number
6. WHEN computing frame CRC, THE Native_FLAC_Decoder SHALL include entire frame except CRC itself
7. WHEN using CRC tables, THE Native_FLAC_Decoder SHALL precompute lookup tables for performance
8. WHEN CRC validation is enabled, THE Native_FLAC_Decoder SHALL compute and compare CRCs

### Requirement 57: Sample Value Range Validation

**User Story:** As a FLAC decoder, I want to validate sample values, so that I can detect corrupted or invalid data.

#### Acceptance Criteria

1. WHEN sample is decoded, THE Native_FLAC_Decoder SHALL verify value fits in bit depth range
2. WHEN bit depth is N, THE Native_FLAC_Decoder SHALL verify sample is in range [-2^(N-1), 2^(N-1)-1]
3. WHEN sample exceeds range, THE Native_FLAC_Decoder SHALL treat as invalid data
4. WHEN all samples are valid, THE Native_FLAC_Decoder SHALL continue decoding
5. WHEN invalid sample is detected, THE Native_FLAC_Decoder SHALL report error
6. WHEN validation is disabled, THE Native_FLAC_Decoder MAY skip range checks for performance
7. WHEN sample validation fails, THE Native_FLAC_Decoder SHALL handle gracefully
8. WHEN range validation is performed, THE Native_FLAC_Decoder SHALL check before output

### Requirement 58: Minimum and Maximum Block Size Constraints

**User Story:** As a FLAC decoder, I want to enforce block size constraints, so that I can reject invalid streams.

#### Acceptance Criteria

1. WHEN minimum block size is <16, THE Native_FLAC_Decoder SHALL reject as forbidden (except last block)
2. WHEN maximum block size is <16, THE Native_FLAC_Decoder SHALL reject as forbidden
3. WHEN maximum block size is >65535, THE Native_FLAC_Decoder SHALL reject as invalid
4. WHEN minimum block size > maximum block size, THE Native_FLAC_Decoder SHALL reject as invalid
5. WHEN frame block size < minimum, THE Native_FLAC_Decoder SHALL reject (except last frame)
6. WHEN frame block size > maximum, THE Native_FLAC_Decoder SHALL reject as invalid
7. WHEN last frame block size ≤ maximum, THE Native_FLAC_Decoder SHALL accept regardless of minimum
8. WHEN block size constraints are violated, THE Native_FLAC_Decoder SHALL report error

### Requirement 59: Fixed vs Variable Block Size Streams

**User Story:** As a FLAC decoder, I want to distinguish fixed and variable block size streams, so that I can interpret coded numbers correctly.

#### Acceptance Criteria

1. WHEN streaminfo min block size equals max block size, THE Native_FLAC_Decoder SHALL recognize fixed block size stream
2. WHEN streaminfo min block size differs from max, THE Native_FLAC_Decoder SHALL recognize variable block size stream
3. WHEN blocking strategy bit is 0, THE Native_FLAC_Decoder SHALL interpret coded number as frame number
4. WHEN blocking strategy bit is 1, THE Native_FLAC_Decoder SHALL interpret coded number as sample number
5. WHEN fixed block size stream, THE Native_FLAC_Decoder SHALL verify frame numbers are sequential
6. WHEN variable block size stream, THE Native_FLAC_Decoder SHALL verify sample numbers are sequential
7. WHEN block size changes in fixed stream, THE Native_FLAC_Decoder SHALL detect as error
8. WHEN block size varies in variable stream, THE Native_FLAC_Decoder SHALL handle correctly

### Requirement 60: Minimum and Maximum Frame Size Handling

**User Story:** As a FLAC decoder, I want to handle frame size information, so that I can optimize buffering and seeking.

#### Acceptance Criteria

1. WHEN streaminfo minimum frame size is 0, THE Native_FLAC_Decoder SHALL treat as unknown
2. WHEN streaminfo maximum frame size is 0, THE Native_FLAC_Decoder SHALL treat as unknown
3. WHEN frame sizes are known, THE Native_FLAC_Decoder MAY use for buffer allocation optimization
4. WHEN frame sizes are unknown, THE Native_FLAC_Decoder SHALL handle frames of any valid size
5. WHEN actual frame size < minimum, THE Native_FLAC_Decoder SHALL accept (encoder may have estimated)
6. WHEN actual frame size > maximum, THE Native_FLAC_Decoder SHALL accept (encoder may have estimated)
7. WHEN frame size information is used, THE Native_FLAC_Decoder SHALL not rely on it for correctness
8. WHEN frame size is invalid, THE Native_FLAC_Decoder SHALL detect via other validation mechanisms

### Requirement 61: Total Samples Handling

**User Story:** As a FLAC decoder, I want to handle total sample count, so that I can report stream duration and progress.

#### Acceptance Criteria

1. WHEN streaminfo total samples is 0, THE Native_FLAC_Decoder SHALL treat as unknown
2. WHEN streaminfo total samples is >0, THE Native_FLAC_Decoder SHALL use for duration calculation
3. WHEN calculating duration, THE Native_FLAC_Decoder SHALL divide total samples by sample rate
4. WHEN reporting progress, THE Native_FLAC_Decoder SHALL use decoded sample count vs total samples
5. WHEN total samples is unknown, THE Native_FLAC_Decoder SHALL report unknown duration
6. WHEN actual samples differ from total, THE Native_FLAC_Decoder SHALL handle gracefully
7. WHEN seeking, THE Native_FLAC_Decoder MAY use total samples for validation
8. WHEN total samples is provided, THE Native_FLAC_Decoder SHALL treat as informational not mandatory

### Requirement 62: Non-Audio Data Support

**User Story:** As a FLAC decoder, I want to support non-audio data encoding, so that I can handle FLAC used for non-audio purposes.

#### Acceptance Criteria

1. WHEN sample rate is 0, THE Native_FLAC_Decoder SHALL recognize non-audio data
2. WHEN non-audio data is detected, THE Native_FLAC_Decoder SHALL decode without audio-specific assumptions
3. WHEN non-audio data is used, THE Native_FLAC_Decoder SHALL check metadata for data description
4. WHEN sample rate is 0 with audio, THE Native_FLAC_Decoder SHALL reject as invalid
5. WHEN non-audio data is decoded, THE Native_FLAC_Decoder SHALL maintain lossless properties
6. WHEN non-audio data is present, THE Native_FLAC_Decoder SHALL not assume time-based interpretation
7. WHEN application metadata describes non-audio, THE Native_FLAC_Decoder SHALL parse if supported
8. WHEN non-audio data is encountered, THE Native_FLAC_Decoder SHALL handle as generic sample stream

### Requirement 63: ID3v2 Tag Skipping

**User Story:** As a FLAC decoder, I want to skip ID3v2 tags, so that I can find FLAC stream marker in files with ID3 tags.

#### Acceptance Criteria

1. WHEN file starts with "ID3", THE Native_FLAC_Decoder SHALL recognize ID3v2 tag
2. WHEN ID3v2 tag is detected, THE Native_FLAC_Decoder SHALL parse tag header for size
3. WHEN tag size is determined, THE Native_FLAC_Decoder SHALL skip tag bytes
4. WHEN tag is skipped, THE Native_FLAC_Decoder SHALL search for fLaC marker
5. WHEN fLaC marker is found, THE Native_FLAC_Decoder SHALL begin normal FLAC parsing
6. WHEN ID3v2 tag is malformed, THE Native_FLAC_Decoder SHALL attempt to find fLaC marker anyway
7. WHEN multiple ID3 tags are present, THE Native_FLAC_Decoder SHALL skip all before fLaC
8. WHEN ID3 tag skipping fails, THE Native_FLAC_Decoder SHALL report error

### Requirement 64: Decoder State Management

**User Story:** As a FLAC decoder, I want to manage decoder state properly, so that I can handle seeking and errors correctly.

#### Acceptance Criteria

1. WHEN decoder is initialized, THE Native_FLAC_Decoder SHALL set state to uninitialized
2. WHEN streaminfo is parsed, THE Native_FLAC_Decoder SHALL transition to initialized state
3. WHEN decoding frames, THE Native_FLAC_Decoder SHALL maintain decoding state
4. WHEN seek is requested, THE Native_FLAC_Decoder SHALL reset frame-level state
5. WHEN error occurs, THE Native_FLAC_Decoder SHALL transition to error state
6. WHEN reset is requested, THE Native_FLAC_Decoder SHALL clear all state and buffers
7. WHEN decoder is destroyed, THE Native_FLAC_Decoder SHALL ensure clean state
8. WHEN state transitions occur, THE Native_FLAC_Decoder SHALL maintain consistency

### Requirement 65: Memory Management and Buffer Allocation

**User Story:** As a FLAC decoder, I want to manage memory efficiently, so that I can decode without excessive memory usage.

#### Acceptance Criteria

1. WHEN allocating buffers, THE Native_FLAC_Decoder SHALL base size on maximum block size
2. WHEN maximum block size is unknown, THE Native_FLAC_Decoder SHALL use reasonable default
3. WHEN allocating for channels, THE Native_FLAC_Decoder SHALL allocate per-channel buffers
4. WHEN allocating for bit depth, THE Native_FLAC_Decoder SHALL use appropriate integer size
5. WHEN memory allocation fails, THE Native_FLAC_Decoder SHALL return error and clean up
6. WHEN buffers are no longer needed, THE Native_FLAC_Decoder SHALL free memory promptly
7. WHEN reallocating buffers, THE Native_FLAC_Decoder SHALL handle size changes gracefully
8. WHEN decoder is destroyed, THE Native_FLAC_Decoder SHALL free all allocated memory

### Requirement 66: Backward Compatibility

**User Story:** As a FLAC decoder, I want to handle legacy format variations, so that I can decode older FLAC files.

#### Acceptance Criteria

1. WHEN pre-2007 variable block size file is encountered, THE Native_FLAC_Decoder SHALL attempt to decode
2. WHEN blocking strategy bit is missing, THE Native_FLAC_Decoder SHALL infer from streaminfo
3. WHEN old block size encoding is used, THE Native_FLAC_Decoder SHALL handle both old and new patterns
4. WHEN 4-bit Rice parameters only are used, THE Native_FLAC_Decoder SHALL decode correctly
5. WHEN negative LPC shift is encountered, THE Native_FLAC_Decoder SHALL reject as forbidden
6. WHEN legacy features are detected, THE Native_FLAC_Decoder SHALL log compatibility warnings
7. WHEN modern features are used, THE Native_FLAC_Decoder SHALL decode per current RFC
8. WHEN backward compatibility is maintained, THE Native_FLAC_Decoder SHALL decode all valid FLAC files

### Requirement 67: Fuzz Testing and Security Hardening

**User Story:** As a secure FLAC decoder, I want to be resilient against malicious input, so that I can handle fuzz testing and attacks.

#### Acceptance Criteria

1. WHEN fuzz testing is performed, THE Native_FLAC_Decoder SHALL not crash or hang
2. WHEN malformed input is provided, THE Native_FLAC_Decoder SHALL detect and reject gracefully
3. WHEN excessive resource usage is attempted, THE Native_FLAC_Decoder SHALL impose limits
4. WHEN buffer overflows are attempted, THE Native_FLAC_Decoder SHALL prevent with bounds checking
5. WHEN infinite loops are attempted, THE Native_FLAC_Decoder SHALL detect and break
6. WHEN CRC checks are disabled for fuzzing, THE Native_FLAC_Decoder SHALL still validate structure
7. WHEN sanitizers are used, THE Native_FLAC_Decoder SHALL pass without errors
8. WHEN security testing is performed, THE Native_FLAC_Decoder SHALL demonstrate robustness

### Requirement 68: Reference Implementation Compatibility

**User Story:** As a FLAC decoder, I want to match reference implementation behavior, so that I can ensure compatibility.

#### Acceptance Criteria

1. WHEN decoding test files, THE Native_FLAC_Decoder SHALL produce identical output to libFLAC
2. WHEN edge cases are encountered, THE Native_FLAC_Decoder SHALL match libFLAC behavior
3. WHEN errors are detected, THE Native_FLAC_Decoder SHALL report similar errors to libFLAC
4. WHEN performance is measured, THE Native_FLAC_Decoder SHALL achieve comparable speed to libFLAC
5. WHEN memory usage is measured, THE Native_FLAC_Decoder SHALL use similar memory to libFLAC
6. WHEN seeking is performed, THE Native_FLAC_Decoder SHALL match libFLAC seeking behavior
7. WHEN metadata is parsed, THE Native_FLAC_Decoder SHALL extract same information as libFLAC
8. WHEN compatibility is tested, THE Native_FLAC_Decoder SHALL pass all libFLAC test cases

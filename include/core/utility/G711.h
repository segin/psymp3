/*
 * G711.h - G.711 A-law and mu-law conversion utilities.
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CORE_UTILITY_G711_H
#define PSYMP3_CORE_UTILITY_G711_H

#include <cstdint>

namespace PsyMP3 {
namespace Core {
namespace Utility {
namespace G711 {

/**
 * @brief Converts an 8-bit A-law sample to a 16-bit linear PCM sample.
 * @param alawbyte The 8-bit A-law encoded sample.
 * @return The 16-bit signed linear PCM sample.
 */
inline int16_t alaw2linear(uint8_t alawbyte)
{
	int16_t sign, exponent, mantissa, sample;

	alawbyte ^= 0x55;
	sign = (alawbyte & 0x80);
	exponent = (alawbyte >> 4) & 0x07;
	mantissa = alawbyte & 0x0F;
	if(exponent == 0)
		sample = mantissa << 4;
	else
		sample = ( (mantissa << 4) | 0x100 ) << (exponent - 1);

	if(sign == 0)
		sample = -sample;

	return sample;
}

/**
 * @brief Converts an 8-bit mu-law sample to a 16-bit linear PCM sample.
 * @param ulawbyte The 8-bit mu-law encoded sample.
 * @return The 16-bit signed linear PCM sample.
 */
inline int16_t ulaw2linear(uint8_t ulawbyte)
{
	static const int exp_lut[8] = {0, 132, 396, 924, 1980, 4092, 8316, 16764};
	int sign, exponent, mantissa, sample;
	ulawbyte = ~ulawbyte;
	sign = (ulawbyte & 0x80);
	exponent = (ulawbyte >> 4) & 0x07;
	mantissa = ulawbyte & 0x0F;
	sample = exp_lut[exponent] + (mantissa << (exponent + 3));
	return sign ? -sample : sample;
}

} // namespace G711
} // namespace Utility
} // namespace Core
} // namespace PsyMP3

#endif // PSYMP3_CORE_UTILITY_G711_H

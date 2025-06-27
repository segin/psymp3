/*
 * fft.h - Class header for the FFT class
 * This file is part of PsyMP3.
 * Copyright © 2011-2024 Kirn Gill <segin2005@gmail.com>
 * Copyright © 2011-2024 Mattis Michel <sic_zer0@hotmail.com>
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

#ifndef FFT_H
#define FFT_H

class FFT {
	private:
		std::vector<float> real, imag;
		int size;
		bool m_use_optimized_fft_flag; // New flag to toggle FFT implementation
		std::vector<float> m_twiddle_cos; // Precomputed cosine factors
		std::vector<float> m_twiddle_sin; // Precomputed sine factors

		static unsigned int bitreverse(unsigned int in, int bits);
		void precompute_twiddle_factors(); // New method to precompute twiddle factors
		void original_fft_impl(float *output, const float *input); // Renamed original FFT
		void optimized_fft_impl(float *output, const float *input); // New, optimized FFT

	public:
		FFT(int size); // size must be a power of 2 (!)
		void fft(float *output, const float *input); // Dispatcher method
		void setUseOptimizedFFT(bool use_optimized); // Setter for the flag
};

#endif // FFT_H

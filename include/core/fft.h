/*
 * fft.h - Class header for the FFT class
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 * Copyright © 2011-2025 Mattis Michel <sic_zer0@hotmail.com>
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

namespace PsyMP3 {
namespace Core {


enum class FFTMode {
    Original, // mat-og
    Optimized, // vibe-1
    NeomatIn, // neomat-in
    NeomatOut // neomat-out
};

class FFT {
	private:
		std::vector<float> real, imag;
		int size;
		FFTMode m_current_fft_mode; // Changed from bool to enum
		std::vector<float> m_twiddle_cos; // Precomputed cosine factors
		std::vector<float> m_twiddle_sin; // Precomputed sine factors

		// New members for Neomat implementations
		std::vector<std::complex<float>> m_neomat_twiddle_factors;
		std::vector<std::complex<float>> m_complex_buffer; // Used for both in-place and out-of-place
		std::vector<std::complex<float>> m_complex_output_buffer; // Only for NeomatOut

		static unsigned int bitreverse(unsigned int in, int bits);
		void precompute_twiddle_factors(); // New method to precompute twiddle factors
		void neomat_fft_init_twiddle_factors(); // New method to precompute twiddle factors for Neomat
		void original_fft_impl(float *output, const float *input); // Renamed original FFT
		void optimized_fft_impl(float *output, const float *input); // New, optimized FFT
		void neomat_in_place_fft_impl(float *output, const float *input);
		void neomat_out_of_place_fft_impl(float *output, const float *input);

	public:
		FFT(int size); // size must be a power of 2 (!)
		void fft(float *output, const float *input); // Dispatcher method (now uses m_current_fft_mode)
		void setFFTMode(FFTMode mode); // Setter for the mode
		FFTMode getFFTMode() const; // Getter for the mode
		std::string getFFTModeName() const; // Getter for the mode name string
};

} // namespace Core
} // namespace PsyMP3

#endif // FFT_H

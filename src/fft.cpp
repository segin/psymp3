/*
 * fft.cpp - Class implementation for the FFT class
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

#include "psymp3.h"

// Define I for float complex
static const std::complex<float> I_f(0.0f, 1.0f);

/**
 * @brief Constructs an FFT object for a given size.
 * @param size The number of samples for the FFT, which must be a power of two.
 * This constructor initializes internal buffers and precomputes the "twiddle factors"
 * (complex roots of unity) used by the various FFT algorithms to improve performance.
 */
FFT::FFT(int size) : real(size), imag(size), size(size), m_current_fft_mode(FFTMode::Original)
{
    precompute_twiddle_factors();
    neomat_fft_init_twiddle_factors();
    m_complex_buffer.resize(size);
    m_complex_output_buffer.resize(size);
}

/**
 * @brief Sets the active FFT implementation.
 * @param mode The FFTMode to use for subsequent calls to fft().
 */
void FFT::setFFTMode(FFTMode mode) {
    m_current_fft_mode = mode;
}

/**
 * @brief Gets the currently active FFT implementation mode.
 * @return The current FFTMode enum value.
 */
FFTMode FFT::getFFTMode() const {
    return m_current_fft_mode;
}

/**
 * @brief Gets the human-readable name of the current FFT mode.
 * @return A std::string representing the current mode (e.g., "mat-og", "vibe-1").
 */
std::string FFT::getFFTModeName() const {
    switch (m_current_fft_mode) {
        case FFTMode::Original: return "mat-og";
        case FFTMode::Optimized: return "vibe-1";
        case FFTMode::NeomatIn: return "neomat-in";
        case FFTMode::NeomatOut: return "neomat-out";
        default: return "Unknown"; // Should not happen
    }
}

/**
 * @brief Precomputes sine and cosine "twiddle factors" for the optimized FFT implementation.
 * 
 * This is done once at initialization to avoid repeated, expensive sin/cos
 * calculations during the FFT process itself.
 */
void FFT::precompute_twiddle_factors() {
    m_twiddle_cos.resize(size / 2);
    m_twiddle_sin.resize(size / 2);
    for (int i = 0; i < size / 2; ++i) {
        float angle = -2 * M_PI_F * i / size; // Use M_PI_F for float
        m_twiddle_cos[i] = cosf(angle); // Use cosf for float
        m_twiddle_sin[i] = sinf(angle); // Use sinf for float
    }
}

/**
 * @brief Precomputes complex "twiddle factors" for the Neomat FFT implementations.
 * 
 * This stores the complex roots of unity in a format optimized for the Neomat
 * algorithms, improving performance by avoiding recalculations.
 */
void FFT::neomat_fft_init_twiddle_factors() {
    // The neomat twiddle factor array has n-1 elements.
    // The largest index used is (n/2 - 1) + (n/2 - 1) = n - 2.
    // So, we need size n-1 to cover indices 0 to n-2.
    m_neomat_twiddle_factors.resize(size - 1);

    for (size_t i = 1; i < static_cast<size_t>(size); i <<= 1) {
        for (size_t k = 0; k < i; ++k) {
            // C99: cexp(-2 * M_PI * I * k / (i << 1));
            // C++: std::exp(std::complex<float>(-2.0f * M_PI_F * k / (static_cast<float>(i) * 2.0f), 0.0f) * I_f);
            // Or more directly: std::complex<float>(cosf(angle), sinf(angle))
            float angle = -2.0f * M_PI_F * k / (static_cast<float>(i) * 2.0f);
            m_neomat_twiddle_factors[i - 1 + k] = std::complex<float>(cosf(angle), sinf(angle));
        }
    }
}

/**
 * @brief The original, unoptimized FFT implementation (mat-og).
 * 
 * This version serves as a baseline and reference. It is functionally correct
 * but less performant than the other implementations.
 * @param output A pointer to the float array to store the resulting frequency magnitudes.
 * @param input A pointer to the float array of time-domain input samples.
 */
void FFT::original_fft_impl(float *output, const float *input) { 
	int nu = (int) (logf(size) / logf(2.0f)); // Number of bits of item indexes
	int n2 = size / 2;
	int nu1 = nu - 1;
	float tr, ti, arg, c, s;

	for (int i = 0; i < size; ++i) {
		real[i] = input[i];
		imag[i] = 0;
	}

	// Stage 1 - calculation (Butterflies)
	float f = -2 * M_PI_F / size;
	for (int l = 1; l <= nu; l++) {
		int k = 0;
		while (k < size) {
			for (int i = 1; i <= n2; i++) {
				arg = bitreverse(static_cast<unsigned int>(k >> nu1), nu) * f;
				c = cos(arg);
				s = sin(arg);
				tr = real[k + n2] * c + imag[k + n2] * s;
				ti = imag[k + n2] * c - real[k + n2] * s;
				real[k + n2] = real[k] - tr;
				imag[k + n2] = imag[k] - ti;
				real[k] += tr;
				imag[k] += ti;
				k++;
			}
			k += n2;
		}
		nu1--;
		n2 /= 2;
	}

	// Stage 2 - normalize the output and feed the magnitudes to the output array
	for (int i = 0; i < size; ++i) // Note: original code normalizes by size here
		output[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]) / size;
 
	// Stage 3 - recombination
	for (int k = 0; k < size; ++k) {
		int r = bitreverse(k, nu);
		if (r > k) {
			tr = output[k];
			output[k] = output[r];
			output[r] = tr;
		}
	}
}

/**
 * @brief An optimized Radix-2 Decimation-in-Time (DIT) FFT implementation (vibe-1).
 * 
 * This version improves performance by using precomputed twiddle factors and a
 * more efficient butterfly calculation structure. It first reorders the input
 * using bit-reversal and then iteratively builds the result.
 * @param output A pointer to the float array to store the resulting frequency magnitudes.
 * @param input A pointer to the float array of time-domain input samples.
 */
void FFT::optimized_fft_impl(float *output, const float *input) { // vibe-1
    int nu = (int) (logf(size) / logf(2.0f)); // Number of bits for bitreverse

    // 1. Copy input to internal real/imag and perform bit-reversal permutation
    for (int i = 0; i < size; ++i) { // Bit-reversal permutation
        int r = bitreverse(i, nu);
        real[r] = input[i];
        imag[r] = 0;
    }

    // 2. Iterative butterfly stages
    for (int len = 2; len <= size; len <<= 1) { // len = current butterfly group size (2, 4, 8, ..., size)
        int half_len = len / 2;
        
        for (int i = 0; i < size; i += len) { // i = start of current butterfly group
            for (int j = 0; j < half_len; ++j) { // j = index within the group
                // Use precomputed twiddle factors
                float cos_val = m_twiddle_cos[j * (size / len)];
                float sin_val = m_twiddle_sin[j * (size / len)];

                float t_real = real[i + j + half_len] * cos_val - imag[i + j + half_len] * sin_val;
                float t_imag = real[i + j + half_len] * sin_val + imag[i + j + half_len] * cos_val;

                real[i + j + half_len] = real[i + j] - t_real;
                imag[i + j + half_len] = imag[i + j] - t_imag;
                real[i + j] += t_real;
                imag[i + j] += t_imag;
            }
        }
    }

    // 3. Calculate magnitudes and normalize
    for (int i = 0; i < size; ++i) {
        output[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]) / size;
    }
}

/**
 * @brief An in-place Cooley-Tukey FFT implementation (neomat-in).
 * 
 * This version operates directly on a single complex buffer, which can be more
 * memory-efficient. It uses precomputed complex twiddle factors for performance.
 * @param output A pointer to the float array to store the resulting frequency magnitudes.
 * @param input A pointer to the float array of time-domain input samples.
 */
void FFT::neomat_in_place_fft_impl(float *output, const float *input) { // neomat-in
    int nu = (int) (logf(size) / logf(2.0f));

    // 1. Convert input to complex and perform bit-reversal permutation
    for (int i = 0; i < size; ++i) {
        int r = bitreverse(i, nu);
        m_complex_buffer[r] = std::complex<float>(input[i], 0.0f);
    }

    // 2. Iterative butterfly stages
    for (size_t i = 1; i < static_cast<size_t>(size); i <<= 1) {
        for (size_t j = 0; j < static_cast<size_t>(size); j += (i << 1)) {
            for (size_t k = 0; k < i; ++k) {
                std::complex<float> tmp = m_neomat_twiddle_factors[i - 1 + k] * m_complex_buffer[j + k + i];
                m_complex_buffer[j + k + i] = m_complex_buffer[j + k] - tmp;
                m_complex_buffer[j + k] += tmp;
            }
        }
    }

    // 3. Calculate magnitudes and normalize
    for (int i = 0; i < size; ++i) {
        output[i] = std::abs(m_complex_buffer[i]) / size; // Normalize by size
    }
}

/**
 * @brief An out-of-place Cooley-Tukey FFT implementation (neomat-out).
 * 
 * This version uses separate input and output buffers, which can sometimes simplify
 * the algorithm and data flow at the cost of higher memory usage.
 * @param output A pointer to the float array to store the resulting frequency magnitudes.
 * @param input A pointer to the float array of time-domain input samples.
 */
void FFT::neomat_out_of_place_fft_impl(float *output, const float *input) { // neomat-out
    int nu = (int) (logf(size) / logf(2.0f));

    // 1. Convert input to complex and perform bit-reversal permutation into output buffer
    for (int i = 0; i < size; ++i) {
        int r = bitreverse(i, nu);
        m_complex_output_buffer[r] = std::complex<float>(input[i], 0.0f);
    }

    // 2. Iterative butterfly stages
    for (size_t i = 1; i < static_cast<size_t>(size); i <<= 1) {
        for (size_t j = 0; j < static_cast<size_t>(size); j += (i << 1)) {
            for (size_t k = 0; k < i; ++k) {
                std::complex<float> tmp = m_neomat_twiddle_factors[i - 1 + k] * m_complex_output_buffer[j + k + i];
                m_complex_output_buffer[j + k + i] = m_complex_output_buffer[j + k] - tmp;
                m_complex_output_buffer[j + k] += tmp;
            }
        }
    }

    // 3. Calculate magnitudes and normalize
    for (int i = 0; i < size; ++i) {
        output[i] = std::abs(m_complex_output_buffer[i]) / size; // Normalize by size
    }
}

/**
 * @brief Performs bit-reversal on an integer index.
 * 
 * This is a crucial helper function for FFT algorithms that reorder the input
 * or output data to achieve computational efficiency.
 * @param in The integer index to reverse.
 * @param bits The number of bits in the index (log2 of the FFT size).
 * @return The bit-reversed integer.
 */
unsigned int FFT::bitreverse(unsigned int in, int bits) {
	unsigned int out = 0;
	while (bits--) {
		out |= (in & 1) << bits;
		in >>= 1;
	}
	return out;
}


/**
 * @brief Performs the FFT using the currently selected algorithm.
 * 
 * This public method acts as a dispatcher, calling the appropriate private
 * implementation based on the mode set by setFFTMode().
 * @param output A pointer to the float array where the frequency magnitudes will be stored.
 * @param input A pointer to the float array of time-domain input samples.
 */
void FFT::fft(float *output, const float *input) {
    switch (m_current_fft_mode) {
        case FFTMode::Original:
            original_fft_impl(output, input);
            break;
        case FFTMode::Optimized:
            optimized_fft_impl(output, input);
            break;
        case FFTMode::NeomatIn:
            neomat_in_place_fft_impl(output, input);
            break;
        case FFTMode::NeomatOut:
            neomat_out_of_place_fft_impl(output, input);
            break;
    }
}
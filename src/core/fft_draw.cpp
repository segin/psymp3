/*
 * fft_draw.cpp - FFT Renderer helper class
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
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

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Core {


/**
 * @brief Constructs a FastFourier object.
 *
 * This constructor initializes the underlying FFT processing engine with the specified
 * size (defaulting to 512 samples) and allocates buffers for time-domain and frequency-domain data.
 * @param fft_size The size of the FFT (must be a power of 2), defaults to 512 for backward compatibility.
 */
FastFourier::FastFourier(int fft_size) :
    fft(std::make_unique<FFT>(fft_size)),
    m_samples(fft_size), // Match FFT size
    m_fft(fft_size),     // Match FFT size
    m_fft_size(fft_size) // Store the size
{
}

/**
 * @brief Destroys the FastFourier object.
 *
 * The default destructor is sufficient as std::unique_ptr will handle the
 * cleanup of the underlying FFT object.
 */
FastFourier::~FastFourier() = default; // std::unique_ptr handles deletion

/**
 * @brief Performs the Fast Fourier Transform.
 *
 * This method takes the time-domain sample data stored internally and computes
 * the frequency-domain (spectrum) data, storing the result.
 */
void FastFourier::doFFT()
{
	fft->fft(m_fft.data(), m_samples.data());
}

/**
 * @brief Sets the current FFT processing mode.
 * @param mode The desired FFTMode to use for subsequent transformations.
 */
void FastFourier::setFFTMode(FFTMode mode)
{
    fft->setFFTMode(mode);
}

/**
 * @brief Gets the current FFT processing mode.
 * @return The currently active FFTMode.
 */
FFTMode FastFourier::getFFTMode() const
{
    return fft->getFFTMode();
}

/**
 * @brief Gets the name of the current FFT processing mode as a string.
 * @return A std::string containing the human-readable name of the current FFT mode.
 */
std::string FastFourier::getFFTModeName() const
{
    return fft->getFFTModeName();
}
} // namespace Core
} // namespace PsyMP3

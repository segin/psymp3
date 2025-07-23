/*
 * fft_draw.h - class header for storing and drawing FFT data.
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef FFT_DRAW_H
#define FFT_DRAW_H

// No direct includes - all includes should be in psymp3.h

class FastFourier
{
    public:
		FastFourier(int fft_size = 512); // Allow configurable FFT size with backward compatibility
		~FastFourier();
		float *getFFT() { return m_fft.data(); }
		float *getTimeDom() { return m_samples.data(); }
		void doFFT();
		void setFFTMode(FFTMode mode);
		FFTMode getFFTMode() const;
		std::string getFFTModeName() const;
		int getFFTSize() const { return m_fft_size; }
    protected:
	private:
		std::unique_ptr<FFT> fft;
		std::vector<float> m_samples;
		std::vector<float> m_fft;
		int m_fft_size; // Store the FFT size
};

#endif // FFT_DRAW_H

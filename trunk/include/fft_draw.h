/*
 * fft_draw.h - class header for storing and drawing FFT data.
 * This file is part of PsyMP3.
 * Copyright © 2011 Kirn Gill <segin2005@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef FFT_DRAW_H
#define FFT_DRAW_H

class FastFourier
{
    public:
        FastFourier();
        ~FastFourier();
        float *getFFT() { return (float *) m_fft; }
        float *getTimeDom() { return (float *) m_samples; }
        void doFFT();
        static void init();
    protected:
    private:
        VisDFT *m_handle;
        float m_samples[1024];
        float m_fft[512];
	// may not need these
	std::vector<float> bitrevtable;
	std::vector<float> sintable;
	std::vector<float> costable;
        unsigned int       sample_count;
        unsigned int       spectrum_size;
        unsigned int       samples_out;
        std::vector<float> real;
        std::vector<float> imag;
 	void perform(float const* samples);
	void fft_bitrev_table_init (unsigned int sample_count);
	void fft_cossin_table_init (unsigned int sample_count);
	void dft_cossin_table_init (unsigned int sample_count);

};

#endif // FFT_DRAW_H

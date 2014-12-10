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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef FFT_DRAW_H
#define FFT_DRAW_H

#include "fft.h"

class FastFourier
{
    public:
		FastFourier();
		~FastFourier();
		float *getFFT() { return (float *) m_fft; }
		float *getTimeDom() { return (float *) m_samples; }
		void doFFT();
    protected:
	private:
		FFT *fft;
		float m_samples[1024];
		float m_fft[512];
};

#endif // FFT_DRAW_H

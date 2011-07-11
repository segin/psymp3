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


class fft_draw
{
    public:
        /** Default constructor */
        fft_draw();
        /** Default destructor */
        virtual ~fft_draw();
        /** Set m_fft[512]
         * \param val New value to set
         */
        void Setfft(float val) { m_fft[512] = val; }
    protected:
    private:
        float m_fft[512]; //!< Member variable "m_fft[512]"
};

#endif // FFT_DRAW_H

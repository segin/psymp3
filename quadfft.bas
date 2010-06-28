/'
    $Id $

    This file is part of PsyMP3.

    PsyMP3 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    PsyMP3 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PsyMP3; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
'/

#Include "psymp3.bi"

''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
' FTT courtesy of Quadrescence
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

sub quadfft Alias "quadfft" _
	( _
		byval n        as integer, _
		byval in_data  as complex_t ptr, _
		byval out_data as complex_t ptr, _
		byval inverse  as integer = 0 _
	)

	dim as integer   br  (0 to n - 1)
	dim as double    FFTB(0 to n - 1)

	CONST AS DOUBLE  PI = 3.1415926535897932384626433
	DIM   AS DOUBLE  c, s, xt, yt, p, a, i, j, k, l, n1, n2, ie, ia
	DIM   AS INTEGER ST
	dim   as integer m = int(log(n) / log(2))

	FOR i = 0 TO (N - 1)
		out_data[i].re = in_data[i].re
		out_data[i].im = in_data[i].im
	NEXT i

	'----------------------------------------------------
	'--------------------------FFT-----------------------
	'----------------------------------------------------
	p = 2*PI/n
	n2 = n
	for k = 1 to m
	n1 = n2
	n2 /= 2
	ie = n/n1
	ia = 1
	for j = 0 to n2-1
		a = (ia - 1)*p
		c = cos(a)
		s = sin(a)
		ia += ie
		for i = j to n-1 step n1
			l = i + n2
			xt = out_data[i].re - out_data[l].re
			out_data[i].re += out_data[l].re
			yt = out_data[i].im - out_data[l].im
			out_data[i].im += out_data[l].im
			out_data[l].re = c * xt + s * yt
			out_data[l].im = c * yt - s * xt
		next i
	next j
	next k
	'----------------------------------------------------
	'----------------------END-FFT-----------------------
	'----------------------------------------------------

	'----------------------------------------------------
	'--------------------------BIT-REVERSAL--------------
	'----------------------------------------------------
	'--------------------REAL
	ST=1
	DO
		FOR k = 1 to 2^(ST-1)
			br(k) *= 2
		NEXT k
		FOR k = 0 to 2^(ST-1)-1
			br(k + 2^(st-1)) = br(k) + 1
		NEXT k
		ST += 1
	LOOP UNTIL ST > M

	FOR k = 0 to N - 1
		FFTB(k) = out_data[br(k)].re
	NEXT K

	FOR k = 0 to N - 1
		out_data[k].re = FFTB(k)
	NEXT k
	'--------------------IMAGINARY
	ST=1
	DO
		FOR k = 1 to 2^(ST-1)
			br(k) *= 2
		NEXT k
		FOR k = 0 to 2^(ST-1)-1
			br(k + 2^(st-1)) = br(k) + 1
		NEXT k
		ST += 1
	LOOP UNTIL ST > M

	FOR k = 0 to N-1
		FFTB(k) = out_data[br(k)].im
	NEXT K

	FOR k = 1 to N - 1
		out_data[k].im = -FFTB(k)
	NEXT k
	'----------------------------------------------------
	'----------------------END-BIT-REVERSAL--------------
	'---------------------------------------------------- 

	'----------------------------------------------------
	'--------------------------Conjugate-Normalize-------
	'----------------------------------------------------
	if inverse then
		for i = 0 to n-1
			out_data[i].im = -out_data[i].im
		next i
	end if

	if inverse = 0 then
		p = n
		for i = 0 to n-1
			out_data[i].im /= p
			out_data[i].re /= p
		next i
	end if
	'----------------------------------------------------
	'----------------------End-Conjugate-Normalize-------
	'----------------------------------------------------

end Sub

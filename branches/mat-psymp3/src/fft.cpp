#include "fft.h"

#include <cmath>

FFT::FFT(int size) {
	real = new float[size];
	imag = new float[size];
	this->size = size;
}

FFT::~FFT() {
	delete[] real;
	delete[] imag;
}

void FFT::fft(float *output, const float *input) { 
	int nu = (int) ((float) log(size) / log(2.0)); // number of bits of item indexes
	int n2 = size / 2;
	int nu1 = nu - 1;
	float tr, ti, p, arg, c, s;

	for (int i = 0; i < size; ++i) {
		real[i] = input[i];
		imag[i] = 0;
	}

	// First phase - calculation
	for (int l = 1; l <= nu; l++) {
		int k = 0;
		while (k < size) {
			for (int i = 1; i <= n2; i++) {
				p = bitreverse(k >> nu1, nu);
				arg = -2 * M_PI * p / size;
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
 
	// Second phase - recombination
	for (int k = 0; k < size; ++k) {
		int r = bitreverse(k, nu);
		if (r > k) {
			tr = real[k];
			ti = imag[k];
			real[k] = real[r];
			imag[k] = imag[r];
			real[r] = tr;
			imag[r] = ti;
		}
	}
	
	// Normalize the output and feed the magnitudes to the output array
	float radice = 1 / sqrt(size);
	for (int i = 0; i < size; ++i) {
		real[i] *= radice;
		imag[i] *= radice;
		output[i] = sqrt(real[i] * real[i] + imag[i] * imag[i]);
	}
}

unsigned int FFT::bitreverse(unsigned int in, int bits) {
	unsigned int out = 0;
	while (bits--) {
		out |= (in & 1) << bits;
		in >>= 1;
	}
	return out;
}


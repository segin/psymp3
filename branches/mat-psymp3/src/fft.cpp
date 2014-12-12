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

#include <iostream>
using namespace std;

void FFT::fft(float *output, const float *input) { 
	int nu = (int) ((float) log(size) / log(2.0)); // Number of bits of item indexes
	int n2 = size / 2;
	int nu1 = nu - 1;
	float tr, ti, arg, c, s;

	for (int i = 0; i < size; ++i) {
		real[i] = input[i];
		imag[i] = 0;
	}

	// Stage 1 - calculation
	float f = -2 * M_PI / size;
	for (int l = 1; l <= nu; l++) {
		int k = 0;
		while (k < size) {
			for (int i = 1; i <= n2; i++) {
				arg = bitreverse(k >> nu1, nu) * f;
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
	for (int i = 0; i < size; ++i)
		output[i] = sqrt(real[i] * real[i] + imag[i] * imag[i]) / size;
 
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

unsigned int FFT::bitreverse(unsigned int in, int bits) {
	unsigned int out = 0;
	while (bits--) {
		out |= (in & 1) << bits;
		in >>= 1;
	}
	return out;
}


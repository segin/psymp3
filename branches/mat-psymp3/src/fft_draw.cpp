#include "psymp3.h"

FastFourier::FastFourier()
{
	fft = new FFT(512);
}

FastFourier::~FastFourier()
{
	delete fft;
}

void FastFourier::doFFT()
{
    fft->fft(m_fft, m_samples);
}



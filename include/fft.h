#ifndef FFT_H
#define FFT_H

class FFT {
	private:
		float *real, *imag;
		int size;

		static unsigned int bitreverse(unsigned int in, int bits);

	public:
		FFT(int size); // size must be a power of 2 (!)
		~FFT();
		void fft(float *output, const float *input);
};

#endif // FFT_H

#include "nsutil_fft.h"

#include <math.h>
#include <stdlib.h>
#include <complex>
#include <vector>

namespace
{
	struct RealFFT
	{
		int n;
		std::vector<std::complex<float>> twiddle;
		std::vector<int> bitrev;
		std::vector<std::complex<float>> work;
	};
}

int nsutil_fft_Create_F32R(nsutil_fft_t *fft, int order, int accuracy)
{
	(void)accuracy;
	RealFFT *f = new RealFFT;
	f->n = 1 << order;
	f->twiddle.resize(f->n / 2);
	for (int k = 0; k < f->n / 2; k++)
		f->twiddle[k] = std::polar(1.0f, (float)(-2.0 * M_PI * k / f->n));
	f->bitrev.resize(f->n);
	for (int i = 0; i < f->n; i++)
	{
		int r = 0;
		for (int b = 0; b < order; b++)
			if (i & (1 << b)) r |= 1 << (order - 1 - b);
		f->bitrev[i] = r;
	}
	f->work.resize(f->n);
	*fft = f;
	return 0;
}

int nsutil_fft_Forward_F32R_IP(nsutil_fft_t fft, float *signal)
{
	RealFFT *f = (RealFFT *)fft;
	const int n = f->n;
	auto &a = f->work;
	for (int i = 0; i < n; i++)
		a[f->bitrev[i]] = std::complex<float>(signal[i], 0.0f);

	for (int len = 2; len <= n; len <<= 1)
	{
		int step = n / len;
		for (int i = 0; i < n; i += len)
			for (int j = 0; j < len / 2; j++)
			{
				std::complex<float> u = a[i + j];
				std::complex<float> v = a[i + j + len / 2] * f->twiddle[j * step];
				a[i + j] = u + v;
				a[i + j + len / 2] = u - v;
			}
	}

	signal[0] = a[0].real();
	signal[1] = a[n / 2].real();
	for (int k = 1; k < n / 2; k++)
	{
		signal[2 * k] = a[k].real();
		signal[2 * k + 1] = a[k].imag();
	}
	return 0;
}

int nsutil_fft_Destroy_F32R(nsutil_fft_t fft)
{
	delete (RealFFT *)fft;
	return 0;
}

int nsutil_window_FillHann_F32_IP(float *window, size_t number_of_samples)
{
	if (number_of_samples < 2)
	{
		if (number_of_samples) window[0] = 1.0f;
		return 0;
	}
	for (size_t i = 0; i < number_of_samples; i++)
		window[i] = (float)(0.5 - 0.5 * cos(2.0 * M_PI * (double)i / (double)(number_of_samples - 1)));
	return 0;
}

int nsutil_window_Multiply_F32_IP(float *signal, const float *window, size_t number_of_samples)
{
	for (size_t i = 0; i < number_of_samples; i++)
		signal[i] *= window[i];
	return 0;
}

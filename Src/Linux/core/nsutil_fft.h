/*
** Winamp for Linux - portable versions of the Src/nsutil FFT/window helpers
** (the Windows build implements these with Intel IPP).
*/
#pragma once
#include <stddef.h>

typedef void *nsutil_fft_t;

int nsutil_fft_Create_F32R(nsutil_fft_t *fft, int order, int accuracy);
// real forward FFT in place, result in IPP "Perm" order: R0, R(N/2), R1, I1, R2, I2 ...
int nsutil_fft_Forward_F32R_IP(nsutil_fft_t fft, float *signal);
int nsutil_fft_Destroy_F32R(nsutil_fft_t fft);

int nsutil_window_FillHann_F32_IP(float *window, size_t number_of_samples);
int nsutil_window_Multiply_F32_IP(float *signal, const float *window, size_t number_of_samples);

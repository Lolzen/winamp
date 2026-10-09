/*
** Classic spectrum analyzer / oscilloscope data.
**
** The algorithms are the ones from Src/Winamp/classic_vis.cpp, SA.cpp and
** SABuffer.cpp. The Windows build calls Intel IPP through nsutil for the FFT
** and the Hann window; nsutil_fft.cpp provides the same functions in portable
** C++ here.
*/
#include "vis.h"
#ifdef WINAMP_HAVE_PROJECTM
#include "projectm_audio.h"
#endif
#include "config.h"
#include "nsutil_fft.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <mutex>
#include <algorithm>

volatile int sa_curmode = 1;
int g_srate_exact = 44100;

/* ---------------- SA.cpp ---------------- */

static int last_pos;
struct sa_l
{
	int timestamp;
	unsigned char data[2 * 75];
	char which;
};

static sa_l *sa_bufs;
static int sa_position;
static int sa_length, sa_size;
static std::mutex cs;

void sa_init(int numframes)
{
	std::lock_guard<std::mutex> lock(cs);
	sa_length = 0;
	if (numframes < 1) numframes = 1;

	if (numframes > sa_size)
	{
		free(sa_bufs);
		sa_bufs = (sa_l *)calloc(numframes, sizeof(sa_l));
		sa_size = numframes;
	}
	sa_position = 0;
	sa_length = numframes;
	last_pos = 0;
}

void sa_deinit()
{
	std::lock_guard<std::mutex> lock(cs);
	sa_length = 0;
#ifdef WINAMP_HAVE_PROJECTM
	projectm_audio::clear();
#endif
}

int sa_add(char *values, int timestamp, int csa)
{
	std::lock_guard<std::mutex> lock(cs);
	if (!sa_bufs || sa_length == 0)
		return 1;

	if (sa_length == 1)
		sa_position = 0;
	if (csa == 3) csa = 1; // dont let it happen unless it has a high bit set
	csa &= 0x7fffffff;

	sa_bufs[sa_position].timestamp = timestamp;
	sa_bufs[sa_position].which = (char)csa;

	if (csa & 1)
	{
		memcpy(sa_bufs[sa_position].data, values, 75);
		values += 75;
	}
	else
		memset(sa_bufs[sa_position].data, 0, 75);

	if (csa & 2)
		memcpy(sa_bufs[sa_position].data + 75, values, 75);
	else
		memset(sa_bufs[sa_position].data + 75, 0, 75);

	sa_position++;
	if (sa_position >= sa_length) sa_position -= sa_length;
	return 0;
}

char *sa_get(int timestamp, int csa, char data[75 * 2 + 8])
{
	static int sa_pos;
	int closest = 1000000, closest_v = -1;
	std::lock_guard<std::mutex> lock(cs);

	if (!sa_bufs || sa_length == 0)
		return 0;

	if (sa_length == 1)
	{
		memcpy(data, sa_bufs[0].data, 75 * 2);
		return data;
	}

	int i = last_pos;
	for (int x = 0; x < sa_length; x++)
	{
		if (i >= sa_length) i = 0;
		int d = timestamp - sa_bufs[i].timestamp;
		if (d < 0) d = -d;
		if (d < closest)
		{
			closest = d;
			closest_v = i;
		}
		else if (closest <= 6) break;
		i++;
	}

	if (closest < 400 && closest_v >= 0 && sa_bufs[closest_v].which & csa)
	{
		sa_pos = 0;
		last_pos = closest_v;
		memcpy(data, sa_bufs[closest_v].data, 75 * 2);
		return data;
	}

	if (closest_v < 0 || !(sa_bufs[closest_v].which & csa) || closest > 400)
	{
		memset(data, 0, 75);
		data[(sa_pos % 150) >= 75 ? 149 - (sa_pos % 150) : (sa_pos % 150)] = 15;
		for (int x = 0; x < 75; x++)
			data[x + 75] = (char)(int)(7.0 * sin((sa_pos + x) * 0.1));
		sa_pos++;
		return data;
	}
	return 0;
}

void sa_setmode(int mode)
{
	if (mode < 0) mode = 0;
	sa_curmode = mode;
}

int sa_getmode()
{
	return sa_curmode;
}

/* ---------------- SABuffer.cpp ---------------- */

#define SABUFFER_WINDOW_INCREMENT 256

static const float const_1_div_128_ = 1.0f / 128.0f;
static const float const_1_div_32768_ = 1.0f / 32768.f;
static const double const_1_div_2147483648_ = 1.0 / 2147483648.0;

static void Int16_To_Float32(float *dest, void *sourceBuffer, signed int sourceStride, unsigned int count)
{
	signed short *src = (signed short *)sourceBuffer;
	while (count--)
	{
		*dest++ = *src * const_1_div_32768_;
		src += sourceStride;
	}
}

static void Int24_To_Float32(float *dest, void *sourceBuffer, signed int sourceStride, unsigned int count)
{
	unsigned char *src = (unsigned char *)sourceBuffer;
	while (count--)
	{
		int32_t temp = (int32_t)(((uint32_t)src[0] << 8) | ((uint32_t)src[1] << 16) | ((uint32_t)src[2] << 24));
		*dest++ = (float)((double)temp * const_1_div_2147483648_);
		src += sourceStride * 3;
	}
}

static void Int32_To_Float32(float *dest, void *sourceBuffer, signed int sourceStride, unsigned int count)
{
	int32_t *src = (int32_t *)sourceBuffer;
	while (count--)
	{
		*dest++ = (float)((double)*src * const_1_div_2147483648_);
		src += sourceStride;
	}
}

static void UInt8_To_Float32(float *dest, void *sourceBuffer, signed int sourceStride, unsigned int count)
{
	unsigned char *src = (unsigned char *)sourceBuffer;
	while (count--)
	{
		*dest++ = (*src - 128) * const_1_div_128_;
		src += sourceStride;
	}
}

class SABuffer
{
public:
	SABuffer() { memset(buffer, 0, sizeof(buffer)); }
	void WindowToFFTBuffer(float *wavetrum)
	{
		for (int i = 0; i < 512; i++)
			wavetrum[i] = (buffer[0][i] + buffer[1][i]);
		nsutil_window_Multiply_F32_IP(wavetrum, window, 512);
	}
	unsigned int AddToBuffer(char *samples, int numChannels, int bps, int ts, unsigned int numSamples)
	{
		(void)ts;
		if (!init)
		{
			nsutil_window_FillHann_F32_IP(window, 512);
			init = true;
		}
		unsigned int toCopy = std::min((unsigned int)(512 - used), numSamples);
		int bytes = bps / 8;
		switch (bps)
		{
		case 8:
			UInt8_To_Float32(buffer[0] + used, samples, numChannels, toCopy);
			UInt8_To_Float32(buffer[1] + used, samples + (numChannels > 1 ? bytes : 0), numChannels, toCopy);
			break;
		case 16:
			Int16_To_Float32(buffer[0] + used, samples, numChannels, toCopy);
			Int16_To_Float32(buffer[1] + used, samples + (numChannels > 1 ? bytes : 0), numChannels, toCopy);
			break;
		case 24:
			Int24_To_Float32(buffer[0] + used, samples, numChannels, toCopy);
			Int24_To_Float32(buffer[1] + used, samples + (numChannels > 1 ? bytes : 0), numChannels, toCopy);
			break;
		case 32:
			Int32_To_Float32(buffer[0] + used, samples, numChannels, toCopy);
			Int32_To_Float32(buffer[1] + used, samples + (numChannels > 1 ? bytes : 0), numChannels, toCopy);
			break;
		}
		used += toCopy;
		return toCopy;
	}
	bool Full() { return used == 512; }
	void CopyHalf()
	{
		memmove(buffer[0], buffer[0] + SABUFFER_WINDOW_INCREMENT, (512 - SABUFFER_WINDOW_INCREMENT) * sizeof(float));
		memmove(buffer[1], buffer[1] + SABUFFER_WINDOW_INCREMENT, (512 - SABUFFER_WINDOW_INCREMENT) * sizeof(float));
		used -= SABUFFER_WINDOW_INCREMENT;
	}

private:
	float buffer[2][512];
	float window[512];
	size_t used = 0;
	bool init = false;
};

/* ---------------- classic_vis.cpp ---------------- */

static inline int lrint_(float f) { return (int)lrintf(f); }

static inline float fastmin(float x, const float b)
{
	x = b - x;
	x += (float)fabs(x);
	x *= 0.5f;
	x = b - x;
	return x;
}

static void makeOscData(char *tempdata, char *data_buf, int little_block, int channels, int bits)
{
	float dd = little_block / 75.0f;
	int stride = bits / 8; // number of bytes between samples
	// we're calculating using only the most significant byte,
	// because we only end up with 6 bit data anyway
	char *sbuf = data_buf;
	for (int x = 0; x < 75; x++)
	{
		float val = 0;
		int index = (int)((float)x * dd);
		char *ptr = &sbuf[index * stride * channels + stride - 1];
		for (int c = 0; c < channels; c++)
		{
			if (bits == 8) val += (float)(signed char)((unsigned char)*ptr - 128) / 8.0f;
			else val += (float)*ptr / 8.0f; // we want our final value to be -32 to 32
			ptr += stride;
		}
		tempdata[x] = (char)lrint_(val / (float)channels);
	}
}

static inline float hermite(float x, float y0, float y1, float y2, float y3)
{
	// 4-point, 3rd-order Hermite (x-form)
	float c0 = y1;
	float c1 = 0.5f * (y2 - y0);
	float c3 = 1.5f * (y1 - y2) + 0.5f * (y3 - y0);
	float c2 = y0 - y1 + c1 - c3;
	return ((c3 * x + c2) * x + c1) * x + c0;
}

static inline float fpow2(const float y)
{
	union
	{
		float f;
		int i;
	} c;
	int integer = lrint_(floorf(y));
	float frac = y - (float)integer;
	c.i = (integer + 127) << 23;
	c.f *= 0.33977f * frac * frac + (1.0f - 0.33977f) * frac + 1.0f;
	return c.f;
}

#define SAPOW(x) (fpow2((float)(x) / 12.f))
#define WARP(x) ((SAPOW(x) - 1.f) * bla)

static nsutil_fft_t fft9;

static void makeSpecData(unsigned char *tempdata, float *wavetrum)
{
	float bla = (255.f / SAPOW(75.f));
	if (!fft9) nsutil_fft_Create_F32R(&fft9, 9, 0);
	nsutil_fft_Forward_F32R_IP(fft9, wavetrum);

	float spec_scale = 0.5;
	for (int i = 0; i < 256; i++)
	{
		float sinT = wavetrum[2 * i];
		float cosT = wavetrum[2 * i + 1];
		wavetrum[i] = sqrtf(sinT * sinT + cosT * cosT) * spec_scale;
	}

	float next = WARP(0) + 1;
	for (int x = 0; x < 75; x++)
	{
		float binF = next;
		next = WARP(x + 1) + 1;

		float thisValue = 0;
		int bin = lrint_(floorf(binF));
		int end = lrint_(floorf(next));
		end = std::min(end, 255);
		float mult = ((float)(bin + 1)) - binF;
		bool herm = true;
		do
		{
			if (bin == end)
			{
				mult = (next - binF);
				herm = true;
			}

			if (herm)
			{
				float C = 0, D = 0;
				if (bin < 255)
				{
					C = wavetrum[bin + 1];
					if (bin < 254)
						D = wavetrum[bin + 2];
				}
				thisValue += hermite(binF - bin, wavetrum[bin > 0 ? bin - 1 : 0], wavetrum[bin], C, D) * mult;
			}
			else
			{
				thisValue += wavetrum[bin];
			}

			herm = false;
			bin++;
			binF = (float)bin;
		} while (bin <= end);

		tempdata[x] = (unsigned char)lrint_(fastmin(thisValue, 255.f));
	}
}

static SABuffer saBuffer;

void sa_addpcmdata(void *_data_buf, int numChannels, int numBits, int ts)
{
	char *data_buf = reinterpret_cast<char *>(_data_buf);
	char tempdata[75 * 2] = {0};
	float wavetrum[512];
	int vis_Csa = sa_curmode;

	if (!data_buf || numChannels < 1 || (numBits != 8 && numBits != 16 && numBits != 24 && numBits != 32))
		return;

#ifdef WINAMP_HAVE_PROJECTM
	projectm_audio::push_pcm(_data_buf, numChannels, numBits, 576);
#endif

	switch (vis_Csa)
	{
	case 4:
		sa_add(tempdata, ts, 4);
		return;
	case 2:
		makeOscData(tempdata, data_buf, 576, numChannels, numBits);
		sa_add(tempdata, ts, 2);
		return;
	case 1:
		break;
	default:
		return;
	}

	size_t samples = 576;
	while (samples)
	{
		unsigned int copied = saBuffer.AddToBuffer(data_buf, numChannels, numBits, ts, (unsigned int)samples);
		samples -= copied;
		data_buf += (copied * (numBits / 8) * numChannels);
		if (saBuffer.Full())
		{
			saBuffer.WindowToFFTBuffer(wavetrum);
			makeSpecData((unsigned char *)tempdata, wavetrum);
			sa_add(tempdata, ts, 1);
			saBuffer.CopyHalf();
			ts += (int)((int64_t)SABUFFER_WINDOW_INCREMENT * 1000 / (g_srate_exact ? g_srate_exact : 44100));
		}
	}
}

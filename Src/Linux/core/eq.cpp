#include "eq.h"
#include "eq10dsp.h"
#include "config.h"
#include "common.h"
#include "plugins.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <mutex>

/* ---------------- EQ processing (Src/Winamp/In.cpp) ---------------- */

static std::mutex eq_lock;
static eq10_t *eq = 0;
static int eq_nch = 0;
static int filter_enabled = 0;
static int filter_srate = 0;
static float preamp_val = 1.0f;
static float eqt[10] = {0};
static float *splbuf = 0;
static int splbuf_alloc = 0;

static const float eq_lookup1[64] = {
	4.000000f, 3.610166f, 3.320019f, 3.088821f, 2.896617f,
	2.732131f, 2.588368f, 2.460685f, 2.345845f, 2.241498f,
	2.145887f, 2.057660f, 1.975760f, 1.899338f, 1.827707f,
	1.760303f, 1.696653f, 1.636363f, 1.579094f, 1.524558f,
	1.472507f, 1.422724f, 1.375019f, 1.329225f, 1.285197f,
	1.242801f, 1.201923f, 1.162456f, 1.124306f, 1.087389f,
	1.051628f, 1.000000f, 0.983296f, 0.950604f, 0.918821f,
	0.887898f, 0.857789f, 0.828454f, 0.799853f, 0.771950f,
	0.744712f, 0.718108f, 0.692110f, 0.666689f, 0.641822f,
	0.617485f, 0.593655f, 0.570311f, 0.547435f, 0.525008f,
	0.503013f, 0.481433f, 0.460253f, 0.439458f, 0.419035f,
	0.398970f, 0.379252f, 0.359868f, 0.340807f, 0.322060f,
	0.303614f, 0.285462f, 0.267593f, 0.250000f
};

static inline double VALTODB(int v)
{
	v -= 31;
	if (v < -31) v = -31;
	if (v > 32) v = 32;

	if (v > 0) return -12.0 * (v / 32.0);
	else if (v < 0) return -12.0 * (v / 31.0);
	return 0.0f;
}

void eq_reset_channels(int nch)
{
	std::lock_guard<std::mutex> lock(eq_lock);
	free(eq);
	eq = nch > 0 ? (eq10_t *)calloc(nch, sizeof(eq10_t)) : 0;
	eq_nch = eq ? nch : 0;
	filter_srate = 0;
}

void eq_set(int on, char data[10], int preamp)
{
	if (in_mod && in_mod->EQSet) in_mod->EQSet(on, data, preamp);

	std::lock_guard<std::mutex> lock(eq_lock);
	int x;
	for (x = 0; x < 10 && data[x] == 31; x++);
	if (!on || (preamp == 31 && x == 10))
		filter_enabled = 0;
	else
		filter_enabled = 1;
	if (preamp < 0) preamp = 0;
	if (preamp > 63) preamp = 63;
	preamp_val = eq_lookup1[preamp];

	for (x = 0; x < 10; x++)
	{
		eqt[x] = (float)VALTODB(data[x]);
		if (filter_srate && eq) eq10_setgain(eq, eq_nch, x, eqt[x]);
	}
}

void eq_apply_config()
{
	eq_set(config_use_eq, (char *)eq_tab, config_preamp);
}

int eq_isactive()
{
	return filter_enabled ? 2 : 0;
}

static void FillFloat(float *out, const void *samples, int bps, size_t count, float gain)
{
	switch (bps)
	{
	case 8:
	{
		const uint8_t *s = (const uint8_t *)samples;
		gain /= 128.0f;
		for (size_t i = 0; i < count; i++) out[i] = (float)((int)s[i] - 128) * gain;
		break;
	}
	case 16:
	{
		const int16_t *s = (const int16_t *)samples;
		gain /= 32768.0f;
		for (size_t i = 0; i < count; i++) out[i] = (float)s[i] * gain;
		break;
	}
	case 24:
	{
		const uint8_t *s = (const uint8_t *)samples;
		gain /= 2147483648.0f;
		for (size_t i = 0; i < count; i++, s += 3)
			out[i] = (float)(int32_t)(((uint32_t)s[0] << 8) | ((uint32_t)s[1] << 16) | ((uint32_t)s[2] << 24)) * gain;
		break;
	}
	case 32:
	{
		const int32_t *s = (const int32_t *)samples;
		gain /= 2147483648.0f;
		for (size_t i = 0; i < count; i++) out[i] = (float)s[i] * gain;
		break;
	}
	}
}

static inline float clampf(float v)
{
	return v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
}

static void FillSamples(void *samples, const float *in, int bps, size_t count)
{
	switch (bps)
	{
	case 8:
	{
		uint8_t *d = (uint8_t *)samples;
		for (size_t i = 0; i < count; i++) d[i] = (uint8_t)(lrintf(clampf(in[i]) * 127.0f) + 128);
		break;
	}
	case 16:
	{
		int16_t *d = (int16_t *)samples;
		for (size_t i = 0; i < count; i++) d[i] = (int16_t)lrintf(clampf(in[i]) * 32767.0f);
		break;
	}
	case 24:
	{
		uint8_t *d = (uint8_t *)samples;
		for (size_t i = 0; i < count; i++, d += 3)
		{
			int32_t v = (int32_t)lrint(clampf(in[i]) * 8388607.0);
			d[0] = (uint8_t)v;
			d[1] = (uint8_t)(v >> 8);
			d[2] = (uint8_t)(v >> 16);
		}
		break;
	}
	case 32:
	{
		int32_t *d = (int32_t *)samples;
		for (size_t i = 0; i < count; i++) d[i] = (int32_t)lrint(clampf(in[i]) * 2147483647.0);
		break;
	}
	}
}

// called by the input plug-in on its decode thread (dsp_dosamples)
int eq_dosamples(short *samples, int numsamples, int bps, int nch, int srate)
{
	std::lock_guard<std::mutex> lock(eq_lock);
	if (!filter_enabled || (in_mod && (in_mod->UsesOutputPlug & IN_MODULE_FLAG_EQ)) || nch < 1)
	{
		filter_srate = 0;
		return numsamples;
	}
	if (!eq || eq_nch != nch)
	{
		free(eq);
		eq = (eq10_t *)calloc(nch, sizeof(eq10_t));
		eq_nch = nch;
		filter_srate = 0;
	}
	if (filter_srate != srate)
	{
		eq10_setup(eq, nch, (float)srate);
		for (int x = 0; x < 10; x++)
			eq10_setgain(eq, nch, x, eqt[x]);
		filter_srate = srate;
	}
	if (splbuf_alloc < numsamples * nch)
	{
		int n = numsamples * nch;
		float *nb = (float *)realloc(splbuf, 2 * sizeof(float) * n);
		if (!nb) return numsamples;
		splbuf = nb;
		splbuf_alloc = n;
	}
	int y = nch * numsamples;
	FillFloat(splbuf, samples, bps, (size_t)y, preamp_val);
	for (int x = 0; x < nch; x++)
		eq10_processf(eq + x, splbuf, splbuf + y, numsamples, x, nch);
	FillSamples(samples, splbuf + y, bps, (size_t)y);
	return numsamples;
}

/* ---------------- preset files (Src/Winamp/Eq.cpp) ---------------- */

#define EQ_MAX_FNAME 256 // _MAX_FNAME on Windows; part of the file format
static const char sig[] = "Winamp EQ library file v1.1\x1A!--";
static const size_t REC = EQ_MAX_FNAME + 1 + 10 + 1;

static bool check_sig(FILE *f)
{
	char s[64] = {0};
	size_t l = strlen(sig);
	return fread(s, 1, l, f) == l && !memcmp(s, sig, l);
}

static void write_record(FILE *f, const char *name, const unsigned char *tab, unsigned char preamp)
{
	char s[REC];
	memset(s, 0, sizeof(s));
	strncpy(s, name, EQ_MAX_FNAME);
	memcpy(s + EQ_MAX_FNAME + 1, tab, 10);
	s[EQ_MAX_FNAME + 1 + 10] = (char)preamp;
	fwrite(s, 1, REC, f);
}

static bool write_eq(const std::string &file, const char *name, const unsigned char *tab, unsigned char preamp, bool keep_existing)
{
	FILE *f = fopen(file.c_str(), "r+b");
	if (!f) f = fopen(file.c_str(), "w+b");
	if (!f) return false;
	if (!check_sig(f))
	{
		fclose(f);
		f = fopen(file.c_str(), "w+b");
		if (!f) return false;
		fwrite(sig, 1, strlen(sig), f);
	}
	char s[REC];
	for (;;)
	{
		long pos = ftell(f);
		if (fread(s, 1, REC, f) != REC)
		{
			fseek(f, 0, SEEK_END);
			break;
		}
		if (!strcasecmp(name, s))
		{
			if (keep_existing)
			{
				fclose(f);
				return true;
			}
			fseek(f, pos, SEEK_SET);
			break;
		}
	}
	write_record(f, name, tab, preamp);
	fclose(f);
	return true;
}

void eq_create_default_presets()
{
	if (wa::file_exists(config_eq_path())) return;
	static const struct
	{
		const char *s;
		unsigned char tab[10];
	} eqsets[] = {
		{"Classical", {31, 31, 31, 31, 31, 31, 44, 44, 44, 48}},
		{"Club", {31, 31, 26, 22, 22, 22, 26, 31, 31, 31}},
		{"Dance", {16, 20, 28, 32, 32, 42, 44, 44, 32, 32}},
		{"Flat", {31, 31, 31, 31, 31, 31, 31, 31, 31, 31}},
		{"Laptop speakers/headphones", {24, 14, 23, 38, 36, 29, 24, 16, 11, 8}},
		{"Large hall", {15, 15, 22, 22, 31, 40, 40, 40, 31, 31}},
		{"Party", {20, 20, 31, 31, 31, 31, 31, 31, 20, 20}},
		{"Pop", {35, 24, 20, 19, 23, 34, 36, 36, 35, 35}},
		{"Reggae", {31, 31, 33, 42, 31, 21, 21, 31, 31, 31}},
		{"Rock", {19, 24, 41, 45, 38, 25, 17, 14, 14, 14}},
		{"Soft", {24, 29, 34, 36, 34, 25, 18, 16, 14, 12}},
		{"Ska", {36, 40, 39, 33, 25, 22, 17, 16, 14, 16}},
		{"Full Bass", {16, 16, 16, 22, 29, 39, 46, 49, 50, 50}},
		{"Soft Rock", {25, 25, 28, 33, 39, 41, 38, 33, 27, 17}},
		{"Full Treble", {48, 48, 48, 39, 27, 14, 6, 6, 6, 4}},
		{"Full Bass & Treble", {20, 22, 31, 44, 40, 29, 18, 14, 12, 12}},
		{"Live", {40, 31, 25, 23, 22, 22, 25, 27, 27, 28}},
		{"Techno", {19, 22, 31, 41, 40, 31, 19, 16, 16, 17}},
	};
	wa::make_dirs(wa::config_dir());
	for (const auto &e : eqsets)
		write_eq(config_eq_path(), e.s, e.tab, 31, true);
}

bool eq_write_preset(const std::string &file, const std::string &name)
{
	return write_eq(file, name.c_str(), eq_tab, (unsigned char)config_preamp, false);
}

bool eq_read_preset(const std::string &file, const std::string &name)
{
	FILE *f = fopen(file.c_str(), "rb");
	if (!f) return false;
	if (!check_sig(f))
	{
		fclose(f);
		return false;
	}
	char s[REC];
	bool found = false;
	while (fread(s, 1, REC, f) == REC)
	{
		s[EQ_MAX_FNAME] = 0;
		if (!strcasecmp(name.c_str(), s))
		{
			found = true;
			break;
		}
	}
	fclose(f);
	if (!found) return false;
	for (int x = 0; x < 10; x++)
	{
		unsigned char v = (unsigned char)s[EQ_MAX_FNAME + 1 + x];
		eq_tab[x] = v > 63 ? 63 : v;
	}
	unsigned char p = (unsigned char)s[EQ_MAX_FNAME + 1 + 10];
	config_preamp = p > 63 ? 63 : p;
	eq_apply_config();
	return true;
}

void eq_delete_preset(const std::string &file, const std::string &name)
{
	FILE *f = fopen(file.c_str(), "rb");
	if (!f) return;
	if (!check_sig(f))
	{
		fclose(f);
		return;
	}
	std::vector<char> keep;
	char s[REC];
	while (fread(s, 1, REC, f) == REC)
	{
		char n[EQ_MAX_FNAME + 1];
		memcpy(n, s, EQ_MAX_FNAME);
		n[EQ_MAX_FNAME] = 0;
		if (strcasecmp(name.c_str(), n)) keep.insert(keep.end(), s, s + REC);
	}
	fclose(f);
	f = fopen(file.c_str(), "wb");
	if (!f) return;
	fwrite(sig, 1, strlen(sig), f);
	if (!keep.empty()) fwrite(keep.data(), 1, keep.size(), f);
	fclose(f);
}

std::vector<std::string> eq_list_presets(const std::string &file)
{
	std::vector<std::string> r;
	FILE *f = fopen(file.c_str(), "rb");
	if (!f) return r;
	if (check_sig(f))
	{
		char s[REC];
		while (fread(s, 1, REC, f) == REC)
		{
			s[EQ_MAX_FNAME] = 0;
			r.push_back(s);
		}
	}
	fclose(f);
	return r;
}

void eq_autoload(const std::string &filename)
{
	if (!config_autoload_eq) return;
	if (!eq_read_preset(config_eq_auto_path(), wa::path_filename(filename)))
		eq_read_preset(config_eq_path(), "Default");
}

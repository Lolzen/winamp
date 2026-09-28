/*
** Winamp for Linux - disk writer output plug-in (out_disk.so), the Linux
** counterpart of the Nullsoft Disk Writer: instead of playing, every song is
** written as a .wav file (as fast as the input can decode).
**
** Output folder: $WINAMP_DISKWRITER_DIR, or ~/Music if unset.
*/
#include "../../sdk/out.h"
#include "../../sdk/wa_ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>

static Out_Module mod;
static FILE *fp = nullptr;
static int srate, nch, bps;
static unsigned long long data_bytes = 0;
static int base_time_ms = 0;
static int is_paused = 0;

static std::string output_dir()
{
	const char *env = getenv("WINAMP_DISKWRITER_DIR");
	if (env && *env) return env;
	const char *home = getenv("HOME");
	return std::string(home ? home : ".") + "/Music";
}

static std::string output_name()
{
	std::string base = "winamp";
	LRESULT pos = SendMessage(mod.hMainWindow, WM_WA_IPC, 0, IPC_GETLISTPOS);
	const char *fn = (const char *)SendMessage(mod.hMainWindow, WM_WA_IPC, (WPARAM)pos, IPC_GETPLAYLISTFILE);
	if (fn && *fn)
	{
		const char *s = strrchr(fn, '/');
		base = s ? s + 1 : fn;
		size_t dot = base.find_last_of('.');
		if (dot != std::string::npos && dot > 0) base = base.substr(0, dot);
	}
	return output_dir() + "/" + base + ".wav";
}

static void put32(unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
	p[2] = (unsigned char)(v >> 16);
	p[3] = (unsigned char)(v >> 24);
}

static void write_header()
{
	unsigned char h[44];
	memcpy(h, "RIFF", 4);
	put32(h + 4, (uint32_t)(36 + data_bytes));
	memcpy(h + 8, "WAVEfmt ", 8);
	put32(h + 16, 16);
	h[20] = 1; h[21] = 0; // PCM
	h[22] = (unsigned char)nch; h[23] = 0;
	put32(h + 24, (uint32_t)srate);
	put32(h + 28, (uint32_t)(srate * nch * bps / 8));
	h[32] = (unsigned char)(nch * bps / 8); h[33] = 0;
	h[34] = (unsigned char)bps; h[35] = 0;
	memcpy(h + 36, "data", 4);
	put32(h + 40, (uint32_t)data_bytes);
	fseek(fp, 0, SEEK_SET);
	fwrite(h, 1, 44, fp);
	fseek(fp, 0, SEEK_END);
}

static void Config(HWND) {}
static void About(HWND) {}
static void Init() {}
static void Close();
static void Quit() { Close(); }

static int Open(int samplerate, int numchannels, int bitspersamp, int, int)
{
	Close();
	srate = samplerate;
	nch = numchannels;
	bps = bitspersamp;
	data_bytes = 0;
	base_time_ms = 0;
	is_paused = 0;
	mkdir(output_dir().c_str(), 0755);
	fp = fopen(output_name().c_str(), "wb");
	if (!fp) return -1;
	write_header();
	return 0;
}

static void Close()
{
	if (!fp) return;
	write_header();
	fclose(fp);
	fp = nullptr;
}

static int Write(char *buf, int len)
{
	if (!fp) return 1;
	fwrite(buf, 1, (size_t)len, fp);
	data_bytes += (unsigned long long)len;
	return 0;
}

static int CanWrite() { return fp && !is_paused ? 65536 : 0; }
static int IsPlaying() { return 0; }
static int Pause(int p)
{
	int o = is_paused;
	is_paused = p;
	return o;
}
static void SetVolume(int) {}
static void SetPan(int) {}
static void Flush(int t) { base_time_ms = t; }

static int GetWrittenTime()
{
	int bpsec = srate * nch * bps / 8;
	return bpsec ? base_time_ms + (int)(data_bytes * 1000 / (unsigned long long)bpsec) : 0;
}

static int GetOutputTime() { return GetWrittenTime(); }

extern "C" WA_EXPORT Out_Module *winampGetOutModule()
{
	mod.version = OUT_VER;
	mod.description = (char *)"Nullsoft Disk Writer v2.14 for Linux";
	mod.id = 33;
	mod.Config = Config;
	mod.About = About;
	mod.Init = Init;
	mod.Quit = Quit;
	mod.Open = Open;
	mod.Close = Close;
	mod.Write = Write;
	mod.CanWrite = CanWrite;
	mod.IsPlaying = IsPlaying;
	mod.Pause = Pause;
	mod.SetVolume = SetVolume;
	mod.SetPan = SetPan;
	mod.Flush = Flush;
	mod.GetOutputTime = GetOutputTime;
	mod.GetWrittenTime = GetWrittenTime;
	return &mod;
}

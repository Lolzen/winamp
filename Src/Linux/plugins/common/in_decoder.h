/*
** Winamp for Linux - shared implementation of the classic input plug-in
** decode loop (the same structure as the SDK's in_raw/in_tone examples):
** a decode thread feeds the output plug-in in 576-sample blocks, hands the
** PCM to the visualizer and the DSP/EQ chain, handles seeking and posts
** WM_WA_MPEG_EOF when the output has played everything.
**
** A plug-in derives from Decoder and instantiates InputPluginImpl<>.
*/
#pragma once

#include "../../sdk/in2.h"
#include "../../sdk/wa_ipc.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <string.h>
#include <strings.h>
#include <unistd.h>

class Decoder
{
public:
	virtual ~Decoder() {}
	// open the file, fill in samplerate/channels/bps/length_ms/bitrate
	virtual bool Open(const char *filename) = 0;
	// decode up to max_bytes of interleaved PCM; returns bytes, 0 at the end, <0 on error
	virtual int Decode(char *buf, int max_bytes) = 0;
	virtual bool Seek(int ms) = 0;
	virtual int CurrentBitrate() { return bitrate; }

	int samplerate = 0;
	int channels = 0;
	int bps = 16;
	int length_ms = -1000;
	int bitrate = 0; // kbps
	bool seekable = true;
};

namespace wa_plugin
{
	// helper: case-insensitive "file.ext" check
	inline bool has_extension(const char *fn, const char *const *exts)
	{
		const char *dot = strrchr(fn, '.');
		if (!dot) return false;
		for (int i = 0; exts[i]; i++)
			if (!strcasecmp(dot + 1, exts[i])) return true;
		return false;
	}

	inline int copy_string(char *dest, int destlen, const std::string &s)
	{
		if (destlen <= 0) return 0;
		size_t n = s.size() < (size_t)(destlen - 1) ? s.size() : (size_t)(destlen - 1);
		memcpy(dest, s.data(), n);
		dest[n] = 0;
		return 1;
	}
}

template <class DecoderT>
class InputPluginImpl
{
public:
	static In_Module mod;

	static int Play(const char *fn)
	{
		Stop();
		DecoderT *d = new DecoderT;
		if (!d->Open(fn))
		{
			delete d;
			return -1;
		}
		if (!mod.outMod) { delete d; return 1; }
		int maxlatency = mod.outMod->Open(d->samplerate, d->channels, d->bps, -1, -1);
		if (maxlatency < 0)
		{
			delete d;
			return 1;
		}
		decoder = d;
		mod.is_seekable = d->seekable && d->length_ms > 0;
		paused = 0;
		seek_needed = -1;
		decode_pos_ms = 0;
		kill = false;
		mod.SetInfo(d->bitrate, d->samplerate / 1000, d->channels, 1);
		mod.SAVSAInit(maxlatency, d->samplerate);
		mod.VSASetInfo(d->samplerate, d->channels);
		mod.outMod->SetVolume(-666); // set the output plug-in's default volume
		thread = std::thread(DecodeThread);
		return 0;
	}

	static void Pause()
	{
		paused = 1;
		if (mod.outMod) mod.outMod->Pause(1);
	}
	static void UnPause()
	{
		paused = 0;
		if (mod.outMod) mod.outMod->Pause(0);
	}
	static int IsPaused() { return paused; }

	static void Stop()
	{
		if (thread.joinable())
		{
			kill = true;
			thread.join();
		}
		if (decoder)
		{
			if (mod.outMod) mod.outMod->Close();
			mod.SAVSADeInit();
			delete decoder;
			decoder = nullptr;
		}
	}

	static int GetLength() { return decoder ? decoder->length_ms : -1000; }
	static int GetOutputTime() { return mod.outMod ? mod.outMod->GetOutputTime() : 0; }
	static void SetOutputTime(int ms)
	{
		if (decoder && mod.is_seekable) seek_needed = ms;
	}
	static void SetVolume(int v)
	{
		if (mod.outMod) mod.outMod->SetVolume(v);
	}
	static void SetPan(int p)
	{
		if (mod.outMod) mod.outMod->SetPan(p);
	}
	static void EQSet(int, char[10], int) {}

private:
	static void DecodeThread()
	{
		DecoderT *d = decoder;
		const int nch = d->channels, bytes = d->bps / 8;
		const int block = 576 * nch * bytes;
		std::vector<char> buf(block * 2 + 64);
		bool done = false;
		int last_bitrate = d->bitrate;
		while (!kill)
		{
			int s = seek_needed.exchange(-1);
			if (s != -1)
			{
				if (d->Seek(s)) decode_pos_ms = s;
				mod.outMod->Flush((int)decode_pos_ms);
				done = false;
			}
			if (done)
			{
				mod.outMod->CanWrite(); // some output plug-ins need this to flush their buffers
				if (!mod.outMod->IsPlaying())
				{
					PostMessage(mod.hMainWindow, WM_WA_MPEG_EOF, 0, 0);
					return;
				}
				usleep(10000);
				continue;
			}
			if (mod.outMod->CanWrite() < block * (mod.dsp_isactive() ? 2 : 1))
			{
				usleep(10000);
				continue;
			}
			// always hand out whole 576 sample blocks (the vis needs them)
			int got = 0;
			while (got < block)
			{
				int r = d->Decode(buf.data() + got, block - got);
				if (r <= 0) break;
				got += r;
			}
			got -= got % (nch * bytes);
			if (got <= 0)
			{
				done = true;
				continue;
			}
			if (got < block) memset(buf.data() + got, 0, block - got);
			int samples = got / (nch * bytes);
			mod.SAAddPCMData(buf.data(), nch, d->bps, (int)decode_pos_ms);
			decode_pos_ms += samples * 1000.0 / d->samplerate;
			if (mod.dsp_isactive())
				samples = mod.dsp_dosamples((short *)buf.data(), samples, d->bps, nch, d->samplerate);
			mod.outMod->Write(buf.data(), samples * nch * bytes);
			int br = d->CurrentBitrate();
			if (br != last_bitrate)
			{
				last_bitrate = br;
				mod.SetInfo(br, -1, -1, 1);
			}
		}
	}

	static DecoderT *decoder;
	static std::thread thread;
	static std::atomic<bool> kill;
	static std::atomic<int> seek_needed;
	static int paused;
	static double decode_pos_ms;
};

template <class D> D *InputPluginImpl<D>::decoder = nullptr;
template <class D> std::thread InputPluginImpl<D>::thread;
template <class D> std::atomic<bool> InputPluginImpl<D>::kill(false);
template <class D> std::atomic<int> InputPluginImpl<D>::seek_needed(-1);
template <class D> int InputPluginImpl<D>::paused = 0;
template <class D> double InputPluginImpl<D>::decode_pos_ms = 0;

/*
** Defines the In_Module and the exported winampGetInModule2().
** DESC: description, EXTS: "mp3;mp2\0MPEG Audio (*.mp3;*.mp2)\0"
** GETFILEINFO/INFOBOX/ISOURFILE/INIT/QUIT: plug-in specific functions
*/
#define WA_DEFINE_INPUT_PLUGIN(DecoderT, DESC, EXTS, INIT, QUIT, GETFILEINFO, INFOBOX, ISOURFILE) \
	template <> In_Module InputPluginImpl<DecoderT>::mod = { \
		IN_VER, (char *)DESC, 0, 0, (char *)EXTS, 1, IN_MODULE_FLAG_USES_OUTPUT_PLUGIN, \
		0, 0, INIT, QUIT, GETFILEINFO, INFOBOX, ISOURFILE, \
		InputPluginImpl<DecoderT>::Play, InputPluginImpl<DecoderT>::Pause, InputPluginImpl<DecoderT>::UnPause, \
		InputPluginImpl<DecoderT>::IsPaused, InputPluginImpl<DecoderT>::Stop, \
		InputPluginImpl<DecoderT>::GetLength, InputPluginImpl<DecoderT>::GetOutputTime, InputPluginImpl<DecoderT>::SetOutputTime, \
		InputPluginImpl<DecoderT>::SetVolume, InputPluginImpl<DecoderT>::SetPan, \
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
		InputPluginImpl<DecoderT>::EQSet, 0, 0, 0 }; \
	extern "C" WA_EXPORT In_Module *winampGetInModule2() { return &InputPluginImpl<DecoderT>::mod; }

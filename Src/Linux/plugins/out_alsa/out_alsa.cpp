/*
** Winamp for Linux - ALSA output plug-in (out_alsa.so).
**
** Buffers what the input plug-in writes and feeds the "default" ALSA device
** from a thread. Volume and panning are applied in software (like
** out_wave on Windows can).
*/
#include "../../sdk/out.h"

#include <alsa/asoundlib.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <string.h>

static Out_Module mod;

static snd_pcm_t *pcm = nullptr;
static std::thread writer;
static std::mutex lock;
static std::condition_variable cv;
static std::vector<char> ring;
static size_t ring_read = 0, ring_used = 0;
static bool quit_thread = false;
static int is_paused = 0;
static int srate = 0, nch = 0, bps = 0, frame_bytes = 0, bytes_per_sec = 0;
static int base_time_ms = 0;
static unsigned long long written_bytes = 0, played_bytes = 0; // since base_time_ms
static std::atomic<int> volume(255), pan(0);
static snd_pcm_sframes_t device_delay = 0;
static bool can_pause = false;

static void apply_volume(char *buf, size_t len)
{
	int v = volume, p = pan;
	if (v == 255 && p == 0) return;
	float l = v / 255.0f, r = l;
	if (p > 0) l *= (128 - p) / 128.0f;
	if (p < 0) r *= (128 + p) / 128.0f;
	size_t samples = len / (bps / 8);
	for (size_t i = 0; i < samples; i++)
	{
		float g = (nch == 2) ? ((i & 1) ? r : l) : (l > r ? l : r);
		switch (bps)
		{
		case 8:
			buf[i] = (char)(uint8_t)((int)(((uint8_t)buf[i] - 128) * g) + 128);
			break;
		case 16:
		{
			int16_t *s = (int16_t *)buf;
			s[i] = (int16_t)(s[i] * g);
			break;
		}
		case 24:
		{
			uint8_t *s = (uint8_t *)buf + i * 3;
			int32_t x = (int32_t)(((uint32_t)s[0] << 8) | ((uint32_t)s[1] << 16) | ((uint32_t)s[2] << 24)) >> 8;
			x = (int32_t)(x * g);
			s[0] = (uint8_t)x;
			s[1] = (uint8_t)(x >> 8);
			s[2] = (uint8_t)(x >> 16);
			break;
		}
		case 32:
		{
			int32_t *s = (int32_t *)buf;
			s[i] = (int32_t)(s[i] * g);
			break;
		}
		}
	}
}

static void writer_thread()
{
	std::vector<char> chunk;
	const size_t period = (size_t)frame_bytes * (srate / 50 > 64 ? srate / 50 : 64); // ~20ms
	for (;;)
	{
		size_t n;
		{
			std::unique_lock<std::mutex> l(lock);
			cv.wait(l, [] { return quit_thread || (!is_paused && ring_used > 0); });
			if (quit_thread) return;
			n = ring_used < period ? ring_used : period;
			n -= n % frame_bytes;
			if (!n) continue;
			chunk.resize(n);
			size_t first = ring.size() - ring_read < n ? ring.size() - ring_read : n;
			memcpy(chunk.data(), ring.data() + ring_read, first);
			memcpy(chunk.data() + first, ring.data(), n - first);
			ring_read = (ring_read + n) % ring.size();
			ring_used -= n;
		}
		apply_volume(chunk.data(), n);
		const char *p = chunk.data();
		snd_pcm_uframes_t frames = n / frame_bytes;
		while (frames > 0)
		{
			snd_pcm_sframes_t w = snd_pcm_writei(pcm, p, frames);
			if (w == -EAGAIN) continue;
			if (w < 0)
			{
				w = snd_pcm_recover(pcm, (int)w, 1);
				if (w < 0) break;
				continue;
			}
			p += w * frame_bytes;
			frames -= (snd_pcm_uframes_t)w;
			std::lock_guard<std::mutex> l(lock);
			played_bytes += (unsigned long long)w * frame_bytes;
		}
		snd_pcm_sframes_t d = 0;
		if (snd_pcm_delay(pcm, &d) == 0)
		{
			std::lock_guard<std::mutex> l(lock);
			device_delay = d;
		}
	}
}

static void Config(HWND) {}
static void About(HWND) {}
static void Init() {}
static void Close();
static void Quit() { Close(); }

static int Open(int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms)
{
	(void)prebufferms;
	Close();
	snd_pcm_format_t fmt;
	switch (bitspersamp)
	{
	case 8: fmt = SND_PCM_FORMAT_U8; break;
	case 16: fmt = SND_PCM_FORMAT_S16_LE; break;
	case 24: fmt = SND_PCM_FORMAT_S24_3LE; break;
	case 32: fmt = SND_PCM_FORMAT_S32_LE; break;
	default: return -1;
	}
	if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
	{
		pcm = nullptr;
		return -1;
	}
	if (snd_pcm_set_params(pcm, fmt, SND_PCM_ACCESS_RW_INTERLEAVED, numchannels, samplerate, 1, 150000) < 0)
	{
		snd_pcm_close(pcm);
		pcm = nullptr;
		return -1;
	}
	snd_pcm_hw_params_t *hw;
	snd_pcm_hw_params_alloca(&hw);
	can_pause = snd_pcm_hw_params_current(pcm, hw) == 0 && snd_pcm_hw_params_can_pause(hw);

	srate = samplerate;
	nch = numchannels;
	bps = bitspersamp;
	frame_bytes = nch * bps / 8;
	bytes_per_sec = srate * frame_bytes;
	if (bufferlenms <= 0) bufferlenms = 2000;
	ring.assign((size_t)bytes_per_sec * bufferlenms / 1000 / frame_bytes * frame_bytes, 0);
	ring_read = ring_used = 0;
	base_time_ms = 0;
	written_bytes = played_bytes = 0;
	device_delay = 0;
	is_paused = 0;
	quit_thread = false;
	writer = std::thread(writer_thread);
	return bufferlenms + 150;
}

static void Close()
{
	if (writer.joinable())
	{
		{
			std::lock_guard<std::mutex> l(lock);
			quit_thread = true;
		}
		cv.notify_all();
		writer.join();
	}
	if (pcm)
	{
		snd_pcm_drop(pcm);
		snd_pcm_close(pcm);
		pcm = nullptr;
	}
}

static int Write(char *buf, int len)
{
	if (!pcm) return 1;
	{
		std::lock_guard<std::mutex> l(lock);
		if (ring.size() - ring_used < (size_t)len) return 1;
		size_t w = (ring_read + ring_used) % ring.size();
		size_t first = ring.size() - w < (size_t)len ? ring.size() - w : (size_t)len;
		memcpy(ring.data() + w, buf, first);
		memcpy(ring.data(), buf + first, len - first);
		ring_used += len;
		written_bytes += len;
	}
	cv.notify_all();
	return 0;
}

static int CanWrite()
{
	if (!pcm || is_paused) return 0;
	std::lock_guard<std::mutex> l(lock);
	return (int)(ring.size() - ring_used);
}

static int IsPlaying()
{
	if (!pcm) return 0;
	std::lock_guard<std::mutex> l(lock);
	if (ring_used > 0) return 1;
	snd_pcm_sframes_t d = 0;
	if (snd_pcm_delay(pcm, &d) == 0) device_delay = d;
	else device_delay = 0;
	return device_delay > 0 && snd_pcm_state(pcm) == SND_PCM_STATE_RUNNING;
}

static int Pause(int p)
{
	int old = is_paused;
	if (!pcm) return old;
	{
		std::lock_guard<std::mutex> l(lock);
		is_paused = p;
		if (can_pause) snd_pcm_pause(pcm, p ? 1 : 0);
	}
	cv.notify_all();
	return old;
}

static void SetVolume(int v)
{
	if (v == -666) return;
	volume = v < 0 ? 0 : v > 255 ? 255 : v;
}

static void SetPan(int p)
{
	pan = p < -128 ? -128 : p > 128 ? 128 : p;
}

static void Flush(int t)
{
	if (!pcm) return;
	std::lock_guard<std::mutex> l(lock);
	ring_read = ring_used = 0;
	snd_pcm_drop(pcm);
	snd_pcm_prepare(pcm);
	if (is_paused && can_pause) snd_pcm_pause(pcm, 1);
	base_time_ms = t;
	written_bytes = played_bytes = 0;
	device_delay = 0;
}

static int GetOutputTime()
{
	if (!bytes_per_sec) return base_time_ms;
	std::lock_guard<std::mutex> l(lock);
	long long ms = (long long)(played_bytes * 1000 / (unsigned long long)bytes_per_sec) - (long long)device_delay * 1000 / srate;
	if (ms < 0) ms = 0;
	return base_time_ms + (int)ms;
}

static int GetWrittenTime()
{
	if (!bytes_per_sec) return base_time_ms;
	return base_time_ms + (int)(written_bytes * 1000 / (unsigned long long)bytes_per_sec);
}

extern "C" WA_EXPORT Out_Module *winampGetOutModule()
{
	mod.version = OUT_VER;
	mod.description = (char *)"Nullsoft ALSA Output v1.0 for Linux";
	mod.id = 73;
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

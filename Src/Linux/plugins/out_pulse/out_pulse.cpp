/*
** Winamp for Linux - PulseAudio output plug-in (out_pulse.so).
** Also the natural choice on PipeWire systems (pipewire-pulse).
**
** Implements the classic Out_Module contract: a non-blocking Write() into our
** buffer, CanWrite()/IsPlaying() for the decode loop, and GetOutputTime()
** that follows what is actually audible (used to sync the visualizer).
*/
#include "../../sdk/out.h"

#include <pulse/pulseaudio.h>
#include <string.h>
#include <stdio.h>
#include <vector>

static Out_Module mod;

static pa_threaded_mainloop *ml = nullptr;
static pa_context *ctx = nullptr;
static pa_stream *stream = nullptr;
static pa_sample_spec spec;
static std::vector<char> ring;
static size_t ring_read = 0, ring_used = 0;
static int bytes_per_sec = 0;
static int base_time_ms = 0;               // time at the last Open()/Flush()
static unsigned long long written_bytes = 0; // written by the input plug-in since base_time_ms
static unsigned long long handed_bytes = 0;  // handed to PulseAudio since base_time_ms
static int is_paused = 0;
static int volume = 255, pan = 0;

static void context_state_cb(pa_context *c, void *)
{
	switch (pa_context_get_state(c))
	{
	case PA_CONTEXT_READY:
	case PA_CONTEXT_FAILED:
	case PA_CONTEXT_TERMINATED:
		pa_threaded_mainloop_signal(ml, 0);
		break;
	default:
		break;
	}
}

static void stream_state_cb(pa_stream *s, void *)
{
	switch (pa_stream_get_state(s))
	{
	case PA_STREAM_READY:
	case PA_STREAM_FAILED:
	case PA_STREAM_TERMINATED:
		pa_threaded_mainloop_signal(ml, 0);
		break;
	default:
		break;
	}
}

// PulseAudio asks for data (runs on the mainloop thread with the lock held)
static void stream_write_cb(pa_stream *s, size_t nbytes, void *)
{
	while (nbytes > 0 && ring_used > 0)
	{
		size_t chunk = ring_used;
		if (chunk > nbytes) chunk = nbytes;
		if (ring_read + chunk > ring.size()) chunk = ring.size() - ring_read;
		pa_stream_write(s, ring.data() + ring_read, chunk, nullptr, 0, PA_SEEK_RELATIVE);
		ring_read = (ring_read + chunk) % ring.size();
		ring_used -= chunk;
		handed_bytes += chunk;
		nbytes -= chunk;
	}
}

static void success_cb(pa_stream *, int, void *)
{
	pa_threaded_mainloop_signal(ml, 0);
}

static void wait_op(pa_operation *op)
{
	if (!op) return;
	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(ml);
	pa_operation_unref(op);
}

static bool connect_context()
{
	if (ctx && pa_context_get_state(ctx) == PA_CONTEXT_READY) return true;
	if (ctx)
	{
		pa_context_disconnect(ctx);
		pa_context_unref(ctx);
		ctx = nullptr;
	}
	ctx = pa_context_new(pa_threaded_mainloop_get_api(ml), "Winamp");
	if (!ctx) return false;
	pa_context_set_state_callback(ctx, context_state_cb, nullptr);
	if (pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) return false;
	for (;;)
	{
		pa_context_state_t st = pa_context_get_state(ctx);
		if (st == PA_CONTEXT_READY) return true;
		if (!PA_CONTEXT_IS_GOOD(st)) return false;
		pa_threaded_mainloop_wait(ml);
	}
}

static void apply_volume()
{
	if (!stream || !ctx) return;
	pa_cvolume cv;
	pa_cvolume_init(&cv);
	cv.channels = spec.channels;
	pa_volume_t base = pa_sw_volume_from_linear(volume / 255.0);
	for (int c = 0; c < spec.channels; c++)
	{
		double f = 1.0;
		if (spec.channels == 2)
		{
			if (c == 0 && pan > 0) f = (128 - pan) / 128.0;
			if (c == 1 && pan < 0) f = (128 + pan) / 128.0;
		}
		cv.values[c] = pa_sw_volume_multiply(base, pa_sw_volume_from_linear(f));
	}
	pa_operation *op = pa_context_set_sink_input_volume(ctx, pa_stream_get_index(stream), &cv, nullptr, nullptr);
	if (op) pa_operation_unref(op);
}

static void Config(HWND) {}
static void About(HWND) {}

static void Init()
{
	ml = pa_threaded_mainloop_new();
	if (ml) pa_threaded_mainloop_start(ml);
}

static void Close();

static void Quit()
{
	Close();
	if (!ml) return;
	pa_threaded_mainloop_lock(ml);
	if (ctx)
	{
		pa_context_disconnect(ctx);
		pa_context_unref(ctx);
		ctx = nullptr;
	}
	pa_threaded_mainloop_unlock(ml);
	pa_threaded_mainloop_stop(ml);
	pa_threaded_mainloop_free(ml);
	ml = nullptr;
}

static int Open(int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms)
{
	(void)prebufferms;
	if (!ml) return -1;
	Close();
	switch (bitspersamp)
	{
	case 8: spec.format = PA_SAMPLE_U8; break;
	case 16: spec.format = PA_SAMPLE_S16LE; break;
	case 24: spec.format = PA_SAMPLE_S24LE; break;
	case 32: spec.format = PA_SAMPLE_S32LE; break;
	default: return -1;
	}
	spec.rate = (uint32_t)samplerate;
	spec.channels = (uint8_t)numchannels;
	if (!pa_sample_spec_valid(&spec)) return -1;

	bytes_per_sec = (int)pa_bytes_per_second(&spec);
	if (bufferlenms <= 0) bufferlenms = 2000;
	ring.assign((size_t)bytes_per_sec * bufferlenms / 1000 / pa_frame_size(&spec) * pa_frame_size(&spec), 0);
	ring_read = ring_used = 0;
	base_time_ms = 0;
	written_bytes = handed_bytes = 0;
	is_paused = 0;

	pa_threaded_mainloop_lock(ml);
	if (!connect_context())
	{
		pa_threaded_mainloop_unlock(ml);
		return -1;
	}
	pa_channel_map map;
	pa_channel_map_init_auto(&map, spec.channels, PA_CHANNEL_MAP_WAVEEX);
	pa_proplist *pl = pa_proplist_new();
	pa_proplist_sets(pl, PA_PROP_MEDIA_ROLE, "music");
	pa_proplist_sets(pl, PA_PROP_APPLICATION_ICON_NAME, "winamp");
	stream = pa_stream_new_with_proplist(ctx, "Winamp", &spec, &map, pl);
	pa_proplist_free(pl);
	if (!stream)
	{
		pa_threaded_mainloop_unlock(ml);
		return -1;
	}
	pa_stream_set_state_callback(stream, stream_state_cb, nullptr);
	pa_stream_set_write_callback(stream, stream_write_cb, nullptr);
	pa_buffer_attr attr;
	attr.maxlength = (uint32_t)-1;
	attr.tlength = (uint32_t)pa_usec_to_bytes(150 * 1000, &spec);
	attr.prebuf = (uint32_t)-1;
	attr.minreq = (uint32_t)-1;
	attr.fragsize = (uint32_t)-1;
	pa_stream_flags_t flags = (pa_stream_flags_t)(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_ADJUST_LATENCY);
	if (pa_stream_connect_playback(stream, nullptr, &attr, flags, nullptr, nullptr) < 0)
	{
		pa_stream_unref(stream);
		stream = nullptr;
		pa_threaded_mainloop_unlock(ml);
		return -1;
	}
	for (;;)
	{
		pa_stream_state_t st = pa_stream_get_state(stream);
		if (st == PA_STREAM_READY) break;
		if (!PA_STREAM_IS_GOOD(st))
		{
			pa_stream_unref(stream);
			stream = nullptr;
			pa_threaded_mainloop_unlock(ml);
			return -1;
		}
		pa_threaded_mainloop_wait(ml);
	}
	apply_volume();
	pa_threaded_mainloop_unlock(ml);
	return bufferlenms + 150;
}

static void Close()
{
	if (!ml || !stream) return;
	pa_threaded_mainloop_lock(ml);
	pa_stream_set_write_callback(stream, nullptr, nullptr);
	pa_stream_set_state_callback(stream, nullptr, nullptr);
	pa_stream_disconnect(stream);
	pa_stream_unref(stream);
	stream = nullptr;
	ring_used = 0;
	pa_threaded_mainloop_unlock(ml);
}

static int Write(char *buf, int len)
{
	if (!stream) return 1;
	pa_threaded_mainloop_lock(ml);
	if (ring.size() - ring_used < (size_t)len)
	{
		pa_threaded_mainloop_unlock(ml);
		return 1;
	}
	size_t w = (ring_read + ring_used) % ring.size();
	size_t first = ring.size() - w < (size_t)len ? ring.size() - w : (size_t)len;
	memcpy(ring.data() + w, buf, first);
	memcpy(ring.data(), buf + first, len - first);
	ring_used += len;
	written_bytes += len;
	// push right away if PulseAudio is waiting for data
	size_t writable = pa_stream_writable_size(stream);
	if (writable != (size_t)-1 && writable > 0) stream_write_cb(stream, writable, nullptr);
	pa_threaded_mainloop_unlock(ml);
	return 0;
}

static int CanWrite()
{
	if (!stream) return 0;
	pa_threaded_mainloop_lock(ml);
	int r = (int)(ring.size() - ring_used);
	pa_threaded_mainloop_unlock(ml);
	return is_paused ? 0 : r;
}

static long long latency_usec()
{
	pa_usec_t lat = 0;
	int neg = 0;
	if (pa_stream_get_latency(stream, &lat, &neg) < 0) return 0;
	return neg ? -(long long)lat : (long long)lat;
}

static int IsPlaying()
{
	if (!stream) return 0;
	pa_threaded_mainloop_lock(ml);
	int r = 0;
	if (ring_used > 0) r = 1;
	else
	{
		// make sure the tail end gets played even when the server buffer isn't full
		pa_operation *op = pa_stream_trigger(stream, nullptr, nullptr);
		if (op) pa_operation_unref(op);
		r = latency_usec() > 0;
	}
	pa_threaded_mainloop_unlock(ml);
	return r;
}

static int Pause(int p)
{
	int old = is_paused;
	if (!stream) return old;
	is_paused = p;
	pa_threaded_mainloop_lock(ml);
	wait_op(pa_stream_cork(stream, p ? 1 : 0, success_cb, nullptr));
	pa_threaded_mainloop_unlock(ml);
	return old;
}

static void SetVolume(int v)
{
	if (v == -666) v = volume;
	volume = v < 0 ? 0 : v > 255 ? 255 : v;
	if (!ml) return;
	pa_threaded_mainloop_lock(ml);
	apply_volume();
	pa_threaded_mainloop_unlock(ml);
}

static void SetPan(int p)
{
	pan = p < -128 ? -128 : p > 128 ? 128 : p;
	if (!ml) return;
	pa_threaded_mainloop_lock(ml);
	apply_volume();
	pa_threaded_mainloop_unlock(ml);
}

static void Flush(int t)
{
	if (!stream) return;
	pa_threaded_mainloop_lock(ml);
	ring_read = ring_used = 0;
	wait_op(pa_stream_flush(stream, success_cb, nullptr));
	base_time_ms = t;
	written_bytes = handed_bytes = 0;
	pa_threaded_mainloop_unlock(ml);
}

static int GetOutputTime()
{
	if (!stream || !bytes_per_sec) return base_time_ms;
	pa_threaded_mainloop_lock(ml);
	long long ms = (long long)(handed_bytes * 1000 / (unsigned long long)bytes_per_sec) - latency_usec() / 1000;
	pa_threaded_mainloop_unlock(ml);
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
	mod.description = (char *)"Nullsoft PulseAudio Output v1.0 for Linux";
	mod.id = 72;
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

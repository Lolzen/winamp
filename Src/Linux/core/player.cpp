#include "player.h"
#include "plugins.h"
#include "playlist.h"
#include "vis.h"
#include "eq.h"
#include "config.h"
#include "common.h"
#include "projectm_audio.h"

#include <glib.h>
#include <atomic>
#include <stdio.h>

int playing = 0, paused = 0;
std::string FileName;
std::string FileTitle;
int g_brate = 0, g_srate = 0, g_nch = 0;
int g_stopaftercur = 0;

static PlayerCallbacks callbacks;
static std::atomic<bool> info_pending(false);
static guint fade_timer = 0;
static int failed_in_a_row = 0;

void player_set_callbacks(const PlayerCallbacks &cb)
{
	callbacks = cb;
}

static void notify_state()
{
	if (callbacks.state_changed) callbacks.state_changed();
}

/* ---------------- callbacks handed to input plug-ins (In.cpp) ---------------- */

static gboolean info_idle(gpointer)
{
	info_pending = false;
	if (callbacks.info_changed) callbacks.info_changed();
	return G_SOURCE_REMOVE;
}

static void setinfo(int bitrate, int srate, int stereo, int synched)
{
	(void)synched;
	if (stereo != -1)
	{
		g_nch = stereo;
		if (g_nch > 0) eq_reset_channels(g_nch);
	}
	if (bitrate != -1) g_brate = bitrate;
	if (srate != -1)
	{
		g_srate = srate;
		switch (srate)
		{
		case 11: g_srate_exact = 11025; break;
		case 22: g_srate_exact = 22050; break;
		case 44: g_srate_exact = 44100; break;
		case 88: g_srate_exact = 88200; break;
		default: g_srate_exact = srate * 1000; break;
		}
	}
	bool expected = false;
	if (info_pending.compare_exchange_strong(expected, true))
		g_idle_add(info_idle, nullptr);
}

static void vissa_init(int maxlatency_in_ms, int srate)
{
	g_srate_exact = srate;
	int nf = (int)((int64_t)maxlatency_in_ms * 4 * srate / 450000);
	sa_init(nf);
}

static void vissa_deinit()
{
	sa_deinit();
}

static int sa_getmode_cb()
{
	return sa_getmode();
}

static int sa_add_cb(void *data, int timestamp, int csa)
{
	return sa_add((char *)data, timestamp, csa);
}

static void projectm_sa_addpcmdata(void *data, int channels, int bits, int timestamp)
{
	(void)timestamp;
	// The Winamp SA callback is delivered once per 576 decoded frames by the
	// shared input decoder. Feed the same source PCM to projectM before the
	// optional DSP chain changes the playback buffer. This keeps the visualizer
	// independent of whether output is PulseAudio/PipeWire or ALSA.
	projectm_audio::push_pcm(data, channels, bits, 576);
}

static void vsa_addpcmdata(void *, int, int, int) {}
static int vsa_getmode(int *specNch, int *waveNch)
{
	if (specNch) *specNch = 0;
	if (waveNch) *waveNch = 0;
	return 0;
}
static int vsa_add(void *, int) { return 0; }
static void vsa_setinfo(int, int) {}

void player_setup_input_module(In_Module *mod)
{
	mod->SAVSAInit = vissa_init;
	mod->SAVSADeInit = vissa_deinit;
	mod->SAAddPCMData = projectm_sa_addpcmdata;
	mod->SAGetMode = sa_getmode_cb;
	mod->SAAdd = sa_add_cb;
	mod->VSAAddPCMData = vsa_addpcmdata;
	mod->VSAGetMode = vsa_getmode;
	mod->VSAAdd = vsa_add;
	mod->VSASetInfo = vsa_setinfo;
	mod->dsp_isactive = eq_isactive;
	mod->dsp_dosamples = eq_dosamples;
	mod->SetInfo = setinfo;
}

/* ---------------- In.cpp ---------------- */

int in_getouttime()
{
	if (in_mod && playing) return in_mod->GetOutputTime();
	return 0;
}

int in_getlength()
{
	if (!in_mod || !playing) return -1;
	int ms = in_mod->GetLength();
	if (ms <= 0) return -1;
	return ms / 1000;
}

bool in_seekable()
{
	return playing && in_mod && in_mod->is_seekable;
}

void in_setvol(int v)
{
	if (v < 0) v = 0;
	if (v > 255) v = 255;
	if (in_mod && playing) in_mod->SetVolume(v);
	else if (out_mod && out_mod->SetVolume) out_mod->SetVolume(v);
}

void in_setpan(int p)
{
	if (p < -127) p = -127;
	if (p > 127) p = 127;
	if (in_mod && playing) in_mod->SetPan(p);
	else if (out_mod && out_mod->SetPan) out_mod->SetPan(p);
}

int in_seek(int time_in_ms)
{
	if (!in_mod || !playing || !in_mod->is_seekable) return -1;
	if (time_in_ms < 0) time_in_ms = 0;
	in_mod->SetOutputTime(time_in_ms);
	return 0;
}

static int in_open(const std::string &fn)
{
	InputPlugin *p = in_find(fn);
	if (!p) return -1;
	if (!out_mod) return -31337; // no sound output available
	in_mod = p->mod;
	in_mod->outMod = out_mod;
	g_brate = g_srate = g_nch = 0;
	eq_apply_config();
	int r = in_mod->Play(fn.c_str());
	if (r != 0) return r;
	in_mod->SetVolume(config_volume);
	in_mod->SetPan(config_pan);
	return 0;
}

static void in_close()
{
	if (in_mod && playing) in_mod->Stop();
	sa_deinit();
}

/* ---------------- transport ---------------- */

void player_update_title()
{
	FileTitle = PlayList_gettitle(PlayList_getPosition());
}

static void cancel_fade()
{
	if (fade_timer)
	{
		g_source_remove(fade_timer);
		fade_timer = 0;
	}
}

void StopPlaying(int fade)
{
	if (fade && playing && !paused && !fade_timer)
	{
		struct Fade
		{
			int step;
		};
		Fade *f = new Fade{0};
		fade_timer = g_timeout_add_full(G_PRIORITY_DEFAULT, 30, [](gpointer d) -> gboolean {
			Fade *f = (Fade *)d;
			f->step++;
			int steps = 1000 / 30;
			if (f->step >= steps || !playing)
			{
				fade_timer = 0;
				StopPlaying(0);
				in_setvol(config_volume);
				return G_SOURCE_REMOVE;
			}
			in_setvol(config_volume * (steps - f->step) / steps);
			return G_SOURCE_CONTINUE;
		}, f, [](gpointer d) { delete (Fade *)d; });
		return;
	}
	cancel_fade();
	if (playing)
	{
		in_close();
		playing = 0;
		paused = 0;
		g_brate = g_srate = g_nch = 0;
		if (callbacks.info_changed) callbacks.info_changed();
	}
	notify_state();
}

static bool Play(const std::string &filename)
{
	cancel_fade();
	g_stopaftercur = 0;
	FileName = filename;
	player_update_title();
	eq_autoload(filename);
	int r = in_open(filename);
	if (r != 0)
	{
		fprintf(stderr, "winamp: can't play %s (error %d)\n", filename.c_str(), r);
		playing = 0;
		paused = 0;
		notify_state();
		return r != -31337;
	}
	playing = 1;
	paused = 0;
	failed_in_a_row = 0;
	notify_state();
	return true;
}

void StartPlaying()
{
	if (playing) StopPlaying(0);
	if (!PlayList_getlength())
	{
		notify_state();
		return;
	}
	std::string fn = PlayList_getfilename(PlayList_getPosition());
	if (!Play(fn))
		return; // no output plug-in, nothing more we can do
	if (!playing)
	{
		// couldn't open this one: skip ahead like Winamp does, but not forever
		if (++failed_in_a_row < PlayList_getlength())
			g_idle_add([](gpointer) -> gboolean { player_on_eof(); return G_SOURCE_REMOVE; }, nullptr);
		else
			failed_in_a_row = 0;
	}
}

void PausePlaying()
{
	if (!playing || !in_mod) return;
	if (paused)
	{
		in_mod->UnPause();
		paused = 0;
	}
	else
	{
		in_mod->Pause();
		paused = 1;
	}
	notify_state();
}

void PlayIndex(int idx)
{
	if (idx < 0 || idx >= PlayList_getlength()) return;
	PlayList_setposition(idx);
	StartPlaying();
}

void PlayNext(bool manual)
{
	int n = PlayList_getnext(manual);
	if (n < 0)
	{
		// at the end of the list without repeat: the button does nothing (Main_OnButton5)
		if (!manual) StopPlaying(0);
		return;
	}
	bool was_playing = playing;
	PlayList_setposition(n);
	if (was_playing || !manual) StartPlaying();
	else
	{
		player_update_title();
		notify_state();
	}
}

void PlayPrevious()
{
	int n = PlayList_getprev();
	if (n < 0) return;
	bool was_playing = playing;
	PlayList_setposition(n);
	if (was_playing) StartPlaying();
	else
	{
		player_update_title();
		notify_state();
	}
}

void player_seek_relative(int ms)
{
	if (!in_seekable()) return;
	int t = in_getouttime() + ms;
	int len = in_mod->GetLength();
	if (t < 0) t = 0;
	if (len > 0 && t > len) t = len;
	in_seek(t);
}

void player_on_eof()
{
	if (g_stopaftercur)
	{
		g_stopaftercur = 0;
		StopPlaying(0);
		return;
	}
	if (!config_pladv)
	{
		// manual playlist advance: repeat the song if repeat is on, otherwise stop
		if (config_repeat) StartPlaying();
		else StopPlaying(0);
		return;
	}
	int n = PlayList_getnext(false);
	if (n < 0)
	{
		StopPlaying(0);
		return;
	}
	PlayList_setposition(n);
	StartPlaying();
}

void player_shutdown()
{
	cancel_fade();
	StopPlaying(0);
}

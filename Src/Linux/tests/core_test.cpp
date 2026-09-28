/*
** Winamp for Linux - headless self test of the player core and plug-ins.
**
** Generates a tagged WAV file, then checks title formatting (tagz), playlist
** loading/saving, EQ preset files and a complete playback through
** in_wave -> EQ -> visualizer -> out_disk.
*/
#include "core/common.h"
#include "core/config.h"
#include "core/eq.h"
#include "core/player.h"
#include "core/playlist.h"
#include "core/plugins.h"
#include "core/titleformat.h"
#include "core/vis.h"
#include "sdk/wa_ipc.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond) \
	do { \
		if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
		else printf("ok   %s\n", #cond); \
	} while (0)

static GMainLoop *loop;

/* ---- host window: what the UI normally provides to plug-ins ---- */
static LRESULT host_send(HWND, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
	static std::string fn;
	if (msg == WM_WA_IPC && lParam == IPC_GETLISTPOS) return PlayList_getPosition();
	if (msg == WM_WA_IPC && lParam == IPC_GETPLAYLISTFILE)
	{
		fn = PlayList_getfilename((int)wParam);
		return (LRESULT)fn.c_str();
	}
	return 0;
}

static int host_post(HWND, unsigned int msg, WPARAM, LPARAM)
{
	if (msg == WM_WA_MPEG_EOF)
		g_idle_add([](gpointer) -> gboolean { player_on_eof(); return G_SOURCE_REMOVE; }, nullptr);
	return 1;
}

static winamp_host_window host = {host_post, host_send};

static void put16(std::vector<unsigned char> &v, uint16_t x) { v.push_back(x & 0xff); v.push_back(x >> 8); }
static void put32(std::vector<unsigned char> &v, uint32_t x) { for (int i = 0; i < 4; i++) v.push_back((x >> (8 * i)) & 0xff); }
static void putstr(std::vector<unsigned char> &v, const char *s) { v.insert(v.end(), s, s + strlen(s)); }

// 2 seconds of a 1 kHz tone, 44.1 kHz stereo, with RIFF INFO title/artist
static void write_test_wav(const std::string &path, const char *title, const char *artist)
{
	const int rate = 44100, secs = 2, frames = rate * secs;
	std::vector<unsigned char> data;
	for (int i = 0; i < frames; i++)
	{
		int16_t s = (int16_t)(12000 * sin(2 * M_PI * 1000.0 * i / rate));
		put16(data, (uint16_t)s);
		put16(data, (uint16_t)s);
	}
	auto info_str = [](std::vector<unsigned char> &v, const char *id, const char *s) {
		putstr(v, id);
		uint32_t n = (uint32_t)strlen(s) + 1;
		put32(v, n);
		v.insert(v.end(), s, s + n);
		if (n & 1) v.push_back(0);
	};
	std::vector<unsigned char> info;
	putstr(info, "INFO");
	info_str(info, "INAM", title);
	info_str(info, "IART", artist);

	std::vector<unsigned char> f;
	putstr(f, "RIFF");
	put32(f, (uint32_t)(4 + 8 + 16 + 8 + info.size() + 8 + data.size()));
	putstr(f, "WAVE");
	putstr(f, "fmt ");
	put32(f, 16);
	put16(f, 1);
	put16(f, 2);
	put32(f, rate);
	put32(f, rate * 4);
	put16(f, 4);
	put16(f, 16);
	putstr(f, "LIST");
	put32(f, (uint32_t)info.size());
	f.insert(f.end(), info.begin(), info.end());
	putstr(f, "data");
	put32(f, (uint32_t)data.size());
	f.insert(f.end(), data.begin(), data.end());
	g_file_set_contents(path.c_str(), (const char *)f.data(), (gssize)f.size(), nullptr);
}

static bool wait_for(bool (*cond)(), int timeout_ms)
{
	gint64 end = g_get_monotonic_time() + (gint64)timeout_ms * 1000;
	while (!cond() && g_get_monotonic_time() < end)
		g_main_context_iteration(nullptr, FALSE), g_usleep(2000);
	return cond();
}

static int max_spectrum = 0;

int main()
{
	gchar *tmp = g_dir_make_tmp("winamp-test-XXXXXX", nullptr);
	std::string dir = tmp;
	g_free(tmp);
	g_setenv("WINAMP_CONFIG_DIR", (dir + "/config").c_str(), TRUE);
	g_setenv("WINAMP_DISKWRITER_DIR", (dir + "/out").c_str(), TRUE);
	loop = g_main_loop_new(nullptr, FALSE);

	config_read();
	std::string wav = dir + "/Tone Test.wav";
	write_test_wav(wav, "Sine Wave", "Nullsoft");

	/* plug-ins */
	plugins_load(&host);
	CHECK(in_find(wav) != nullptr);
	CHECK(in_is_supported(dir + "/x.mp3") == (in_find(dir + "/x.mp3") != nullptr));
	out_select("out_disk.so");
	CHECK(out_current() && out_current()->file == "out_disk.so");

	/* metadata + tagz */
	std::string v;
	CHECK(in_get_extended_fileinfo(wav, "title", v) && v == "Sine Wave");
	CHECK(in_get_extended_fileinfo(wav, "length", v) && atoi(v.c_str()) == 2000);
	std::string title;
	int len = 0;
	CHECK(format_title(wav, title, len));
	CHECK(title == "Nullsoft - Sine Wave");
	CHECK(len == 2);
	CHECK(format_title_spec(L"$upper(%title%) [%album%]|$filepart(%filename%)", wav) == "SINE WAVE |Tone Test");

	/* playlist */
	static bool changed = false;
	PlayList_setchangecallback([] { changed = true; });
	PlayList_add(wav);
	PlayList_add(dir + "/missing.mp3");
	CHECK(PlayList_getlength() == 2);
	CHECK(wait_for([] { return changed && PlayList_getsonglength(0) == 2; }, 5000));
	CHECK(PlayList_gettitle(0) == "Nullsoft - Sine Wave");
	CHECK(PlayList_getitem_pl(0) == "1. Nullsoft - Sine Wave");
	CHECK(PlayList_gettitle(1) == "missing");

	std::string m3u = dir + "/list.m3u8", pls = dir + "/list.pls";
	CHECK(PlayList_save(m3u));
	CHECK(PlayList_save(pls));
	PlayList_removemissing();
	CHECK(PlayList_getlength() == 1);
	PlayList_clear();
	CHECK(PlayList_load(pls) == 2);
	CHECK(PlayList_getfilename(0) == wav);
	PlayList_clear();
	CHECK(PlayList_load(m3u) == 2);
	CHECK(PlayList_gettitle(0) == "Nullsoft - Sine Wave");
	PlayList_delete(1);

	/* EQ presets (Windows compatible winamp.q1) */
	eq_create_default_presets();
	auto presets = eq_list_presets(config_eq_path());
	CHECK(presets.size() == 18);
	CHECK(eq_read_preset(config_eq_path(), "Rock"));
	CHECK(eq_tab[0] == 19 && eq_tab[9] == 14);
	eq_tab[3] = 5;
	CHECK(eq_write_preset(config_eq_path(), "Test"));
	eq_tab[3] = 31;
	CHECK(eq_read_preset(config_eq_path(), "Test") && eq_tab[3] == 5);
	eq_delete_preset(config_eq_path(), "Test");
	CHECK(eq_list_presets(config_eq_path()).size() == 18);

	/* playback: in_wave -> EQ -> vis -> out_disk */
	config_use_eq = 1;
	sa_setmode(1);
	StartPlaying();
	CHECK(playing);
	CHECK(in_getlength() == 2);
	guint vis_timer = g_timeout_add(5, [](gpointer) -> gboolean {
		char data[75 * 2 + 8];
		if (char *d = sa_get(in_getouttime(), 1, data))
			for (int i = 0; i < 75; i++) max_spectrum = std::max(max_spectrum, (int)(unsigned char)d[i]);
		return G_SOURCE_CONTINUE;
	}, nullptr);
	CHECK(wait_for([] { return !playing; }, 20000));
	g_source_remove(vis_timer);
	CHECK(max_spectrum > 0);

	gchar *contents = nullptr;
	gsize size = 0;
	CHECK(g_file_get_contents((dir + "/out/Tone Test.wav").c_str(), &contents, &size, nullptr));
	// 2 s of 16 bit stereo, rounded up to whole 576 sample blocks, plus the header
	CHECK(size >= 44 + 44100 * 4 * 2 && size <= 44 + 44100 * 4 * 2 + 576 * 4);
	if (contents && size > 44 + 20000)
	{
		// the EQ ("Test" preset was read back above) must have changed the signal
		int16_t out_sample, in_sample = (int16_t)(12000 * sin(2 * M_PI * 1000.0 * 5000 / 44100));
		memcpy(&out_sample, contents + 44 + 5000 * 4, 2);
		CHECK(out_sample != in_sample);
	}
	g_free(contents);

	PlayList_shutdown();
	plugins_unload();
	printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}

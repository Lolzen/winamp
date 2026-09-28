/*
** Winamp for Linux - module input plug-in (in_mod.so), using libopenmpt
** like the Windows in_mod-openmpt.
*/
#include "../common/in_decoder.h"
#include "../common/tag_cache.h"

#include <libopenmpt/libopenmpt.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

static std::vector<char> read_file(const char *fn)
{
	std::vector<char> data;
	FILE *f = fopen(fn, "rb");
	if (!f) return data;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz > 0 && sz < 256 * 1024 * 1024)
	{
		data.resize((size_t)sz);
		if (fread(data.data(), 1, (size_t)sz, f) != (size_t)sz) data.clear();
	}
	fclose(f);
	return data;
}

static openmpt_module *open_module(const char *fn)
{
	std::vector<char> data = read_file(fn);
	if (data.empty()) return nullptr;
	return openmpt_module_create_from_memory2(data.data(), data.size(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

class ModDecoder : public Decoder
{
public:
	~ModDecoder() override
	{
		if (mod) openmpt_module_destroy(mod);
	}

	bool Open(const char *fn) override
	{
		mod = open_module(fn);
		if (!mod) return false;
		samplerate = 44100;
		channels = 2;
		bps = 16;
		double d = openmpt_module_get_duration_seconds(mod);
		length_ms = d > 0 ? (int)(d * 1000) : -1000;
		bitrate = 0;
		return true;
	}

	int Decode(char *buf, int max_bytes) override
	{
		size_t frames = openmpt_module_read_interleaved_stereo(mod, samplerate, (size_t)max_bytes / 4, (int16_t *)buf);
		return (int)(frames * 4);
	}

	bool Seek(int ms) override
	{
		openmpt_module_set_position_seconds(mod, ms / 1000.0);
		return true;
	}

private:
	openmpt_module *mod = nullptr;
};

static void read_tags(const char *fn, TagMap &t)
{
	openmpt_module *m = open_module(fn);
	if (!m) return;
	double d = openmpt_module_get_duration_seconds(m);
	if (d > 0) t["length"] = std::to_string((long long)(d * 1000));
	const char *keys[][2] = {{"title", "title"}, {"artist", "artist"}, {"message", "comment"}, {"type_long", "genre"}};
	for (auto &k : keys)
	{
		const char *v = openmpt_module_get_metadata(m, k[0]);
		if (v)
		{
			if (*v) t[k[1]] = v;
			openmpt_free_string(v);
		}
	}
	openmpt_module_destroy(m);
}

extern "C" WA_EXPORT int winampGetExtendedFileInfo(const char *fn, const char *data, char *dest, int destlen)
{
	if (!strcasecmp(data, "type")) return wa_plugin::copy_string(dest, destlen, "0");
	if (!strcasecmp(data, "family")) return wa_plugin::copy_string(dest, destlen, "Module");
	static TagCache cache;
	return cache.Get(fn, data, dest, destlen, read_tags);
}

static void GetFileInfo(const char *file, char *title, int *length_in_ms)
{
	if (!file || !*file) return;
	char buf[64];
	if (length_in_ms) *length_in_ms = winampGetExtendedFileInfo(file, "length", buf, sizeof(buf)) ? atoi(buf) : -1000;
	if (title) title[0] = 0;
}

static int Init() { return IN_INIT_SUCCESS; }
static void Quit() {}
static int InfoBox(const char *, HWND) { return INFOBOX_UNCHANGED; }
static int IsOurFile(const char *) { return 0; }

WA_DEFINE_INPUT_PLUGIN(ModDecoder, "Nullsoft Module Decoder (libopenmpt) for Linux",
	"mod;s3m;xm;it;mptm;stm;669;mtm;med;okt;far;ult;umx;amf;dsm;mdl;psm\0Module Files (*.mod;*.s3m;*.xm;*.it;...)\0",
	Init, Quit, GetFileInfo, InfoBox, IsOurFile)

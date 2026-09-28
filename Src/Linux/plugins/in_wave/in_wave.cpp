/*
** Winamp for Linux - uncompressed audio input plug-in (in_wave.so).
** Like the Windows in_wave, decoding is done by libsndfile.
*/
#include "../common/in_decoder.h"
#include "../common/tag_cache.h"

#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>

class WaveDecoder : public Decoder
{
public:
	~WaveDecoder() override
	{
		if (sf) sf_close(sf);
	}

	bool Open(const char *fn) override
	{
		SF_INFO info;
		memset(&info, 0, sizeof(info));
		sf = sf_open(fn, SFM_READ, &info);
		if (!sf) return false;
		samplerate = info.samplerate;
		channels = info.channels;
		bps = 16;
		seekable = info.seekable != 0;
		length_ms = info.frames > 0 && samplerate > 0 ? (int)(info.frames * 1000 / samplerate) : -1000;
		int sub = info.format & SF_FORMAT_SUBMASK;
		int bits = sub == SF_FORMAT_PCM_S8 || sub == SF_FORMAT_PCM_U8 ? 8 : sub == SF_FORMAT_PCM_24 ? 24 : sub == SF_FORMAT_PCM_32 || sub == SF_FORMAT_FLOAT ? 32 : 16;
		bitrate = samplerate * channels * bits / 1000;
		return channels > 0 && channels <= 8 && samplerate > 0;
	}

	int Decode(char *buf, int max_bytes) override
	{
		sf_count_t frames = max_bytes / (2 * channels);
		sf_count_t got = sf_readf_short(sf, (short *)buf, frames);
		return got > 0 ? (int)(got * 2 * channels) : 0;
	}

	bool Seek(int ms) override
	{
		return sf_seek(sf, (sf_count_t)ms * samplerate / 1000, SEEK_SET) >= 0;
	}

private:
	SNDFILE *sf = nullptr;
};

static void read_tags(const char *fn, TagMap &t)
{
	SF_INFO info;
	memset(&info, 0, sizeof(info));
	SNDFILE *sf = sf_open(fn, SFM_READ, &info);
	if (!sf) return;
	if (info.samplerate > 0 && info.frames > 0)
		t["length"] = std::to_string((long long)(info.frames * 1000 / info.samplerate));
	t["samplerate"] = std::to_string(info.samplerate);
	static const struct { int id; const char *name; } fields[] = {
		{SF_STR_TITLE, "title"}, {SF_STR_ARTIST, "artist"}, {SF_STR_ALBUM, "album"}, {SF_STR_DATE, "year"},
		{SF_STR_COMMENT, "comment"}, {SF_STR_GENRE, "genre"}, {SF_STR_TRACKNUMBER, "track"},
	};
	for (auto &f : fields)
		if (const char *v = sf_get_string(sf, f.id)) t[f.name] = v;
	sf_close(sf);
}

extern "C" WA_EXPORT int winampGetExtendedFileInfo(const char *fn, const char *data, char *dest, int destlen)
{
	if (!strcasecmp(data, "type")) return wa_plugin::copy_string(dest, destlen, "0");
	if (!strcasecmp(data, "family")) return wa_plugin::copy_string(dest, destlen, "Waveform Audio");
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

WA_DEFINE_INPUT_PLUGIN(WaveDecoder, "Nullsoft Waveform Decoder v3.3 (libsndfile) for Linux",
	"wav;w64;aif;aiff;aifc;au;snd;voc;caf\0Waveform Audio (*.wav;*.aif;*.au;*.voc;*.caf)\0",
	Init, Quit, GetFileInfo, InfoBox, IsOurFile)

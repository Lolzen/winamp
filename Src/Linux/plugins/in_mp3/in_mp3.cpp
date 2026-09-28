/*
** Winamp for Linux - MPEG audio input plug-in (in_mp3.so).
**
** Like the Windows in_mp3 (which uses mpg123 through Src/mp3-mpg123), decoding
** is done by libmpg123. ID3v1/ID3v2 tags are exposed through
** winampGetExtendedFileInfo() for playlist titles.
*/
#include "../common/in_decoder.h"
#include "../common/tag_cache.h"

#include <mpg123.h>
#include <stdio.h>
#include <stdlib.h>
#include <mutex>

static std::once_flag init_once;
static void mp3_init_lib()
{
	std::call_once(init_once, [] {
#if MPG123_API_VERSION < 46
		mpg123_init();
#endif
	});
}

static mpg123_handle *open_handle(const char *fn)
{
	mp3_init_lib();
	int err = 0;
	mpg123_handle *h = mpg123_new(nullptr, &err);
	if (!h) return nullptr;
	mpg123_param(h, MPG123_ADD_FLAGS, MPG123_QUIET | MPG123_GAPLESS | MPG123_SEEKBUFFER, 0);
	mpg123_param(h, MPG123_REMOVE_FLAGS, MPG123_AUTO_RESAMPLE, 0);
	mpg123_format_none(h);
	const long *rates = nullptr;
	size_t nrates = 0;
	mpg123_rates(&rates, &nrates);
	for (size_t i = 0; i < nrates; i++)
		mpg123_format(h, rates[i], MPG123_MONO | MPG123_STEREO, MPG123_ENC_SIGNED_16);
	if (mpg123_open(h, fn) != MPG123_OK)
	{
		mpg123_delete(h);
		return nullptr;
	}
	return h;
}

class Mp3Decoder : public Decoder
{
public:
	~Mp3Decoder() override
	{
		if (h)
		{
			mpg123_close(h);
			mpg123_delete(h);
		}
	}

	bool Open(const char *fn) override
	{
		h = open_handle(fn);
		if (!h) return false;
		long rate = 0;
		int ch = 0, enc = 0;
		if (mpg123_getformat(h, &rate, &ch, &enc) != MPG123_OK || rate <= 0 || ch <= 0)
			return false;
		// lock the format so a (rare) mid-stream change can't surprise the output
		mpg123_format_none(h);
		mpg123_format(h, rate, ch, enc);
		samplerate = (int)rate;
		channels = ch;
		bps = 16;
		off_t len = mpg123_length(h);
		length_ms = len > 0 ? (int)((double)len * 1000.0 / rate) : -1000;
		struct mpg123_frameinfo fi;
		if (mpg123_info(h, &fi) == MPG123_OK) bitrate = fi.bitrate;
		return true;
	}

	int Decode(char *buf, int max_bytes) override
	{
		size_t done = 0;
		int r = mpg123_read(h, (unsigned char *)buf, (size_t)max_bytes, &done);
		if (r == MPG123_DONE && !done) return 0;
		if (r == MPG123_NEW_FORMAT) return (int)done;
		if (r != MPG123_OK && r != MPG123_DONE && !done) return r == MPG123_ERR ? -1 : 0;
		return (int)done;
	}

	bool Seek(int ms) override
	{
		off_t target = (off_t)((double)ms * samplerate / 1000.0);
		return mpg123_seek(h, target, SEEK_SET) >= 0;
	}

	int CurrentBitrate() override
	{
		struct mpg123_frameinfo fi;
		if (mpg123_info(h, &fi) == MPG123_OK && fi.bitrate > 0) return fi.bitrate;
		return bitrate;
	}

private:
	mpg123_handle *h = nullptr;
};

/* ---------------- tags ---------------- */

static std::string id3_str(mpg123_string *s)
{
	if (!s || !s->p || !s->fill) return "";
	std::string r(s->p, strnlen(s->p, s->fill));
	return r;
}

static std::string trimmed(const char *p, size_t n)
{
	std::string s(p, strnlen(p, n));
	while (!s.empty() && (s.back() == ' ' || s.back() == 0)) s.pop_back();
	// ID3v1 is Latin-1; turn it into UTF-8
	std::string out;
	for (unsigned char c : s)
	{
		if (c < 0x80) out.push_back((char)c);
		else
		{
			out.push_back((char)(0xC0 | (c >> 6)));
			out.push_back((char)(0x80 | (c & 0x3F)));
		}
	}
	return out;
}

static const char *const id3v1_genres[] = {
	"Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal",
	"New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial",
	"Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk",
	"Fusion", "Trance", "Classical", "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
	"Alt. Rock", "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic",
	"Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta Rap",
	"Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave", "Psychedelic", "Rave", "Showtunes",
	"Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock",
};

static std::string genre_name(const std::string &g)
{
	// "(17)" or "17" -> "Rock"
	const char *p = g.c_str();
	if (*p == '(') p++;
	char *end = nullptr;
	long n = strtol(p, &end, 10);
	if (end != p && (*end == 0 || *end == ')') && n >= 0 && n < (long)(sizeof(id3v1_genres) / sizeof(*id3v1_genres)))
		return id3v1_genres[n];
	return g;
}

static void read_tags(const char *fn, TagMap &t)
{
	mpg123_handle *h = open_handle(fn);
	if (!h) return;
	long rate = 0;
	int ch = 0, enc = 0;
	mpg123_getformat(h, &rate, &ch, &enc);
	off_t len = mpg123_length(h);
	if (len > 0 && rate > 0) t["length"] = std::to_string((long long)((double)len * 1000.0 / rate));
	struct mpg123_frameinfo fi;
	if (mpg123_info(h, &fi) == MPG123_OK) t["bitrate"] = std::to_string(fi.bitrate);
	t["samplerate"] = std::to_string(rate);

	mpg123_id3v1 *v1 = nullptr;
	mpg123_id3v2 *v2 = nullptr;
	if (mpg123_meta_check(h) & MPG123_ID3) mpg123_id3(h, &v1, &v2);
	auto set = [&](const char *k, const std::string &v) {
		if (!v.empty() && t.find(k) == t.end()) t[k] = v;
	};
	if (v2)
	{
		set("title", id3_str(v2->title));
		set("artist", id3_str(v2->artist));
		set("album", id3_str(v2->album));
		set("year", id3_str(v2->year));
		set("comment", id3_str(v2->comment));
		set("genre", genre_name(id3_str(v2->genre)));
		static const char *const frames[][2] = {
			{"TRCK", "track"}, {"TPE2", "albumartist"}, {"TCOM", "composer"}, {"TPOS", "disc"}, {"TPUB", "publisher"},
		};
		for (size_t i = 0; i < v2->texts; i++)
			for (auto &f : frames)
				if (!memcmp(v2->text[i].id, f[0], 4)) set(f[1], id3_str(&v2->text[i].text));
	}
	if (v1)
	{
		set("title", trimmed(v1->title, 30));
		set("artist", trimmed(v1->artist, 30));
		set("album", trimmed(v1->album, 30));
		set("year", trimmed(v1->year, 4));
		set("comment", trimmed(v1->comment, 28));
		if (v1->comment[28] == 0 && v1->comment[29]) set("track", std::to_string((unsigned char)v1->comment[29]));
		if (v1->genre < sizeof(id3v1_genres) / sizeof(*id3v1_genres)) set("genre", id3v1_genres[v1->genre]);
	}
	mpg123_close(h);
	mpg123_delete(h);
}

extern "C" WA_EXPORT int winampGetExtendedFileInfo(const char *fn, const char *data, char *dest, int destlen)
{
	using wa_plugin::copy_string;
	if (!strcasecmp(data, "type")) return copy_string(dest, destlen, "0");
	if (!strcasecmp(data, "family")) return copy_string(dest, destlen, "MPEG Audio");

	static TagCache cache;
	return cache.Get(fn, data, dest, destlen, read_tags);
}

static void GetFileInfo(const char *file, char *title, int *length_in_ms)
{
	if (!file || !*file) return;
	if (title)
	{
		const char *p = strrchr(file, '/');
		p = p ? p + 1 : file;
		snprintf(title, GETFILEINFO_TITLE_LENGTH, "%s", p);
		char *dot = strrchr(title, '.');
		if (dot) *dot = 0;
	}
	if (length_in_ms)
	{
		char buf[32];
		*length_in_ms = winampGetExtendedFileInfo(file, "length", buf, sizeof(buf)) ? atoi(buf) : -1000;
	}
}

static int Init()
{
	mp3_init_lib();
	return IN_INIT_SUCCESS;
}
static void Quit() {}
static int InfoBox(const char *, HWND) { return INFOBOX_UNCHANGED; }
static int IsOurFile(const char *) { return 0; }

WA_DEFINE_INPUT_PLUGIN(Mp3Decoder, "Nullsoft MPEG Audio Decoder (libmpg123) v4.8 for Linux",
	"mp3;mp2;mp1\0MPEG Audio Files (*.mp3;*.mp2;*.mp1)\0",
	Init, Quit, GetFileInfo, InfoBox, IsOurFile)

/*
** Winamp for Linux - Ogg Vorbis input plug-in (in_vorbis.so), using
** libvorbisfile like the Windows in_vorbis.
*/
#include "../common/in_decoder.h"
#include "../common/tag_cache.h"

#include <vorbis/vorbisfile.h>
#include <stdio.h>
#include <stdlib.h>

class VorbisDecoder : public Decoder
{
public:
	~VorbisDecoder() override
	{
		if (opened) ov_clear(&vf);
	}

	bool Open(const char *fn) override
	{
		if (ov_fopen(fn, &vf) != 0) return false;
		opened = true;
		vorbis_info *vi = ov_info(&vf, -1);
		if (!vi) return false;
		samplerate = (int)vi->rate;
		channels = vi->channels;
		bps = 16;
		double t = ov_time_total(&vf, -1);
		length_ms = t > 0 ? (int)(t * 1000.0) : -1000;
		seekable = ov_seekable(&vf) != 0;
		long br = ov_bitrate(&vf, -1);
		bitrate = br > 0 ? (int)(br / 1000) : 0;
		return channels > 0 && samplerate > 0;
	}

	int Decode(char *buf, int max_bytes) override
	{
		int section = 0;
		for (;;)
		{
			long r = ov_read(&vf, buf, max_bytes, 0, 2, 1, &section);
			if (r == OV_HOLE) continue;
			if (r < 0) return -1;
			if (section != last_section)
			{
				// chained streams may change format; we only handle the common case of same format
				last_section = section;
			}
			return (int)r;
		}
	}

	bool Seek(int ms) override
	{
		return ov_time_seek(&vf, ms / 1000.0) == 0;
	}

	int CurrentBitrate() override
	{
		long br = ov_bitrate_instant(&vf);
		if (br > 0) cur_bitrate = (int)(br / 1000);
		return cur_bitrate ? cur_bitrate : bitrate;
	}

private:
	OggVorbis_File vf;
	bool opened = false;
	int last_section = 0;
	int cur_bitrate = 0;
};

static void read_tags(const char *fn, TagMap &t)
{
	OggVorbis_File vf;
	if (ov_fopen(fn, &vf) != 0) return;
	double len = ov_time_total(&vf, -1);
	if (len > 0) t["length"] = std::to_string((long long)(len * 1000.0));
	long br = ov_bitrate(&vf, -1);
	if (br > 0) t["bitrate"] = std::to_string(br / 1000);
	if (vorbis_info *vi = ov_info(&vf, -1)) t["samplerate"] = std::to_string(vi->rate);
	if (vorbis_comment *vc = ov_comment(&vf, -1))
	{
		for (int i = 0; i < vc->comments; i++)
		{
			std::string c(vc->user_comments[i], vc->comment_lengths[i]);
			size_t eq = c.find('=');
			if (eq == std::string::npos) continue;
			std::string k = c.substr(0, eq), v = c.substr(eq + 1);
			for (auto &ch : k) ch = (char)tolower((unsigned char)ch);
			if (k == "date") k = "year";
			else if (k == "tracknumber") k = "track";
			else if (k == "description") k = "comment";
			else if (k == "discnumber") k = "disc";
			if (!t.count(k)) t[k] = v;
		}
	}
	ov_clear(&vf);
}

extern "C" WA_EXPORT int winampGetExtendedFileInfo(const char *fn, const char *data, char *dest, int destlen)
{
	if (!strcasecmp(data, "type")) return wa_plugin::copy_string(dest, destlen, "0");
	if (!strcasecmp(data, "family")) return wa_plugin::copy_string(dest, destlen, "Ogg Vorbis");
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

WA_DEFINE_INPUT_PLUGIN(VorbisDecoder, "Nullsoft Vorbis Decoder v1.79 for Linux",
	"ogg;oga\0Ogg Vorbis Files (*.ogg;*.oga)\0",
	Init, Quit, GetFileInfo, InfoBox, IsOurFile)

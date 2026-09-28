/*
** Winamp for Linux - FLAC input plug-in (in_flac.so), using libFLAC like
** the Windows in_flac.
*/
#include "../common/in_decoder.h"
#include "../common/tag_cache.h"

#include <FLAC/stream_decoder.h>
#include <FLAC/metadata.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

class FlacDecoder : public Decoder
{
public:
	~FlacDecoder() override
	{
		if (dec)
		{
			FLAC__stream_decoder_finish(dec);
			FLAC__stream_decoder_delete(dec);
		}
	}

	bool Open(const char *fn) override
	{
		dec = FLAC__stream_decoder_new();
		if (!dec) return false;
		if (FLAC__stream_decoder_init_file(dec, fn, write_cb, metadata_cb, error_cb, this) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
			return false;
		if (!FLAC__stream_decoder_process_until_end_of_metadata(dec) || !samplerate)
			return false;
		// output 16 bit for <= 16 bit sources, 24 bit otherwise (packed, like Winamp)
		out_bps = src_bps <= 16 ? 16 : 24;
		bps = out_bps;
		if (total_samples > 0)
		{
			length_ms = (int)(total_samples * 1000 / samplerate);
			FILE *f = fopen(fn, "rb");
			if (f)
			{
				fseek(f, 0, SEEK_END);
				long sz = ftell(f);
				fclose(f);
				if (length_ms > 0) bitrate = (int)((double)sz * 8.0 / length_ms);
			}
		}
		return channels > 0;
	}

	int Decode(char *buf, int max_bytes) override
	{
		int written = 0;
		while (written < max_bytes)
		{
			if (pending_pos < pending.size())
			{
				size_t n = std::min(pending.size() - pending_pos, (size_t)(max_bytes - written));
				memcpy(buf + written, pending.data() + pending_pos, n);
				pending_pos += n;
				written += (int)n;
				continue;
			}
			pending.clear();
			pending_pos = 0;
			if (FLAC__stream_decoder_get_state(dec) == FLAC__STREAM_DECODER_END_OF_STREAM) break;
			if (!FLAC__stream_decoder_process_single(dec)) return written ? written : -1;
			if (pending.empty() && FLAC__stream_decoder_get_state(dec) == FLAC__STREAM_DECODER_END_OF_STREAM) break;
		}
		return written;
	}

	bool Seek(int ms) override
	{
		pending.clear();
		pending_pos = 0;
		FLAC__uint64 target = (FLAC__uint64)ms * samplerate / 1000;
		if (total_samples && target >= total_samples) target = total_samples - 1;
		if (!FLAC__stream_decoder_seek_absolute(dec, target))
		{
			if (FLAC__stream_decoder_get_state(dec) == FLAC__STREAM_DECODER_SEEK_ERROR)
				FLAC__stream_decoder_flush(dec);
			return false;
		}
		return true; // the write callback already received the samples from the target on
	}

private:
	static FLAC__StreamDecoderWriteStatus write_cb(const FLAC__StreamDecoder *, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client)
	{
		FlacDecoder *self = (FlacDecoder *)client;
		unsigned n = frame->header.blocksize, ch = frame->header.channels;
		if ((int)ch != self->channels) return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
		int shift_from = frame->header.bits_per_sample;
		size_t base = self->pending.size();
		size_t bytes = self->out_bps / 8;
		self->pending.resize(base + (size_t)n * ch * bytes);
		char *out = self->pending.data() + base;
		for (unsigned i = 0; i < n; i++)
			for (unsigned c = 0; c < ch; c++)
			{
				int32_t s = buffer[c][i];
				if (self->out_bps == 16)
				{
					if (shift_from < 16) s <<= (16 - shift_from);
					int16_t v = (int16_t)s;
					memcpy(out, &v, 2);
					out += 2;
				}
				else
				{
					if (shift_from < 24) s <<= (24 - shift_from);
					else if (shift_from > 24) s >>= (shift_from - 24);
					out[0] = (char)(s & 0xff);
					out[1] = (char)((s >> 8) & 0xff);
					out[2] = (char)((s >> 16) & 0xff);
					out += 3;
				}
			}
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}

	static void metadata_cb(const FLAC__StreamDecoder *, const FLAC__StreamMetadata *m, void *client)
	{
		FlacDecoder *self = (FlacDecoder *)client;
		if (m->type == FLAC__METADATA_TYPE_STREAMINFO)
		{
			self->samplerate = (int)m->data.stream_info.sample_rate;
			self->channels = (int)m->data.stream_info.channels;
			self->src_bps = (int)m->data.stream_info.bits_per_sample;
			self->total_samples = m->data.stream_info.total_samples;
		}
	}

	static void error_cb(const FLAC__StreamDecoder *, FLAC__StreamDecoderErrorStatus, void *) {}

	FLAC__StreamDecoder *dec = nullptr;
	std::vector<char> pending;
	size_t pending_pos = 0;
	int src_bps = 16, out_bps = 16;
	FLAC__uint64 total_samples = 0;
};

static void read_tags(const char *fn, TagMap &t)
{
	FLAC__StreamMetadata si;
	if (FLAC__metadata_get_streaminfo(fn, &si))
	{
		if (si.data.stream_info.sample_rate && si.data.stream_info.total_samples)
			t["length"] = std::to_string((long long)(si.data.stream_info.total_samples * 1000 / si.data.stream_info.sample_rate));
		t["samplerate"] = std::to_string(si.data.stream_info.sample_rate);
	}
	FLAC__StreamMetadata *tags = nullptr;
	if (FLAC__metadata_get_tags(fn, &tags) && tags)
	{
		const FLAC__StreamMetadata_VorbisComment &vc = tags->data.vorbis_comment;
		for (FLAC__uint32 i = 0; i < vc.num_comments; i++)
		{
			std::string c((const char *)vc.comments[i].entry, vc.comments[i].length);
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
		FLAC__metadata_object_delete(tags);
	}
}

extern "C" WA_EXPORT int winampGetExtendedFileInfo(const char *fn, const char *data, char *dest, int destlen)
{
	if (!strcasecmp(data, "type")) return wa_plugin::copy_string(dest, destlen, "0");
	if (!strcasecmp(data, "family")) return wa_plugin::copy_string(dest, destlen, "FLAC");
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

WA_DEFINE_INPUT_PLUGIN(FlacDecoder, "Nullsoft FLAC Decoder v3.2 for Linux",
	"flac\0FLAC Files (*.flac)\0",
	Init, Quit, GetFileInfo, InfoBox, IsOurFile)

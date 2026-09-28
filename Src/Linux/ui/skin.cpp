#include "skin.h"
#include "../core/config.h"
#include "../core/common.h"

#include <gio/gio.h>
#include <zip.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <algorithm>
#include <map>
#include <fstream>
#include <sstream>

Skin g_skin;

namespace
{
	// the files of a skin, keyed by lower case base name
	class SkinFiles
	{
	public:
		bool OpenDir(const std::string &dir)
		{
			DIR *d = opendir(dir.c_str());
			if (!d) return false;
			while (struct dirent *e = readdir(d))
			{
				std::string n = e->d_name;
				if (n == "." || n == "..") continue;
				std::string full = wa::path_join(dir, n);
				if (wa::file_exists(full)) paths[wa::lower(n)] = full;
			}
			closedir(d);
			return true;
		}

		bool OpenZip(const std::string &file)
		{
			int err = 0;
			zip_t *z = zip_open(file.c_str(), ZIP_RDONLY, &err);
			if (!z) return false;
			zip_int64_t n = zip_get_num_entries(z, 0);
			for (zip_int64_t i = 0; i < n; i++)
			{
				const char *name = zip_get_name(z, (zip_uint64_t)i, 0);
				if (!name) continue;
				std::string fn = name;
				std::replace(fn.begin(), fn.end(), '\\', '/');
				std::string base = wa::lower(wa::path_filename(fn));
				if (base.empty()) continue;
				if (base == "skin.xml") modern = true;
				std::string ext = wa::path_extension(base);
				if (ext != "bmp" && ext != "txt" && ext != "cur" && ext != "ini" && ext != "png") continue;
				zip_stat_t st;
				if (zip_stat_index(z, (zip_uint64_t)i, 0, &st) != 0 || st.size > 8 * 1024 * 1024) continue;
				zip_file_t *f = zip_fopen_index(z, (zip_uint64_t)i, 0);
				if (!f) continue;
				std::vector<unsigned char> buf((size_t)st.size);
				zip_int64_t r = zip_fread(f, buf.data(), st.size);
				zip_fclose(f);
				if (r == (zip_int64_t)st.size) mem[base] = std::move(buf); // later files win, like extraction
			}
			zip_close(z);
			return true;
		}

		bool Read(const char *name, std::vector<unsigned char> &out) const
		{
			std::string key = wa::lower(name);
			auto m = mem.find(key);
			if (m != mem.end())
			{
				out = m->second;
				return true;
			}
			auto p = paths.find(key);
			if (p == paths.end()) return false;
			std::ifstream f(p->second, std::ios::binary);
			if (!f) return false;
			out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
			return true;
		}

		bool ReadText(const char *name, std::string &out) const
		{
			std::vector<unsigned char> d;
			if (!Read(name, d)) return false;
			out.assign(d.begin(), d.end());
			return true;
		}

		bool modern = false;

	private:
		std::map<std::string, std::string> paths;
		std::map<std::string, std::vector<unsigned char>> mem;
	};
}

static bool load_builtin(const char *name, Bitmap &out)
{
	std::string path = std::string("/winamp/skin/") + name;
	GBytes *b = g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);
	if (!b) return false;
	gsize size = 0;
	const unsigned char *p = (const unsigned char *)g_bytes_get_data(b, &size);
	std::vector<unsigned char> data(p, p + size);
	g_bytes_unref(b);
	return LoadImageData(data, out);
}

static bool load_skin_bitmap(const SkinFiles *files, const char *filename, Bitmap &out)
{
	std::vector<unsigned char> data;
	return files && files->Read(filename, data) && LoadImageData(data, out);
}

// draw_LBitmap(): the skin's file, else the built-in bitmap
static void load_bitmap(const SkinFiles *files, const char *filename, Bitmap &out)
{
	if (load_skin_bitmap(files, filename, out)) return;
	if (!load_builtin(filename, out)) out.Create(1, 1);
}

// "#00FF00" style colours from pledit.txt (hexGetPrivateProfileInt)
static bool parse_hex_color(const std::string &v, COLORREF &out)
{
	const char *p = v.c_str();
	while (*p == ' ' || *p == '\t') p++;
	if (*p == '#') p++;
	char *end = nullptr;
	unsigned long x = strtoul(p, &end, 16);
	if (end == p) return false;
	out = RGB((x >> 16) & 0xff, (x >> 8) & 0xff, x & 0xff);
	return true;
}

// minimal INI reader for pledit.txt / region.txt (tolerant of junk, like GetPrivateProfileString)
static std::map<std::string, std::map<std::string, std::string>> parse_ini(const std::string &text)
{
	std::map<std::string, std::map<std::string, std::string>> r;
	std::istringstream in(text);
	std::string line, section;
	while (std::getline(in, line))
	{
		while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
		size_t a = line.find_first_not_of(" \t");
		if (a == std::string::npos) continue;
		line = line.substr(a);
		if (line[0] == ';') continue;
		if (line[0] == '[')
		{
			size_t e = line.find(']');
			section = wa::lower(line.substr(1, e == std::string::npos ? std::string::npos : e - 1));
			continue;
		}
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string k = line.substr(0, eq);
		while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
		std::string v = line.substr(eq + 1);
		size_t vs = v.find_first_not_of(" \t");
		v = vs == std::string::npos ? "" : v.substr(vs);
		k = wa::lower(k);
		if (!r[section].count(k)) r[section][k] = v;
	}
	return r;
}

static std::vector<int> parse_numbers(const std::string &s)
{
	std::vector<int> r;
	const char *p = s.c_str();
	while (*p)
	{
		while (*p && !(*p >= '0' && *p <= '9') && *p != '-') p++;
		if (!*p) break;
		char *end = nullptr;
		long v = strtol(p, &end, 10);
		if (end == p) { p++; continue; }
		r.push_back((int)v);
		p = end;
	}
	return r;
}

static void load_region(const std::map<std::string, std::map<std::string, std::string>> &ini, const char *section, SkinRegion &out)
{
	out = SkinRegion();
	auto s = ini.find(wa::lower(section));
	if (s == ini.end()) return;
	auto np = s->second.find("numpoints"), pl = s->second.find("pointlist");
	if (np == s->second.end() || pl == s->second.end()) return;
	std::vector<int> counts = parse_numbers(np->second);
	std::vector<int> pts = parse_numbers(pl->second);
	size_t total = 0;
	for (int c : counts)
	{
		if (c <= 0) return;
		total += (size_t)c;
	}
	if (total * 2 != pts.size() || counts.empty()) return; // "points in PointList don't match NumPoints"
	out.counts = counts;
	out.points = pts;
}

static void load_viscolors(const SkinFiles *files, uint32_t *colors)
{
	static const unsigned char ppal2[] = {
		0, 0, 0,        // color 0 = black
		24, 24, 41,     // color 1 = grey for dots
		239, 49, 16,    // color 2 = top of spec
		206, 41, 16,    // 3
		214, 90, 0,     // 4
		214, 102, 0,    // 5
		214, 115, 0,    // 6
		198, 123, 8,    // 7
		222, 165, 24,   // 8
		214, 181, 33,   // 9
		189, 222, 41,   // 10
		148, 222, 33,   // 11
		41, 206, 16,    // 12
		50, 190, 16,    // 13
		57, 181, 16,    // 14
		49, 156, 8,     // 15
		41, 148, 0,     // 16
		24, 132, 8,     // 17
		255, 255, 255,  // 18 = osc 1
		214, 214, 222,  // 19 = osc 2 (slightly dimmer)
		181, 189, 189,  // 20 = osc 3
		160, 170, 175,  // 21 = osc 4
		148, 156, 165,  // 22 = osc 4
		150, 150, 150,  // 23 = analyzer peak
	};
	unsigned char ppal[sizeof(ppal2)];
	memcpy(ppal, ppal2, sizeof(ppal));
	std::string text;
	if (files && files->ReadText("viscolor.txt", text))
	{
		std::istringstream in(text);
		std::string line;
		for (int x = 0; x < 24 && std::getline(in, line); x++)
		{
			const char *p = line.c_str();
			bool bad = false;
			for (int t = 0; t < 3; t++)
			{
				int b = 0, s = 0;
				while (*p == ' ' || *p == ',' || *p == '\t') p++;
				while (*p >= '0' && *p <= '9') { s = 1; b = b * 10 + *p++ - '0'; }
				if (!s) { bad = true; break; }
				ppal[x * 3 + t] = (unsigned char)b;
			}
			if (bad) break;
		}
	}
	for (int c = 0; c < 24; c++)
		colors[c] = ((uint32_t)ppal[c * 3] << 16) | ((uint32_t)ppal[c * 3 + 1] << 8) | ppal[c * 3 + 2];
}

std::vector<std::string> skin_dirs()
{
	std::vector<std::string> dirs;
	dirs.push_back(config_skin_dirs_user());
	dirs.push_back(wa::path_join(wa::exe_dir(), "Skins"));
#ifdef WINAMP_DATADIR
	dirs.push_back(wa::path_join(WINAMP_DATADIR, "Skins"));
#endif
	return dirs;
}

std::string skin_resolve(const std::string &name)
{
	if (name.empty()) return "";
	if (name[0] == '/') return name;
	for (auto &d : skin_dirs())
	{
		std::string p = wa::path_join(d, name);
		if (wa::file_exists(p) || wa::dir_exists(p)) return p;
	}
	return "";
}

std::vector<std::string> skin_list()
{
	std::vector<std::string> r;
	for (auto &d : skin_dirs())
	{
		DIR *dir = opendir(d.c_str());
		if (!dir) continue;
		while (struct dirent *e = readdir(dir))
		{
			std::string n = e->d_name;
			if (n[0] == '.') continue;
			std::string full = wa::path_join(d, n);
			std::string ext = wa::path_extension(n);
			bool ok = wa::dir_exists(full) || ext == "wsz" || ext == "zip";
			if (ok && std::find(r.begin(), r.end(), n) == r.end()) r.push_back(n);
		}
		closedir(dir);
	}
	std::sort(r.begin(), r.end(), [](const std::string &a, const std::string &b) { return strcasecmp(a.c_str(), b.c_str()) < 0; });
	return r;
}

bool skin_install(const std::string &path)
{
	std::string dir = config_skin_dirs_user();
	wa::make_dirs(dir);
	GFile *src = g_file_new_for_path(path.c_str());
	GFile *dst = g_file_new_for_path(wa::path_join(dir, wa::path_filename(path)).c_str());
	bool ok = g_file_copy(src, dst, G_FILE_COPY_OVERWRITE, nullptr, nullptr, nullptr, nullptr);
	g_object_unref(src);
	g_object_unref(dst);
	return ok;
}

void skin_load()
{
	Skin s;
	SkinFiles files;
	bool have_files = false;
	if (!config_skin.empty())
	{
		std::string path = skin_resolve(config_skin);
		if (!path.empty())
			have_files = wa::dir_exists(path) ? files.OpenDir(path) : files.OpenZip(path);
		if (!have_files)
		{
			fprintf(stderr, "winamp: can't load skin '%s', using the classic skin\n", config_skin.c_str());
			config_skin.clear();
		}
	}
	const SkinFiles *f = have_files ? &files : nullptr;
	s.name = config_skin;
	s.is_default = !have_files;
	s.modern = have_files && files.modern;

	load_bitmap(f, "main.bmp", s.main);
	load_bitmap(f, "cbuttons.bmp", s.cbuttons);
	load_bitmap(f, "monoster.bmp", s.monoster);
	load_bitmap(f, "playpaus.bmp", s.playpaus);
	load_bitmap(f, "shufrep.bmp", s.shufrep);
	s.nums_ex = load_skin_bitmap(f, "nums_ex.bmp", s.numbers);
	if (!s.nums_ex) load_bitmap(f, "numbers.bmp", s.numbers);
	load_bitmap(f, "volume.bmp", s.volume);
	// a skin without balance.bmp uses volume.bmp for the balance slider
	if (f)
	{
		if (!load_skin_bitmap(f, "balance.bmp", s.balance)) s.balance = s.volume;
	}
	else
		load_builtin("balance.bmp", s.balance);
	load_bitmap(f, "text.bmp", s.text);
	load_bitmap(f, "posbar.bmp", s.posbar);
	load_bitmap(f, "titlebar.bmp", s.titlebar);
	load_bitmap(f, "pledit.bmp", s.pledit);
	load_bitmap(f, "gen.bmp", s.gen);

	// draw_eq_init()
	s.enable_eq_windowshade_button = 2;
	bool got_eq = false;
	if (config_eq_frequencies != EQ_FREQUENCIES_WINAMP)
		got_eq = load_skin_bitmap(f, "eqmain_iso.bmp", s.eqmain);
	if (!got_eq) got_eq = load_skin_bitmap(f, "eqmain.bmp", s.eqmain);
	if (got_eq)
		s.enable_eq_windowshade_button = 0;
	else
		load_builtin(config_eq_frequencies == EQ_FREQUENCIES_WINAMP ? "eqmain.bmp" : "eqmain_iso.bmp", s.eqmain);
	if (!load_skin_bitmap(f, "eq_ex.bmp", s.eq_ex))
	{
		if (!f) s.enable_eq_windowshade_button = 1;
		load_builtin("eq_ex.bmp", s.eq_ex);
	}
	else
		s.enable_eq_windowshade_button = 1;

	load_viscolors(f, s.viscolors);

	const COLORREF defcolors[6] = {RGB(0, 255, 0), RGB(255, 255, 255), RGB(0, 0, 0), RGB(0, 0, 198), RGB(0, 255, 0), RGB(0, 0, 0)};
	memcpy(s.pl_colors, defcolors, sizeof(defcolors));
	std::string text;
	if (f && files.ReadText("pledit.txt", text))
	{
		auto ini = parse_ini(text);
		auto &t = ini["text"];
		const char *keys[6] = {"normal", "current", "normalbg", "selectedbg", "mbfg", "mbbg"};
		for (int i = 0; i < 6; i++)
			if (t.count(keys[i])) parse_hex_color(t[keys[i]], s.pl_colors[i]);
		if (t.count("font")) s.pl_font = t["font"];
	}
	if (f && files.ReadText("region.txt", text))
	{
		auto ini = parse_ini(text);
		load_region(ini, "Normal", s.rgn_main);
		load_region(ini, "WindowShade", s.rgn_main_ws);
		load_region(ini, "Equalizer", s.rgn_eq);
		load_region(ini, "EqualizerWS", s.rgn_eq_ws);
	}
	g_skin = s;
}

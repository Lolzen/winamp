#include "plugins.h"
#include "player.h"
#include "vis.h"
#include "eq.h"
#include "config.h"
#include "common.h"

#include <dlfcn.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <mutex>

HWND hMainWindow = nullptr;
std::vector<InputPlugin> g_inputs;
std::vector<OutputPlugin> g_outputs;
In_Module *in_mod = nullptr;
Out_Module *out_mod = nullptr;
static int out_index = -1;

std::vector<std::string> plugins_search_dirs()
{
	std::vector<std::string> dirs;
	const char *env = getenv("WINAMP_PLUGIN_DIR");
	if (env && *env) dirs.push_back(env);
	dirs.push_back(wa::path_join(wa::data_dir(), "Plugins"));
	dirs.push_back(wa::path_join(wa::exe_dir(), "Plugins"));
#ifdef WINAMP_LIBDIR
	dirs.push_back(wa::path_join(WINAMP_LIBDIR, "Plugins"));
#endif
	return dirs;
}

static std::vector<std::string> list_dir(const std::string &dir, const char *prefix)
{
	std::vector<std::string> r;
	DIR *d = opendir(dir.c_str());
	if (!d) return r;
	while (struct dirent *e = readdir(d))
	{
		const char *n = e->d_name;
		size_t l = strlen(n);
		if (strncmp(n, prefix, strlen(prefix)) == 0 && l > 3 && !strcmp(n + l - 3, ".so"))
			r.push_back(n);
	}
	closedir(d);
	std::sort(r.begin(), r.end());
	return r;
}

// "mp3;mp2\0desc\0ogg\0desc\0\0" -> extension list + filters
static void parse_extensions(InputPlugin &p)
{
	const char *s = p.mod->FileExtensions;
	while (s && *s)
	{
		std::string exts = s;
		s += exts.size() + 1;
		std::string desc = *s ? s : exts;
		if (*s) s += strlen(s) + 1;
		std::string pattern;
		size_t start = 0;
		while (start <= exts.size())
		{
			size_t end = exts.find(';', start);
			std::string e = wa::lower(exts.substr(start, end == std::string::npos ? std::string::npos : end - start));
			if (!e.empty())
			{
				p.extensions.push_back(e);
				if (!pattern.empty()) pattern += ";";
				pattern += "*." + e;
			}
			if (end == std::string::npos) break;
			start = end + 1;
		}
		if (!pattern.empty()) p.filters.emplace_back(pattern, desc);
	}
}

static void load_input(const std::string &path)
{
	for (auto &p : g_inputs)
		if (wa::path_filename(p.path) == wa::path_filename(path)) return; // first one found wins

	void *h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (!h)
	{
		fprintf(stderr, "winamp: can't load %s: %s\n", path.c_str(), dlerror());
		return;
	}
	auto get = (winampGetInModule2Func)dlsym(h, "winampGetInModule2");
	In_Module *mod = get ? get() : nullptr;
	if (!mod || (mod->version & 0xFFF) != IN_VER)
	{
		dlclose(h);
		return;
	}
	mod->hMainWindow = hMainWindow;
	mod->hDllInstance = h;
	mod->outMod = nullptr;
	player_setup_input_module(mod);
	if (mod->Init && mod->Init() == IN_INIT_FAILURE)
	{
		dlclose(h);
		return;
	}
	InputPlugin p;
	p.path = path;
	p.handle = h;
	p.mod = mod;
	p.getExtendedFileInfo = (winampGetExtendedFileInfoFunc)dlsym(h, "winampGetExtendedFileInfo");
	parse_extensions(p);
	g_inputs.push_back(p);
}

static void load_output(const std::string &path)
{
	for (auto &p : g_outputs)
		if (p.file == wa::path_filename(path)) return;
	void *h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (!h)
	{
		fprintf(stderr, "winamp: can't load %s: %s\n", path.c_str(), dlerror());
		return;
	}
	auto get = (winampGetOutModuleFunc)dlsym(h, "winampGetOutModule");
	Out_Module *mod = get ? get() : nullptr;
	if (!mod || mod->version != OUT_VER)
	{
		dlclose(h);
		return;
	}
	mod->hMainWindow = hMainWindow;
	mod->hDllInstance = h;
	if (mod->Init) mod->Init();
	g_outputs.push_back({path, wa::path_filename(path), h, mod});
}

void plugins_load(HWND host)
{
	hMainWindow = host;
	for (const auto &dir : plugins_search_dirs())
	{
		for (const auto &f : list_dir(dir, "in_"))
			load_input(wa::path_join(dir, f));
		for (const auto &f : list_dir(dir, "out_"))
			load_output(wa::path_join(dir, f));
	}
	out_select(config_outname);
}

void plugins_unload()
{
	for (auto &p : g_inputs)
	{
		if (p.mod->Quit) p.mod->Quit();
		dlclose(p.handle);
	}
	g_inputs.clear();
	for (auto &p : g_outputs)
	{
		if (p.mod->Quit) p.mod->Quit();
		dlclose(p.handle);
	}
	g_outputs.clear();
	in_mod = nullptr;
	out_mod = nullptr;
}

InputPlugin *in_find(const std::string &filename)
{
	// IsOurFile() first (lets plug-ins claim URLs etc.), then the extension
	for (auto &p : g_inputs)
		if (p.mod->IsOurFile && p.mod->IsOurFile(filename.c_str())) return &p;
	std::string ext = wa::path_extension(filename);
	if (!ext.empty())
		for (auto &p : g_inputs)
			for (auto &e : p.extensions)
				if (e == ext) return &p;
	return nullptr;
}

bool in_is_supported(const std::string &filename)
{
	return in_find(filename) != nullptr;
}

std::vector<std::string> in_extensions()
{
	std::vector<std::string> r;
	for (auto &p : g_inputs)
		for (auto &e : p.extensions)
			if (std::find(r.begin(), r.end(), e) == r.end()) r.push_back(e);
	return r;
}

bool in_get_extended_fileinfo(const std::string &filename, const char *metadata, std::string &out)
{
	InputPlugin *p = in_find(filename);
	if (!p || !p->getExtendedFileInfo) return false;
	char buf[4096] = {0};
	if (!p->getExtendedFileInfo(filename.c_str(), metadata, buf, sizeof(buf))) return false;
	out = buf;
	return true;
}

void in_get_fileinfo(const std::string &filename, std::string &title, int &length_sec)
{
	title.clear();
	length_sec = -1;
	InputPlugin *p = in_find(filename);
	if (!p || !p->mod->GetFileInfo) return;
	char buf[GETFILEINFO_TITLE_LENGTH] = {0};
	int len = -1;
	p->mod->GetFileInfo(filename.c_str(), buf, &len);
	title = buf;
	if (len > 0) length_sec = len / 1000;
}

bool in_infobox(const std::string &filename)
{
	InputPlugin *p = in_find(filename);
	if (!p || !p->mod->InfoBox) return false;
	p->mod->InfoBox(filename.c_str(), hMainWindow);
	return true;
}

void out_select(const std::string &file)
{
	out_index = -1;
	for (size_t i = 0; i < g_outputs.size(); i++)
		if (g_outputs[i].file == file) out_index = (int)i;
	if (out_index < 0 && !g_outputs.empty())
	{
		// prefer PulseAudio (works with PipeWire too), then ALSA
		const char *pref[] = {"out_pulse.so", "out_alsa.so"};
		for (const char *p : pref)
			for (size_t i = 0; out_index < 0 && i < g_outputs.size(); i++)
				if (g_outputs[i].file == p) out_index = (int)i;
		if (out_index < 0) out_index = 0;
	}
	out_mod = out_index >= 0 ? g_outputs[out_index].mod : nullptr;
	if (out_index >= 0) config_outname = g_outputs[out_index].file;
}

OutputPlugin *out_current()
{
	return out_index >= 0 ? &g_outputs[out_index] : nullptr;
}

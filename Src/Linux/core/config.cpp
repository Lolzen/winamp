#include "config.h"
#include "common.h"
#include "ini.h"

#include <stdio.h>
#include <stdlib.h>

#define WA_CONFIG_DEFINE(type, name, def) type config_##name = (def);
WA_CONFIG_INTS(WA_CONFIG_DEFINE)
#undef WA_CONFIG_DEFINE

unsigned char eq_tab[10] = {31, 31, 31, 31, 31, 31, 31, 31, 31, 31};
std::string config_titlefmt = "[%artist% - ]$if2(%title%,$filepart(%filename%))";
std::string config_skin;
std::string config_outname = "out_pulse.so";
std::string config_cwd;
std::string config_plfont;

static const char *kSection = "Winamp";

std::string config_ini_path()
{
	return wa::path_join(wa::config_dir(), "winamp.ini");
}

std::string config_m3u_path()
{
	return wa::path_join(wa::config_dir(), "winamp.m3u8");
}

std::string config_eq_path()
{
	return wa::path_join(wa::config_dir(), "winamp.q1");
}

std::string config_eq_auto_path()
{
	return wa::path_join(wa::config_dir(), "winamp.q2");
}

std::string config_skin_dirs_user()
{
	return wa::path_join(wa::data_dir(), "Skins");
}

void config_read()
{
	wa::make_dirs(wa::config_dir());
	IniFile ini;
	ini.Load(config_ini_path());

#define WA_CONFIG_READ(type, name, def) config_##name = (type)ini.GetInt(kSection, #name, config_##name);
	WA_CONFIG_INTS(WA_CONFIG_READ)
#undef WA_CONFIG_READ

	config_titlefmt = ini.GetString(kSection, "titlefmt", config_titlefmt);
	config_skin = ini.GetString(kSection, "skin", config_skin);
	config_outname = ini.GetString(kSection, "outname", config_outname);
	config_cwd = ini.GetString(kSection, "cwd", config_cwd);
	config_plfont = ini.GetString(kSection, "plfont", config_plfont);

	std::string eq = ini.GetString(kSection, "eq_bands", "");
	if (eq.size() == 20)
	{
		for (int x = 0; x < 10; x++)
		{
			int v = (int)strtol(eq.substr(x * 2, 2).c_str(), nullptr, 16);
			eq_tab[x] = (unsigned char)(v < 0 ? 0 : v > 63 ? 63 : v);
		}
	}

	// sanity
	if (config_volume < 0) config_volume = 0;
	if (config_volume > 255) config_volume = 255;
	if (config_pan < -127) config_pan = -127;
	if (config_pan > 127) config_pan = 127;
	if (config_preamp < 0 || config_preamp > 63) config_preamp = 31;
	if (config_pe_width < 275) config_pe_width = 275;
	if (config_pe_height != 14 && config_pe_height < 116) config_pe_height = 116;
	if (config_pe_fontsize < 5 || config_pe_fontsize > 40) config_pe_fontsize = 12;
	if (config_sa < 0 || config_sa > 2) config_sa = 1;
}

void config_write()
{
	wa::make_dirs(wa::config_dir());
	IniFile ini;
	ini.Load(config_ini_path()); // keep keys we don't know about

#define WA_CONFIG_WRITE(type, name, def) ini.SetInt(kSection, #name, (int)config_##name);
	WA_CONFIG_INTS(WA_CONFIG_WRITE)
#undef WA_CONFIG_WRITE

	ini.SetString(kSection, "titlefmt", config_titlefmt);
	ini.SetString(kSection, "skin", config_skin);
	ini.SetString(kSection, "outname", config_outname);
	ini.SetString(kSection, "cwd", config_cwd);
	ini.SetString(kSection, "plfont", config_plfont);

	char eq[21] = {0};
	for (int x = 0; x < 10; x++)
		snprintf(eq + x * 2, 3, "%02x", eq_tab[x]);
	ini.SetString(kSection, "eq_bands", eq);

	ini.Save(config_ini_path());
}

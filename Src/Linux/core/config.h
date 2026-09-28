/*
** Winamp for Linux - player configuration.
**
** Variable names and defaults follow Src/Winamp/config.h and are stored in
** ~/.config/winamp/winamp.ini under [Winamp] with the same key names the
** Windows player uses (config_volume -> "volume", ...).
*/
#pragma once

#include <string>

// X(type, name, default)
#define WA_CONFIG_INTS(X) \
	X(int, wx, 26) X(int, wy, 29) \
	X(int, eq_wx, 26) X(int, eq_wy, 145) X(int, eq_open, 1) X(int, mw_open, 1) \
	X(int, pe_wx, 26) X(int, pe_wy, 261) X(int, pe_open, 1) X(int, pe_width, 275) X(int, pe_height, 116) X(int, pe_height_ws, 0) \
	X(int, pe_fontsize, 12) \
	X(int, volume, 200) X(int, pan, 0) \
	X(int, shuffle, 0) X(int, repeat, 0) X(int, pladv, 1) \
	X(int, windowshade, 0) X(int, eq_ws, 0) \
	X(int, dsize, 0) X(int, eqdsize, 1) \
	X(int, aot, 0) X(int, easymove, 1) X(int, snap, 1) X(int, snaplen, 10) X(int, keeponscreen, 1) \
	X(int, hilite, 1) X(int, timeleftmode, 0) \
	X(int, autoscrollname, 1) X(int, dotitlenum, 1) X(int, shownumsinpl, 1) \
	X(int, sa, 1) X(int, safire, 4) X(int, saref, 2) X(int, safalloff, 2) X(int, sa_peaks, 1) X(int, sa_peak_falloff, 1) \
	X(int, use_eq, 0) X(int, autoload_eq, 0) X(int, preamp, 31) X(int, eq_limiter, 1) X(int, eq_frequencies, 0) \
	X(int, ascb_new, 1) X(int, bifont, 0) X(int, ospb, 0) X(int, pilp, 0) \
	X(int, embedwnd_freesize, 0) X(int, useexttitles, 1) X(int, rofiob, 0) \
	X(int, minimized, 0)

#define WA_CONFIG_DECLARE(type, name, def) extern type config_##name;
WA_CONFIG_INTS(WA_CONFIG_DECLARE)
#undef WA_CONFIG_DECLARE

extern unsigned char eq_tab[10];            // 0..63, 31 is 0 dB
extern std::string config_titlefmt;         // tagz title format
extern std::string config_skin;             // skin file/folder name, empty = built-in classic skin
extern std::string config_outname;          // output plug-in file name
extern std::string config_cwd;              // last directory used in file dialogs
extern std::string config_plfont;           // custom playlist font ("" = skin/default)

#define EQ_FREQUENCIES_WINAMP 0
#define EQ_FREQUENCIES_ISO 1

void config_read();
void config_write();

// paths (created on demand)
std::string config_ini_path();     // winamp.ini
std::string config_m3u_path();     // winamp.m3u8 (the current playlist)
std::string config_eq_path();      // winamp.q1  (EQ presets, Windows compatible binary format)
std::string config_eq_auto_path(); // winamp.q2  (auto-load presets)
std::string config_skin_dirs_user(); // ~/.local/share/winamp/Skins

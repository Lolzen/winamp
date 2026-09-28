/*
** Winamp for Linux - classic skin loading (port of the classic parts of
** Src/Winamp/Skins.cpp and draw.cpp's bitmap loading).
**
** Skins can be the built-in "Winamp Classic" skin (the original bitmaps from
** Src/Winamp/resource, embedded in the binary), a folder, or a .wsz/.zip file.
** Files are matched case-insensitively and zip folders are flattened, like
** the Windows player does when it extracts a skin.
*/
#pragma once

#include "gfx.h"
#include <string>
#include <vector>

struct SkinRegion
{
	std::vector<int> points; // x,y pairs
	std::vector<int> counts; // points per polygon
	bool Empty() const { return counts.empty(); }
};

struct Skin
{
	std::string name;   // "" for the built-in skin
	bool is_default = true;
	bool modern = false; // contains a skin.xml (Modern skin, only the classic parts are used)

	Bitmap main, cbuttons, monoster, playpaus, shufrep, numbers, volume, balance,
	       text, posbar, titlebar, eqmain, eq_ex, pledit, gen;
	bool nums_ex = false;          // numbers come from nums_ex.bmp (has a minus sign)
	int enable_eq_windowshade_button = 1;

	uint32_t viscolors[24];        // 0x00RRGGBB
	COLORREF pl_colors[6];         // Normal, Current, NormalBG, SelectedBG, mbFG, mbBG
	std::string pl_font;           // from pledit.txt, "" = default

	SkinRegion rgn_main, rgn_main_ws, rgn_eq, rgn_eq_ws;
};

extern Skin g_skin;

// loads config_skin (falls back to the built-in skin if it can't be read)
void skin_load();
std::vector<std::string> skin_list();      // skins found in the skin folders
std::string skin_resolve(const std::string &name); // full path of a skin name
std::vector<std::string> skin_dirs();
bool skin_install(const std::string &path); // copy a .wsz into the user's skin folder

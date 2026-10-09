/*
** Winamp for Linux - command ids that only exist in the Linux build
** (the classic visualizer options menu and the skin list). They live above
** the range used by Src/Winamp/resource.h.
*/
#pragma once

#include <string>

enum
{
	WAL_FIRST = 46000,
	WAL_VIS_ANALYZER = WAL_FIRST,
	WAL_VIS_SCOPE,
	WAL_VIS_OFF,
	WAL_VIS_PROJECTM,
	WAL_VIS_PROJECTM_NEXT,
	WAL_VIS_NORMAL,
	WAL_VIS_FIRE,
	WAL_VIS_LINE,
	WAL_VIS_PEAKS,
	WAL_VIS_THICK,
	WAL_VIS_THIN,
	WAL_VIS_DOTS,
	WAL_VIS_LINES,
	WAL_VIS_SOLID,
	WAL_VIS_REFRESH1,
	WAL_VIS_REFRESH2,
	WAL_VIS_REFRESH4,
	WAL_VIS_REFRESH8,
	WAL_VIS_FALLOFF0,       // .. +4
	WAL_VIS_PFALLOFF0 = WAL_VIS_FALLOFF0 + 5, // .. +4
	WAL_SKIN_CLASSIC = WAL_VIS_PFALLOFF0 + 5,
	WAL_SKIN_FIRST = 46100, // one id per skin in the skin menus
	WAL_SKIN_LAST = 46600,
};

std::string skin_menu_name(int id); // skin file name for a WAL_SKIN_FIRST+n id

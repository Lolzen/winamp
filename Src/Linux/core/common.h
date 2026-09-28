/*
** Winamp for Linux - shared helpers for the player core and UI.
*/
#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#define APP_NAME "Winamp"
#define APP_VERSION "5.9.2"
#define APP_VERSION_STRING "5.9.2 (Linux)"

// mouse/keyboard state flags passed to the ported *_handlemouseevent functions
// (same meaning as the Windows MK_* flags)
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT 0x0004
#define MK_CONTROL 0x0008
#define MK_MBUTTON 0x0010

struct RECT
{
	int left, top, right, bottom;
};

typedef uint32_t COLORREF; // 0x00BBGGRR, like Windows
#define RGB(r, g, b) ((COLORREF)(((uint8_t)(r)) | ((uint16_t)((uint8_t)(g)) << 8) | (((uint32_t)(uint8_t)(b)) << 16)))
#define GetRValue(c) ((uint8_t)(c))
#define GetGValue(c) ((uint8_t)((c) >> 8))
#define GetBValue(c) ((uint8_t)((c) >> 16))

namespace wa
{
	std::wstring widen(const std::string &utf8);
	std::string narrow(const std::wstring &wide);
	std::string lower(const std::string &s);
	bool iequals(const std::string &a, const std::string &b);

	std::string path_join(const std::string &a, const std::string &b);
	std::string path_filename(const std::string &p);   // "foo.mp3"
	std::string path_dirname(const std::string &p);    // "/music"
	std::string path_extension(const std::string &p);  // "mp3" (lower case, no dot)
	bool path_is_url(const std::string &p);
	bool file_exists(const std::string &p);
	bool dir_exists(const std::string &p);
	bool make_dirs(const std::string &p);
	std::string file_uri_to_path(const std::string &uri);

	// directories
	std::string config_dir();   // ~/.config/winamp
	std::string data_dir();     // ~/.local/share/winamp
	std::string exe_dir();      // directory of the running binary

	unsigned int tick_count();  // ms, monotonic
	int rand_int(int max);      // [0, max)
}

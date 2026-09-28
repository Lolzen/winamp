/*
** Winamp for Linux - main window drawing.
**
** A line by line port of Src/Winamp/draw_main.cpp and the relevant parts of
** Src/Winamp/draw.cpp. All coordinates and bitmap offsets are the original
** ones; BitBlt()/mainDC work on Bitmaps instead of GDI device contexts.
*/
#include "ui.h"
#include "../core/player.h"
#include "../core/playlist.h"

#include <wchar.h>
#include <stdio.h>

int eggstat = 0;
TextFont mfont;
uint32_t mfont_fgcolor = 0x00FF00, mfont_bgcolor = 0;

static Bitmap mainDC;              // the 275x116 window image (mainBM)
static int updateen = 1;
static bool draw_initted = false;

void update_area(int x1, int y1, int w, int h)
{
	if (!updateen || !g_main_wnd || !draw_initted) return;
	BitBlt(g_main_wnd->Buffer(), x1, y1, w, h, mainDC, x1, y1);
	g_main_wnd->InvalidateRect(x1, y1, w, h);
}

static void update_rect(const RECT &r)
{
	update_area(r.left, r.top, r.right - r.left, r.bottom - r.top);
}

void draw_setnoupdate(int v)
{
	updateen = !v;
	if (!v) update_area(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
}

void getXYfromChar(wchar_t ic, int *x, int *y)
{
	int c, c2 = 0;
	switch (ic)
	{
	case L'°': ic = L'0'; break;
	case L'Ç': ic = L'C'; break;
	case L'ü': ic = L'u'; break;
	case L'è': case L'ë': case L'ê': case L'é': ic = L'e'; break;
	case L'á': case L'à': case L'â': ic = L'a'; break;
	case L'ç': ic = L'c'; break;
	case L'í': case L'ì': case L'î': case L'ï': ic = L'i'; break;
	case L'É': ic = L'E'; break;
	case L'æ': ic = L'a'; break;
	case L'Æ': ic = L'A'; break;
	case L'ó': case L'ò': case L'ô': ic = L'o'; break;
	case L'ú': case L'ù': case L'û': ic = L'u'; break;
	case L'ÿ': ic = L'y'; break;
	case L'Ü': ic = L'U'; break;
	case L'ƒ': ic = L'f'; break;
	case L'Ñ': case L'ñ': ic = L'n'; break;
	default: break;
	} // quick relocations
	if (ic <= L'Z' && ic >= L'A') c = (ic - 'A');
	else if (ic <= L'z' && ic >= L'a') c = (ic - 'a');
	else
	{
		c2 = 6;
		if (ic == L'\1') c = 10;
		else if (ic == L'.') c = 11;
		else if (ic <= L'9' && ic >= L'0') c = ic - '0';
		else if (ic == L':') c = 12;
		else if (ic == L'(') c = 13;
		else if (ic == L')') c = 14;
		else if (ic == L'-') c = 15;
		else if (ic == L'\'' || ic == '`') c = 16;
		else if (ic == L'!') c = 17;
		else if (ic == L'_') c = 18;
		else if (ic == L'+') c = 19;
		else if (ic == L'\\') c = 20;
		else if (ic == L'/') c = 21;
		else if (ic == L'[' || ic == L'{' || ic == L'<') c = 22;
		else if (ic == L']' || ic == L'}' || ic == L'>') c = 23;
		else if (ic == L'~' || ic == L'^') c = 24;
		else if (ic == L'&') c = 25;
		else if (ic == L'%') c = 26;
		else if (ic == L',') c = 27;
		else if (ic == L'=') c = 28;
		else if (ic == L'$') c = 29;
		else if (ic == L'#') c = 30;
		else
		{
			c2 = 12;
			if (ic == L'Å' || ic == L'å') c = 0;
			else if (ic == L'Ö' || ic == L'ö') c = 1;
			else if (ic == L'Ä' || ic == L'ä') c = 2;
			else if (ic == L'?') c = 3;
			else if (ic == L'*') c = 4;
			else
			{
				c2 = 0;
				if (ic == L'"') c = 26;
				else if (ic == L'@') c = 27;
				else c = 30;
			}
		}
	}
	c *= 5;
	*x = c;
	*y = c2;
}

/* ---- font colours for the non-bitmap title font (draw_reinit_plfont) ---- */
static void init_mfont()
{
	const Bitmap &fontBM = g_skin.text;
	mfont.family = g_skin.pl_font.empty() ? "Arial" : g_skin.pl_font;
	if (!config_plfont.empty()) mfont.family = config_plfont;
	mfont.pixel_size = 10;
	mfont.antialias = false;
	uint32_t bg = fontBM.GetPixel(150, 4);
	if (bg == CLR_INVALID) bg = 0;
	mfont_fgcolor = mfont_bgcolor = bg;
	int ld = 0;
	for (int y = 0; y < 6; y++)
		for (int x = 0; x < 20; x++)
		{
			uint32_t r = fontBM.GetPixel(x, y);
			if (r == CLR_INVALID) continue;
			int a = (int)(r & 0xff) - (int)(bg & 0xff);
			int b = (int)((r >> 8) & 0xff) - (int)((bg >> 8) & 0xff);
			int c = (int)((r >> 16) & 0xff) - (int)((bg >> 16) & 0xff);
			int d = a * a + b * b + c * c;
			if (d > ld)
			{
				ld = d;
				mfont_fgcolor = r;
			}
		}
}

void draw_init()
{
	mainDC.Create(WINDOW_WIDTH, WINDOW_HEIGHT);
	draw_initted = true;
	init_mfont();
}

void draw_tbuttons(int b1, int b2, int b3, int b4)
{
	const Bitmap &tbBM = g_skin.titlebar;
	if (b1 != -1) BitBlt(mainDC, 6, 3, 9, 9, tbBM, 0, b1 * 9);
	if (b2 != -1) BitBlt(mainDC, 244, 3, 9, 9, tbBM, 9, b2 * 9);
	if (b3 != -1) BitBlt(mainDC, 264, 3, 9, 9, tbBM, 18, b3 * 9);
	if (b4 != -1) BitBlt(mainDC, 254, 3, 9, 9, tbBM, b4 * 9, config_windowshade ? 27 : 18);
	update_area(6, 3, 9, 9);
	update_area(243, 3, 274 - 243, 9);
}

void draw_eject(int pressed)
{
	RECT r;
	if (!draw_initted) return;
	r.left = 132 + 7 - 4 + 1;
	r.top = 60 + 14 + 15;
	r.right = r.left + 22;
	r.bottom = r.top + 16;
	BitBlt(mainDC, r.left, r.top, r.right - r.left, r.bottom - r.top, g_skin.cbuttons, 114, (pressed ? 16 : 0));
	update_rect(r);
}

void draw_tbar(int active, int windowshade, int egg)
{
	int t[4] = {0, 15, 29, 42};
	int l = t[((active ? 0 : 1) + (windowshade ? 2 : 0))];

	if (egg && !windowshade)
		l = active ? 57 : 72;

	if (!draw_initted)
		return;

	BitBlt(mainDC, 0, 0, WINDOW_WIDTH, 14, g_skin.titlebar, 27, l);

	if (windowshade && config_windowshade)
	{
		int pos = 0;
		draw_songname(L"", &pos, 0);
	}

	update_area(0, 0, WINDOW_WIDTH, 14);
}

static int g_need_erase = 0;

static int _draw_songname(const wchar_t *str, int startpos, int offs_in_first)
{
	const Bitmap &fontBM = g_skin.text;
	int xp;
	int o = offs_in_first;
	str += startpos;
	xp = 111;
	if (g_need_erase)
	{
		FillRect(mainDC, 111, 12 + 13, 264 - 111, 12 + 15 + 8 - (12 + 13), mfont_bgcolor);
		g_need_erase = 0;
	}

	while (xp < 265 && *str)
	{
		int c2 = 0, c = 0;
		getXYfromChar(*str++, &c, &c2);
		if (xp >= 265 - 5)
		{
			BitBlt(mainDC, xp, 12 + 15, 265 - xp, 6, fontBM, c, c2);
			xp = 265;
		}
		else
		{
			BitBlt(mainDC, xp, 12 + 15, 5 - o, 6, fontBM, c + o, c2);
			xp += 5 - o;
			o = 0;
		}
	}
	while (xp < 265) BitBlt(mainDC, xp++, 12 + 15, 1, 6, fontBM, 4, 0);
	update_area(111, 10 + 15, 265 - 111, 10);
	return 0;
}

static int _draw_songname_winfont(const std::wstring &str, int start_pix)
{
	g_need_erase = 1;
	RECT clip = {111, 12 + 12, 265, 12 + 16 + 8};
	FillRect(mainDC, 111, 12 + 12, 265 - 111, 12 + 16 + 8 - (12 + 12), mfont_bgcolor);
	DrawTextAt(mainDC, clip, 111 - start_pix, 12 + 11, wa::narrow(str), mfont, mfont_fgcolor);
	update_area(111, 12 + 11, 265 - 111, 4 + 9);
	return 0;
}

void draw_songname(const std::wstring &name, int *out_position, int songlen) // position is the number of chars over it is
{
	int position = *out_position;
	std::wstring buf;
	int len_of_str, len_of_spacer;
	int mode = !config_bifont;

	if (!draw_initted)
		return;

	static int hold;
	if (!name.empty())
	{
		wchar_t tmp[2048];
		if (songlen >= 0)
		{
			if (hold > 0)
			{
				hold--;
				return;
			}
			if (config_dotitlenum)
				swprintf(tmp, 2048, L"%d. %ls (%d:%02d)", PlayList_getPosition() + 1, name.c_str(), songlen / 60, songlen % 60);
			else
				swprintf(tmp, 2048, L"%ls (%d:%02d)", name.c_str(), songlen / 60, songlen % 60);
			buf = tmp;
		}
		else if (songlen == -3) // FileTitle without a known length ("the big hack" in the original)
		{
			if (hold > 0)
			{
				hold--;
				return;
			}
			swprintf(tmp, 2048, L"%d. %ls", PlayList_getPosition() + 1, name.c_str());
			buf = config_dotitlenum ? std::wstring(tmp) : name;
		}
		else
		{
			hold = (config_autoscrollname & 1 ? ((songlen == -2) ? 5 : 1) : 0);
			buf = name;
		}
	}
	else
		buf = wa::widen(std::string(APP_NAME) + " " + APP_VERSION_STRING);

	if (!mode)
	{
		len_of_str = (int)buf.size() * 5;
		len_of_spacer = 7 * 5;
	}
	else
	{
		len_of_str = mfont.TextWidth(wa::narrow(buf));
		len_of_spacer = mfont.TextWidth("  ***  ");
	}

	const int textAreaWidth = 258 - 112;
	std::wstring draw_buf = buf;
	if (len_of_str > textAreaWidth)
	{
		draw_buf = buf + L"  ***  " + buf;
		while (position < 0) position += len_of_str + len_of_spacer;
		if (position >= len_of_str + len_of_spacer) position -= len_of_str + len_of_spacer;
	}
	else
		position = 0;

	if (!mode)
	{
		int start = position / 5;
		if (start > (int)draw_buf.size()) start = (int)draw_buf.size();
		_draw_songname(draw_buf.c_str(), start, position % 5);
	}
	else
		_draw_songname_winfont(draw_buf, position);
	*out_position = position;
}

void draw_songname_title()
{
	int len = playing ? in_getlength() : PlayList_getcurrentlength();
	std::wstring t = wa::widen(FileTitle);
	if (t.empty() && PlayList_getlength()) t = wa::widen(PlayList_gettitle(PlayList_getPosition()));
	draw_songname(t, &ui_songposition, t.empty() ? 0 : (len >= 0 ? len : -3));
}

void draw_time(int minutes, int seconds, int clear)
{
	int ex = 0;
	int tlm;
	if (!draw_initted) return;

	tlm = config_timeleftmode && playing && in_seekable();
	if (tlm)
	{
		int s;
		int tmp = in_getlength();
		s = minutes * 60 + seconds;
		s = tmp - s;
		if (tmp > 0)
		{
			minutes = s / 60;
			seconds = -(s % 60);
		}
		else
			minutes = seconds = tlm = 0;
	}
	const Bitmap *bm;
	if (config_windowshade)
		bm = &g_skin.text;
	else
	{
		bm = &g_skin.numbers;
		ex = g_skin.nums_ex;
	}
	const Bitmap &bmDC = *bm;
	if (clear)
	{
		if (config_windowshade)
		{
			BitBlt(mainDC, 126, 4, 3, 6, bmDC, 142, 0);
			BitBlt(mainDC, 130, 4, 3, 6, bmDC, 142, 0);
			BitBlt(mainDC, 130 + 4, 4, 5, 6, bmDC, 142, 0);
			BitBlt(mainDC, 130 + 4 + 5, 4, 5, 6, bmDC, 142, 0);
			BitBlt(mainDC, 130 + 4 + 5 + 4 + 4, 4, 5, 6, bmDC, 142, 0);
			BitBlt(mainDC, 130 + 4 + 5 + 4 + 5 + 4, 4, 5, 6, bmDC, 142, 0);
		}
		else
		{
			BitBlt(mainDC, 23 + 7 + 6, 11 + 15, 9, 13, bmDC, 90, 0);
			if (ex)
				BitBlt(mainDC, 36 + 4 - 2, 11 + 15, 9, 13, bmDC, 90, 0);
			else
				BitBlt(mainDC, 36 + 4, 32, 5, 1, bmDC, 9, 6);
			BitBlt(mainDC, 35 + 7 + 6, 11 + 15, 9, 13, bmDC, 90, 0);
			BitBlt(mainDC, 47 + 7 + 6, 11 + 15, 9, 13, bmDC, 90, 0);
			BitBlt(mainDC, 65 + 7 + 6, 11 + 15, 9, 13, bmDC, 90, 0);
			BitBlt(mainDC, 77 + 7 + 6, 11 + 15, 9, 13, bmDC, 90, 0);
		}
	}
	else
	{
		if (config_windowshade)
		{
			if (tlm)
			{
				if (minutes < 0) minutes = -minutes;
				if (seconds < 0) seconds = -seconds;
				if (minutes / 100)
					BitBlt(mainDC, 130 - 4, 4, 3, 6, bmDC, 75, 6);
				else
					BitBlt(mainDC, 130, 4, 3, 6, bmDC, 75, 6);
			}
			else
			{
				if (!(minutes / 100))
					BitBlt(mainDC, 130, 4, 3, 6, bmDC, 142, 0);
				BitBlt(mainDC, 130 - 4, 4, 3, 6, bmDC, 142, 0);
			}
			if (minutes / 100)
				BitBlt(mainDC, 130 - 1, 4, 5, 6, bmDC, 5 * ((minutes / 100) % 10), 6);
			BitBlt(mainDC, 130 + 4, 4, 5, 6, bmDC, 5 * ((minutes / 10) % 10), 6);
			BitBlt(mainDC, 130 + 4 + 5, 4, 5, 6, bmDC, 5 * ((minutes) % 10), 6);
			BitBlt(mainDC, 130 + 4 + 5 + 4 + 4, 4, 5, 6, bmDC, 5 * ((seconds / 10) % 10), 6);
			BitBlt(mainDC, 130 + 4 + 5 + 4 + 5 + 4, 4, 5, 6, bmDC, 5 * ((seconds) % 10), 6);
		}
		else
		{
			BitBlt(mainDC, 23 + 7 + 6, 11 + 15, 9, 13, bmDC, 90, 0);
			if (tlm)
			{
				int t = 36 + 4 - 2;
				if (minutes < 0) minutes = -minutes;
				if (seconds < 0) seconds = -seconds;
				if (minutes / 100)
					BitBlt(mainDC, 23 + 7 + 6, 11 + 15, 9, 13, bmDC, ((minutes / 100) % 10) * 9, 0);
				if (ex)
					BitBlt(mainDC, t - 2, 11 + 15, 9, 13, bmDC, 99, 0);
				else
					BitBlt(mainDC, t, 32, 5, 1, bmDC, 20, 6);
			}
			else
			{
				if (minutes / 100)
					BitBlt(mainDC, 23 + 7 + 6, 11 + 15, 9, 13, bmDC, ((minutes / 100) % 10) * 9, 0);
			}

			BitBlt(mainDC, 35 + 7 + 6, 11 + 15, 9, 13, bmDC, ((minutes / 10) % 10) * 9, 0);
			BitBlt(mainDC, 47 + 7 + 6, 11 + 15, 9, 13, bmDC, ((minutes) % 10) * 9, 0);
			BitBlt(mainDC, 65 + 7 + 6, 11 + 15, 9, 13, bmDC, ((seconds / 10) % 10) * 9, 0);
			BitBlt(mainDC, 77 + 7 + 6, 11 + 15, 9, 13, bmDC, ((seconds) % 10) * 9, 0);
		}
	}

	if (config_pe_open && config_pe_height != 14)
		draw_pe_timedisp(minutes, seconds, tlm, clear);
	if (config_windowshade) update_area(125, 4, 32, 6);
	else update_area(36, 11 + 15, 96 - 36 + 4, 13);
}

void draw_playicon(int whichicon) // 0 = none, 1 = play, 2 = stop, 4 = pause, 8 = lost sync play
{
	int offset;
	RECT r;
	if (!draw_initted) return;
	r.left = 19 + 7;
	r.top = 13 + 15;
	r.right = r.left + 9;
	r.bottom = r.top + 9;
	switch (whichicon)
	{
	case 0: offset = 27; break;
	case 1: offset = 0; break;
	case 2: offset = 18; break;
	case 4: offset = 9; break;
	case 8: offset = 0; break;
	default: return;
	}
	const Bitmap &bmDC = g_skin.playpaus;
	BitBlt(mainDC, r.left, r.top, r.right - r.left, r.bottom - r.top, bmDC, offset, 0);
	r.left -= 2;
	r.right = r.left + 3;
	if (whichicon == 1 || whichicon == 8)
	{
		offset = (whichicon == 1 ? 36 : 39);
		BitBlt(mainDC, r.left, r.top, r.right - r.left, r.bottom - r.top, bmDC, offset, 0);
	}
	else
	{
		r.right = r.left + 2;
		offset = 27;
		BitBlt(mainDC, r.left, r.top, r.right - r.left, r.bottom - r.top, bmDC, offset, 0);
	}
	r.right = r.left + 11;
	update_rect(r);
}

void draw_buttonbar(int buttonpressed) // starts at 0 with leftmost, -1 = no button
{
	if (!draw_initted) return;
	const Bitmap &bmDC = g_skin.cbuttons;
	if (buttonpressed == -1)
	{
		BitBlt(mainDC, 8 + 8, 58 + 14 + 15 + 1, 114, 18, bmDC, 0, 0);
	}
	else
	{
		int d1[5] = {0, 23, 46, 69, 92};   // width of first section
		int d2[5] = {23, 46, 69, 92, 114}; // start of next button
		if (buttonpressed)
			BitBlt(mainDC, 8 + 8, 58 + 14 + 15 + 1, d1[buttonpressed], 18, bmDC, 0, 0);
		BitBlt(mainDC, 8 + 8 + d1[buttonpressed], 58 + 14 + 15 + 1, d2[buttonpressed] - d1[buttonpressed], 18, bmDC, d1[buttonpressed], 18);
		if (buttonpressed != 4)
			BitBlt(mainDC, 8 + 8 + d2[buttonpressed], 58 + 14 + 15 + 1, 114 - d2[buttonpressed], 18, bmDC, d2[buttonpressed], 0);
	}
	update_area(8 + 7, 58 + 14 + 15, 116, 20);
}

void draw_bitmixrate(int bitrate, int mixrate)
{
	static int l1 = -1, l2 = -1;
	if (bitrate < 0) bitrate = l1;
	if (mixrate < 0) mixrate = l2;
	if (bitrate < 0) return;
	if (!draw_initted) return;
	const Bitmap &bmDC = g_skin.text;
	if (bitrate / 10000)
	{
		if (bitrate / 100000) BitBlt(mainDC, 111, 28 + 15, 5, 6, bmDC, (((bitrate / 100000) % 10)) * 5, 6);
		else BitBlt(mainDC, 111, 28 + 15, 5, 6, bmDC, 100, 12); // blank
		BitBlt(mainDC, 111 + 5, 28 + 15, 5, 6, bmDC, (((bitrate / 10000) % 10)) * 5, 6);
		int x, y;
		getXYfromChar(L'C', &x, &y);
		BitBlt(mainDC, 111 + 5 * 2, 28 + 15, 5, 6, bmDC, x, y);
	}
	else if (bitrate / 1000)
	{
		BitBlt(mainDC, 111, 28 + 15, 5, 6, bmDC, (((bitrate / 1000) % 10)) * 5, 6);
		BitBlt(mainDC, 111 + 5, 28 + 15, 5, 6, bmDC, (((bitrate / 100) % 10)) * 5, 6);
		int x, y;
		getXYfromChar(L'H', &x, &y);
		BitBlt(mainDC, 111 + 5 * 2, 28 + 15, 5, 6, bmDC, x, y);
	}
	else
	{
		if (bitrate / 100)
			BitBlt(mainDC, 111, 28 + 15, 5, 6, bmDC, (((bitrate / 100) % 10)) * 5, 6);
		else
			BitBlt(mainDC, 111, 28 + 15, 5, 6, bmDC, 100, 12); // blank
		if (bitrate / 10)
			BitBlt(mainDC, 111 + 5, 28 + 15, 5, 6, bmDC, (((bitrate / 10) % 10)) * 5, 6);
		else
			BitBlt(mainDC, 111 + 5, 28 + 15, 5, 6, bmDC, 100, 12);
		BitBlt(mainDC, 111 + 5 * 2, 28 + 15, 5, 6, bmDC, ((bitrate % 10)) * 5, 6);
	}

	if (mixrate / 10) BitBlt(mainDC, 156, 28 + 15, 5, 6, bmDC, (((mixrate / 10) % 10)) * 5, 6);
	else BitBlt(mainDC, 156, 28 + 15, 5, 6, bmDC, 100, 12);
	BitBlt(mainDC, 156 + 5, 28 + 15, 5, 6, bmDC, ((mixrate % 10)) * 5, 6);
	update_area(111, 28 + 15, 176 - 111, 6);
	l1 = bitrate;
	l2 = mixrate;
}

void draw_positionbar(int position, int pressed) // position is 0-256
{
	RECT r;
	int op = position;
	if (!draw_initted) return;
	position = (position * (248 - 29)) / 256;
	if (position < 0) position = 0;
	if (position > (248 - 29)) position = 248 - 29;
	r.left = 9 + 7;
	r.top = 57 + 15;
	r.right = r.left + 248;
	r.bottom = r.top + 10;
	const Bitmap &bmDC = g_skin.posbar;
	if (position)
		BitBlt(mainDC, r.left, r.top, position, r.bottom - r.top, bmDC, 0, 0);
	BitBlt(mainDC, r.left + position, r.top, 29, 10, bmDC, pressed ? 278 : 248, 0);
	if (position != 248 - 29)
		BitBlt(mainDC, r.left + position + 29, r.top, 248 - 29 - position, r.bottom - r.top, bmDC, position + 29, 0);
	update_rect(r);
	if (config_windowshade)
	{
		int a;
		r.left = 226;
		r.top = 4;
		r.right = 243;
		r.bottom = 11;
		op = (op * 12) / 256;
		if (++op < 1) op = 1;
		if (op > 13) op = 13;
		if (op < 6) a = 0;
		else if (op < 9) a = 1;
		else a = 2;
		BitBlt(mainDC, r.left, r.top, r.right - r.left, r.bottom - r.top, g_skin.titlebar, 0, 36);
		BitBlt(mainDC, r.left + op, r.top, 3, r.bottom - r.top, g_skin.titlebar, 17 + a * 3, 36);
		update_rect(r);
	}
}

void draw_monostereo(int value) // 0 is clear, 1 is mono, 2 is stereo
{
	static int l = -1;
	if (!draw_initted) return;
	if (value < 0) value = l;
	if (value < 0) return;
	BitBlt(mainDC, 7 + 205, 41, 28, 12, g_skin.monoster, 29, value == 1 ? 0 : 12);
	BitBlt(mainDC, 7 + 232, 41, 29, 12, g_skin.monoster, 0, value == 2 ? 0 : 12);
	update_area(199, 41, 7 + 232 + 29 - 199, 12);
	l = value;
}

#define PANBAR_WIDTH 38
#define PANBAR_SLIDER_WIDTH 14
#define PANBAR_LENGTH (PANBAR_WIDTH - PANBAR_SLIDER_WIDTH)
#undef ABS
#define ABS(x) ((x) > 0 ? (x) : -(x))
void draw_panbar(int volume, int pressed) // volume is -127..127
{
	int ypos = ((ABS(volume) * 27) / 127) * 15;
	RECT r;
	if (!draw_initted) return;
	if (ypos < 0) ypos = 0;
	else if (ypos > 27 * 15) ypos = 27 * 15;
	r.left = 177;
	r.top = 42 + 15;
	r.right = r.left + PANBAR_WIDTH;
	r.bottom = r.top + 13;
	const Bitmap &bmDC = g_skin.balance;
	BitBlt(mainDC, r.left, r.top, r.right - r.left, r.bottom - r.top, bmDC, 9, ypos);
	{
		int xpos = ((volume * 12) / 127) + 12;
		if (xpos > PANBAR_LENGTH)
			xpos = PANBAR_LENGTH;
		else if (xpos < 0)
			xpos = 0;
		BitBlt(mainDC, r.left + xpos, r.top + 1, 14, 11, bmDC, pressed ? 0 : 15, 422);
	}
	update_rect(r);
}

void draw_shuffle(int on, int pressed)
{
	RECT r;
	if (!draw_initted) return;
	r.left = 164;
	r.top = 89;
	r.right = r.left + 79 - 28 - 4;
	r.bottom = r.top + 15;
	BitBlt(mainDC, r.left, r.top, r.right - r.left, r.bottom - r.top, g_skin.shufrep, 28, (on ? 30 : 0) + (pressed ? 15 : 0));
	update_rect(r);
}

void draw_repeat(int on, int pressed)
{
	RECT r;
	if (!draw_initted) return;
	r.left = 182 + 7 - 4 - 4 + 29;
	r.top = 60 + 14 + 15;
	r.right = r.left + 28;
	r.bottom = r.top + 15;
	BitBlt(mainDC, r.left, r.top, r.right - r.left, r.bottom - r.top, g_skin.shufrep, 0, (on ? 30 : 0) + (pressed ? 15 : 0));
	update_rect(r);
}

void update_panning_text(int songlen)
{
	wchar_t buf[128];
	int v = config_pan;
	v *= 100;
	v /= 127;
	if (v)
		swprintf(buf, 128, L"Balance: %d%% %ls", v < 0 ? -v : v, v < 0 ? L"left" : L"right");
	else
		swprintf(buf, 128, L"Balance: Center");
	v = 0;
	draw_songname(buf, &v, songlen);
}

void update_volume_text(int songlen)
{
	wchar_t buf[128];
	int v = config_volume;
	v *= 100;
	v /= 255;
	swprintf(buf, 128, L"Volume: %d%%", v);
	draw_songname(buf, &v, songlen);
}

#define VOLBAR_WIDTH 68
#define VOLBAR_SLIDER_WIDTH 14
#define VOLBAR_LENGTH (VOLBAR_WIDTH - VOLBAR_SLIDER_WIDTH)
void draw_volumebar(int volume, int pressed) // volume is 0-255
{
	int ypos = ((volume * 27) / 255) * 15;
	RECT r;
	if (!draw_initted) return;
	if (ypos < 0) ypos = 0;
	else if (ypos > 27 * 15) ypos = 27 * 15;
	r.left = 107;
	r.top = 42 + 15;
	r.right = r.left + VOLBAR_WIDTH;
	r.bottom = r.top + 13;
	const Bitmap &bmDC = g_skin.volume;
	BitBlt(mainDC, r.left, r.top, r.right - r.left, r.bottom - r.top, bmDC, 0, ypos);
	{
		int xpos = (volume * 51) / 255;
		if (xpos > VOLBAR_LENGTH) xpos = VOLBAR_LENGTH;
		else if (xpos < 0) xpos = 0;
		BitBlt(mainDC, r.left + xpos, r.top + 1, 14, 11, bmDC, pressed ? 0 : 15, 422);
	}
	update_rect(r);
}

void draw_eqplbut(int eqon, int eqpressed, int plon, int plpressed)
{
	RECT urect = {219, 58, 265, 70};
	int x, y;
	if (!draw_initted) return;
	x = eqpressed ? 46 : 0;
	y = eqon ? 12 : 0;
	BitBlt(mainDC, urect.left, urect.top, (urect.right - urect.left) / 2, urect.bottom - urect.top, g_skin.shufrep, x, y + 61);
	x = plpressed ? 46 : 0;
	y = plon ? 12 : 0;
	BitBlt(mainDC, urect.left + (urect.right - urect.left) / 2, urect.top, (urect.right - urect.left) / 2, urect.bottom - urect.top, g_skin.shufrep, x + 23, y + 61);
	update_rect(urect);
}

void draw_clutterbar(int enable)
{
	int x, y;
	if (!draw_initted) return;
	if (config_ascb_new && !enable) enable = 1;
	if (!enable)
	{
		x = 8;
		y = 0;
	}
	else if (enable == 1)
	{
		x = 0;
		y = 0;
	}
	else
	{
		y = 44;
		x = (enable - 2) * 8;
	}
	const Bitmap &tbBM = g_skin.titlebar;
	BitBlt(mainDC, 10, 22, 18 - 10, 65 - 22, tbBM, 304 + x, y);
	if (enable != 3 && (config_ascb_new || enable))
	{
		if (config_aot)
			BitBlt(mainDC, 11, 22 + 11, 18 - 10 - 1, 65 - 22 - 34 - 1, tbBM, 312 + 1, 44 + 11);
		else
			BitBlt(mainDC, 11, 22 + 11, 18 - 10 - 1, 65 - 22 - 34 - 1, tbBM, 304 + 1, 11);
	}
	if (enable != 5 && (config_ascb_new || enable))
	{
		if (config_dsize)
			BitBlt(mainDC, 11, 22 + 27, 18 - 10 - 1, 6, tbBM, 328 + 1, 44 + 27);
		else
			BitBlt(mainDC, 11, 22 + 27, 18 - 10 - 1, 6, tbBM, 304 + 1, 27);
	}
	update_area(10, 22, 8, 65 - 22);
}

// draw_clear(): restore the skin's main.bmp and the static parts
void draw_clear()
{
	if (!draw_initted) return;
	BitBlt(mainDC, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, g_skin.main, 0, 0);
	draw_playicon(2);
	draw_tbar(config_hilite ? (g_main_wnd && g_main_wnd->Active() ? 1 : 0) : 1, config_windowshade, eggstat);
	update_area(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
}

// the full main window for the current state (after skin changes, windowshade etc.)
void draw_main_all()
{
	if (!draw_initted) return;
	int old = updateen;
	updateen = 0;
	draw_clear();
	draw_tbuttons(0, 0, 0, 0);
	draw_buttonbar(-1);
	draw_eject(0);
	draw_shuffle(config_shuffle, 0);
	draw_repeat(config_repeat, 0);
	draw_eqplbut(config_eq_open, 0, config_pe_open, 0);
	draw_clutterbar(0);
	draw_volumebar(config_volume, 0);
	draw_panbar(config_pan, 0);
	if (playing)
	{
		draw_playicon(paused ? 4 : 1);
		draw_monostereo(g_nch <= 0 ? 0 : g_nch == 1 ? 1 : 2);
		draw_bitmixrate(g_brate, g_srate);
		int t = in_getouttime() / 1000;
		draw_time(t / 60, t % 60, 0);
		if (in_seekable() && in_getlength() > 0)
			draw_positionbar((t * 256) / in_getlength(), 0);
		else
			draw_positionbar(0, 0);
	}
	else
	{
		draw_playicon(2);
		draw_monostereo(0);
	}
	draw_songname_title();
	updateen = old;
	update_area(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
}

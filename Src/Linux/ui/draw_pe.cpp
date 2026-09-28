/*
** Winamp for Linux - playlist editor drawing, ported from
** Src/Winamp/draw_pe.cpp. Coordinates and pledit.bmp offsets are the
** original ones; text goes through Pango instead of DrawTextW().
*/
#include "ui.h"
#include "../core/player.h"
#include "../core/playlist.h"

#include <stdio.h>
#include <wctype.h>
#include <algorithm>

int pe_fontheight = 12;
int pledit_disp_offs = 0;
TextFont plfont;

static Bitmap &hdcout()
{
	return g_pl_wnd->Buffer();
}

static const Bitmap &bmDC()
{
	return g_skin.pledit;
}

void draw_pe_init()
{
	plfont.family = !config_plfont.empty() ? config_plfont : (!g_skin.pl_font.empty() ? g_skin.pl_font : "Arial");
	plfont.pixel_size = config_pe_fontsize;
	plfont.antialias = false;
	pe_fontheight = plfont.Height();
	if (pe_fontheight < 1) pe_fontheight = 1;
}

int pe_num_songs()
{
	return (config_pe_height - 38 - 20 - 2) / pe_fontheight;
}

static void invalidate(int x, int y, int w, int h)
{
	if (g_pl_wnd) g_pl_wnd->InvalidateRect(x, y, w, h);
}

void draw_pe_iobut(int which) // -1 = none, 0 = load, 1=save, 2=clear
{
	if (!g_pl_wnd) return;
	int offs = config_pe_width - 44;
	BitBlt(hdcout(), offs - 3, config_pe_height - 30 - 18 * 2, 3, 18 * 3, bmDC(), 250, 111);
	BitBlt(hdcout(), offs, config_pe_height - 30 - 18 * 2, 22, 18, bmDC(), (which == 2) ? 227 : 204, 111);
	BitBlt(hdcout(), offs, config_pe_height - 30 - 18, 22, 18, bmDC(), (which == 1) ? 227 : 204, 130);
	BitBlt(hdcout(), offs, config_pe_height - 30, 22, 18, bmDC(), (which == 0) ? 227 : 204, 149);
	invalidate(offs - 3, config_pe_height - 30 - 18 * 2, 25, 18 * 3);
}

static std::string last_infostr;

static void draw_pe_infostr(const std::wstring &str)
{
	std::wstring data = str.substr(0, 18);
	while (data.size() < 18) data += L' ';
	int xp = config_pe_width - 143, yp = config_pe_height - 28;
	for (int x = 0; x < 18; x++)
	{
		int c = 0, c2 = 0;
		getXYfromChar(data[x], &c, &c2);
		BitBlt(hdcout(), xp, yp, 5, 6, g_skin.text, c, c2);
		xp += 5;
	}
}

void draw_pe_miscbut(int which) // -1 = none, 0 = inf, 1 = sort, 2=misc
{
	if (!g_pl_wnd) return;
	BitBlt(hdcout(), 98, config_pe_height - 30 - 18 * 2, 3, 18 * 3, bmDC(), 200, 111);
	BitBlt(hdcout(), 101, config_pe_height - 30 - 18 * 2, 22, 18, bmDC(), (which == 2) ? 177 : 154, 111);
	BitBlt(hdcout(), 101, config_pe_height - 30 - 18, 22, 18, bmDC(), (which == 1) ? 177 : 154, 130);
	BitBlt(hdcout(), 101, config_pe_height - 30, 22, 18, bmDC(), (which == 0) ? 177 : 154, 149);
	invalidate(98, config_pe_height - 30 - 18 * 2, 25, 18 * 3);
}

void draw_pe_selbut(int which) // -1 = none, 0 = all, 1 = none, 2=inv
{
	if (!g_pl_wnd) return;
	BitBlt(hdcout(), 69, config_pe_height - 30 - 18 * 2, 3, 18 * 3, bmDC(), 150, 111);
	BitBlt(hdcout(), 72, config_pe_height - 30 - 18 * 2, 22, 18, bmDC(), (which == 2) ? 127 : 104, 111);
	BitBlt(hdcout(), 72, config_pe_height - 30 - 18, 22, 18, bmDC(), (which == 1) ? 127 : 104, 130);
	BitBlt(hdcout(), 72, config_pe_height - 30, 22, 18, bmDC(), (which == 0) ? 127 : 104, 149);
	invalidate(69, config_pe_height - 30 - 18 * 2, 25, 18 * 3);
}

void draw_pe_rembut(int which) // -1 = none, 0 = sel, 1 = crop, 2 = all, 3 = misc
{
	if (!g_pl_wnd) return;
	BitBlt(hdcout(), 40, config_pe_height - 30 - 18 * 3, 3, 18 * 4, bmDC(), 100, 111);
	BitBlt(hdcout(), 43, config_pe_height - 30 - 18 * 3, 22, 18, bmDC(), (which == 3) ? 77 : 54, 168);
	BitBlt(hdcout(), 43, config_pe_height - 30 - 18 * 2, 22, 18, bmDC(), (which == 2) ? 77 : 54, 111);
	BitBlt(hdcout(), 43, config_pe_height - 30 - 18, 22, 18, bmDC(), (which == 1) ? 77 : 54, 130);
	BitBlt(hdcout(), 43, config_pe_height - 30, 22, 18, bmDC(), (which == 0) ? 77 : 54, 149);
	invalidate(40, config_pe_height - 30 - 18 * 3, 25, 18 * 4);
}

void draw_pe_addbut(int which) // -1 = none, 0 = file, 1 = dir, 2 = loc
{
	if (!g_pl_wnd) return;
	BitBlt(hdcout(), 11, config_pe_height - 30 - 18 * 2, 3, 18 * 3, bmDC(), 48, 111);
	BitBlt(hdcout(), 14, config_pe_height - 30 - 18 * 2, 22, 18, bmDC(), (which == 2) ? 23 : 0, 111);
	BitBlt(hdcout(), 14, config_pe_height - 30 - 18, 22, 18, bmDC(), (which == 1) ? 23 : 0, 130);
	BitBlt(hdcout(), 14, config_pe_height - 30, 22, 18, bmDC(), (which == 0) ? 23 : 0, 149);
	invalidate(11, config_pe_height - 30 - 18 * 2, 25, 18 * 3);
}

void draw_pe_vslide(int pushed, int pos) // pos 0..playlist_getlength()-num_songs
{
	if (!g_pl_wnd) return;
	int track_h = config_pe_height - 20 - 38;
	int slid_h = 18;
	int yp, y, oy;
	{
		int num_songs = pe_num_songs();
		int t = PlayList_getlength() - num_songs;
		if (t < 1) yp = 0;
		else
		{
			if (pos > t) pos = t;
			yp = ((track_h - slid_h) * pos) / t;
			if (yp < 0) yp = 0;
		}
	}
	Bitmap &out = hdcout();
	const Bitmap &src = bmDC();
	for (y = 0; y < yp - 28; y += 29)
		BitBlt(out, config_pe_width - 15, 20 + y, 8, 29, src, 36, 42);
	oy = y + 29;
	if (slid_h + yp > oy) oy += 29;
	if (y < yp) BitBlt(out, config_pe_width - 15, 20 + y, 8, yp - y, src, 36, 42);
	BitBlt(out, config_pe_width - 15, 20 + yp, 8, slid_h, src, pushed ? 61 : 52, 53);
	y = yp + slid_h;
	if (y < oy && y < track_h) BitBlt(out, config_pe_width - 15, 20 + y, 8, ((y < track_h - 29) ? oy - y : track_h - y), src, 36, 42 + 29 - (oy - y));
	for (y = oy; y < track_h; y += 29)
		BitBlt(out, config_pe_width - 15, 20 + y, 8, (y < track_h - 29) ? 29 : track_h - y, src, 36, 42);
	invalidate(config_pe_width - 15, 20, 8, track_h);
}

static void draw_pe_song(int pos, std::wstring name, int time, int sel)
{
	Bitmap &out = hdcout();
	int ypos, left, right, num_songs, fontHeight;
	const TextFont *useFont;
	uint32_t foreground, background;
	if (pos < 0)
	{
		right = config_pe_width - 29;
		ypos = config_bifont ? 4 : 2;
		left = 4;
		num_songs = 1;
		useFont = &mfont;
		fontHeight = 10;
		sel = 0;
		foreground = mfont_fgcolor;
		background = mfont_bgcolor;
	}
	else
	{
		fontHeight = pe_fontheight;
		right = config_pe_width - 20;
		ypos = 22 + pos * fontHeight;
		left = 12;
		num_songs = pe_num_songs();
		useFont = &plfont;
		foreground = colorref_to_rgb((sel & 2) ? g_skin.pl_colors[1] : g_skin.pl_colors[0]);
		background = colorref_to_rgb(!(sel & 1) ? g_skin.pl_colors[2] : g_skin.pl_colors[3]);
	}
	if (pos >= num_songs) return;

	int xpos = left;
	int endp = right;
	int num_chars = (endp - xpos) / 5;
	if (pos >= 0 || !config_bifont)
	{
		RECT r = {xpos, ypos, endp, ypos + fontHeight};
		FillRect(out, r.left, r.top, r.right - r.left, r.bottom - r.top, background);
		if (time >= 0)
		{
			char str[123];
			snprintf(str, sizeof(str), " %d:%02d ", time / 60, time % 60);
			int tw = useFont->TextWidth(str);
			if (tw < endp - xpos)
			{
				RECT r2 = {endp - tw, r.top - (pos == -1 ? 1 : 0), endp, r.bottom - (pos == -1 ? 1 : 0)};
				DrawText(out, r2, str, *useFont, foreground, CLR_INVALID, DT_RIGHT | DT_VCENTER);
				r.right -= tw;
				endp -= tw;
			}
		}
		if (pos == -1)
			for (auto &c : name) c = towupper(c);
		DrawText(out, r, wa::narrow(name), *useFont, foreground, CLR_INVALID, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
		if (pos >= 0 && (!pos || pos == num_songs - 1))
		{
			uint32_t normal = colorref_to_rgb(g_skin.pl_colors[2]);
			if (!pos)
				FillRect(out, left, 20, right - left, 23 - 20, normal);
			if (pos == num_songs - 1)
				FillRect(out, left, ypos + fontHeight, right - left, config_pe_height - 38 - (ypos + fontHeight), normal);
		}
	}
	else
	{
		const Bitmap &fontBM = g_skin.text;
		int x;
		char str[16] = "";
		if (time >= 0)
		{
			snprintf(str, sizeof(str), "%d:%02d", time / 60, (time) % 60);
			num_chars -= (int)strlen(str) + 1;
		}

		for (x = 0; x < num_chars && x < (int)name.size(); x++)
		{
			int c2 = 0, c = 0;
			wchar_t oc = name[x];
			if (x == num_chars - 1 && x + 1 < (int)name.size()) oc = L'\1';
			getXYfromChar(oc, &c, &c2);
			BitBlt(out, xpos, ypos, 5, 6, fontBM, c, c2);
			xpos += 5;
		}

		if (x >= (int)name.size())
		{
			int c2 = 0, c = 0;
			getXYfromChar(L' ', &c, &c2);
			if (time >= 0) num_chars++;
			while (x++ < num_chars)
			{
				BitBlt(out, xpos, ypos, 5, 6, fontBM, c, c2);
				xpos += 5;
			}
		}

		if (time >= 0)
		{
			int c2 = 0, c = 0;
			getXYfromChar(' ', &c, &c2);
			BitBlt(out, xpos, ypos, 5, 6, fontBM, c, c2);
			xpos += 5;
			if (pos < 0)
				xpos = config_pe_width - 29 - 5 * (int)strlen(str);
			else
				xpos = config_pe_width - 24 - 5 * (int)strlen(str);
			for (x = 0; str[x]; x++)
			{
				getXYfromChar(str[x], &c, &c2);
				BitBlt(out, xpos, ypos, 5, 6, fontBM, c, c2);
				xpos += 5;
			}
		}

		if (xpos < config_pe_width - (pos < 0 ? 30 : 20))
		{
			int c2 = 0, c = 0;
			getXYfromChar(' ', &c, &c2);
			BitBlt(out, xpos, ypos, config_pe_width - (pos < 0 ? 30 : 20) - xpos, 6, fontBM, c, c2);
		}
	}
}

static void draw_pe_tbar(int state)
{
	Bitmap &out = hdcout();
	const Bitmap &src = bmDC();
	state = state ? 0 : 21;
	if (config_pe_height != 14)
	{
		int nt;
		int xp = 0;
		BitBlt(out, xp, 0, 25, 20, src, 0, state);
		xp += 25;
		nt = (config_pe_width - 25 - 25 - 100) / 25;
		if (nt)
		{
			if (nt & 1)
			{
				BitBlt(out, xp, 0, 12, 20, src, 127, state);
				xp += 12;
			}
			nt /= 2;
			while (nt-- > 0)
			{
				BitBlt(out, xp, 0, 25, 20, src, 127, state);
				xp += 25;
			}
		}

		BitBlt(out, xp, 0, 100, 20, src, 26, state);
		xp += 100;
		nt = (config_pe_width - 25 - 25 - 100) / 25;
		if (nt)
		{
			if (nt & 1)
			{
				BitBlt(out, xp, 0, 13, 20, src, 127, state);
				xp += 13;
			}
			nt /= 2;
			while (nt-- > 0)
			{
				BitBlt(out, xp, 0, 25, 20, src, 127, state);
				xp += 25;
			}
		}
		nt = (config_pe_width - 25 - 25 - 100) % 25;
		if (nt)
		{
			StretchBlt(out, xp, 0, nt, 20, src, 127, state, 25, 20);
			xp += nt;
		}
		BitBlt(out, xp, 0, 25, 20, src, 153, state);
	}
	else
	{
		int xpos = 0;
		int n = (config_pe_width - 50) / 25;
		BitBlt(out, xpos, 0, 25, 14, src, 72, 42);
		xpos += 25;
		n--;
		while (n-- > 0)
		{
			BitBlt(out, xpos, 0, 25, 14, src, 72, 57);
			xpos += 25;
		}
		n = (config_pe_width - 50) % 25;
		if (n)
		{
			StretchBlt(out, xpos, 0, n, 14, src, 72, 57, 24, 14);
			xpos += n;
		}
		BitBlt(out, xpos, 0, 50, 14, src, 99, state ? 57 : 42);
	}

	if (config_pe_height == 14)
	{
		std::wstring ft;
		int q = PlayList_getPosition();
		if (q >= 0 && q < PlayList_getlength())
		{
			ft = wa::widen(PlayList_getitem_pl(q));
			q = PlayList_getsonglength(q);
		}
		else
		{
			ft = L"[No file]";
			q = -1;
		}
		draw_pe_song(-1, ft, q, state ? 0 : 2);
	}
}

void draw_pe_timedisp(int minutes, int seconds, int tlm, int clear)
{
	static int lastm, lasts, lastc = 1, lasttlm;
	if (!g_pl_wnd) return;
	int x = config_pe_width - 94 + 8;
	int y = config_pe_height - 15;
	if (minutes == -666)
	{
		minutes = lastm;
		seconds = lasts;
		clear = lastc;
		tlm = lasttlm;
	}
	lastm = minutes;
	lasts = seconds;
	lastc = clear;
	lasttlm = tlm;
	if (config_pe_height == 14) return;

	Bitmap &mainDC = hdcout();
	const Bitmap &bm = g_skin.text;
	if (clear)
	{
		BitBlt(mainDC, x, y, 4, 6, bm, 142, 0);
		BitBlt(mainDC, x + 4, y, 5, 6, bm, 142, 0);
		BitBlt(mainDC, x + 5 + 4, y, 5, 6, bm, 142, 0);
		BitBlt(mainDC, x + 5 + 4 + 5, y, 5, 6, bm, 142, 0);
		BitBlt(mainDC, x + 5 + 4 + 5 + 4 + 4, y, 5, 6, bm, 142, 0);
		BitBlt(mainDC, x + 5 + 4 + 5 + 4 + 5 + 4, y, 5, 6, bm, 142, 0);
	}
	else
	{
		if (tlm)
		{
			if (minutes < 0) minutes = -minutes;
			if (seconds < 0) seconds = -seconds;
			if (!(minutes / 100)) BitBlt(mainDC, x, y, 4, 6, bm, 142, 0);
			BitBlt(mainDC, x + ((minutes / 100) ? 0 : 4), y, 3, 6, bm, 75, 6);
		}
		else
		{
			if (!(minutes / 100)) BitBlt(mainDC, x, y, 4, 6, bm, 142, 0);
			BitBlt(mainDC, x + ((minutes / 100) ? 0 : 4), y, 3, 6, bm, 142, 0);
		}

		if (minutes / 100) BitBlt(mainDC, x + 4, y, 5, 6, bm, 5 * ((minutes / 100) % 10), 6);
		BitBlt(mainDC, x + 5 + 4, y, 5, 6, bm, 5 * ((minutes / 10) % 10), 6);
		BitBlt(mainDC, x + 5 + 4 + 5, y, 5, 6, bm, 5 * ((minutes) % 10), 6);
		BitBlt(mainDC, x + 5 + 4 + 5 + 4 + 4, y, 5, 6, bm, 5 * ((seconds / 10) % 10), 6);
		BitBlt(mainDC, x + 5 + 4 + 5 + 4 + 5 + 4, y, 5, 6, bm, 5 * ((seconds) % 10), 6);
	}
	invalidate(x, y, 40, 6);
}

void draw_pe_tbutton(int b2, int b3, int b2_ws)
{
	if (!g_pl_wnd) return;
	Bitmap &out = hdcout();
	const Bitmap &src = bmDC();
	if (!b3)
		BitBlt(out, config_pe_width - 11, 3, 9, 9, src, 167, 3);
	else
		BitBlt(out, config_pe_width - 11, 3, 9, 9, src, 52, 42);
	if (!b2)
	{
		if (!b2_ws) BitBlt(out, config_pe_width - 20, 3, 9, 9, src, 158, 3);
		else BitBlt(out, config_pe_width - 20, 3, 9, 9, src, 128, 45);
	}
	else
		BitBlt(out, config_pe_width - 20, 3, 9, 9, src, b2_ws ? 150 : 62, 42);
	invalidate(config_pe_width - 20, 3, 18, 9);
}

static void draw_pl()
{
	Bitmap &out = hdcout();
	const Bitmap &src = bmDC();
	draw_pe_tbar(g_pl_wnd->Active() ? 1 : (config_hilite ? 0 : 1));
	if (config_pe_height != 14)
	{
		int y;
		int yp = 20;
		y = (config_pe_height - 20 - 38) / 29;
		while (y-- > 0)
		{
			BitBlt(out, 0, yp, 12, 29, src, 0, 42);
			BitBlt(out, config_pe_width - 20, yp, 5, 29, src, 31, 42);
			BitBlt(out, config_pe_width - 7, yp, 7, 29, src, 31 + 13, 42);
			yp += 29;
		}
		y = (config_pe_height - 20 - 38) % 29;
		if (y)
		{
			StretchBlt(out, 0, yp, 12, y, src, 0, 42, 12, 29);
			StretchBlt(out, config_pe_width - 20, yp, 5, y, src, 31, 42, 5, 29);
			StretchBlt(out, config_pe_width - 7, yp, 7, y, src, 31 + 13, 42, 7, 29);
			yp += y;
		}

		BitBlt(out, 0, yp, 125, 38, src, 0, 72);
		int x = (config_pe_width - 125 - 150) / 25;

		int xp = 125, s = 0;
		if (x >= 3)
		{
			x -= 3;
			s = 1;
		}
		while (x-- > 0)
		{
			BitBlt(out, xp, yp, 25, 38, src, 179, 0);
			xp += 25;
		}
		x = (config_pe_width - 125 - 150) % 25;
		if (x)
		{
			StretchBlt(out, xp, yp, x, 38, src, 179, 0, 25, 38);
			xp += x;
		}
		if (s)
		{
			BitBlt(out, xp, yp, 75, 38, src, 205, 0);
			xp += 75;
		}
		BitBlt(out, xp, yp, 150, 38, src, 126, 72);
	}
}

static void draw_pl2()
{
	if (config_pe_height == 14) return;
	int x, t;
	int num_songs = pe_num_songs();
	if (pledit_disp_offs < 0) pledit_disp_offs = 0;
	t = PlayList_getlength() - pledit_disp_offs;
	if (t < 1)
	{
		pledit_disp_offs = 0;
		t = PlayList_getlength();
	}
	if (t > num_songs) t = num_songs;
	for (x = 0; x < t; x++)
	{
		int idx = x + pledit_disp_offs;
		draw_pe_song(x, wa::widen(PlayList_getitem_pl(idx)), PlayList_getsonglength(idx),
		             (PlayList_getselect(idx) ? 1 : 0) + (PlayList_getPosition() == idx ? 2 : 0));
	}
	for (; x < num_songs; x++)
		draw_pe_song(x, L" ", -1, 0);
	draw_pe_vslide(0, pledit_disp_offs);
	draw_pe_timedisp(-666, 0, 0, 0);

	// "selected time/total time"
	wchar_t str[64] = L"", str2[32] = L"";
	int v, p = 0;
	int seltime = 0, st2 = 0, st1 = 0, ttime = 0;
	v = PlayList_getlength();
	for (t = 0; t < v; t++)
	{
		int a = PlayList_getsonglength(t);
		if (PlayList_getselect(t))
		{
			p++;
			if (a < 0) st2 = 2;
			else seltime += a;
		}
		if (a < 0) st1 = 1;
		if (a > 0) ttime += a;
	}
	if (!seltime && !st2) swprintf(str, 64, L"0:00");
	else if (seltime)
	{
		if (seltime < 60 * 60) swprintf(str, 64, L"%d:%02d", seltime / 60, seltime % 60);
		else swprintf(str, 64, L"%d:%02d:%02d", seltime / 60 / 60, (seltime / 60) % 60, seltime % 60);
		if (st2) wcscat(str, L"+");
	}
	else if (st2) wcscat(str, L"?");

	if (!ttime && !st1) swprintf(str2, 32, L"0:00");
	else if (ttime)
	{
		if (ttime < 60 * 60) swprintf(str2, 32, L"%d:%02d", ttime / 60, ttime % 60);
		else swprintf(str2, 32, L"%d:%02d:%02d", ttime / 60 / 60, (ttime / 60) % 60, ttime % 60);
		if (st1) wcscat(str2, L"+");
	}
	else if (st1) wcscat(str2, L"?");
	wcscat(str, L"/");
	wcscat(str, str2);
	draw_pe_infostr(str);
}

void draw_pe_paint()
{
	if (!g_pl_wnd) return;
	draw_pl();
	draw_pl2();
}

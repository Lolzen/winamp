/*
** Winamp for Linux - the classic spectrum analyzer / oscilloscope renderer.
**
** draw_sa() is Src/Winamp/draw_sa.cpp unchanged in substance: it writes palette
** indices into the same 152x32 bottom-up "specData" DIB layout, so bars, peaks,
** dots and the oscilloscope styles come out exactly as on Windows. The result
** is converted with the skin's viscolor.txt palette and shown through the main
** window overlay (at 2x resolution in double size mode, like the original).
** vis_timer_tick() replaces the "Classic Viz" thread (bivis_thread in SA.cpp).
*/
#include "ui.h"
#include "../core/player.h"
#include "../core/vis.h"

#include <string.h>
#include <algorithm>

static unsigned char specData[76 * 2 * 16 * 2];

static void present(bool have_values)
{
	if (!g_main_wnd) return;
	int ws = config_windowshade && config_mw_open;
	int s = config_dsize ? 1 : 0;
	if (!have_values)
	{
		g_main_wnd->HideOverlay();
		return;
	}
	int w = (ws ? 38 : 76) << s, h = (ws ? 5 : 16) << s;
	Bitmap &o = g_main_wnd->Overlay();
	if (o.Width() != w || o.Height() != h) o.Create(w, h);
	// bottom-up DIB: image row y is memory row 31 - y
	for (int y = 0; y < h; y++)
	{
		const unsigned char *row = specData + (31 - y) * 76 * 2;
		uint32_t *out = o.Pixels() + (size_t)y * w;
		for (int x = 0; x < w; x++)
			out[x] = g_skin.viscolors[row[x] < 24 ? row[x] : 0];
	}
	if (ws) g_main_wnd->ShowOverlay(79, 5);
	else g_main_wnd->ShowOverlay(24, 43);
}

void draw_sa(unsigned char *values, int draw)
{
	static int bx[75];
	static int t_bx[75];
	static float t_vx[75];
	int x;
	int fo[5] = {3, 6, 12, 16, 32};
	float pfo[5] = {1.05f, 1.1f, 1.2f, 1.4f, 1.6f};
	int dbx;
	float spfo;

	int ws = (config_windowshade && config_mw_open);
	int s = (config_dsize && config_mw_open) ? 1 : 0;
	unsigned char *gmem;

	dbx = fo[std::max(std::min(config_safalloff, 4), 0)];
	spfo = pfo[std::max(std::min(config_sa_peak_falloff, 4), 0)];

	if (s && draw)
	{
		int y;
		gmem = specData;
		if (!ws)
		{
			for (y = 0; y < 8; y++)
			{
				// "*(int *)gmem = 0x0101" 76 times: two dotted rows of 2x2 dots
				for (int i = 0; i < 76 * 4; i++) gmem[i] = (i & 3) < 2 ? 1 : 0;
				gmem += 76 * 2 * 2;
				memset(gmem, 0, 76 * 2 * 2);
				gmem += 76 * 2 * 2;
			}
		}
		else
		{
			gmem += 76 * 2 * (32 - 10);
			for (y = 0; y < 10; y++)
			{
				memset(gmem, 0, 76);
				gmem += 76 * 2;
			}
		}
	}
	else if (draw)
	{
		int y;
		gmem = specData + 76 * 2 * 16;
		if (!ws)
		{
			for (y = 0; y < 8; y++)
			{
				for (int x = 0; x < 76; x++) gmem[x] = (x & 1) ? 0 : 1; // 0x10001: dot every other pixel
				gmem += 76 * 2;
				memset(gmem, 0, 76);
				gmem += 76 * 2;
			}
		}
		else
		{
			gmem += 76 * 2 * (16 - 5);
			for (y = 0; y < 5; y++)
			{
				memset(gmem, 0, 76 / 2);
				gmem += 76 * 2;
			}
		}
	}

	if (!values)
	{
		memset(bx, 0, 75 * sizeof(int));
	}
	else if (!s) // singlesize
	{
		if (!ws) // non windowshade singlesize
		{
			if (config_sa == 2) // non windowshade singlesize oscilliscope
			{
				gmem = specData + 76 * 2 * 14;
				if (draw)
				{
					int lv = -1;
					if (((config_safire >> 2) & 3) == 0) for (x = 0; x < 75; x++)
					{
						int v; char c;
						v = (((int)((signed char *)values)[x])) + 8;
						if (v < 0) v = 0; if (v > 15) v = 15; c = v / 2 - 4; if (c < 0) c = -c; c += 18;
						gmem[v * 76 * 2] = c;
						gmem++;
					}
					else if (((config_safire >> 2) & 3) == 1) for (x = 0; x < 75; x++)
					{
						int v, t; char c;
						v = (((int)((signed char *)values)[x])) + 8;
						if (v < 0) v = 0; if (v > 15) v = 15; c = v / 2 - 4; if (c < 0) c = -c; c += 18;
						if (lv == -1) lv = v;
						t = lv;
						lv = v;
						if (v >= t) while (v >= t) gmem[v-- * 76 * 2] = c;
						else while (v < t) gmem[v++ * 76 * 2] = c;
						gmem++;
					}
					else if (((config_safire >> 2) & 3) == 2) for (x = 0; x < 75; x++) // solid
					{
						int v; char c;
						v = (((int)((signed char *)values)[x])) + 8;
						if (v < 0) v = 0; if (v > 15) v = 15; c = v / 2 - 4; if (c < 0) c = -c; c += 18;
						if (v > 7) while (v > 7) gmem[v-- * 76 * 2] = c;
						else while (v <= 7) gmem[v++ * 76 * 2] = c;
						gmem++;
					}
				}
			}
			else // non windowshade singlesize spectrum analyzer
			{
				for (x = 0; x < 75; x++)
				{
					int y, v, t;
					t = x & ~3;
					if (!(config_safire & 32))
					{
						int a = values[t], b = values[t + 1], c = values[t + 2], d = values[t + 3];
						v = a + b + c + d;
						v /= 4;
					}
					else v = (((int)values[x]));
					if (v > 15) v = 15;
					if ((v << 4) < bx[x]) v = (bx[x] -= dbx) >> 4;
					else bx[x] = v << 4;
					if (bx[x] < 0) bx[x] = 0;
					if (v < 0) v = 0;
					gmem = specData + 76 * 2 * 14 + x;
					if ((config_safire & 3) == 1) t = v + 2;
					else if ((config_safire & 3) == 2) t = 17 - (v);
					else t = 17;

					if (t_bx[x] <= v * 256)
					{
						t_bx[x] = v * 256;
						t_vx[x] = 3.0f;
					}
					if (draw && (config_safire & 32 || (x & 3) != 3))
					{
						if ((config_safire & 3) != 2) for (y = 0; y < v; y++)
						{
							*gmem = t - y;
							gmem += 76 * 2;
						}
						else for (y = 0; y < v; y++)
						{
							*gmem = t;
							gmem += 76 * 2;
						}
						if (config_sa_peaks && t_bx[x] / 256 >= 0 && t_bx[x] / 256 <= 15)
						{
							specData[76 * 2 * 14 + (t_bx[x] / 256) * 76 * 2 + x] = 23;
						}
					}
					t_bx[x] -= (int)t_vx[x];
					t_vx[x] *= spfo;
					if (t_bx[x] < 0) t_bx[x] = 0;
				}
			}
		}
		else // windowshade singlesize
		{
			if (config_sa == 1) // windowshade singlesize spectrum analyzer
			{
				for (x = 0; x < 37; x++)
				{
					int y, v, t;
					t = ((x) & ~3) * 2;
					if (!(config_safire & 32))
					{
						int a = values[t], b = values[t + 1], c = values[t + 2], d = values[t + 3];
						v = a + b + c + d;
						v /= 4;
					}
					else v = (((int)values[x * 2]) + ((int)values[x * 2 + 1])) / 2;
					if (v > 15) v = 15;
					if ((v << 4) < bx[x * 2]) v = (bx[x * 2] -= dbx) >> 4;
					else bx[x * 2] = v << 4;
					if (bx[x * 2] < 0) bx[x * 2] = 0;
					if (v < 0) v = 0;
					gmem = specData + 76 * 2 * (32 - 5) + x;
					if ((config_safire & 3) == 1) t = v + 2;
					else if ((config_safire & 3) == 2) t = 17 - (v);
					else t = 17;

					if (t_bx[x * 2] <= v * 256)
					{
						t_bx[x * 2] = v * 256;
						t_vx[x * 2] = 3.0f;
					}
					v = (v * 5) / 15;
					if (v > 5) v = 5;
					if (draw && (config_safire & 32 || (x & 3) != 3))
					{
						int poo = (t_bx[x * 2] * 5) / 15 / 256;
						if ((config_safire & 3) != 2) for (y = 0; y < v; y++)
						{
							*gmem = t - (y * 15) / 5;
							gmem += 76 * 2;
						}
						else for (y = 0; y < v; y++)
						{
							*gmem = t;
							gmem += 76 * 2;
						}
						if (config_sa_peaks && poo >= 0 && poo <= 4)
						{
							specData[76 * 2 * (32 - 5) + poo * 76 * 2 + x] = 23;
						}
					}
					t_bx[x * 2] -= (int)t_vx[x * 2];
					t_vx[x * 2] *= spfo;
					if (t_bx[x * 2] < 0) t_bx[x * 2] = 0;
				}
			}
			else if (config_sa == 2) // windowshade singlesize oscilliscope
			{
				int wm = ((config_safire >> 2) & 3);
				int lastv = -5;
				gmem = specData + 76 * 2 * (32 - 5);
				for (x = 0; x < 38; x++)
				{
					int v = (((int)((signed char *)values)[x])) + 8;
					v *= 5;
					v /= 16;
					if (v < 0) v = 0; if (v > 4) v = 4;
					if (wm == 0 || lastv == -5)
					{
						lastv = v;
						gmem[x + v * 76 * 2] = 18;
					}
					else if (wm == 1)
					{
						int tmp = lastv;
						lastv = v;
						if (v >= tmp) while (v >= tmp) { gmem[x + v-- * 76 * 2] = 18; }
						else while (v <= tmp) { gmem[x + v++ * 76 * 2] = 18; }
					}
					else if (wm == 2)
					{
						if (v >= 2) while (v >= 2) { gmem[x + v-- * 76 * 2] = 18; }
						else while (v <= 2) { gmem[x + v++ * 76 * 2] = 18; }
					}
				}
			}
		}
	}
	else // doublesize
	{
		if (!ws)
		{
			if (config_sa == 2)
			{
				gmem = specData;
				if (draw)
				{
					int lv = -1;
					if (((config_safire >> 2) & 3) == 0) for (x = 0; x < 75 * 2; x += 2)
					{
						int v; char c;
						v = (((int)((signed char *)values)[x / 2])) + 8;
						if (v < 0) v = 0; if (v > 15) v = 15; c = v / 2 - 4; if (c < 0) c = -c; c += 18;
						gmem[v * 76 * 2 * 2] = c; gmem++[v * 76 * 2 * 2 + 76 * 2] = c;
						gmem[v * 76 * 2 * 2] = c; gmem++[v * 76 * 2 * 2 + 76 * 2] = c;
					}
					else if (((config_safire >> 2) & 3) == 1) for (x = 0; x < 75 * 2; x += 2)
					{
						int v, t; char c;
						v = (((int)((signed char *)values)[x / 2])) + 8;
						if (v < 0) v = 0; if (v > 15) v = 15; c = v / 2 - 4; if (c < 0) c = -c; c += 18;
						if (lv == -1) lv = v;
						t = lv;
						lv = v;
						if (v >= t) while (v >= t)
						{
							gmem[v * 76 * 2 * 2] = c;
							gmem[v * 76 * 2 * 2 + 76 * 2] = c;
							gmem[v * 76 * 2 * 2 + 1] = c;
							gmem[v * 76 * 2 * 2 + 76 * 2 + 1] = c;
							v--;
						}
						else while (v < t)
						{
							gmem[v * 76 * 2 * 2] = c;
							gmem[v * 76 * 2 * 2 + 76 * 2] = c;
							gmem[v * 76 * 2 * 2 + 1] = c;
							gmem[v * 76 * 2 * 2 + 76 * 2 + 1] = c;
							v++;
						}
						gmem += 2;
					}
					else if (((config_safire >> 2) & 3) == 2) for (x = 0; x < 75 * 2; x += 2)
					{
						int v; char c;
						v = (((int)((signed char *)values)[x / 2])) + 8;
						if (v < 0) v = 0; if (v > 15) v = 15; c = v / 2 - 4; if (c < 0) c = -c; c += 18;
						if (v > 7) while (v > 7)
						{
							gmem[v * 76 * 2 * 2] = c;
							gmem[v * 76 * 2 * 2 + 76 * 2] = c;
							gmem[v * 76 * 2 * 2 + 1] = c;
							gmem[v * 76 * 2 * 2 + 76 * 2 + 1] = c;
							v--;
						}
						else while (v <= 7)
						{
							gmem[v * 76 * 2 * 2] = c;
							gmem[v * 76 * 2 * 2 + 76 * 2] = c;
							gmem[v * 76 * 2 * 2 + 1] = c;
							gmem[v * 76 * 2 * 2 + 76 * 2 + 1] = c;
							v++;
						}
						gmem += 2;
					}
				}
			}
			else
			{
				for (x = 0; x < 75 * 2;)
				{
					int y, v, t;
					t = (x / 2) & ~3;
					if (!(config_safire & 32))
					{
						int a = values[t], b = values[t + 1], c = values[t + 2], d = values[t + 3];
						v = a + b + c + d;
						v /= 4;
					}
					else v = (((int)values[x / 2]));
					if (v > 15) v = 15;
					if ((v << 4) < bx[x / 2]) v = (bx[x / 2] -= dbx) >> 4;
					else bx[x / 2] = v << 4;
					if (bx[x / 2] < 0) bx[x / 2] = 0;
					if (v < 0) v = 0;
					gmem = specData + x;
					if ((config_safire & 3) == 1) t = v + 2;
					else if ((config_safire & 3) == 2) t = 17 - v;
					else t = 17;
					if (t_bx[x / 2] <= v * 256)
					{
						t_bx[x / 2] = v * 256;
						t_vx[x / 2] = 3.0f;
					}
					v *= 2;
					if (draw && (config_safire & 32 || ((x / 2) & 3) != 3))
					{
						if ((config_safire & 3) != 2) for (y = 0; y < v; y++)
						{
							gmem[0] = gmem[1] = t - y / 2;
							gmem += 76 * 2;
						}
						else for (y = 0; y < v; y++)
						{
							gmem[0] = gmem[1] = t;
							gmem += 76 * 2;
						}
						if (config_sa_peaks && t_bx[x / 2] / 256 > 0 && t_bx[x / 2] / 256 <= 15)
						{
							specData[(t_bx[x / 2] / 256) * 76 * 4 + x] = specData[(t_bx[x / 2] / 256) * 76 * 4 + x + 1] = 23;
							specData[(t_bx[x / 2] / 256) * 76 * 4 + x + 76 * 2] = specData[(t_bx[x / 2] / 256) * 76 * 4 + x + 1 + 76 * 2] = 23;
						}
					}
					t_bx[x / 2] -= (int)t_vx[x / 2];
					t_vx[x / 2] *= spfo;
					if (t_bx[x / 2] < 0) t_bx[x / 2] = 0;
					x += 2;
				}
			}
		}
		else
		{
			if (config_sa == 2) // doublesize window shade scope
			{
				int wm = ((config_safire >> 2) & 3);
				int lastv = -5;
				gmem = specData + 76 * 2 * (32 - 10);
				for (x = 0; x < 75; x++)
				{
					int v = (((int)((signed char *)values)[x])) + 8;
					v *= 10;
					v /= 16;
					if (v < 0) v = 0; if (v > 9) v = 9;
					if (wm == 0 || lastv == -5)
					{
						lastv = v;
						gmem[x + v * 76 * 2] = 18;
					}
					else if (wm == 1)
					{
						int tmp = lastv;
						lastv = v;
						if (v >= tmp) while (v >= tmp) { gmem[x + v-- * 76 * 2] = 18; }
						else while (v <= tmp) { gmem[x + v++ * 76 * 2] = 18; }
					}
					else if (wm == 2)
					{
						if (v >= 4) while (v >= 4) { gmem[x + v-- * 76 * 2] = 18; }
						else while (v <= 4) { gmem[x + v++ * 76 * 2] = 18; }
					}
				}
			}
			if (config_sa == 1) // doublesize window shade spectrum
			{
				for (x = 0; x < 75; x++)
				{
					int y, v, t;
					t = (x) & ~3;
					if (!(config_safire & 32))
					{
						int a = values[t], b = values[t + 1], c = values[t + 2], d = values[t + 3];
						v = a + b + c + d;
						v /= 4;
					}
					else v = (int)values[x];
					if (v > 15) v = 15;
					if ((v << 4) < bx[x]) v = (bx[x] -= dbx) >> 4;
					else bx[x] = v << 4;
					if (bx[x] < 0) bx[x] = 0;
					if (v < 0) v = 0;
					gmem = specData + 76 * 2 * (32 - 10) + x;
					if ((config_safire & 3) == 1) t = v + 2;
					else if ((config_safire & 3) == 2) t = 17 - (v);
					else t = 17;

					if (t_bx[x] <= v * 256)
					{
						t_bx[x] = v * 256;
						t_vx[x] = 3.0f;
					}
					v = (v * 10) / 15;
					if (draw && (config_safire & 32 || (x & 3) != 3))
					{
						int poo = (t_bx[x] * 10) / 15 / 256;
						if ((config_safire & 3) != 2) for (y = 0; y < v; y++)
						{
							*gmem = t - (y * 15) / 10;
							gmem += 76 * 2;
						}
						else for (y = 0; y < v; y++)
						{
							*gmem = t;
							gmem += 76 * 2;
						}
						if (config_sa_peaks && poo >= 0 && poo <= 9)
						{
							specData[76 * 2 * (32 - 10) + poo * 76 * 2 + x] = 23;
						}
					}
					t_bx[x] -= (int)t_vx[x];
					t_vx[x] *= spfo;
					if (t_bx[x] < 0) t_bx[x] = 0;
				}
			}
		}
	}

	if (draw && config_mw_open)
		present(values != nullptr);
}

// bivis_thread(): ~60 times a second, drawing every config_saref'th time
void vis_timer_tick()
{
	static int cycleCount = 0;
	static bool blanked = true;
	char data[75 * 2 + 8];

	if (config_sa && !paused && playing && !config_minimized && config_mw_open)
	{
		int draw;
		if (++cycleCount < config_saref)
			draw = 0;
		else
		{
			cycleCount = 0;
			draw = 1;
		}
		int t = config_sa;
		char *c = sa_get(in_getouttime(), t, data);
		if (c)
		{
			if (t == 2) c += 75;
			else memset(c + 75, 0, 4);
			draw_sa((unsigned char *)c, draw);
			blanked = false;
		}
	}
	else if (!blanked && !(paused && playing))
	{
		draw_sa(nullptr, 1);
		blanked = true;
	}
}
